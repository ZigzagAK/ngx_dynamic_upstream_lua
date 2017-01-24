Name
====

dynamic-upstream-module-lua - Lua bindings for dynamic-upstream-module

This module supports http and stream upstream types.

[![Build Status](https://drone.io/github.com/ZigzagAK/ngx_dynamic_upstream_lua/status.png)](https://drone.io/github.com/ZigzagAK/ngx_dynamic_upstream_lua/latest)

Table of Contents
=================

* [Name](#name)
* [Status](#status)
* [Synopsis](#synopsis)
* [Description](#description)
* [Install](#install)
* [Configuration directives](#configuration-directives)
    * [check](#check)
    * [check_request_uri](#check_request_uri)
    * [check_request_headers](#check_request_headers)
    * [check_request_body](#check_request_body)
    * [check_response_codes](#check_response_codes)
    * [check_response_body](#check_response_body)
    * [disconnect_backup_if_primary_up](#disconnect_backup_if_primary_up)
    * [disconnect_if_market_down](#disconnect_if_market_down)
    * [disconnect_on_exiting](#disconnect_on_exiting)
* [Packages](#packages)
* [Methods](#methods)
    * [get_upstreams](#get_upstreams)
    * [get_healthcheck](#get_healthcheck)
    * [get_peers](#get_peers)
    * [get_primary_peers](#get_primary_peers)
    * [get_backup_peers](#get_backup_peers)
    * [set_peer_down](#set_peer_down)
    * [set_peer_up](#set_peer_up)
    * [add_primary_peer](#add_primary_peer)
    * [add_backup_peer](#add_backup_peer)
    * [remove_peer](#remove_peer)
    * [update_peer](#update_peer)
    * [current_upstream](#current_upstream)
* [Latest build](#latest-build)

Dependencies
============

This module has several dependencies:
* [dynamic-upstream-module](https://github.com/ZigzagAK/ngx_dynamic_upstream)

Status
======

This library is production ready.

Description
===========

This module provides Lua bindings to manage content of nginx upstreams.

[Back to TOC](#table-of-contents)

Install
=======

Build nginx.
All dependencies are downloaded automaticaly.

Pre requirements (for example centos/redhat)

```
sudo yum gcc-c++.x86_64 zlib-devel openssl-devel
```

Build

```
git clone git@github.com:ZigzagAK/ngx_dynamic_upstream_lua.git
cd ngx_dynamic_upstream_lua
./build.sh
```

Archive will be placed in the `install` folder after successful build.

[Back to TOC](#table-of-contents)

Configuration directives
========================

To use healthcheck parameters you need [nginx-resty-auto-healthcheck-config](https://github.com/ZigzagAK/nginx-resty-auto-healthcheck-config)

check
-----
* **syntax**: `check fall=2 rise=2 timeout=1000 type=http|tcp`
* **default**: `none`
* **context**: `upstream`

Configure healthcheck base parameters.

check_request_uri
-----------------
* **syntax**: `check_request_uri GET|POST /ping`
* **default**: `none`
* **context**: `upstream`

Configure http request for healthcheck.

check_request_headers
---------------------
* **syntax**: `check_request_headers a=1 b=2`
* **default**: `none`
* **context**: `upstream`

Configure http request headers for healthcheck.

check_request_body
------------------
* **syntax**: `check_request_body hello`
* **default**: `none`
* **context**: `upstream`

Configure http request body for healthcheck.

check_response_codes
--------------------
* **syntax**: `check_response_codes 200 201 202`
* **default**: `none`
* **context**: `upstream`

Configure http response codes for healthcheck.

check_response_body
-------------------
* **syntax**: `check_response_body .*`
* **default**: `none`
* **context**: `upstream`

Configure regular expression for http response body.

disconnect_backup_if_primary_up
-------------------------------
* **syntax**: `disconnect_backup_if_primary_up`
* **default**: `none`
* **context**: `stream/upstream`

Disconnect from backup peers when primary peers becomes available.

disconnect_if_market_down
-------------------------
* **syntax**: `disconnect_if_market_down`
* **default**: `none`
* **context**: `stream/upstream`

Disconnect peers when server is market down by healthcheck.

disconnect_on_exiting
---------------------
* **syntax**: `disconnect_on_exiting`
* **default**: `none`
* **context**: `stream/upstream`

Disconnect from upstream when nginx reloaded.

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

  server {
    listen 8888;

    default_type text/plain;

    # curl "http://localhost:8888/backend/add?peer=127.0.0.1:9091&weight=1&max_fails=2&fail_timeout=30&max_conns=100"
    # curl "http://localhost:8888/backend/add?peer=127.0.0.1:9092&backup=1&weight=1&max_fails=2&fail_timeout=30&max_conns=100"
    location ~* ^/(.+)/add {
      set $upstream $1;
      content_by_lua_block {
        local upstream = require "ngx.dynamic_upstream"

        local peer = ngx.var.arg_peer
        local u = ngx.var.upstream

        local ok, err
        if ngx.var.arg_backup and ngx.var.arg_backup == 1 then
          ok, _, err = upstream.add_backup_peer(u, peer)
        else
          ok, _, err = upstream.add_primary_peer(u, peer)
        end

        if not ok then
          say("Failed to add peer " .. peer .. ": ", err)
          return
        end

        local peer_args = {
          weight       = ngx.var.arg_weight or 1,
          max_fails    = ngx.var.arg_max_fails or 2,
          fail_timeout = ngx.var.arg_fail_timeout or 5,
          max_conns    = ngx.var.arg_max_conns or 1,
          down         = 1
        }

        ok, _, err = upstream.update_peer(u, peer, peer_args)
        if not ok then
          ngx.say("Failed to update peer " .. peer .. " params, error: ", err)
          return
        end

        ngx.say("Added " .. peer .. " to " .. u .. " upstream")
      }
    }

    # remove peer
    # curl "http://localhost:8888/backend/remove?peer=127.0.0.1:9091"
    location ~* ^/(.+)/remove {
      set $upstream $1;
      content_by_lua_block {
        local upstream = require "ngx.dynamic_upstream"

        local peer = ngx.var.arg_peer
        local u = ngx.var.upstream

        local ok, _, err = upstream.remove_peer(u, peer)
        if not ok then
          ngx.say("Failed to remove peer " .. peer .. ": ", err)
          return
        end

        ngx.say("Removed " .. peer .. " from " .. u .. " upstream")
      }
    }

    # status page for all the peers:
    location = /status {
      content_by_lua_block {
        local upstream = require "ngx.dynamic_upstream"

        local ok, upstreams, err = upstream.get_upstreams()
        if not ok then
          ngx.say(err)
          ngx.exit(200)
        end

        local get_peers = function(u)
          local ok, peers, err = upstream.get_primary_peers(u)
          if not ok then
            ngx.say(err)
            ngx.exit(200)
          end
  
          local t = {}

          for _, peer in pairs(peers)
          do
            table.insert(t, peer)
          end

          ok, peers, err = upstream.get_backup_peers(u)
          if not ok then
            ngx.say(err)
            ngx.exit(200)
          end
  
          for _, peer in pairs(peers)
          do
            table.insert(t, peer)
          end

          return t
        end

        local tointeger = function (b) if b then return 1 else return 0 end end

        for _, u in pairs(upstreams)
        do
          ngx.say(u)

          for _, peer in pairs(get_peers(u))
          do
            local status = "up"
            if peer.down ~= nil then
              status = "down"
            end

            ngx.say("    server " .. peer.name .. " backup=" .. tointeger(peer.backup) .. " weight=" .. peer.weight .. " max_conns=" .. peer.max_conns .. " max_fails=" .. peer.max_fails .. " fail_timeout=" .. peer.fail_timeout .. " status=" .. status)
          end
        end
      }
    }
  }
}
```
[Back to TOC](#table-of-contents)

Packages
=======

Package `ngx.dynamic_upstream` is used for manipulation http upstreams.
Package `ngx.dynamic_upstream.stream` is used for manipulation stream upstreams.

Functionality of both packages are same.


Methods
=======

get_upstreams
-------------
**syntax:** `ok, upstreams, error = dynamic_upstream.get_upstreams()`

**context:** *&#42;_by_lua&#42;*

Get table of upstreams.

Returns true and lua table on success, or false and a string describing an error otherwise.


get_healthcheck
---------------
**syntax:** `ok, healthcheck, error = dynamic_upstream.get_healthcheck()`

**context:** *&#42;_by_lua&#42;*

Get healthcheck parameters.

Returns true and lua table on success, or false and a string describing an error otherwise.


get_peers
-------------
**syntax:** `ok, servers, error = dynamic_upstream.get_peers(upstream)`

**context:** *&#42;_by_lua&#42;*

Get table of servers in the `upstream`.

Returns true and lua table on success, or false and a string describing an error otherwise.


get_primary_peers
-------------
**syntax:** `ok, peers, error = dynamic_upstream.get_primary_peers(upstream)`

**context:** *&#42;_by_lua&#42;*

Get table of primary peers in the `upstream`.

Returns true and lua table on success, or false and a string describing an error otherwise.


get_backup_peers
-------------
**syntax:** `ok, peers, error = dynamic_upstream.get_backup_peers(upstream)`

**context:** *&#42;_by_lua&#42;*

Get table of backup peers in the `upstream`.

Returns true and lua table on success, or false and a string describing an error otherwise.


set_peer_down
-------------
**syntax:** `ok, _, error = dynamic_upstream.set_peer_down(upstream, peer)`

**context:** *&#42;_by_lua&#42;*

Go `peer` of the `upstream` to DOWN state.

Returns true on success, or false and a string describing an error otherwise.


set_peer_up
-------------
**syntax:** `ok, _, error = dynamic_upstream.set_peer_up(upstream, peer)`

**context:** *&#42;_by_lua&#42;*

Go `peer` of the `upstream` to UP state.

Returns true on success, or false and a string describing an error otherwise.


add_primary_peer
-------------
**syntax:** `ok, _, error = dynamic_upstream.add_primary_peer(upstream, peer)`

**context:** *&#42;_by_lua&#42;*

Add `peer` to the `upstream` as primary.

Returns true on success, or false and a string describing an error otherwise.


add_backup_peer
-------------
**syntax:** `ok, _, error = dynamic_upstream.add_backup_peer(upstream, peer)`

**context:** *&#42;_by_lua&#42;*

Add `peer` to the `upstream` as backup.

Returns true on success, or false and a string describing an error otherwise.


remove_peer
-------------
**syntax:** `ok, _, error = dynamic_upstream.remove_peer(upstream, peer)`

**context:** *&#42;_by_lua&#42;*

Remove `peer` from the `upstream`.

Returns true on success, or false and a string describing an error otherwise.


update_peer
-----------
**syntax:** `ok, _, error = dynamic_upstream.update_peer(upstream, peer, {
              weight = N,
              max_fails = N,
              fail_timeout = N (seconds),
              max_conns = N,
              down = 0/1
            })`

**context:** *&#42;_by_lua&#42;*

Update `peer` attributes.

Returns true on success, or false and a string describing an error otherwise.


current_upstream
----------------
**syntax:** `ok, _, error = dynamic_upstream.current_upstream()`

**context:** *&#42;_by_lua&#42;*

Returns true and current upstream name on success, or false and a string describing an error otherwise.

[Back to TOC](#table-of-contents)

# Latest build
  * https://drone.io/github.com/ZigzagAK/ngx_dynamic_upstream_lua/files
