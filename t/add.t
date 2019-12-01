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
            local ok, _, err = upstream.add_primary_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
127.0.0.1:6666
127.0.0.1:6001


=== TEST 2: add primary server
--- http_config
    upstream backends {
        zone shm-backends 128k;
        dns_update 1s;
        server 127.0.0.1:6001;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, _, err = upstream.add_primary_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            ngx.sleep(2)
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.server)
               ngx.say(peer.name)
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=localhost4:6666
--- response_body_like
localhost4:6666
127.0.0.1:6666
127.0.0.1:6001
127.0.0.1:6001
--- timeout: 3


=== TEST 3: add backup peer
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, _, err = upstream.add_backup_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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


=== TEST 4: add backup server
--- http_config
    upstream backends {
        zone shm-backends 128k;
        dns_update 1s;
        server 127.0.0.1:6001;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, _, err = upstream.add_backup_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            ngx.sleep(2)
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.server)
               ngx.say(peer.name)
            end
            local ok, peers, err = upstream.get_backup_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.server .. " backup")
               ngx.say(peer.name .. " backup")
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=localhost4:6666
--- response_body
127.0.0.1:6001
127.0.0.1:6001
localhost4:6666 backup
127.0.0.1:6666 backup
--- timeout: 3


=== TEST 5: add stream primary peer
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
            local ok, _, err = upstream.add_primary_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
127.0.0.1:6666
127.0.0.1:6001


=== TEST 6: add stream primary server
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        dns_update 1s;
        server 127.0.0.1:6001;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, _, err = upstream.add_primary_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            ngx.sleep(2)
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.server)
               ngx.say(peer.name)
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=localhost4:6666
--- response_body
localhost4:6666
127.0.0.1:6666
127.0.0.1:6001
127.0.0.1:6001
--- timeout: 3


=== TEST 7: add stream backup peer
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
            local ok, _, err = upstream.add_backup_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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


=== TEST 8: add stream backup server
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        dns_update 1s;
        server 127.0.0.1:6001;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, _, err = upstream.add_backup_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            ngx.sleep(2)
            local ok, peers, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.server)
               ngx.say(peer.name)
            end
            local ok, peers, err = upstream.get_backup_peers(ngx.var.arg_upstream)
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, peer in pairs(peers)
            do
               ngx.say(peer.server .. " backup")
               ngx.say(peer.name .. " backup")
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=localhost4:6666
--- response_body
127.0.0.1:6001
127.0.0.1:6001
localhost4:6666 backup
127.0.0.1:6666 backup
--- timeout: 3

