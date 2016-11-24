local _M = {
    _VERSION = "0.99.1"
}

local common = require "resty.upstream.dynamic.healthcheck.common"

local upstream = require "ngx.upstream.stream"

function _M.new(opts)
    opts.typ = "tcp"
    local r = common.new("stream", opts, { set_peer_down = _M.set_peer_down,
                                           set_peer_up   = _M.set_peer_up,
                                           get_peers     = _M.get_peers,
                                           get_upstreams = _M.get_upstreams
                                         })
    return r
end

function _M.successes(dict, upstream, peer)
    return common.successes("stream", dict, upstream, peer)
end

function _M.fails(dict, upstream, peer)
    return common.fails("stream", dict, upstream, peer)
end

--- Wrappers -----------------------------------------------------------------------

function _M.set_peer_down(u, peer)
    local ok, err = upstream.set_peer_down(u, peer.is_backup, peer.id, true)
    return ok, nil, err
end

function _M.set_peer_up(u, peer)
    local ok, err = upstream.set_peer_down(u, peer.is_backup, peer.id, false)
    return ok, nil, err
end

function _M.get_peers(u)
    local peers = {}

    local pp = upstream.get_primary_peers(u)
    local bp = upstream.get_backup_peers(u)

    if pp then
        for _, peer in pairs(pp)
        do
            peer.is_backup = false
            table.insert(peers, peer)
        end
    end

    if bp then
        for _, peer in pairs(bp)
        do
            peer.is_backup = true
            table.insert(peers, peer)
        end
    end

    return true, peers, nil
end

function _M.get_upstreams()
    return true, upstream.get_upstreams(), nil
end

return _M
