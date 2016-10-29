Name
====

dynamic-upstream-module-lua - Lua bindings for dynamic-upstream-module

Table of Contents
=================

* [Name](#name)
* [Status](#status)
* [Synopsis](#synopsis)
* [Description](#description)
* [Methods](#methods)
    * [get_upstreams](#get_upstreams)
    * [get_servers](#get_servers)
    * [set_peer_down](#set_peer_down)
    * [set_peer_up](#set_peer_up)
    * [add_peer](#add_peer)
    * [remove_peer](#remove_peer)
    * [update_peer](#update_peer)

Dependencies
============

This module has several dependencies:
* [dynamic-upstream-module](https://github.com/cubicdaiya/ngx_dynamic_upstream)

Status
======

This library is still under early development but is already production ready.

Description
===========

This module provides Lua bindings to manage content of nginx upstreams.

[Back to TOC](#table-of-contents)

Synopsis
========

```nginx
http {
    lua_package_path "/path/to/lua-ngx-dynamic-upstream-lua/lib/?.lua;;";

    upstream backend {
        zone backend 1m;
        server 127.0.0.1:9090;
    }

    lua_socket_log_errors off;

    server {
        listen 9090;
        listen 9091;
        
        default_type text/plain;
        
        location / {
          return 200 'Hello world!';
        }
    }
    
    server {
        listen 8888;

        default_type text/plain;

        # add peer
        # curl "http://localhost:8888/backend/add?peer=127.0.0.1:9091"
        location ~* ^/(.+)/add {
          set $upstream $1;
          content_by_lua_block {
            local dynamic_upstream = require "ngx.dynamic_upstream"

            local peer = ngx.var.arg_peer
            local upstream = ngx.var.upstream
            
            local result, _, err = dynamic_upstream.add_peer(upstream, peer)
            if not result then
              say("Failed to add peer " .. peer .. ": ", err)
              return
            end
            
            local peer_args = {
              weight = 1,
              max_fails = 2,
              fail_timeout = 30,
              down = 1
            }
            
            result, _, err = dynamic_upstream.update_peer(upstream, peer, peer_args)
            if not result then
             ngx.say("Failed to update peer " .. peer .. " params, error: ", err)
             return
            end
            
            ngx.say("Added " .. peer .. " to " .. upstream .. " upstream")
          }
        }

        # remove peer
        # curl "http://localhost:8888/backend/remove?peer=127.0.0.1:9091"
        location ~* ^/(.+)/remove {
          set $upstream $1;
          content_by_lua_block {
            local dynamic_upstream = require "ngx.dynamic_upstream"

            local peer = ngx.var.arg_peer
            local upstream = ngx.var.upstream
            
            local result, _, err = dynamic_upstream.remove_peer(upstream, peer)
            if not result then
              say("Failed to remove peer " .. peer .. ": ", err)
              return
            end

            ngx.say("Removed " .. peer .. " from " .. upstream .. " upstream")
          }
        }
        
        # status page for all the peers:
        location = /status {
          content_by_lua_block {
            local dynamic_upstream = require "ngx.dynamic_upstream"
            local ok, upstreams, error = dynamic_upstream.get_upstreams()
            if ok then
              for _, upstream in pairs(upstreams)
              do
                ngx.say(upstream)
                local ok, servers, error = dynamic_upstream.get_servers(upstream)
                if ok then
                  for _, server in pairs(servers)
                  do
                    local status = "up"
                    if server.down ~= nil then
                      status = "down"
                    end
                    ngx.say("    server " .. server.name .. " weight=" .. server.weight .. " max_fails=" .. server.max_fails .. " fail_timeout=" .. server.fail_timeout .. " status=" .. status)
                  end
                else
                  ngx.say(error)
                end
                ngx.say()
              end
            else
              ngx.say(error)
            end
          }
        }
    }
}
```
[Back to TOC](#table-of-contents)

Methods
=======

get_upstreams
-------------
**syntax:** `ok, upstreams, error = dynamic_upstream.get_upstreams()`

**context:** *&#42;_by_lua&#42;*

Get table of upstreams.

Returns true and lua table on success, or false and a string describing an error otherwise.


get_servers
-------------
**syntax:** `ok, servers, error = dynamic_upstream.get_servers(upstream)`

**context:** *&#42;_by_lua&#42;*

Get table of servers in the `upstream`.

Returns true and lua table on success, or false and a string describing an error otherwise.


set_peer_down
-------------
**syntax:** `ok, nil, error = dynamic_upstream.set_peer_down(upstream, peer)`

**context:** *&#42;_by_lua&#42;*

Go `peer` of the `upstream` to DOWN state.

Returns true on success, or false and a string describing an error otherwise.


set_peer_up
-------------
**syntax:** `ok, nil, error = dynamic_upstream.set_peer_up(upstream, peer)`

**context:** *&#42;_by_lua&#42;*

Go `peer` of the `upstream` to UP state.

Returns true on success, or false and a string describing an error otherwise.


add_peer
-------------
**syntax:** `ok, nil, error = dynamic_upstream.add_peer(upstream, peer)`

**context:** *&#42;_by_lua&#42;*

Add `peer` to the `upstream`.

Returns true on success, or false and a string describing an error otherwise.

remove_peer
-------------
**syntax:** `ok, nil, error = dynamic_upstream.remove_peer(upstream, peer)`

**context:** *&#42;_by_lua&#42;*

Remove `peer` from the `upstream`.

Returns true on success, or false and a string describing an error otherwise.


update_peer
-------------
**syntax:** `ok, nil, error = dynamic_upstream.update_peer(upstream, peer, {
              weight = N,
              max_fails = N,
              fail_timeout = N (seconds),
              down = 0/1
            })`

**context:** *&#42;_by_lua&#42;*

Update `peer` attributes.

Returns true on success, or false and a string describing an error otherwise.

[Back to TOC](#table-of-contents)

If you indeed have spawned a healthchecker in `init_worker_by_lua*`, then you should really
check out the NGINX error log file to see if there is any fatal errors aborting the healthchecker threads.

[Back to TOC](#table-of-contents)