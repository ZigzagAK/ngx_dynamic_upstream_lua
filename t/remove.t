use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: remove primary peer
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
    GET /test?upstream=backends&peer=127.0.0.1:6002
--- response_body_like
127.0.0.1:6001


=== TEST 2: remove primary server
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server localhost4:6002;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
    GET /test?upstream=backends&peer=localhost4:6002
--- response_body_like
127.0.0.1:6001


=== TEST 3: remove backup peer
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002 backup;
        server 127.0.0.1:6003 backup;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
    GET /test?upstream=backends&peer=127.0.0.1:6002
--- response_body_like
127.0.0.1:6001
127.0.0.1:6003 backup


=== TEST 4: remove backup server
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server localhost4:6002 backup;
        server 127.0.0.1:6003 backup;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
    GET /test?upstream=backends&peer=localhost4:6002
--- response_body_like
127.0.0.1:6001
127.0.0.1:6003 backup


=== TEST 5: remove all backup peers
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002 backup;
        server 127.0.0.1:6003 backup;
        server 127.0.0.1:6004 backup;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
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
               local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, peer.name)
               if not ok then
                   ngx.say(err)
                   ngx.exit(200)
               end
               ngx.say("removed " .. peer.name)
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
    GET /test?upstream=backends
--- response_body_like
127.0.0.1:6001
127.0.0.1:6002 backup
removed 127.0.0.1:6002
127.0.0.1:6003 backup
removed 127.0.0.1:6003
127.0.0.1:6004 backup
removed 127.0.0.1:6004


=== TEST 6: remove stream primary peer
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
    GET /test?upstream=backends&peer=127.0.0.1:6002
--- response_body_like
127.0.0.1:6001


=== TEST 7: remove stream primary server
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server localhost4:6002;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
    GET /test?upstream=backends&peer=localhost4:6002
--- response_body_like
127.0.0.1:6001


=== TEST 8: remove backup peer
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002 backup;
        server 127.0.0.1:6003 backup;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
    GET /test?upstream=backends&peer=127.0.0.1:6002
--- response_body_like
127.0.0.1:6001
127.0.0.1:6003 backup


=== TEST 9: remove backup server
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server localhost4:6002 backup;
        server 127.0.0.1:6003 backup;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, ngx.var.arg_peer)
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
    GET /test?upstream=backends&peer=localhost4:6002
--- response_body_like
127.0.0.1:6001
127.0.0.1:6003 backup


=== TEST 10: remove all backup peers
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002 backup;
        server 127.0.0.1:6003 backup;
        server 127.0.0.1:6004 backup;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
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
               local ok, _, err = upstream.remove_peer(ngx.var.arg_upstream, peer.name)
               if not ok then
                   ngx.say(err)
                   ngx.exit(200)
               end
               ngx.say("removed " .. peer.name)
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
    GET /test?upstream=backends
--- response_body_like
127.0.0.1:6001
127.0.0.1:6002 backup
removed 127.0.0.1:6002
127.0.0.1:6003 backup
removed 127.0.0.1:6003
127.0.0.1:6004 backup
removed 127.0.0.1:6004
