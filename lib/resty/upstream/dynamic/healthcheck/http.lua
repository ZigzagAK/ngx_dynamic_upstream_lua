local _M = {
  _VERSION = "1.5.2"
}

local common = require "resty.upstream.dynamic.healthcheck.common"

local upstream = require "ngx.dynamic_upstream"

function _M.new(opts)
  return common.new("http", opts, { set_peer_down = _M.set_peer_down,
                                    set_peer_up   = _M.set_peer_up,
                                    get_peers     = _M.get_peers,
                                    get_upstreams = _M.get_upstreams
                                  })
end

function _M.successes(dict, upstream, peer)
  return common.successes("http", dict, upstream, peer)
end

function _M.fails(dict, upstream, peer)
  return common.fails("http", dict, upstream, peer)
end

function _M.disable_peer(dict, u, peer_name)
  common.disable_peer("http", dict, u, peer_name)
  upstream.set_peer_down(u, peer_name)
end

function _M.enable_peer(dict, u, peer_name)
  common.enable_peer("http", dict, u, peer_name)
end

--- Wrappers -----------------------------------------------------------------------

function _M.set_peer_down(u, peer)
  return upstream.set_peer_down(u, peer.name)
end

function _M.set_peer_up(u, peer)
  return upstream.set_peer_up(u, peer.name)
end

function _M.get_peers(u)
  return upstream.get_peers(u)
end

function _M.get_upstreams()
  return upstream.get_healthcheck()
end

return _M
