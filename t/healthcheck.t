use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: healthcheck http
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        check type=http fall=2 rise=1 timeout=1500 interval=60;
        check_request_uri GET /heartbeat;
        check_request_headers aaa=333 bbb=444;
        check_request_body hello;
        check_response_codes 200 201;
        check_response_body .*;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, upstreams, err = upstream.get_healthcheck()
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, u in ipairs(upstreams)
            do
               ngx.say(string.format("%s %s %d %d %d %d", u.name, u.healthcheck.typ, u.healthcheck.fall, u.healthcheck.rise, u.healthcheck.timeout, u.healthcheck.interval))
               for k,v in pairs(u.healthcheck.command.headers)
               do
                 ngx.say(k,"=",v)
               end
               ngx.say(string.format("%s %s %s %s", u.healthcheck.command.uri, u.healthcheck.command.method, u.healthcheck.command.body, u.healthcheck.command.expected.body))
               for _,c in ipairs(u.healthcheck.command.expected.codes)
               do
                 ngx.say(c)
               end
            end
        }
    }
--- request
    GET /test
--- response_body
backends http 2 1 1500 60
aaa=333
bbb=444
/heartbeat GET hello .*
200
201


=== TEST 2: healthcheck http defargs type=http
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        check type=http;
        check_request_uri GET /heartbeat;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, upstreams, err = upstream.get_healthcheck()
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, u in ipairs(upstreams)
            do
               ngx.say(string.format("%s %s %d %d %d %d", u.name, u.healthcheck.typ, u.healthcheck.fall, u.healthcheck.rise, u.healthcheck.timeout, u.healthcheck.interval or 0))
               ngx.say(string.format("%s %s", u.healthcheck.command.uri, u.healthcheck.command.method))
            end
        }
    }
--- request
    GET /test
--- response_body
backends http 1 1 1000 10
/heartbeat GET


=== TEST 3: healthcheck http defargs type=tcp
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        check type=tcp;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, upstreams, err = upstream.get_healthcheck()
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, u in ipairs(upstreams)
            do
               ngx.say(string.format("%s %s %d %d %d %d", u.name, u.healthcheck.typ, u.healthcheck.fall, u.healthcheck.rise, u.healthcheck.timeout, u.healthcheck.interval or 0))
            end
        }
    }
--- request
    GET /test
--- response_body
backends tcp 1 1 1000 10


=== TEST 4: healthcheck http defargs
--- http_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        check;
    }
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream"
            local ok, upstreams, err = upstream.get_healthcheck()
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, u in ipairs(upstreams)
            do
               ngx.say(string.format("%s %s %d %d %d %d", u.name, u.healthcheck.typ, u.healthcheck.fall, u.healthcheck.rise, u.healthcheck.timeout, u.healthcheck.interval or 0))
            end
        }
    }
--- request
    GET /test
--- response_body
backends tcp 1 1 1000 10


=== TEST 5: healthcheck stream
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        check fall=2 rise=1 timeout=1500 interval=60;
        check_request_body "ping";
        check_response_body "pong";
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, upstreams, err = upstream.get_healthcheck()
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, u in ipairs(upstreams)
            do
               ngx.say(string.format("%s %d %d %d %d %s %s", u.name, u.healthcheck.fall, u.healthcheck.rise, u.healthcheck.timeout, u.healthcheck.interval, u.healthcheck.command.body, u.healthcheck.command.expected.body))
            end
        }
    }
--- request
    GET /test
--- response_body
backends 2 1 1500 60 ping pong


=== TEST 6: healthcheck stream defargs
--- stream_config
    upstream backends {
        zone shm-backends 128k;
        server 127.0.0.1:6001;
        check;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /test {
        content_by_lua_block {
            local upstream = require "ngx.dynamic_upstream.stream"
            local ok, upstreams, err = upstream.get_healthcheck()
            if not ok then
                ngx.say(err)
                ngx.exit(200)
            end
            for _, u in ipairs(upstreams)
            do
               ngx.say(string.format("%s %d %d %d %d", u.name, u.healthcheck.fall, u.healthcheck.rise, u.healthcheck.timeout, u.healthcheck.interval or 0))
            end
        }
    }
--- request
    GET /test
--- response_body
backends 1 1 1000 10
