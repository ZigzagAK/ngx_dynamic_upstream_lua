#include <ngx_core.h>
#include <ngx_http.h>
#include <lauxlib.h>
#include "ngx_http_lua_api.h"


#include "../ngx_dynamic_upstream/src/ngx_dynamic_upstream_module.h"


ngx_module_t ngx_http_dynamic_upstream_lua_module;

extern ngx_int_t ngx_stream_dynamic_upstream_lua_init(ngx_conf_t *cf);


static ngx_int_t ngx_http_dynamic_upstream_lua_init(ngx_conf_t *cf);
static int ngx_http_dynamic_upstream_lua_create_module(lua_State * L);


static int ngx_http_dynamic_upstream_lua_get_upstreams(lua_State * L);
static int ngx_http_dynamic_upstream_lua_get_healthchecks(lua_State * L);
static int ngx_http_dynamic_upstream_lua_get_peers(lua_State * L);
static int ngx_http_dynamic_upstream_lua_get_primary_peers(lua_State * L);
static int ngx_http_dynamic_upstream_lua_get_backup_peers(lua_State * L);
static int ngx_http_dynamic_upstream_lua_set_peer_down(lua_State * L);
static int ngx_http_dynamic_upstream_lua_set_peer_up(lua_State * L);
static int ngx_http_dynamic_upstream_lua_add_primary_peer(lua_State * L);
static int ngx_http_dynamic_upstream_lua_add_backup_peer(lua_State * L);
static int ngx_http_dynamic_upstream_lua_remove_peer(lua_State * L);
static int ngx_http_dynamic_upstream_lua_update_peer(lua_State * L);


static ngx_http_upstream_main_conf_t *
    ngx_http_lua_upstream_get_upstream_main_conf(lua_State *L);


struct ngx_header_s {
    ngx_str_t name;
    ngx_str_t value;
};
typedef struct ngx_header_s ngx_header_t;


struct ngx_http_dynamic_upstream_lua_srv_conf_s {
    ngx_str_t upstream;

    ngx_str_t  type;
    ngx_uint_t fall;
    ngx_uint_t rise;
    ngx_msec_t timeout; 

    ngx_str_t    request_uri;
    ngx_str_t    request_method;
    ngx_array_t *request_headers;
    ngx_str_t    request_body;

    ngx_array_t *response_codes;
    ngx_str_t    response_body;    
};
typedef struct ngx_http_dynamic_upstream_lua_srv_conf_s ngx_http_dynamic_upstream_lua_srv_conf_t;

static char *ngx_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_dynamic_upstream_lua_check_request_uri(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_dynamic_upstream_lua_check_request_headers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_dynamic_upstream_lua_check_response_codes(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char *
ngx_http_dynamic_upstream_lua_init_main_conf(ngx_conf_t *cf, void *conf);

static void *
ngx_http_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf);
static char *
ngx_http_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf, void *conf);


static ngx_command_t ngx_dynamic_upstream_lua_commands[] = {

    { ngx_string("check"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_dynamic_upstream_lua_check,
      0,
      0,
      NULL },

    { ngx_string("check_request_uri"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE2,
      ngx_dynamic_upstream_lua_check_request_uri,
      0,
      0,
      NULL },

    { ngx_string("check_request_headers"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_dynamic_upstream_lua_check_request_headers,
      0,
      0,
      NULL },

    { ngx_string("check_request_body"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_dynamic_upstream_lua_check_request_body,
      0,
      0,
      NULL },

    { ngx_string("check_response_codes"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_dynamic_upstream_lua_check_response_codes,
      0,
      0,
      NULL },

    { ngx_string("check_response_body"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_dynamic_upstream_lua_check_response_body,
      0,
      0,
      NULL },

    ngx_null_command

};


static ngx_http_module_t ngx_http_dynamic_upstream_lua_ctx = {
    NULL,                                         /* preconfiguration */
    ngx_http_dynamic_upstream_lua_init,           /* postconfiguration */
    NULL,                                         /* create main configuration */
    ngx_http_dynamic_upstream_lua_init_main_conf, /* init main configuration */
    ngx_http_dynamic_upstream_lua_create_srv_conf,/* create server configuration */
    NULL,                                         /* merge server configuration */
    NULL,                                         /* create location configuration */
    NULL                                          /* merge location configuration */
};


ngx_module_t ngx_http_dynamic_upstream_lua_module = {
    NGX_MODULE_V1,
    &ngx_http_dynamic_upstream_lua_ctx,  /* module context */
    ngx_dynamic_upstream_lua_commands,   /* module directives */
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


static char *
ngx_http_dynamic_upstream_lua_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_uint_t                                  i;
    ngx_http_upstream_srv_conf_t              **uscfp;
    ngx_http_upstream_main_conf_t              *umcf;

    umcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);

    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        if (ngx_http_dynamic_upstream_lua_init_srv_conf(cf, uscfp[i]) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_dynamic_upstream_lua_srv_conf_t));
    if (ucscf == NULL) {
        return NULL;
    }

    bzero(ucscf, sizeof(ngx_http_dynamic_upstream_lua_srv_conf_t));

    ucscf->fall     = NGX_CONF_UNSET_UINT;
    ucscf->rise     = NGX_CONF_UNSET_UINT;
    ucscf->timeout  = NGX_CONF_UNSET_MSEC;

    ucscf->request_headers = ngx_array_create(cf->pool, 500, sizeof(ngx_header_t));
    if (ucscf->request_headers == NULL)
    {
        return NULL;
    }

    ucscf->response_codes = ngx_array_create(cf->pool, 20, sizeof(ngx_uint_t));
    if (ucscf->response_codes == NULL)
    {
        return NULL;
    }

    return ucscf;
}


static char *
ngx_http_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_upstream_srv_conf_t             *us = conf;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    if (us->srv_conf == NULL) {
        return NGX_CONF_OK;
    }

    ucscf = ngx_http_conf_upstream_srv_conf(us, ngx_http_dynamic_upstream_lua_module);

    if (ucscf->fall == NGX_CONF_UNSET_UINT) {
        ucscf->fall = 2;
    }

    if (ucscf->rise == NGX_CONF_UNSET_UINT) {
        ucscf->rise = 2;
    }

    if (ucscf->timeout == NGX_CONF_UNSET_MSEC) {
        ucscf->timeout = 1000;
    }

    ucscf->upstream = us->host;

    return NGX_CONF_OK;
}


static char *ngx_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_uint_t i;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);
    
    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; ++i)
    {
        if (ngx_strncmp(value[i].data, "type=", 5) == 0) {
            ucscf->type.len = value[i].len - 5;
            ucscf->type.data = value[i].data + 5;

            if (ngx_strncmp(ucscf->type.data, "http", 4) != 0 && ngx_strncmp(ucscf->type.data, "tcp", 3) != 0) {
                goto invalid_check_parameter;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "timeout=", 8) == 0) {
            ucscf->timeout = ngx_atoi(value[i].data + 8, value[i].len - 8);

            if (ucscf->timeout == (ngx_msec_t) NGX_ERROR || ucscf->timeout == 0) {
                goto invalid_check_parameter;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "rise=", 5) == 0) {
            ucscf->rise = ngx_atoi(value[i].data + 5, value[i].len - 5);

            if (ucscf->rise == (ngx_uint_t) NGX_ERROR || ucscf->rise == 0) {
                goto invalid_check_parameter;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "fall=", 5) == 0) {
            ucscf->fall = ngx_atoi(value[i].data + 5, value[i].len - 5);

            if (ucscf->fall == (ngx_uint_t) NGX_ERROR || ucscf->fall == 0) {
                goto invalid_check_parameter;
            }

            continue;
        }
    }
    
    return NGX_CONF_OK;

invalid_check_parameter:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static char *ngx_dynamic_upstream_lua_check_request_uri(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_str_t                                *value;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);

    value = cf->args->elts;

    ucscf->request_method = value[1];
    ucscf->request_uri = value[2];

    return NGX_CONF_OK;
}


static char *ngx_dynamic_upstream_lua_check_request_headers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_header_t                             *header;
    char                                     *sep;
    ngx_uint_t i;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);
    
    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; ++i)
    {
        header = ngx_array_push(ucscf->request_headers);
        if (header == NULL)
        {
            return NGX_CONF_ERROR;
        }

        sep = ngx_strchr(value[i].data, '=');
        if (sep == NULL)
        {
            goto invalid_check_parameter;
        }

        header->name.data = value[i].data;
        header->name.len = (u_char *) sep - value[i].data;

        header->value.data = (u_char *) sep + 1;
        header->value.len = (ngx_uint_t) ((value[i].data + value[i].len - (u_char *) sep) - 1);
    }

    return NGX_CONF_OK;

invalid_check_parameter:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid header desc \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static char *ngx_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);

    ucscf->request_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}


static char *
ngx_dynamic_upstream_lua_check_response_codes(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_uint_t                               *code;
    ngx_uint_t                                i;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);
    
    value = cf->args->elts;

    ucscf->response_codes = ngx_array_create(cf->pool, cf->args->nelts, sizeof(ngx_uint_t));

    for (i = 1; i < cf->args->nelts; ++i)
    {
        code = ngx_array_push(ucscf->response_codes);
        *code = ngx_atoi(value[i].data, value[i].len);
        if (*code == (ngx_uint_t) NGX_ERROR || *code == 0) {
            goto invalid_check_parameter;
        }
    }

    return NGX_CONF_OK;

invalid_check_parameter:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid response code \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static char *ngx_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);

    ucscf->response_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}


ngx_int_t
ngx_http_dynamic_upstream_lua_init(ngx_conf_t *cf)
{
    if (ngx_http_lua_add_package_preload(cf, "ngx.dynamic_upstream",
                                         ngx_http_dynamic_upstream_lua_create_module)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_stream_dynamic_upstream_lua_init(cf)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static int
ngx_http_dynamic_upstream_lua_create_module(lua_State * L)
{
    lua_createtable(L, 0, 11);

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_get_upstreams);
    lua_setfield(L, -2, "get_upstreams");

    lua_pushcfunction(L, ngx_http_dynamic_upstream_lua_get_healthchecks);
    lua_setfield(L, -2, "get_healthchecks");

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


static void
ngx_http_dynamic_upstream_lua_push_healthchecks(lua_State *L, ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *uduscf;
    int n = 4;
    ngx_uint_t i;

    uduscf = ngx_http_conf_upstream_srv_conf(us, ngx_http_dynamic_upstream_lua_module);

    if (uduscf == NULL || uduscf->type.data == NULL) {
        lua_pushliteral(L, "healthcheck");
        lua_pushnil(L);
        lua_rawset(L, -3);
        return;
    }
  
    if (uduscf->request_uri.len != 0) {
        ++n;
    }

    lua_pushliteral(L, "healthcheck");
    lua_createtable(L, 0, n);

    lua_pushliteral(L, "typ");
    lua_pushlstring(L, (char *) uduscf->type.data,
                                uduscf->type.len);
    lua_rawset(L, -3);

    lua_pushliteral(L, "fall");
    lua_pushinteger(L, (lua_Integer) uduscf->fall);
    lua_rawset(L, -3);

    lua_pushliteral(L, "rise");
    lua_pushinteger(L, (lua_Integer) uduscf->rise);
    lua_rawset(L, -3);

    lua_pushliteral(L, "timeout");
    lua_pushinteger(L, (lua_Integer) uduscf->timeout);
    lua_rawset(L, -3);

    if (uduscf->request_uri.len != 0) {
        n = 2;

        if (uduscf->request_headers != NGX_CONF_UNSET_PTR) {
          ++n;
        }
        if (uduscf->request_body.len != 0) {
          ++n;
        }
        if (uduscf->response_codes != NGX_CONF_UNSET_PTR || uduscf->response_body.len != 0) {
          ++n;
        }
  
        lua_pushliteral(L, "command");
        lua_createtable(L, 0, n);

        {
            lua_pushliteral(L, "uri");
            lua_pushlstring(L, (char *) uduscf->request_uri.data,
                                        uduscf->request_uri.len);
            lua_rawset(L, -3);

            if (uduscf->request_headers != NGX_CONF_UNSET_PTR) {
                ngx_header_t *headers = uduscf->request_headers->elts;

                lua_pushliteral(L, "headers");
                lua_createtable(L, 0, uduscf->request_headers->nelts);

                for (i = 0; i < uduscf->request_headers->nelts; ++i) {
                    lua_pushlstring(L, (char *) headers[i].name.data,
                                                headers[i].name.len);
                    lua_pushlstring(L, (char *) headers[i].value.data,
                                                headers[i].value.len);
                    lua_rawset(L, -3);
                }

                lua_rawset(L, -3);
            }

            if (uduscf->request_body.len != 0) {
                lua_pushliteral(L, "body");
                lua_pushlstring(L, (char *) uduscf->request_body.data,
                                            uduscf->request_body.len);
                lua_rawset(L, -3);
            }

            if (uduscf->response_codes != NGX_CONF_UNSET_PTR || uduscf->response_body.len != 0) {
                n = 0;
                n = n + (uduscf->response_codes != NGX_CONF_UNSET_PTR ? 1 : 0);
                n = n + (uduscf->response_body.len != 0 ? 1 : 0);

                lua_pushliteral(L, "expected");
                lua_createtable(L, 0, n);

                if (uduscf->response_codes != NGX_CONF_UNSET_PTR) {
                    ngx_uint_t *codes = uduscf->response_codes->elts;

                    lua_pushliteral(L, "codes");
                    lua_createtable(L, uduscf->response_codes->nelts, 0);

                    for (i = 0; i < uduscf->response_codes->nelts; ++i) {
                        lua_pushinteger(L, (lua_Integer) codes[i]);
                        lua_rawseti(L, -2, i + 1);
                    }

                    lua_rawset(L, -3);
                }

                if (uduscf->response_body.len != 0) {
                    lua_pushliteral(L, "body");
                    lua_pushlstring(L, (char *) uduscf->response_body.data,
                                                uduscf->response_body.len);
                    lua_rawset(L, -3);
                }

                lua_rawset(L, -3);
            }
        }

        lua_rawset(L, -3);
    }

    lua_rawset(L, -3);
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


static int
ngx_http_dynamic_upstream_lua_get_healthchecks(lua_State * L)
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

        ngx_http_dynamic_upstream_lua_push_healthchecks(L, uscf);

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
