use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: update primary peer
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
            local ok, err = upstream.update_peer(ngx.var.arg_upstream, ngx.var.arg_peer, {
              max_fails=ngx.var.arg_max_fails,
              max_conns=ngx.var.arg_max_conns,
              fail_timeout=ngx.var.arg_fail_timeout,
              weight=ngx.var.arg_weight,
              down=ngx.var.arg_down
            })
            local ok, pp, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            for _, peer in pairs(pp)
            do
                ngx.say(peer.name .. ";" .. peer.weight .. ";" .. peer.max_fails .. ";" .. peer.max_conns .. ";" .. peer.fail_timeout .. ";" .. tointeger(peer.down))
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6002&max_fails=99&max_conns=88&fail_timeout=77&weight=66&down=1
--- response_body
127.0.0.1:6001;1;2;100;10;1
127.0.0.1:6002;1;2;100;10;0
127.0.0.1:6003;1;1;200;10;0;backup
127.0.0.1:6001;1;2;100;10;1
127.0.0.1:6002;66;99;88;77;1


=== TEST 2: update backup peer
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
            local ok, err = upstream.update_peer(ngx.var.arg_upstream, ngx.var.arg_peer, {
              max_fails=ngx.var.arg_max_fails,
              max_conns=ngx.var.arg_max_conns,
              fail_timeout=ngx.var.arg_fail_timeout,
              weight=ngx.var.arg_weight,
              down=ngx.var.arg_down
            })
            local ok, pp, err = upstream.get_backup_peers(ngx.var.arg_upstream)
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            for _, peer in pairs(pp)
            do
                ngx.say(peer.name .. ";" .. peer.weight .. ";" .. peer.max_fails .. ";" .. peer.max_conns .. ";" .. peer.fail_timeout .. ";" .. tointeger(peer.down) .. ";backup")
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6003&max_fails=99&max_conns=88&fail_timeout=77&weight=66&down=1
--- response_body
127.0.0.1:6001;1;2;100;10;1
127.0.0.1:6002;1;2;100;10;0
127.0.0.1:6003;1;1;200;10;0;backup
127.0.0.1:6003;66;99;88;77;1;backup


=== TEST 3: update stream primary peer
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
            local ok, err = upstream.update_peer(ngx.var.arg_upstream, ngx.var.arg_peer, {
              max_fails=ngx.var.arg_max_fails,
              max_conns=ngx.var.arg_max_conns,
              fail_timeout=ngx.var.arg_fail_timeout,
              weight=ngx.var.arg_weight,
              down=ngx.var.arg_down
            })
            local ok, pp, err = upstream.get_primary_peers(ngx.var.arg_upstream)
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            for _, peer in pairs(pp)
            do
                ngx.say(peer.name .. ";" .. peer.weight .. ";" .. peer.max_fails .. ";" .. peer.max_conns .. ";" .. peer.fail_timeout .. ";" .. tointeger(peer.down))
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6002&max_fails=99&max_conns=88&fail_timeout=77&weight=66&down=1
--- response_body
127.0.0.1:6001;1;2;100;10;1
127.0.0.1:6002;1;2;100;10;0
127.0.0.1:6003;1;1;200;10;0;backup
127.0.0.1:6001;1;2;100;10;1
127.0.0.1:6002;66;99;88;77;1


=== TEST 4: update backup peer
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
            local ok, err = upstream.update_peer(ngx.var.arg_upstream, ngx.var.arg_peer, {
              max_fails=ngx.var.arg_max_fails,
              max_conns=ngx.var.arg_max_conns,
              fail_timeout=ngx.var.arg_fail_timeout,
              weight=ngx.var.arg_weight,
              down=ngx.var.arg_down
            })
            local ok, pp, err = upstream.get_backup_peers(ngx.var.arg_upstream)
            if not ok then
               ngx.say(err)
               ngx.exit(500)
            end
            for _, peer in pairs(pp)
            do
                ngx.say(peer.name .. ";" .. peer.weight .. ";" .. peer.max_fails .. ";" .. peer.max_conns .. ";" .. peer.fail_timeout .. ";" .. tointeger(peer.down) .. ";backup")
            end
        }
    }
--- request
    GET /test?upstream=backends&peer=127.0.0.1:6003&max_fails=99&max_conns=88&fail_timeout=77&weight=66&down=1
--- response_body
127.0.0.1:6001;1;2;100;10;1
127.0.0.1:6002;1;2;100;10;0
127.0.0.1:6003;1;1;200;10;0;backup
127.0.0.1:6003;66;99;88;77;1;backup
