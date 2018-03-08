local _M = {
  _VERSION = "1.5.2"
}

local common = require "resty.upstream.dynamic.healthcheck.common"
local upstream = require "ngx.dynamic_upstream.stream"

local hc

function _M.create(opts)
  assert(not hc, "already created")
  hc = assert(common.new("stream", opts, { set_peer_down = _M.set_peer_down,
                                           set_peer_up   = _M.set_peer_up,
                                           get_peers     = _M.get_peers,
                                           get_upstreams = _M.get_upstreams
                                         }))
  return hc
end

function _M.successes(upstream, peer)
  return hc and hc:successes(upstream, peer) or "-"
end

function _M.fails(upstream, peer)
  return hc and hc:fails(upstream, peer) or "-"
end

function _M.disable_peer(u, peer_name)
  if hc then
    hc:disable_peer(u, peer_name)
  end
  upstream.set_peer_down(u, peer_name)
end

function _M.enable_peer(u, peer_name)
  if hc then
    hc:enable_peer(u, peer_name)
  end
end

function _M.disable_ip(ip)
  if hc then
    hc:disable_ip(ip)
  end
end

function _M.enable_ip(ip)
  if hc then
    hc:enable_ip(ip)
  end
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
