#include <ngx_core.h>
#include <ngx_http.h>
#include <lauxlib.h>
#include "ngx_http_lua_api.h"

#include "../ngx_dynamic_upstream/src/ngx_dynamic_upstream_module.h"

extern ngx_int_t ngx_dynamic_upstream_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op,
                                         ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf);

ngx_module_t ngx_http_dynamic_upstream_lua_module;

static ngx_int_t ngx_http_dynamic_upstream_lua_init(ngx_conf_t *cf);
static int ngx_http_dynamic_upstream_lua_create_module(lua_State * L);

static int ngx_http_dynamic_upstream_lua_get_upstreams(lua_State * L);
static int ngx_http_dynamic_upstream_lua_get_servers(lua_State * L);
static int ngx_http_dynamic_upstream_lua_set_peer_down(lua_State * L);
static int ngx_http_dynamic_upstream_lua_set_peer_up(lua_State * L);
static int ngx_http_dynamic_upstream_lua_add_peer(lua_State * L);
static int ngx_http_dynamic_upstream_lua_remove_peer(lua_State * L);
static int ngx_http_dynamic_upstream_lua_update_peer(lua_State * L);

static ngx_http_upstream_main_conf_t *
    ngx_http_lua_upstream_get_upstream_main_conf(lua_State *L);

static ngx_http_module_t ngx_http_dynamic_upstream_lua_ctx = {
    NULL,                                   /* preconfiguration */
    ngx_http_dynamic_upstream_lua_init,     /* postconfiguration */
    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */
    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */
    NULL,                                   /* create location configuration */
    NULL                                    /* merge location configuration */
};


ngx_module_t ngx_http_dynamic_upstream_lua_module = {
    NGX_MODULE_V1,
    &ngx_http_dynamic_upstream_lua_ctx,  /* module context */
    NULL,                                /* module directives */
    NGX_HTTP_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};

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
    lua_createtable(L, 0, 7);

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_get_upstreams);
    lua_setfield(L, -2, "get_upstreams");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_get_servers);
    lua_setfield(L, -2, "get_servers");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_set_peer_down);
    lua_setfield(L, -2, "set_peer_down");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_set_peer_up);
    lua_setfield(L, -2, "set_peer_up");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_add_peer);
    lua_setfield(L, -2, "add_peer");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_remove_peer);
    lua_setfield(L, -2, "remove_peer");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_update_peer);
    lua_setfield(L, -2, "update_peer");

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
ngx_dynamic_upstream_get_zone(lua_State * L, ngx_dynamic_upstream_op_t *op)
{
    ngx_uint_t                      i;
    ngx_http_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_http_upstream_main_conf_t  *umcf;

    umcf  = ngx_http_lua_upstream_get_upstream_main_conf(L);
    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        uscf = uscfp[i];
        if (uscf->shm_zone != NULL &&
            uscf->shm_zone->shm.name.len == op->upstream.len &&
            ngx_strncmp(uscf->shm_zone->shm.name.data, op->upstream.data, op->upstream.len) == 0)
        {
            return uscf;
        }
    }

    return NULL;
}

static void
ngx_dynamic_upstream_lua_create_response(ngx_http_upstream_rr_peers_t *peers, lua_State * L)
{
    ngx_http_upstream_rr_peer_t  *peer;
    int                           size, n, i;

    for (peer = peers->peer, size = 0; peer; peer = peer->next) ++size;

    lua_newtable(L);
    lua_createtable(L, size, 0);

    for (peer = peers->peer, i = 1; peer; peer = peer->next, ++i) {
        n = 4;

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

        lua_pushliteral(L, "max_fails");
        lua_pushinteger(L, (lua_Integer) peer->max_fails);
        lua_rawset(L, -3);

        lua_pushliteral(L, "fail_timeout");
        lua_pushinteger(L, (lua_Integer) peer->fail_timeout);
        lua_rawset(L, -3);

        if (peer->down) {
            lua_pushliteral(L, "down");
            lua_pushboolean(L, 1);
            lua_rawset(L, -3);
        }

        lua_rawseti(L, -2, i);
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
ngx_http_dynamic_upstream_lua_op(lua_State * L, ngx_dynamic_upstream_op_t *op)
{
    ngx_int_t                       rc;
    ngx_http_upstream_srv_conf_t   *uscf;
    ngx_slab_pool_t                *shpool;

    uscf = ngx_dynamic_upstream_get_zone(L, op);
    if (uscf == NULL) {
        return ngx_http_dynamic_upstream_lua_error(L, "Upstream not found");
    }

    shpool = (ngx_slab_pool_t *) uscf->shm_zone->shm.addr;

    ngx_shmtx_lock(&shpool->mutex);

    rc = ngx_dynamic_upstream_op(ngx_http_lua_get_request(L), op, shpool, uscf);
    if (rc != NGX_OK) {
        ngx_shmtx_unlock(&shpool->mutex);
        return ngx_http_dynamic_upstream_lua_error(L, "Internal server error");
    }

    ngx_shmtx_unlock(&shpool->mutex);

    lua_pushboolean(L, 1);

    if (op->verbose) {
        ngx_dynamic_upstream_lua_create_response((ngx_http_upstream_rr_peers_t *)uscf->peer.data, L);
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
        lua_pushlstring(L, (char *) uscf->shm_zone->shm.name.data, uscf->shm_zone->shm.name.len);
        lua_rawseti(L, -2, i + 1);
    }

    lua_pushnil(L);

    return 3;
}

static int
ngx_http_dynamic_upstream_lua_get_servers(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;
    if (lua_gettop(L) != 1) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly one argument expected");
    }
    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_LIST);
    op.verbose = 1;
    return ngx_http_dynamic_upstream_lua_op(L, &op);
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

    return ngx_http_dynamic_upstream_lua_op(L, &op);
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

    return ngx_http_dynamic_upstream_lua_op(L, &op);
}

static int
ngx_http_dynamic_upstream_lua_add_peer(lua_State * L)
{
    ngx_dynamic_upstream_op_t op;

    if (lua_gettop(L) != 2) {
        return ngx_http_dynamic_upstream_lua_error(L, "exactly 2 arguments expected");
    }

    ngx_http_dynamic_upstream_lua_op_defaults(L, &op, NGX_DYNAMIC_UPSTEAM_OP_ADD);

    op.server.data = (u_char *) luaL_checklstring(L, 2, &op.server.len);

    return ngx_http_dynamic_upstream_lua_op(L, &op);
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

    return ngx_http_dynamic_upstream_lua_op(L, &op);
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

    return ngx_http_dynamic_upstream_lua_op(L, &op);
}
