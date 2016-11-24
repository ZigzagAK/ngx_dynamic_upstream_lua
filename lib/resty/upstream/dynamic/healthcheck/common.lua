local _M = {
    _VERSION = '0.99.1'
}

--- Pre checks --------------------------------------------------------------------------------

if not ngx.config or not ngx.config.ngx_lua_version or ngx.config.ngx_lua_version < 9005 then
    error("ngx_lua 0.9.5+ required")
end

local ok, lock = pcall(require, "resty.lock")
if not ok then
    error("resty.lock module required")
end

--- Log wrappers-------------------------------------------------------------------------------

local function info(...)
    ngx.log(ngx.INFO, "healthcheck: ", ...)
end

local function warn(...)
    ngx.log(ngx.WARN, "healthcheck: ", ...)
end

local function errlog(...)
    ngx.log(ngx.ERR, "healthcheck: ", ...)
end

local function debug(...)
    ngx.log(ngx.DEBUG, "healthcheck: ", ...)
end

--- Helpers -----------------------------------------------------------------------------------

local function gen_peer_key(prefix, peer)
    return prefix .. peer.upstream .. ":" .. peer.name
end

local ok, new_tab = pcall(require, "table.new")
if not ok or type(new_tab) ~= "function" then
    new_tab = function (narr, nrec) return {} end
end

--- Implementation ----------------------------------------------------------------------------

local function set_peer_state_globally(ctx, peer, state)
    local ok, _, err = state.fun(peer.upstream, peer)
    if not ok then
        errlog("failed to set peer state: ", err)
    end
    peer.down = state.down
    ctx.set_dict_key("d", peer, peer.down)
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
    debug("peer ", peer.name, " was checked to be not ok")

    local ok, fails = ctx.get_dict_key("fail", peer, 0)
    if not ok then
        return
    end

    fails = fails + 1

    if fails == 1 then
        ctx.set_dict_key("succ", peer, 0)
    end

    ctx.set_dict_key("fail", peer, fails)

    if fails >= ctx.fall and not peer.down then
        warn("peer ", peer.name, " is turned down after ", fails, " failure(s)")
        set_peer_state_globally(ctx, peer, state_down(ctx))
    end

    ctx.incr_dict_key("g_fails", peer)
end

local function peer_ok(ctx, peer)
    debug("peer ", peer.name, " was checked to be ok")

    local ok, succ = ctx.get_dict_key("succ", peer, 0)
    if not ok then
        return
    end

    succ = succ + 1

    if succ == 1 then
        ctx.set_dict_key("fail", peer, 0)
    end

    ctx.set_dict_key("succ", peer, succ)

    if succ >= ctx.rise and peer.down then
        warn("peer ", peer.name, " is turned up after ", succ, " success(es)")
        set_peer_state_globally(ctx, peer, state_up(ctx))
    end

    ctx.incr_dict_key("g_successes", peer)
end

local function peer_error(ctx, peer, ...)
  if not peer.down then
      errlog(...)
  end
  peer_fail(ctx, peer)
end

local function check_peer(ctx, peer)
    local ok, err

    debug("Checking " .. ctx.upstream_type .. " peer " .. peer.name)

    local sock, err = ngx.socket.tcp()
    if not sock then
        errlog("failed to create stream socket: ", err)
        return
    end

    sock:settimeout(ctx.timeout)

    if peer.host then
        ok, err = sock:connect(peer.host, peer.port)
    else
        ok, err = sock:connect(peer.name)
    end
    if not ok then
        if not peer.down then
            errlog("failed to connect to ", peer.name, ": ", err)
        end
        return peer_fail(ctx, peer)
    end

    if ctx.typ == "tcp" or not ctx.get_ping_req then
        peer_ok(ctx, peer)
        sock:close()
        return
    end

    local http_req = ctx.get_ping_req(peer.upstream)

    local bytes, err = sock:send(http_req)
    if not bytes then
        return peer_error(ctx, peer, "failed to send request to ", peer.name, ": ", err)
    end

    local status_line, err = sock:receive()
    if not status_line then
        peer_error(ctx, peer, "failed to receive status line from ", peer.name, ": ", err)
        if err == "timeout" then
            sock:close()
        end
        return
    end

    if ctx.statuses then
        local from, to, err = ngx.re.find(status_line,
                                          [[^HTTP/\d+\.\d+\s+(\d+)]],
                                          "joi", nil, 1)
        if not from then
            peer_error(ctx, peer, "bad status line from ", peer.name, ": ", status_line)
            sock:close()
            return
        end

        local status = tonumber(status_line:sub(from, to))
        if not ctx.statuses[status] then
            peer_error(ctx, peer, "bad status code from ", peer.name, ": ", status)
            sock:close()
            return
        end
    end

    peer_ok(ctx, peer)
    sock:close()
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
            ngx.thread.wait(threads[j])
        end
    end
end

local function preprocess_peers(peers, upstream)
    for _, peer in pairs(peers)
    do
        if peer.name then
            local from, to, err = ngx.re.find(peer.name, [[^(.*):\d+$]], "jo", nil, 1)
            if from then
                peer.upstream = upstream
                peer.host = peer.name:sub(1, to)
                peer.port = tonumber(peer.name:sub(to + 2))
            end
        end
    end
    return peers
end

local function do_check(ctx)
    debug("healthcheck: run a check cycle")

    local elapsed, err = ctx.mutex:lock("mutex:hc:cycle")
    if not elapsed then
        return
    end

    local all_peers = {}

    for _, u in pairs(ctx.upstreams)
    do
        local ok, peers, err = ctx.get_peers(u)
        if not ok then
            warn("failed to get peers [upstream:" .. u .. "]: ", err)
        else
            for i, peer in pairs(preprocess_peers(peers, u))
            do
                table.insert(all_peers, peer)
            end
        end
    end

    check_peers(ctx, all_peers)

    ctx.mutex:unlock()
end

local check
check = function (premature, ctx)
    if premature or ctx.stop then
        return
    end

    local ok, err = pcall(do_check, ctx)
    if not ok then
        errlog("failed to run healthcheck cycle: ", err)
    end

    local ok, err = ngx.timer.at(ctx.interval, check, ctx)
    if not ok then
        if err ~= "process exiting" then
            errlog("failed to create timer: ", err)
        end
    end
end

local function spawn_checker(self)
    local opts = self.opts

    self.ctx = {
        upstream_type = opts.upstream_type,

        get_ping_req = opts.get_ping_req,
        timeout      = opts.timeout or 1000,
        interval     = (opts.interval or 1000) / 1000,
        dict         = ngx.shared[opts.shm],
        typ          = opts.typ or "tcp",
        shm          = opts.shm,
        fall         = opts.fall or 5,
        rise         = opts.rise or 2,
        concurrency  = opts.concurrency or 1,

        set_peer_down = self.tab.set_peer_down,
        set_peer_up   = self.tab.set_peer_up,
        get_peers     = self.tab.get_peers,
        get_upstreams = self.tab.get_upstreams,

        upstreams = opts.upstreams,

        set_dict_key = function(prefix, peer, val)
            local key = gen_peer_key(self.ctx.upstream_type .. ":" .. prefix .. ":", peer)
            local ok, err = self.ctx.dict:set(key, val)
            if not ok then
                errlog("failed to set key " .. key .. ", error: ", err)
            end
        end,

        get_dict_key = function(prefix, peer, default)
            local key = gen_peer_key(self.ctx.upstream_type .. ":" .. prefix .. ":", peer)
            local val, err = self.ctx.dict:get(key)
            if not val then
                if err then
                    errlog("failed to get key " .. key .. ", error: ", err)
                    return false, nil
                end
                val = default
            end
            return true, val
        end,

        incr_dict_key = function(prefix, peer)
            local key = gen_peer_key(self.ctx.upstream_type .. ":" .. prefix .. ":", peer)
            local ok, err = self.ctx.dict:incr(key, 1, 0)
            if not ok then
                errlog("failed to increment key " .. key .. ", error: ", err)
            end
        end,
    }

    local ctx = self.ctx

    if not ctx.shm then
        return nil, "'shm' option required"
    end

    if not ctx.dict then
        return nil, "shm '" .. tostring(ctx.shm) .. "' not found"
    end

    if ctx.typ ~= "http" and ctx.typ ~= "tcp" then
        return nil, "'http' and 'tcp' type can be used"
    end

    if not ctx.upstreams then
        local ok, err
        ok, ctx.upstreams, err = self.tab.get_upstreams()
        if not ok then
            return nil, err
        end
    end

    if ctx.interval < 0.01 then
        interval = 0.01
    end

    if opts.valid_statuses then
        ctx.statuses = new_tab(0, #opts.valid_statuses)
        for _, status in ipairs(opts.valid_statuses) do
            ctx.statuses[status] = true
        end
    end

    local err
    ctx.mutex, err = lock:new(ctx.shm)
    if not ctx.mutex then
        return nil, "failed to create timer: " .. err
    end

    local ok, err = ngx.timer.at(0, check, ctx)
    if not ok then
        return nil, "failed to create timer: " .. err
    end

    return true
end

--- API ---------------------------------------------------------------------------------------

function _M.new(upstream_type, opts, tab)
    local hc = {}

    hc.opts = opts
    hc.opts.upstream_type = upstream_type
    hc.tab = tab

    local mt = { __index = {
      start         = spawn_checker,
      stop          = function() self.ctx.stop = true end,
      upstream_type = _M.upstream_type
    } }

    return setmetatable(hc, mt)
end

function _M.successes(upstream_type, dict, u, peer)
    peer.upstream = u
    local val, _ = dict:get(gen_peer_key(upstream_type .. ":g_successes:", peer))
    if not val then
        val = "-"
    end
    return val
end

function _M.fails(upstream_type, dict, u, peer)
    peer.upstream = u
    local val, _ = dict:get(gen_peer_key(upstream_type .. ":g_fails:", peer))
    if not val then
        val = "-"
    end
    return val
end

function _M.upstream_type(self)
    return self.ctx.upstream_type
end

return _M
