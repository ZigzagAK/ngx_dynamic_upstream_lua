use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: down primary peer
--- http_config
    upstream backends {
        zone backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, err = upstream.update_peer(ngx.var.arg_upstream, ngx.var.arg_peer, { down = 1 })
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            local tointeger = function(b) if b then return 1 else return 0 end end
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name .. " down=" .. tointeger(peer.down))
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6002
--- response_body
127.0.0.1:6001 down=0
127.0.0.1:6002 down=1


=== TEST 2: up primary peer
--- http_config
    upstream backends {
        zone backends 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.1:6002 down;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, err = upstream.update_peer(ngx.var.arg_upstream, ngx.var.arg_peer, { down = 0 })
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            local tointeger = function(b) if b then return 1 else return 0 end end
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name .. " down=" .. tointeger(peer.down))
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6001
--- response_body
127.0.0.1:6001 down=0
127.0.0.1:6002 down=1


=== TEST 3: down primary peer
--- stream_config
    upstream backends {
        zone backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, err = upstream.update_peer(ngx.var.arg_upstream, ngx.var.arg_peer, { down = 1 })
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            local tointeger = function(b) if b then return 1 else return 0 end end
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name .. " down=" .. tointeger(peer.down))
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6002
--- response_body
127.0.0.1:6001 down=0
127.0.0.1:6002 down=1


=== TEST 3: up stream primary peer
--- stream_config
    upstream backends {
        zone backends 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.1:6002 down;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, err = upstream.update_peer(ngx.var.arg_upstream, ngx.var.arg_peer, { down = 0 })
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            local tointeger = function(b) if b then return 1 else return 0 end end
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name .. " down=" .. tointeger(peer.down))
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6001
--- response_body
127.0.0.1:6001 down=0
127.0.0.1:6002 down=1
