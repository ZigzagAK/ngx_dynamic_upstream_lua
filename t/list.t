use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: list upstreams
--- http_config
    upstream backends1 {
        zone backends1 128k;
        server 127.0.0.1:6001;
    }
    upstream backends2 {
        zone backends2 128k;
        server 127.0.0.1:6002;
    }
    upstream backends3 {
        server 127.0.0.1:6003;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, ups, err = upstream.get_upstreams()
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            for _, u in pairs(ups)
            do
               ngx.say(u)
            end
        }
    }
--- request
    GET /test
--- response_body
backends1
backends2
backends3


=== TEST 2: list peers
--- http_config
    upstream backends {
        zone backends 128k;
        server 127.0.0.1:6001 weight=1 max_fails=2 max_conns=100 fail_timeout=10 down;
        server 127.0.0.1:6002 weight=1 max_fails=2 max_conns=100 fail_timeout=10;
        server 127.0.0.1:6003 weight=1 max_fails=1 max_conns=200 fail_timeout=10 backup;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, pp, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            local ok, bp, err = upstream.get_backup_peers(ngx.var.arg_upstream)
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            local tointeger = function(b) if b then return 1 else return 0 end end
            for _, peer in pairs(pp)
            do
                ngx.say(peer.name .. ";" .. peer.weight .. ";" .. peer.max_fails .. ";" .. peer.max_conns .. ";" .. peer.fail_timeout .. ";" .. tointeger(peer.down))
            end
            for _, peer in pairs(bp)
            do
                ngx.say(peer.name .. ";" .. peer.weight .. ";" .. peer.max_fails .. ";" .. peer.max_conns .. ";" .. peer.fail_timeout .. ";" .. tointeger (peer.down) .. ";backup")
            end
        }
    }
--- request
    GET /test?upstream=backends
--- response_body
127.0.0.1:6001;1;2;100;10;1
127.0.0.1:6002;1;2;100;10;0
127.0.0.1:6003;1;1;200;10;0;backup


=== TEST 3: list stream upstreams
--- stream_config
    upstream backends1 {
        zone backends1 128k;
        server 127.0.0.1:6001;
    }
    upstream backends2 {
        zone backends2 128k;
        server 127.0.0.1:6002;
    }
    upstream backends3 {
        server 127.0.0.1:6003;
    }
--- stream_server_config
    proxy_pass backends1;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, ups, err = upstream.get_upstreams()
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            for _, u in pairs(ups)
            do
               ngx.say(u)
            end
        }
    }
--- request
    GET /test
--- response_body
backends1
backends2
backends3

=== TEST 4: list stream peers
--- stream_config
    upstream backends {
        zone backends 128k;
        server 127.0.0.1:6001 weight=1 max_fails=2 max_conns=100 fail_timeout=10 down;
        server 127.0.0.1:6002 weight=1 max_fails=2 max_conns=100 fail_timeout=10;
        server 127.0.0.1:6003 weight=1 max_fails=1 max_conns=200 fail_timeout=10 backup;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, pp, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            local ok, bp, err = upstream.get_backup_peers(ngx.var.arg_upstream)
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            local tointeger = function(b) if b then return 1 else return 0 end end
            for _, peer in pairs(pp)
            do
                ngx.say(peer.name .. ";" .. peer.weight .. ";" .. peer.max_fails .. ";" .. peer.max_conns .. ";" .. peer.fail_timeout .. ";" .. tointeger(peer.down))
            end
            for _, peer in pairs(bp)
            do
                ngx.say(peer.name .. ";" .. peer.weight .. ";" .. peer.max_fails .. ";" .. peer.max_conns .. ";" .. peer.fail_timeout .. ";" .. tointeger (peer.down) .. ";backup")
            end
        }
    }
--- request
    GET /test?upstream=backends
--- response_body
127.0.0.1:6001;1;2;100;10;1
127.0.0.1:6002;1;2;100;10;0
127.0.0.1:6003;1;1;200;10;0;backup
