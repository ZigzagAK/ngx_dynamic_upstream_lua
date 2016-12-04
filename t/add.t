use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: add primary peer
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, err = upstream.add_primary_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name)
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6666
--- response_body
127.0.0.1:6001
127.0.0.1:6666


=== TEST 2: add backup peer
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, err = upstream.add_backup_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name)
            end
            local ok, peers, err = upstream.get_backup_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name .. " backup")
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6666
--- response_body
127.0.0.1:6001
127.0.0.1:6666 backup


=== TEST 3: add stream primary peer
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, err = upstream.add_primary_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name)
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6666
--- response_body
127.0.0.1:6001
127.0.0.1:6666


=== TEST 4: add stream backup peer
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, err = upstream.add_backup_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name)
            end
            local ok, peers, err = upstream.get_backup_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.name .. " backup")
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6666
--- response_body
127.0.0.1:6001
127.0.0.1:6666 backup


