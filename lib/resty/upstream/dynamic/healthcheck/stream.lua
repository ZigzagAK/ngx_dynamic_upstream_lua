local _M = {
    _VERSION = '0.99.1'
}

local common = require "resty.upstream.dynamic.healthcheck.common"

local upstream = require "ngx.dynamic_upstream.stream"

function _M.new(opts)
    local r = common.new("http", opts, { set_peer_down = _M.set_peer_down,
                                         set_peer_up   = _M.set_peer_up,
                                         get_peers     = _M.get_peers,
                                         get_upstreams = _M.get_upstreams
                                       })
    return r
end

function _M.successes(dict, upstream, peer)
    return common.successes("http", dict, upstream, peer)
end

function _M.fails(dict, upstream, peer)
    return common.fails("http", dict, upstream, peer)
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
