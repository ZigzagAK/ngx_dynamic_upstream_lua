#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_stream.h>
#include <lauxlib.h>
#include "ngx_http_lua_api.h"


#include "ngx_dynamic_shm.h"
#include "ngx_dynamic_upstream_stream_lua.h"


#include "../ngx_dynamic_upstream/src/ngx_dynamic_upstream_module.h"


extern ngx_module_t ngx_stream_dynamic_upstream_lua_module;


static int
ngx_stream_dynamic_upstream_lua_get_upstreams(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_get_healthcheck(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_get_peers(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_get_primary_peers(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_get_backup_peers(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_set_peer_down(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_set_peer_up(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_add_primary_peer(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_add_backup_peer(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_remove_peer(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_update_peer(lua_State * L);
static int
ngx_stream_dynamic_upstream_lua_update_healthcheck(lua_State * L);


static ngx_stream_upstream_main_conf_t *
ngx_stream_lua_upstream_get_upstream_main_conf();


int
ngx_stream_dynamic_upstream_lua_create_module(lua_State * L)
{
    lua_createtable(L, 0, 12);

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_get_upstreams);
    lua_setfield(L, -2, "get_upstreams");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_get_healthcheck);
    lua_setfield(L, -2, "get_healthcheck");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_get_peers);
    lua_setfield(L, -2, "get_peers");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_get_primary_peers);
    lua_setfield(L, -2, "get_primary_peers");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_get_backup_peers);
    lua_setfield(L, -2, "get_backup_peers");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_set_peer_down);
    lua_setfield(L, -2, "set_peer_down");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_set_peer_up);
    lua_setfield(L, -2, "set_peer_up");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_add_primary_peer);
    lua_setfield(L, -2, "add_primary_peer");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_add_backup_peer);
    lua_setfield(L, -2, "add_backup_peer");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_remove_peer);
    lua_setfield(L, -2, "remove_peer");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_update_peer);
    lua_setfield(L, -2, "update_peer");

    lua_pushcfunction(L, ngx_stream_dynamic_upstream_lua_update_healthcheck);
    lua_setfield(L, -2, "update_healthcheck");

    return 1;
}


static ngx_stream_upstream_main_conf_t *
ngx_stream_lua_upstream_get_upstream_main_conf()
{
    return ngx_stream_cycle_get_module_main_conf(ngx_cycle,
                                                 ngx_stream_upstream_module);
}


static ngx_stream_upstream_srv_conf_t *
ngx_dynamic_upstream_get(lua_State * L, ngx_dynamic_upstream_op_t *op)
{
    ngx_uint_t                       i;
    ngx_stream_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_stream_upstream_main_conf_t  *umcf;

    umcf  = ngx_stream_lua_upstream_get_upstream_main_conf();
    if (umcf == NULL) {
        return NULL;
    }
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
ngx_dynamic_upstream_lua_create_response(ngx_stream_upstream_rr_peers_t *primary, lua_State * L, int flags)
{
    ngx_stream_upstream_rr_peer_t  *peer;
    ngx_stream_upstream_rr_peers_t *peers, *backup;
    int                             size = 0, n, i = 1;

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
ngx_stream_dynamic_upstream_lua_op_defaults(lua_State * L, ngx_dynamic_upstream_op_t *op, int operation)
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
    op->op_param     = NGX_DYNAMIC_UPSTEAM_OP_PARAM_STREAM;

    op->upstream.data = (u_char *) luaL_checklstring(L, 1, &op->upstream.len);
}


static int
ngx_stream_dynamic_upstream_lua_error(lua_State * L, const char *error)
{
    lua_pushboolean(L, 0);
    lua_pushnil(L);
    lua_pushlstring(L, error, strlen(error));
    return 3;
}


static int
ngx_stream_dynamic_upstream_lua_op(lua_State * L, ngx_dynamic_upstream_op_t *op, int flags)
{
    ngx_int_t                       rc;
    ngx_stream_upstream_srv_conf_t *uscf;
    ngx_stream_upstream_rr_peers_t *primary;

    uscf = ngx_dynamic_upstream_get(L, op);
    if (uscf == NULL) {
        return ngx_stream_dynamic_upstream_lua_error(L, "Upstream not found");
    }

    if (op->op & (NGX_DYNAMIC_UPSTEAM_OP_ADD | NGX_DYNAMIC_UPSTEAM_OP_REMOVE)) {
        if (uscf->shm_zone == NULL) {
            return ngx_stream_dynamic_upstream_lua_error(L, "Shared zone segment is not defined in upstream");
        }
    }

    rc = ngx_dynamic_upstream_stream_op(ngx_http_lua_get_request(L)->connection->log, op, uscf);
    if (rc != NGX_OK) {
        return ngx_stream_dynamic_upstream_lua_error(L, "Internal server error");
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
ngx_stream_dynamic_upstream_lua_get_upstreams(lua_State * L)
{
    ngx_uint_t                       i, count = 0;
    ngx_stream_upstream_srv_conf_t **uscfp, *uscf;
    ngx_stream_upstream_main_conf_t *umcf;

    if (lua_gettop(L) != 0) {
        return ngx_stream_dynamic_upstream_lua_error(L, "no argument expected");
    }

    umcf = ngx_stream_lua_upstream_get_upstream_main_conf();
    uscfp = umcf->upstreams.elts;

    lua_pushboolean(L, 1);

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        uscf = uscfp[i];
        if (uscf->srv_conf != NULL) {
            ++count;
        }
    }

    lua_newtable(L);
    lua_createtable(L, count, 0);

    umcf  = ngx_stream_lua_upstream_get_upstream_main_conf();
    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        uscf = uscfp[i];
        if (uscf->srv_conf != NULL) {
            lua_pushlstring(L, (char *) uscf->host.data, uscf->host.len);
            lua_rawseti(L, -2, i + 1);
        }
    }

    lua_pushnil(L);

    return 3;
}


extern ngx_str_t shared_zone_stream_prefix;


static ngx_stream_dynamic_upstream_lua_srv_conf_t *
ngx_stream_get_dynamic_upstream_lua_srv_conf(ngx_stream_upstream_srv_conf_t *uscf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_stream_conf_upstream_srv_conf(uscf, ngx_stream_dynamic_upstream_lua_module);

    if (ucscf->shm_zone == NULL) {
        ucscf->shm_zone = ngx_shared_memory_find(ngx_cycle,
                                                 shared_zone_stream_prefix,
                                                 ucscf->conf->upstream,
                                                 &ngx_stream_dynamic_upstream_lua_module);
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
ngx_stream_dynamic_upstream_lua_push_healthcheck(lua_State *L, ngx_stream_upstream_srv_conf_t *uscf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;
    int                                         n;

    ucscf = ngx_stream_get_dynamic_upstream_lua_srv_conf(uscf);

    if (ucscf == NULL || ucscf->data == NULL) {
        goto empty;
    }

    ngx_shmtx_lock(&ucscf->shpool->mutex);

    n = (ucscf->data->fall == NGX_CONF_UNSET_UINT ? 0 : 1) +
        (ucscf->data->rise == NGX_CONF_UNSET_UINT ? 0 : 1) +
        (ucscf->data->timeout == NGX_CONF_UNSET_MSEC ? 0 : 1);

    if (n == 0) {
        goto empty;
    }
  
    lua_pushliteral(L, "healthcheck");
    lua_createtable(L, 0, n);

    if (ucscf->data->fall != NGX_CONF_UNSET_UINT) {
        lua_pushliteral(L, "fall");
        lua_pushinteger(L, (lua_Integer) ucscf->data->fall);
        lua_rawset(L, -3);
    }

    if (ucscf->data->rise != NGX_CONF_UNSET_UINT) {
        lua_pushliteral(L, "rise");
        lua_pushinteger(L, (lua_Integer) ucscf->data->rise);
        lua_rawset(L, -3);
    }

    if (ucscf->data->timeout != NGX_CONF_UNSET_MSEC) {
        lua_pushliteral(L, "timeout");
        lua_pushinteger(L, (lua_Integer) ucscf->data->timeout);
        lua_rawset(L, -3);
    }

    lua_rawset(L, -3);

    ngx_shmtx_unlock(&ucscf->shpool->mutex);

    return;

empty:

    ngx_shmtx_unlock(&ucscf->shpool->mutex);

    lua_pushliteral(L, "healthcheck");
    lua_pushnil(L);
    lua_rawset(L, -3);
}


static int
ngx_stream_dynamic_upstream_lua_get_healthcheck(lua_State * L)
{
    ngx_uint_t                    i, count = 0;
    ngx_stream_upstream_srv_conf_t  **uscfp, *uscf;
    ngx_stream_upstream_main_conf_t *umcf;

    if (lua_gettop(L) != 0) {
        return ngx_stream_dynamic_upstream_lua_error(L, "no argument expected");
    }

    umcf = ngx_stream_lua_upstream_get_upstream_main_conf();
    uscfp = umcf->upstreams.elts;

    lua_pushboolean(L, 1);

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        uscf = uscfp[i];
        if (uscf->srv_conf != NULL) {
            ++count;
        }
    }

    lua_newtable(L);
    lua_createtable(L, count, 0);

    umcf  = ngx_stream_lua_upstream_get_upstream_main_conf();
    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        uscf = uscfp[i];

        if (uscf->srv_conf != NULL) {
            lua_createtable(L, 0, 2);

            lua_pushliteral(L, "name");
            lua_pushlstring(L, (char *) uscf->host.data, uscf->host.len);
            lua_rawset(L, -3);

            ngx_stream_dynamic_upstream_lua_push_healthcheck(L, uscf);

            lua_rawseti(L, -2, i + 1);
        }
    }

    lua_pushnil(L);

    return 3;
}


static int
ngx_stream_dynamic_upstream_lua_get_peers(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;
    if (lua_gettop(L) != 1) {
        return ngx_stream_dynamic_upstream_lua_error(L, "exactly one argument expected");
    }
    ngx_stream_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_LIST);
    op.verbose = 1;
    return ngx_stream_dynamic_upstream_lua_op(L, &op, PRIMARY|BACKUP);
}


static int
ngx_stream_dynamic_upstream_lua_get_primary_peers(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;
    if (lua_gettop(L) != 1) {
        return ngx_stream_dynamic_upstream_lua_error(L, "exactly one argument expected");
    }
    ngx_stream_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_LIST);
    op.verbose = 1;
    return ngx_stream_dynamic_upstream_lua_op(L, &op, PRIMARY);
}


static int
ngx_stream_dynamic_upstream_lua_get_backup_peers(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;
    if (lua_gettop(L) != 1) {
        return ngx_stream_dynamic_upstream_lua_error(L, "exactly one argument expected");
    }
    ngx_stream_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_LIST);
    op.verbose = 1;
    return ngx_stream_dynamic_upstream_lua_op(L, &op, BACKUP);
}


static int
ngx_stream_dynamic_upstream_lua_set_peer_down(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 2) {
        return ngx_stream_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    ngx_stream_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_PARAM);

    op.down = 1;
    op.op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN;

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    return ngx_stream_dynamic_upstream_lua_op(L, &op, 0);
}


static int
ngx_stream_dynamic_upstream_lua_set_peer_up(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 2) {
        return ngx_stream_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    ngx_stream_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_PARAM);

    op.up = 1;
    op.op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_UP;

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    return ngx_stream_dynamic_upstream_lua_op(L, &op, 0);
}


static int
ngx_stream_dynamic_upstream_lua_add_peer_impl(lua_State * L, int backup)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 2) {
        return ngx_stream_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    ngx_stream_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_ADD);

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    op.backup = backup;

    return ngx_stream_dynamic_upstream_lua_op(L, &op, 0);
}


static int
ngx_stream_dynamic_upstream_lua_add_primary_peer(lua_State * L)
{
    return ngx_stream_dynamic_upstream_lua_add_peer_impl(L, 0);
}


static int
ngx_stream_dynamic_upstream_lua_add_backup_peer(lua_State * L)
{
    return ngx_stream_dynamic_upstream_lua_add_peer_impl(L, 1);
}


static int
ngx_stream_dynamic_upstream_lua_remove_peer(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 2) {
        return ngx_stream_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    ngx_stream_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_REMOVE);

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    return ngx_stream_dynamic_upstream_lua_op(L, &op, 0);
}


static int
ngx_stream_dynamic_upstream_lua_update_peer_parse_params(lua_State * L, ngx_dynamic_upstream_op_t *op)
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
ngx_stream_dynamic_upstream_lua_update_peer(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 3 || !lua_istable(L, 3)) {
        return ngx_stream_dynamic_upstream_lua_error(L, "exactly 3 arguments expected");
    }

    ngx_stream_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_PARAM);

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    ngx_stream_dynamic_upstream_lua_update_peer_parse_params(L, &op);

    return ngx_stream_dynamic_upstream_lua_op(L, &op, 0);
}

static int
ngx_stream_dynamic_upstream_lua_update_healthcheck(lua_State *L)
{
    ngx_stream_upstream_srv_conf_t             *uscf;
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf = NULL;
    ngx_dynamic_upstream_op_t                   op;
    const char                                 *error = "Unknown error";

    if (lua_gettop(L) != 2 && !lua_istable(L, 2)) {
        return ngx_stream_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    op.upstream.data = (u_char *) luaL_checklstring(L, 1, &op.upstream.len);

    uscf = ngx_dynamic_upstream_get(L, &op);
    if (uscf != NULL) {
        ucscf = ngx_stream_get_dynamic_upstream_lua_srv_conf(uscf);
    }

    if (ucscf == NULL) {
        return ngx_stream_dynamic_upstream_lua_error(L, "Upstream not found");
    }

    ngx_shmtx_lock(&ucscf->shpool->mutex);

    if (ucscf->data == NULL) {
        ucscf->data = ngx_slab_calloc(ucscf->shpool, sizeof(ngx_stream_upstream_check_opts_t));
        if (ucscf->data == NULL) {
            error = "Memory allocation error";
            goto error;
        }
    }

    lua_getfield(L, 2, "fall");
    lua_getfield(L, 2, "rise");
    lua_getfield(L, 2, "timeout");

    ucscf->data->fall = lua_tointeger(L, -3);
    ucscf->data->rise = lua_tointeger(L, -2);
    ucscf->data->timeout = lua_tointeger(L, -1);

    lua_pop(L, 3);

    if (ucscf->data->fall == 0) {
        ucscf->data->fall = NGX_CONF_UNSET_UINT;
    }

    if (ucscf->data->rise == 0) {
        ucscf->data->rise = NGX_CONF_UNSET_UINT;
    }

    if (ucscf->data->timeout == 0) {
        ucscf->data->timeout = NGX_CONF_UNSET_MSEC;
    }

    ngx_shmtx_unlock(&ucscf->shpool->mutex);

    lua_pushboolean(L, 1);
    lua_pushnil(L);

    return 2;

error:

    ngx_shmtx_unlock(&ucscf->shpool->mutex);

    return ngx_stream_dynamic_upstream_lua_error(L, error);
}