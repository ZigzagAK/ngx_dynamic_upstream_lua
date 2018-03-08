local _M = {
  _VERSION = "1.5.2"
}

local cjson = require "cjson"

--- Pre checks --------------------------------------------------------------------------------

if not ngx.config or not ngx.config.ngx_lua_version or ngx.config.ngx_lua_version < 9005 then
  error("ngx_lua 0.9.5+ required")
end

local lock, http

do
  local ok

  ok, lock = pcall(require, "resty.lock")
  if not ok then
    error("resty.lock module required")
  end

  ok, http = pcall(require, "resty.http")
  if not ok then
    error("resty.http module required")
  end
end

--- Log wrappers-------------------------------------------------------------------------------

local debug_enabled = false

--- Implementation ----------------------------------------------------------------------------

local function foreachi(t, f)
  for i=1,#t do f(t[i]) end
end

local function set_peer_state_globally(ctx, peer, state)
  local ok, _, err = state.fun(peer.upstream.name, peer)
  if not ok then
    ctx:errlog("failed to set peer state: ", err)
  end
  peer.down = state.down
  ctx.dict:set_dict_key("down", peer, peer.down)
end

local function state_up(ctx)
  return {
    fun = ctx.set_peer_up,
    down = false
  }
end

local function state_down(ctx)
  return {
    fun = ctx.set_peer_down,
    down = true
  }
end

local function peer_fail(ctx, peer)
  local fails = ctx.dict:get_dict_key("fail", peer, 0)
  if not fails then
    return
  end

  fails = fails + 1

  if fails == 1 then
    ctx.dict:set_dict_key("succ", peer, 0)
  end

  ctx.dict:set_dict_key("fail", peer, fails)

  if fails >= peer.upstream.healthcheck.fall and not peer.down then
    ctx:warn("upstream: ", peer.upstream.name, " peer: ", peer.name, " is turned down after ", fails, " failure(s)")
    set_peer_state_globally(ctx, peer, state_down(ctx))
  end

  ctx.dict:incr_dict_key("g_fails", peer)
end

local function peer_ok(ctx, peer)
  local succ = ctx.dict:get_dict_key("succ", peer, 0)
  if not succ then
    return
  end

  succ = succ + 1

  if succ == 1 then
    ctx.dict:set_dict_key("fail", peer, 0)
  end

  local g_successes = ctx.dict:incr_dict_key("g_successes", peer)
  if g_successes == 1 then
    --[[ first check on start --]]
    succ = peer.upstream.healthcheck.rise
    ctx.dict:set_dict_key("g_successes", peer, succ)
  end

  ctx.dict:set_dict_key("succ", peer, succ)

  if peer.down and succ >= peer.upstream.healthcheck.rise then
    if g_successes > peer.upstream.healthcheck.rise then
      ctx:debug("upstream: ", peer.upstream.name, " peer: ", peer.name, " is turned up after ", succ, " success(es)")
    else
      ctx:debug("upstream: ", peer.upstream.name, " peer: ", peer.name, " is turned up on startup")
    end
    set_peer_state_globally(ctx, peer, state_up(ctx))
  end
end

local function change_peer_state(ok, ctx, peer)
  if not ok then
    peer_fail(ctx, peer)
  else
    peer_ok(ctx, peer)
  end
end

local function check_tcp(ctx, peer)
  local sock
  local ok, err

  ctx:debug("checking peer ", peer.name, " with tcp")

  sock, err = ngx.socket.tcp()
  if not sock then
    ctx:errlog("failed to create stream socket: ", err)
    return
  end

  sock:settimeout(peer.upstream.healthcheck.timeout)

  if peer.host then
    ok, err = sock:connect(peer.host, peer.port)
  else
    ok, err = sock:connect(peer.name)
  end

  if not ok then
    if not peer.down then
      ctx:errlog("failed to connect to ", peer.name, ": ", err)
    end
  else
    if peer.upstream.healthcheck.command then
      local bytes
      bytes, err = sock:send(peer.upstream.healthcheck.command.body)
      local expected = peer.upstream.healthcheck.command.expected or {}
      if not err and expected.body then
        local data = ""
        while true
        do
          local part
          part, err = sock:receive("*l")
          if part then
            data = data .. part
            if ngx.re.match(data, expected.body, "mx") then
              break
            end
          else
            err = "pattern is not found"
            break
          end
        end
      end
      if err then
        ok = nil
        if not peer.down then
          ctx:errlog("failed to check ", peer.name, ": ", err)
        end
      end
    end
    sock:close()
  end

  change_peer_state(ok, ctx, peer)
end

local function check_peer(ctx, peer)
  if peer.name:match("^unix:") then
    peer_ok(ctx, peer)
    return
  end

  if ctx.type == "tcp" or not peer.upstream.healthcheck.command then
    check_tcp(ctx, peer)
    return
  end

  ctx:debug("checking upstream: ", peer.upstream.name, " peer: ", peer.name, " with http")

  local httpc = http.new()

  httpc:set_timeout(peer.upstream.healthcheck.timeout)

  local ok, err = httpc:connect(peer.host, peer.port or 80)
  if not ok then
    ctx:warn("checking upstream: ", peer.upstream.name, " peer ", peer.name, " : ", err)
    peer_fail(ctx, peer)
    return
  end

  peer.upstream.healthcheck.command.path = peer.upstream.healthcheck.command.uri

  local response, err = httpc:request(peer.upstream.healthcheck.command)

  if not response then
    ctx:warn("checking upstream: ", peer.upstream.name, " peer ", peer.name, " : ", err)
    peer_fail(ctx, peer)
    return
  end

  if not peer.upstream.healthcheck.command.expected then
    httpc:close()
    peer_ok(ctx, peer)
    return
  end

  local check_status = function(valid_statuses, status)
    if not valid_statuses then
      return true
    end
    for i=1,#valid_statuses
    do
      if valid_statuses[i] == status then
        return true
      end
    end
    ctx:debug("checking upstream: ", peer.upstream.name,
              " peer ", peer.name, " : http status=", status, " is not in the valid statuses list")
    return false
  end

  response.body, err = response:read_body()

  httpc:close()

  if err then
    ctx:warn("checking upstream: ", peer.upstream.name, " peer ", peer.name, " : ", err)
    peer_fail(ctx, peer)
    return
  end

  ctx:debug("checking upstream: ", peer.upstream.name,
            " peer ", peer.name, " : http status=", response.status, " body=[", response.body, "]")

  if not check_status(peer.upstream.healthcheck.command.expected.codes, response.status) then
    peer_fail(ctx, peer)
    return
  end

  if peer.upstream.healthcheck.command.expected.body then
    if not ngx.re.match(response.body, peer.upstream.healthcheck.command.expected.body) then
      ctx:debug("checking upstream: ", peer.upstream.name, " peer ", peer.name,
                " : body=[", response.body, "] is not match the re=", peer.upstream.healthcheck.command.expected.body)
      peer_fail(ctx, peer)
      return
    end
  end

  peer_ok(ctx, peer)
end

local function check_peers(ctx, peers)
  local total = #peers
  local step_size = math.min(ctx.concurrency, total)
  local steps = math.ceil(total / step_size)

  for i = 0, steps - 1
  do
    local threads = {}
    local size = math.min(step_size, total - i * step_size)

    for j = 1, size
    do
      table.insert(threads, ngx.thread.spawn(check_peer, ctx, peers[i * step_size + j]))
    end

    for j = 1, size
    do
      local ok, err = ngx.thread.wait(threads[j])
      if not ok then
        ctx:warn(err)
      end
    end
  end
end

local function preprocess_peers(peers, upstream)
  foreachi(peers, function(peer)
    peer.upstream = upstream
    if peer.name then
      local from, to = ngx.re.find(peer.name, [[^(.*):\d+$]], "jo", nil, 1)
      if from then
        peer.host = peer.name:sub(1, to)
        peer.port = tonumber(peer.name:sub(to + 2))
      end
    end
  end)
  return peers
end

local function peer_disabled(ctx, peer)
  return ctx.dict:get_dict_key("disabled", peer) or
         ctx.dict:get_dict_key("disabled", {
           name = peer.name:match("^(.+):%d+$") or "not-ip-addr",
           upstream = { name = "*" }
         })
end

local function do_check(ctx)
  local elapsed = ctx.mutex:lock("mutex:hc:cycle")
  if not elapsed then
    return
  end

  local all_peers = {}

  local ok, upstreams, err = ctx.get_upstreams()
  if not ok then
    ctx:warn("failed to get upstreams: ", err)
    return
  end

  local now = ngx.now()

  foreachi(upstreams, function(u)
    local ok, peers, err = ctx.get_peers(u.name)
    if not ok then
      ctx:warn("failed to get peers [upstream:" .. u.name .. "]: ", err)
      return
    end

    foreachi(preprocess_peers(peers, u), function(peer)
      if peer_disabled(ctx, peer) then
        return
      end

      if peer.upstream.healthcheck or ctx.check_all then
        if not peer.upstream.healthcheck then
          peer.upstream.healthcheck = ctx.healthcheck
        end

        if not peer.upstream.healthcheck.interval then
          peer.upstream.healthcheck.interval = ctx.healthcheck.interval
        end

        if ctx.dict:get_dict_key("last", peer, 0) + peer.upstream.healthcheck.interval <= now then
          table.insert(all_peers, peer)
          ctx.dict:set_dict_key("last", peer, now)
        end
      end
    end)
  end)

  check_peers(ctx, all_peers)

  ctx.mutex:unlock()
end

local check
check = function (premature, ctx)
  if premature or ctx.stop_flag then
    return
  end

  if ctx.dict.shm:add(ctx.upstream_type .. ":lock", true, 600) then
    local ok, err = pcall(do_check, ctx)
    ctx.dict.shm:delete(ctx.upstream_type .. ":lock")
    if not ok then
      ctx:errlog("failed to run healthcheck cycle: ", err)
    end
  end

  local ok, err = ngx.timer.at(1, check, ctx)
  if not ok then
    if err ~= "process exiting" then
      ctx:errlog("failed to create timer: ", err)
    end
  end
end

local function init_state(ctx)
  local ok, upstreams, err = ctx.get_upstreams()
  assert(ok, err)
  foreachi(upstreams, function(u)
    local ok, peers, err = ctx.get_peers(u.name)
    assert(ok, err)
    foreachi(peers, function(peer)
      peer.upstream = u
      if u.healthcheck or ctx.check_all then
        local disabled = peer_disabled(ctx, peer) 
        local down = ctx.dict:get_dict_key("down", peer, true)
        ctx:debug("init state upstream=", u.name, ", peer=", peer.name,
                  " disabled=", disabled, " down=", down)
        if not disabled and not down then
          local succ = ctx.dict:get_dict_key("succ", peer, 0)
          if succ and succ >= (u.healthcheck or ctx.healthcheck).rise then
            ctx:debug("init state upstream=", u.name, ", peer=", peer.name, " up")
            ctx.set_peer_up(u.name, peer)
          end
        else
          ctx:debug("init state upstream=", u.name, ", peer=", peer.name, " down")
          ctx.set_peer_down(u.name, peer)
        end
      end
    end)
  end)
end

local function gen_peer_key(prefix, peer)
  return prefix .. ":" .. peer.upstream.name .. ":" .. peer.name
end

local dict_class = {}

function dict_class:set_dict_key(prefix, peer, val)
  local key = gen_peer_key(prefix, peer)
  return assert(self.shm:set(key, val))
end

function dict_class:get_dict_key(prefix, peer, default)
  local key = gen_peer_key(prefix, peer)
  local val = self.shm:get(key)
  if val ~= nil then
    return val
  end
  return default
end

function dict_class:incr_dict_key(prefix, peer)
  local key = gen_peer_key(prefix, peer)
  return assert(self.shm:incr(key, 1, 0))
end

function dict_class:delete_dict_key(prefix, peer)
  self.shm:delete(gen_peer_key(prefix, peer))
end

local function initialize(opts)
  local ctx = {
    upstream_type = opts.upstream_type,

    healthcheck = opts.healthcheck or {
      fall = 1,
      rise = 1,
      timeout = 1000
    },

    check_all = opts.check_all,
    type      = opts.type or "tcp",

    concurrency = opts.concurrency or 100,

    set_peer_down = opts.tab.set_peer_down,
    set_peer_up   = opts.tab.set_peer_up,
    get_peers     = opts.tab.get_peers,
    get_upstreams = opts.tab.get_upstreams
  }

  ctx.dict = setmetatable({
    shm = assert(ngx.shared.healthcheck, "lua_shared_dict 'healthcheck' is not found"),
    upstream_type = ctx.upstream_type
  }, {
    __index = dict_class
  })

  if not ctx.healthcheck.interval then
    ctx.healthcheck.interval = ((opts.interval or 10) > 0) and (opts.interval or 10) or 10
  end

  assert(ctx.type == "http" or ctx.type == "tcp", "'http' or 'tcp' type can be used")

  ctx.mutex = assert(lock:new("healthcheck"))

  return ctx
end

--- API ---------------------------------------------------------------------------------------

function _M.enable_debug()
  debug_enabled = true
end

local healthcheck_class = {}

function healthcheck_class:warn(...)
  ngx.log(ngx.WARN, "[", self.upstream_type, "] healthcheck: ", ...)
end

function healthcheck_class:info(...)
  ngx.log(ngx.INFO, "[", self.upstream_type, "] healthcheck: ", ...)
end

function healthcheck_class:errlog(...)
  ngx.log(ngx.ERR, "[", self.upstream_type, "] healthcheck: ", ...)
end

function healthcheck_class:debug(...)
  if debug_enabled then
    ngx.log(ngx.DEBUG, "[", self.upstream_type, "] healthcheck: ", ...)
  end
end

function healthcheck_class:start()
  self:debug("start")
  init_state(self)
  return ngx.timer.at(0, check, self)
end

function healthcheck_class:stop()
  self:debug("stop")
  self.stop_flag = true
end

function healthcheck_class:upstream_type()
  return self.ctx.upstream_type
end

function healthcheck_class:successes(u, peer)
  peer.upstream = { name = u }
  return self.dict:get_dict_key("g_successes", peer) or "-"
end

function healthcheck_class:fails(u, peer)
  peer.upstream = { name = u }
  return self.dict:get_dict_key("g_fails", peer) or "-"
end

function healthcheck_class:upstream_type()
  return self.upstream_type
end

function healthcheck_class:disable_peer(u, peer_name)
  if u and peer_name then
    self:debug("disable peer upstream=", u, " peer=", peer_name)
    self.dict:set_dict_key("disabled", {
      name = peer_name,
      upstream = { name = u }
    }, true)
  end
end

function healthcheck_class:enable_peer(u, peer_name)
  if u and peer_name then
    self:debug("enable peer upstream=", u, " peer=", peer_name)
    self.dict:delete_dict_key("disabled", {
      name = peer_name,
      upstream = { name = u }
    })
  end
end

function healthcheck_class:disable_ip(ip)
  if not ip then
    return
  end

  self:debug("disable ip=", ip)

  self.dict:set_dict_key("disabled", {
    name = ip,
    upstream = { name = "*" }
  }, true)

  local ok, upstreams, err = self.get_upstreams()
  assert(ok, err)

  foreachi(upstreams, function(u)
    local ok, peers, err = self.get_peers(u.name)
    assert(ok, err)

    foreachi(peers, function(peer)
      local peer_ip = peer.name:match("^(.+):%d+$")

      if peer_ip and peer_ip == ip then
        self:info("disable ip: upstream=", u.name, " peer=", peer.name)
        peer.upstream = u
        set_peer_state_globally(self, peer, state_down(self))
      end
    end)
  end)
end

function healthcheck_class:enable_ip(ip)
  if not ip then
    return
  end

  self:debug("enable ip=", ip)

  self.dict:delete_dict_key("disabled", {
    name = ip,
    upstream = { name = "*" }
  })
end

function _M.new(upstream_type, opts, tab)
  opts.upstream_type = upstream_type
  opts.tab = tab 
  return setmetatable(initialize(opts), {
    __index = healthcheck_class
  })
end

return _M