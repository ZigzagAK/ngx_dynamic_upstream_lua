#include <ngx_core.h>
#include <ngx_http.h>
#include <lauxlib.h>
#include "ngx_http_lua_api.h"


#include "ngx_dynamic_shm.h"
#include "ngx_dynamic_upstream_lua.h"


#include "../ngx_dynamic_upstream/src/ngx_dynamic_upstream_module.h"


extern ngx_module_t ngx_http_dynamic_upstream_lua_module;


static int
ngx_http_dynamic_upstream_lua_create_module(lua_State * L);


static int
ngx_http_dynamic_upstream_lua_get_upstreams(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_get_healthcheck(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_get_peers(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_get_primary_peers(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_get_backup_peers(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_set_peer_down(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_set_peer_up(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_add_primary_peer(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_add_backup_peer(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_remove_peer(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_update_peer(lua_State * L);
static int
ngx_http_dynamic_upstream_lua_current_upstream(lua_State *L);
static int
ngx_http_dynamic_upstream_lua_update_healthcheck(lua_State *L);


ngx_int_t
ngx_http_dynamic_upstream_lua_init(ngx_conf_t *cf)
{
    if (ngx_http_lua_add_package_preload(cf, "ngx.dynamic_upstream",
                                         ngx_http_dynamic_upstream_lua_create_module)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static int
ngx_http_dynamic_upstream_lua_create_module(lua_State * L)
{
    lua_createtable(L, 0, 13);

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_get_upstreams);
    lua_setfield(L, -2, "get_upstreams");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_get_healthcheck);
    lua_setfield(L, -2, "get_healthcheck");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_get_peers);
    lua_setfield(L, -2, "get_peers");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_get_primary_peers);
    lua_setfield(L, -2, "get_primary_peers");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_get_backup_peers);
    lua_setfield(L, -2, "get_backup_peers");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_set_peer_down);
    lua_setfield(L, -2, "set_peer_down");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_set_peer_up);
    lua_setfield(L, -2, "set_peer_up");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_add_primary_peer);
    lua_setfield(L, -2, "add_primary_peer");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_add_backup_peer);
    lua_setfield(L, -2, "add_backup_peer");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_remove_peer);
    lua_setfield(L, -2, "remove_peer");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_update_peer);
    lua_setfield(L, -2, "update_peer");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_current_upstream);
    lua_setfield(L, -2, "current_upstream");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_update_healthcheck);
    lua_setfield(L, -2, "update_healthcheck");

    return 1;
}


static ngx_http_upstream_main_conf_t *
ngx_http_lua_upstream_get_upstream_main_conf(lua_State *L)
{
    ngx_http_request_t *r;

    r = ngx_http_lua_get_request(L);

    if (r == NULL) {
        return ngx_http_cycle_get_module_main_conf(ngx_cycle,
                                                   ngx_http_upstream_module);
    }

    return ngx_http_get_module_main_conf(r, ngx_http_upstream_module);
}


static ngx_http_upstream_srv_conf_t *
ngx_dynamic_upstream_get(lua_State * L, ngx_dynamic_upstream_op_t *op)
{
    ngx_uint_t                      i;
    ngx_http_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_http_upstream_main_conf_t  *umcf;

    umcf  = ngx_http_lua_upstream_get_upstream_main_conf(L);
    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        uscf = uscfp[i];
        if (ngx_strncmp(uscf->host.data, op->upstream.data, op->upstream.len) == 0)
        {
            return uscf;
        }
    }

    return NULL;
}


static const int PRIMARY = 1;
static const int BACKUP  = 2;


static void
ngx_dynamic_upstream_lua_create_response(ngx_http_upstream_rr_peers_t *primary, lua_State * L, int flags)
{
    ngx_http_upstream_rr_peer_t  *peer;
    ngx_http_upstream_rr_peers_t *peers, *backup;
    int                           size = 0, n, i = 1;

    backup = primary->next;

    if (flags & PRIMARY) {
        size += primary->number;
    }
    if (flags & BACKUP && backup) {
        size += backup->number;
    }

    lua_newtable(L);
    lua_createtable(L, size, 0);

    for (peers = primary; peers; peers = peers->next) {
        if ( (flags & PRIMARY && peers == primary) || (flags & BACKUP && peers == backup) ) {
            for (peer = peers->peer; peer; peer = peer->next, ++i) {
                n = 7;

                if (peer->down) {
                    n++;
                }

                lua_createtable(L, 0, n);

                lua_pushliteral(L, "name");
                lua_pushlstring(L, (char *) peer->name.data,
                                            peer->name.len);
                lua_rawset(L, -3);

                lua_pushliteral(L, "weight");
                lua_pushinteger(L, (lua_Integer) peer->weight);
                lua_rawset(L, -3);

                lua_pushliteral(L, "max_conns");
                lua_pushinteger(L, (lua_Integer) peer->max_conns);
                lua_rawset(L, -3);

                lua_pushliteral(L, "conns");
                lua_pushinteger(L, (lua_Integer) peer->conns);
                lua_rawset(L, -3);

                lua_pushliteral(L, "max_fails");
                lua_pushinteger(L, (lua_Integer) peer->max_fails);
                lua_rawset(L, -3);

                lua_pushliteral(L, "fail_timeout");
                lua_pushinteger(L, (lua_Integer) peer->fail_timeout);
                lua_rawset(L, -3);

                lua_pushliteral(L, "backup");
                lua_pushboolean(L, peers != primary);
                lua_rawset(L, -3);

                if (peer->down) {
                    lua_pushliteral(L, "down");
                    lua_pushboolean(L, 1);
                    lua_rawset(L, -3);
                }

                lua_rawseti(L, -2, i);
            }
        }
    }
}


static void
ngx_http_dynamic_upstream_lua_op_defaults(lua_State * L, ngx_dynamic_upstream_op_t *op, int operation)
{
    ngx_memzero(op, sizeof(ngx_dynamic_upstream_op_t));

    op->op = operation;

    op->status = NGX_HTTP_OK;
    ngx_str_null(&op->upstream);
    op->weight       = 1;
    op->max_fails    = 1;
    op->fail_timeout = 10;
    op->verbose      = 0;
    op->backup       = 0;

    op->upstream.data = (u_char *) luaL_checklstring(L, 1, &op->upstream.len);
}


static int
ngx_http_dynamic_upstream_lua_error(lua_State * L, const char *error)
{
    lua_pushboolean(L, 0);
    lua_pushnil(L);
    lua_pushlstring(L, error, strlen(error));
    return 3;
}


static int
ngx_http_dynamic_upstream_lua_op(lua_State * L, ngx_dynamic_upstream_op_t *op, int flags)
{
    ngx_int_t                       rc;
    ngx_http_upstream_srv_conf_t   *uscf;
    ngx_http_upstream_rr_peers_t   *primary;

    uscf = ngx_dynamic_upstream_get(L, op);
    if (uscf == NULL) {
        return ngx_http_dynamic_upstream_lua_error(L, "Upstream not found");
    }

    if (op->op & (NGX_DYNAMIC_UPSTEAM_OP_ADD | NGX_DYNAMIC_UPSTEAM_OP_REMOVE)) {
        if (uscf->shm_zone == NULL) {
            return ngx_http_dynamic_upstream_lua_error(L, "Shared zone segment is not defined in upstream");
        }
    }

    rc = ngx_dynamic_upstream_op(ngx_http_lua_get_request(L)->connection->log, op, uscf);
    if (rc != NGX_OK) {
        return ngx_http_dynamic_upstream_lua_error(L, "Internal server error");
    }

    lua_pushboolean(L, 1);

    if (op->verbose) {
        primary = uscf->peer.data;
        ngx_dynamic_upstream_lua_create_response(primary, L, flags);
    } else {
        lua_pushnil(L);
    }

    lua_pushnil(L);

    return 3;
}


static int
ngx_http_dynamic_upstream_lua_get_upstreams(lua_State * L)
{
    ngx_uint_t                    i;
    ngx_http_upstream_srv_conf_t  **uscfp, *uscf;
    ngx_http_upstream_main_conf_t *umcf;

    if (lua_gettop(L) != 0) {
        return ngx_http_dynamic_upstream_lua_error(L, "no argument expected");
    }

    umcf = ngx_http_lua_upstream_get_upstream_main_conf(L);
    uscfp = umcf->upstreams.elts;

    lua_pushboolean(L, 1);

    lua_newtable(L);
    lua_createtable(L, umcf->upstreams.nelts, 0);

    umcf  = ngx_http_lua_upstream_get_upstream_main_conf(L);
    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        uscf = uscfp[i];
        lua_pushlstring(L, (char *) uscf->host.data, uscf->host.len);
        lua_rawseti(L, -2, i + 1);
    }

    lua_pushnil(L);

    return 3;
}


extern ngx_str_t shared_zone_http_prefix;


static ngx_http_dynamic_upstream_lua_srv_conf_t *
ngx_http_get_dynamic_upstream_lua_srv_conf(ngx_http_upstream_srv_conf_t *uscf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_http_conf_upstream_srv_conf(uscf, ngx_http_dynamic_upstream_lua_module);

    if (ucscf->shm_zone == NULL) {
        ucscf->shm_zone = ngx_shared_memory_find(ngx_cycle,
                                                 shared_zone_http_prefix,
                                                 ucscf->conf->upstream,
                                                 &ngx_http_dynamic_upstream_lua_module);
        if (ucscf->shm_zone == NULL) {
            return NULL;
        }
    }

    if (ucscf->shm_zone != NULL) {
        ucscf->conf->upstream = uscf->host;
        ucscf->data = ucscf->shm_zone->data;
        ucscf->data->upstream = uscf->host;
        ucscf->shpool = (ngx_slab_pool_t *) ucscf->shm_zone->shm.addr;
    }

    return ucscf;
}

static void
ngx_http_dynamic_upstream_lua_push_healthcheck(lua_State *L, ngx_http_upstream_srv_conf_t *uscf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;
    int n =                                   4;
    ngx_uint_t                                i;
    
    ucscf = ngx_http_get_dynamic_upstream_lua_srv_conf(uscf);

    if (ucscf == NULL || ucscf->data == NULL || ucscf->data->type.data == NULL) {
        lua_pushliteral(L, "healthcheck");
        lua_pushnil(L);
        lua_rawset(L, -3);
        return;
    }

    ngx_shmtx_lock(&ucscf->shpool->mutex);

    if (ucscf->data->request_uri.len != 0) {
        ++n;
    }

    lua_pushliteral(L, "healthcheck");
    lua_createtable(L, 0, n);

    lua_pushliteral(L, "typ");
    lua_pushlstring(L, (char *) ucscf->data->type.data,
                                ucscf->data->type.len);
    lua_rawset(L, -3);

    lua_pushliteral(L, "fall");
    lua_pushinteger(L, (lua_Integer) ucscf->data->fall);
    lua_rawset(L, -3);

    lua_pushliteral(L, "rise");
    lua_pushinteger(L, (lua_Integer) ucscf->data->rise);
    lua_rawset(L, -3);

    lua_pushliteral(L, "timeout");
    lua_pushinteger(L, (lua_Integer) ucscf->data->timeout);
    lua_rawset(L, -3);

    if (ucscf->data->request_uri.len != 0) {
        n = 2;

        if (ucscf->data->request_headers) {
          ++n;
        }
        if (ucscf->data->request_body.len != 0) {
          ++n;
        }
        if (ucscf->data->response_codes || ucscf->data->response_body.len != 0) {
          ++n;
        }
  
        lua_pushliteral(L, "command");
        lua_createtable(L, 0, n);

        {
            lua_pushliteral(L, "uri");
            lua_pushlstring(L, (char *) ucscf->data->request_uri.data,
                                        ucscf->data->request_uri.len);
            lua_rawset(L, -3);

            lua_pushliteral(L, "method");
            lua_pushlstring(L, (char *) ucscf->data->request_method.data,
                                        ucscf->data->request_method.len);
            lua_rawset(L, -3);

            if (ucscf->data->request_headers) {
                lua_pushliteral(L, "headers");
                lua_createtable(L, 0, ucscf->data->request_headers_count);

                for (i = 0; i < ucscf->data->request_headers_count; ++i) {
                    lua_pushlstring(L, (char *) ucscf->data->request_headers[i].name.data,
                                                ucscf->data->request_headers[i].name.len);
                    lua_pushlstring(L, (char *) ucscf->data->request_headers[i].value.data,
                                                ucscf->data->request_headers[i].value.len);
                    lua_rawset(L, -3);
                }

                lua_rawset(L, -3);
            }

            if (ucscf->data->request_body.len != 0) {
                lua_pushliteral(L, "body");
                lua_pushlstring(L, (char *) ucscf->data->request_body.data,
                                            ucscf->data->request_body.len);
                lua_rawset(L, -3);
            }

            if (ucscf->data->response_codes || ucscf->data->response_body.len != 0) {
                n = 0;
                n = n + (ucscf->data->response_codes ? 1 : 0);
                n = n + (ucscf->data->response_body.len != 0 ? 1 : 0);

                lua_pushliteral(L, "expected");
                lua_createtable(L, 0, n);

                if (ucscf->data->response_codes) {
                    lua_pushliteral(L, "codes");
                    lua_createtable(L, ucscf->data->response_codes_count, 0);

                    for (i = 0; i < ucscf->data->response_codes_count; ++i) {
                        lua_pushinteger(L, (lua_Integer) ucscf->data->response_codes[i]);
                        lua_rawseti(L, -2, i + 1);
                    }

                    lua_rawset(L, -3);
                }

                if (ucscf->data->response_body.len != 0) {
                    lua_pushliteral(L, "body");
                    lua_pushlstring(L, (char *) ucscf->data->response_body.data,
                                                ucscf->data->response_body.len);
                    lua_rawset(L, -3);
                }

                lua_rawset(L, -3);
            }
        }

        lua_rawset(L, -3);
    }

    lua_rawset(L, -3);
    
    ngx_shmtx_unlock(&ucscf->shpool->mutex);
}


static int
ngx_http_dynamic_upstream_lua_get_healthcheck(lua_State * L)
{
    ngx_uint_t                    i;
    ngx_http_upstream_srv_conf_t  **uscfp, *uscf;
    ngx_http_upstream_main_conf_t *umcf;

    if (lua_gettop(L) != 0) {
        return ngx_http_dynamic_upstream_lua_error(L, "no argument expected");
    }

    umcf = ngx_http_lua_upstream_get_upstream_main_conf(L);
    uscfp = umcf->upstreams.elts;

    lua_pushboolean(L, 1);

    lua_newtable(L);
    lua_createtable(L, umcf->upstreams.nelts, 0);

    umcf  = ngx_http_lua_upstream_get_upstream_main_conf(L);
    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        uscf = uscfp[i];

        lua_createtable(L, 0, 2);

        lua_pushliteral(L, "name");
        lua_pushlstring(L, (char *) uscf->host.data, uscf->host.len);
        lua_rawset(L, -3);

        ngx_http_dynamic_upstream_lua_push_healthcheck(L, uscf);

        lua_rawseti(L, -2, i + 1);
    }

    lua_pushnil(L);

    return 3;
}


static int
ngx_http_dynamic_upstream_lua_get_peers(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;
    if (lua_gettop(L) != 1) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly one argument expected");
    }
    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_LIST);
    op.verbose = 1;
    return ngx_http_dynamic_upstream_lua_op(L, &op, PRIMARY|BACKUP);
}


static int
ngx_http_dynamic_upstream_lua_get_primary_peers(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;
    if (lua_gettop(L) != 1) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly one argument expected");
    }
    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_LIST);
    op.verbose = 1;
    return ngx_http_dynamic_upstream_lua_op(L, &op, PRIMARY);
}


static int
ngx_http_dynamic_upstream_lua_get_backup_peers(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;
    if (lua_gettop(L) != 1) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly one argument expected");
    }
    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_LIST);
    op.verbose = 1;
    return ngx_http_dynamic_upstream_lua_op(L, &op, BACKUP);
}


static int
ngx_http_dynamic_upstream_lua_set_peer_down(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 2) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_PARAM);

    op.down = 1;
    op.op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN;

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    return ngx_http_dynamic_upstream_lua_op(L, &op, 0);
}


static int
ngx_http_dynamic_upstream_lua_set_peer_up(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 2) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_PARAM);

    op.up = 1;
    op.op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_UP;

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    return ngx_http_dynamic_upstream_lua_op(L, &op, 0);
}


static int
ngx_http_dynamic_upstream_lua_add_peer_impl(lua_State * L, int backup)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 2) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_ADD);

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    op.backup = backup;

    return ngx_http_dynamic_upstream_lua_op(L, &op, 0);
}


static int
ngx_http_dynamic_upstream_lua_add_primary_peer(lua_State * L)
{
    return ngx_http_dynamic_upstream_lua_add_peer_impl(L, 0);
}


static int
ngx_http_dynamic_upstream_lua_add_backup_peer(lua_State * L)
{
    return ngx_http_dynamic_upstream_lua_add_peer_impl(L, 1);
}


static int
ngx_http_dynamic_upstream_lua_remove_peer(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 2) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_REMOVE);

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    return ngx_http_dynamic_upstream_lua_op(L, &op, 0);
}


static int
ngx_http_dynamic_upstream_lua_update_peer_parse_params(lua_State * L, ngx_dynamic_upstream_op_t *op)
{
    const char *key;

    lua_pushvalue(L, -1);
    lua_pushnil(L);

    while (lua_next(L, -2))
    {
        lua_pushvalue(L, -2);
        key = lua_tostring(L, -1);
        if (strcmp(key, "weight") == 0) {
            op->weight = lua_tonumber(L, -2);
            op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_WEIGHT;
        } else if (strcmp(key, "max_fails") == 0) {
            op->max_fails = lua_tonumber(L, -2);
            op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_FAILS;
        } else if (strcmp(key, "max_conns") == 0) {
            op->max_conns = lua_tonumber(L, -2);
            op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_CONNS;
        } else if (strcmp(key, "fail_timeout") == 0) {
            op->fail_timeout = lua_tonumber(L, -2);
            op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_FAIL_TIMEOUT;
        } else if (strcmp(key, "down") == 0) {
            if (lua_tonumber(L, -2) == 1) {
                op->down = 1;         
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN;
            } else if (lua_tonumber(L, -2) == 0) {
                op->up = 1;
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_UP;
            }
        }
        lua_pop(L, 2);
    }

    lua_pop(L, 1);

    return NGX_OK;
}


static int
ngx_http_dynamic_upstream_lua_update_peer(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 3 || !lua_istable(L, 3)) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly 3 arguments expected");
    }

    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_PARAM);

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    ngx_http_dynamic_upstream_lua_update_peer_parse_params(L, &op);

    return ngx_http_dynamic_upstream_lua_op(L, &op, 0);
}


static int
ngx_http_dynamic_upstream_lua_current_upstream(lua_State *L)
{
    ngx_http_request_t           *r;
    ngx_http_upstream_t          *us;
    ngx_http_upstream_conf_t     *ucf;
    ngx_http_upstream_srv_conf_t *uscf;

    r = ngx_http_lua_get_request(L);
    if (r == NULL) {
        return ngx_http_dynamic_upstream_lua_error(L, "no request object found");
    }

    us = r->upstream;
    if (us == NULL) {
        return ngx_http_dynamic_upstream_lua_error(L, "no proxying");
    }

    uscf = us->upstream;

    if (uscf == NULL) {
        ucf = us->conf;
        if (ucf == NULL) {
            return ngx_http_dynamic_upstream_lua_error(L, "no conf for upstream");
        }
        uscf = ucf->upstream;
        if (uscf == NULL) {
            return ngx_http_dynamic_upstream_lua_error(L, "no srv conf for upstream");
        }
    }

    lua_pushboolean(L, 1);
    lua_pushlstring(L, (char *) uscf->host.data, uscf->host.len);
    lua_pushnil(L);

    return 3;
}


static ngx_str_t
lua_get_string(lua_State *L, ngx_slab_pool_t *shpool, int index)
{
    ngx_str_t r = { .data = NULL, .len = 0 };
    const char *s;
    s = lua_tostring(L, index);
    if (s != NULL) {
        r.len = strlen(s);
        r.data = ngx_slab_calloc_locked(shpool, r.len);
        if (r.data != NULL) {
            ngx_memcpy(r.data, s, r.len);
        } else {
            r.len = 0;
        }
    }
    return r;
}


static ngx_str_t type_http = { .data = (u_char *) "http", .len = 4 };
static ngx_str_t type_tcp = { .data = (u_char *) "tcp", .len = 3 };
static ngx_str_t http_GET = { .data = (u_char *) "GET", .len = 3 };


static int
ngx_http_dynamic_upstream_lua_update_healthcheck(lua_State *L)
{
    ngx_http_upstream_srv_conf_t             *uscf;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf = NULL;
    ngx_dynamic_upstream_op_t                 op;
    int                                       top = lua_gettop(L);
    const char                               *error = "Unknown error", *s0, *s1;
    ngx_pair_t                               *header;
    ngx_array_t                              *headers;
    ngx_uint_t                               *code;
    ngx_array_t                              *codes;
    ngx_uint_t                                i;
    ngx_str_t                                 s;
    ngx_http_request_t                       *r;

    r = ngx_http_lua_get_request(L);
    if (r == NULL) {
        return ngx_http_dynamic_upstream_lua_error(L, "no request object found");
    }

    if (top != 2 && !lua_istable(L, 2)) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    op.upstream.data = (u_char *) luaL_checklstring(L, 1, &op.upstream.len);

    uscf = ngx_dynamic_upstream_get(L, &op);
    if (uscf != NULL) {
        ucscf = ngx_http_get_dynamic_upstream_lua_srv_conf(uscf);
    }

    if (ucscf == NULL) {
        return ngx_http_dynamic_upstream_lua_error(L, "Upstream not found");
    }

    ngx_shmtx_lock(&ucscf->shpool->mutex);

    if (ucscf->data == NULL) {
        ucscf->data = ngx_slab_calloc(ucscf->shpool, sizeof(ngx_http_upstream_check_opts_t));
        if (ucscf->data == NULL) {
            error = "Memory allocation error";
            goto error;
        }
    }

    ngx_safe_slab_free(ucscf->shpool, (void **) &ucscf->data->type.data);
    ngx_safe_slab_free(ucscf->shpool, (void **) &ucscf->data->request_uri.data);
    ngx_safe_slab_free(ucscf->shpool, (void **) &ucscf->data->request_method.data);
    ngx_safe_slab_free(ucscf->shpool, (void **) &ucscf->data->request_body.data);
    ngx_safe_slab_free(ucscf->shpool, (void **) &ucscf->data->response_body.data);
    ngx_safe_slab_free(ucscf->shpool, (void **) &ucscf->data->response_codes);
    if (ucscf->data->request_headers) {
        for (i = 0; i < ucscf->data->request_headers_count; ++i) {
            ngx_safe_slab_free(ucscf->shpool, (void **) &ucscf->data->request_headers[i].name.data);
            ngx_safe_slab_free(ucscf->shpool, (void **) &ucscf->data->request_headers[i].value.data);
        }
        ngx_safe_slab_free(ucscf->shpool, (void **) &ucscf->data->request_headers);
        ucscf->data->request_headers_count = 0;
    }
    ucscf->data->response_codes_count = 0;

    lua_getfield(L, 2, "fall");
    lua_getfield(L, 2, "rise");
    lua_getfield(L, 2, "timeout");

    ucscf->data->fall = lua_tointeger(L, -3);
    ucscf->data->rise = lua_tointeger(L, -2);
    ucscf->data->timeout = lua_tointeger(L, -1);

    if (ucscf->data->fall == 0) {
        ucscf->data->fall = 1;
    }

    if (ucscf->data->rise == 0) {
        ucscf->data->rise = 1;
    }

    if (ucscf->data->timeout == 0) {
        ucscf->data->timeout = 1000;
    }

    lua_pop(L, 3);

    lua_getfield(L, 2, "command");

    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "uri");
        lua_getfield(L, 3, "body");

        s0 = lua_tostring(L, -2);
        ucscf->data->request_body = lua_get_string(L, ucscf->shpool, -1);

        lua_pop(L, 2);

        if (s0) {
            ucscf->data->type = ngx_shm_copy_string(ucscf->shpool, type_http);
            s1 = strchr(s0, ' ');
            if (s1 != NULL) {
//              GET /
                s.data = (u_char *) s0; s.len = s1 - s0;
                ucscf->data->request_method = ngx_shm_copy_string(ucscf->shpool, s);
                s.data = (u_char *) ++s1; s.len = strlen(s1);
                ucscf->data->request_uri = ngx_shm_copy_string(ucscf->shpool, s);
            } else {
//              /
                ucscf->data->request_method = ngx_shm_copy_string(ucscf->shpool, http_GET);
                s.data = (u_char *) s0; s.len = strlen(s0);
                ucscf->data->request_uri = ngx_shm_copy_string(ucscf->shpool, s);
            }
        } else {
            ucscf->data->type = ngx_shm_copy_string(ucscf->shpool, type_tcp);
        }

        lua_getfield(L, 3, "headers");

        if (lua_istable(L, 4)) {
            lua_pushvalue(L, -1);
            lua_pushnil(L);

            headers = ngx_array_create(r->pool, 100, sizeof(ngx_pair_t));
            if (headers == NULL)
            {
                error = "Memory allocation error";
                goto error;
            }

            while (lua_next(L, -2))
            {
                header = ngx_array_push(headers);
                if (header == NULL)
                {
                    error = "Memory allocation error";
                    goto error;
                }

                lua_pushvalue(L, -2);

                header->name = lua_get_string(L, ucscf->shpool, -1);
                header->value = lua_get_string(L, ucscf->shpool, -2);

                lua_pop(L, 2);
            }

            ucscf->data->request_headers_count = headers->nelts;
            ucscf->data->request_headers = ngx_slab_calloc_locked(ucscf->shpool,
                                                                  ucscf->data->request_headers_count * sizeof(ngx_pair_t));
            if (ucscf->data->request_headers == NULL) {
                ngx_array_destroy(headers);
                error = "Memory allocation error";
                goto error;
            }

            header = headers->elts;
            for (i = 0; i < headers->nelts; ++i) {
                ucscf->data->request_headers[i].name = header[i].name;
                ucscf->data->request_headers[i].value = header[i].value;
            }

            ngx_array_destroy(headers);

            lua_pop(L, 1);
        }

        lua_pop(L, 1);

        lua_getfield(L, 3, "expected");

        if (lua_istable(L, 4)) {
            lua_getfield(L, 4, "body");
            ucscf->data->response_body = lua_get_string(L, ucscf->shpool, -1);
            lua_pop(L, 1);

            lua_getfield(L, 4, "codes");

            if (lua_istable(L, 4)) {
                lua_pushvalue(L, -1);
                lua_pushnil(L);

                codes = ngx_array_create(r->pool, 100, sizeof(ngx_uint_t));
                if (codes == NULL)
                {
                    error = "Memory allocation error";
                    goto error;
                }

                while (lua_next(L, -2))
                {
                    code = ngx_array_push(codes);
                    if (code == NULL)
                    {
                        error = "Memory allocation error";
                        ngx_array_destroy(codes);
                        goto error;
                    }

                    lua_pushvalue(L, -2);
                    *code = lua_tointeger(L, -2);

                    lua_pop(L, 2);
                }

                ucscf->data->response_codes_count = codes->nelts;
                ucscf->data->response_codes = ngx_slab_calloc_locked(ucscf->shpool,
                                                                     ucscf->data->response_codes_count * sizeof(ngx_uint_t));
                if (ucscf->data->response_codes == NULL) {
                    ngx_array_destroy(codes);
                    error = "Memory allocation error";
                    goto error;
                }

                code = codes->elts;
                for (i = 0; i < codes->nelts; ++i) {
                    ucscf->data->response_codes[i] = code[i];
                }

                ngx_array_destroy(codes);

                lua_pop(L, 1);
            }

            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    
    ngx_shmtx_unlock(&ucscf->shpool->mutex);

    lua_settop(L, top);

    lua_pushboolean(L, 1);
    lua_pushnil(L);

    return 2;

error:

    ngx_shmtx_unlock(&ucscf->shpool->mutex);

    lua_settop(L, top);

    return ngx_http_dynamic_upstream_lua_error(L, error);
}