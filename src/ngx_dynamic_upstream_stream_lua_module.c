#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_stream.h>
#include <lauxlib.h>


#ifndef NO_NGX_STREAM_LUA_MODULE
#include "ngx_stream_lua_api.h"
#endif

#include "ngx_dynamic_shm.h"
#include "ngx_dynamic_upstream_stream_lua.h"


ngx_module_t ngx_stream_dynamic_upstream_lua_module;


static char *
ngx_stream_dynamic_upstream_lua_init_main_conf(ngx_conf_t *cf, void *conf);


static ngx_int_t
ngx_stream_dynamic_upstream_lua_post_conf(ngx_conf_t *cf);


static void *
ngx_stream_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf);
static char *
ngx_stream_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf, void *conf);


static char *
ngx_stream_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_stream_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_stream_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_stream_dynamic_upstream_lua_disconnect_backup_if_primary_up(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_stream_dynamic_upstream_lua_disconnect_if_market_down(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_stream_dynamic_upstream_lua_disconnect_on_exiting(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_shm_zone_t *
ngx_stream_create_shm_zone(ngx_conf_t *cf, ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf);


static ngx_int_t ngx_stream_dynamic_upstream_write_filter(ngx_stream_session_t *s,
    ngx_chain_t *in, ngx_uint_t from_upstream);


static ngx_command_t ngx_stream_dynamic_upstream_lua_commands[] = {

    { ngx_string("check"),
      NGX_STREAM_UPS_CONF|NGX_CONF_1MORE,
      ngx_stream_dynamic_upstream_lua_check,
      0,
      0,
      NULL },

    { ngx_string("check_request_body"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE1,
      ngx_stream_dynamic_upstream_lua_check_request_body,
      0,
      0,
      NULL },

    { ngx_string("check_response_body"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE1,
      ngx_stream_dynamic_upstream_lua_check_response_body,
      0,
      0,
      NULL },

    { ngx_string("disconnect_backup_if_primary_up"),
      NGX_STREAM_UPS_CONF|NGX_CONF_NOARGS,
      ngx_stream_dynamic_upstream_lua_disconnect_backup_if_primary_up,
      0,
      0,
      NULL },

    { ngx_string("disconnect_if_market_down"),
      NGX_STREAM_UPS_CONF|NGX_CONF_NOARGS,
      ngx_stream_dynamic_upstream_lua_disconnect_if_market_down,
      0,
      0,
      NULL },

    { ngx_string("disconnect_on_exiting"),
      NGX_STREAM_UPS_CONF|NGX_CONF_NOARGS,
      ngx_stream_dynamic_upstream_lua_disconnect_on_exiting,
      0,
      0,
      NULL },

    ngx_null_command

};


static ngx_stream_module_t ngx_stream_dynamic_upstream_lua_ctx = {
    NULL,                                           /* preconfiguration */
    ngx_stream_dynamic_upstream_lua_post_conf,      /* postconfiguration */
    NULL,                                           /* create main configuration */
    ngx_stream_dynamic_upstream_lua_init_main_conf, /* init main configuration */
    ngx_stream_dynamic_upstream_lua_create_srv_conf,/* create server configuration */
    NULL                                            /* merge server configuration */
};


ngx_module_t ngx_stream_dynamic_upstream_lua_module = {
    NGX_MODULE_V1,
    &ngx_stream_dynamic_upstream_lua_ctx,     /* module context */
    ngx_stream_dynamic_upstream_lua_commands, /* module directives */
    NGX_STREAM_MODULE,                        /* module type */
    NULL,                                     /* init master */
    NULL,                                     /* init module */
    NULL,                                     /* init process */
    NULL,                                     /* init thread */
    NULL,                                     /* exit thread */
    NULL,                                     /* exit process */
    NULL,                                     /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_stream_filter_pt
ngx_stream_next_filter;


static ngx_uint_t
ngx_stream_dynamic_upstream_alive_primary(ngx_stream_upstream_rr_peers_t *peers, ngx_stream_upstream_rr_peer_t *current)
{
    ngx_stream_upstream_rr_peer_t  *peer;
    int c = 0;

    ngx_http_upstream_rr_peers_wlock(peers);

    for (peer = peers->peer; peer; peer = peer->next) {
        if (current == peer) {
            ngx_http_upstream_rr_peers_unlock(peers);
            return 0;
        }
        c = c + (peer->down == 0);
    }

    ngx_http_upstream_rr_peers_unlock(peers);

    return c;
}


static ngx_int_t
ngx_stream_dynamic_upstream_write_filter(ngx_stream_session_t *s, ngx_chain_t *in,
    ngx_uint_t from_upstream)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_stream_upstream_srv_conf_t             *uscf;
    ngx_stream_upstream_rr_peer_data_t         *peer_data;
    ngx_stream_upstream_rr_peer_t              *current;
    u_char                                     *p;

    if (s->upstream == NULL) {
        goto skip;
    }

    uscf = s->upstream->upstream;
    if (uscf == NULL) {
        goto skip;
    }

    ucscf = ngx_stream_conf_upstream_srv_conf(uscf, ngx_stream_dynamic_upstream_lua_module);
    if (ucscf == NULL) {
        goto skip;
    }

    peer_data = (ngx_stream_upstream_rr_peer_data_t*) s->upstream->peer.data;
    if (peer_data == NULL) {
        goto skip;
    }

    current = peer_data->current;
    if (current == NULL) {
        goto skip;
    }

    p = alloca(current->name.len + 1);
    ngx_memcpy(p, current->name.data, current->name.len);
    p[current->name.len] = 0;

    if (ucscf->disconnect_down && current->down) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
                      "[disconnect_if_market_down] disconnect from peer=%s upstream=%s", p, uscf->host.data);
        return NGX_ERROR;
    }

    if (ucscf->disconnect_on_exiting && ngx_exiting) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
                      "[disconnect_on_exiting] disconnect from peer=%s upstream=%s", p, uscf->host.data);
        return NGX_ERROR;
    }

    if (ucscf->disconnect_backup && ngx_stream_dynamic_upstream_alive_primary(uscf->peer.data, current)) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
                      "[disconnect_backup_if_primary_up] disconnect from peer=%s upstream=%s", p, uscf->host.data);
        return NGX_ERROR;
    }

skip:

    if (ngx_stream_next_filter) {
        return ngx_stream_next_filter(s, in, from_upstream);
    }

    return NGX_OK; 
}


ngx_int_t
ngx_stream_dynamic_upstream_lua_post_conf(ngx_conf_t *cf)
{
#ifndef NO_NGX_STREAM_LUA_MODULE
    if (ngx_stream_lua_add_package_preload(cf, "ngx.dynamic_upstream.stream",
                                           ngx_stream_dynamic_upstream_lua_create_module)
        != NGX_OK)
    {
        return NGX_ERROR;
    }
#endif

    ngx_stream_next_filter = ngx_stream_top_filter;
    ngx_stream_top_filter = ngx_stream_dynamic_upstream_write_filter;

    return NGX_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_uint_t                                  i;
    ngx_stream_upstream_srv_conf_t              **uscfp;
    ngx_stream_upstream_main_conf_t              *umcf;

    umcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_upstream_module);

    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        if (ngx_stream_dynamic_upstream_lua_init_srv_conf(cf, uscfp[i]) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf, void *conf)
{
    ngx_stream_upstream_srv_conf_t             *uscf = conf;
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;

    if (uscf->srv_conf == NULL) {
        return NGX_CONF_OK;
    }

    ucscf = ngx_stream_conf_upstream_srv_conf(uscf, ngx_stream_dynamic_upstream_lua_module);
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }

    ucscf->conf->upstream = uscf->host;
    ucscf->shm_zone = ngx_stream_create_shm_zone(cf, ucscf);

    if (ucscf->shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_stream_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf = shm_zone->data;
    ngx_int_t                                   rc = 1;

    ucscf->shm_zone = shm_zone;
    ucscf->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (data) {
        ucscf->data = data;
        return NGX_OK;
    }

    ucscf->data = ngx_slab_calloc(ucscf->shpool, sizeof(ngx_stream_upstream_check_opts_t));
    if (ucscf->data == NULL) {
        return NGX_ERROR;
    }

    ucscf->data->fall     = ucscf->conf->fall;
    ucscf->data->rise     = ucscf->conf->rise;
    ucscf->data->timeout  = ucscf->conf->timeout;
    ucscf->data->upstream = ngx_shm_copy_string(ucscf->shpool, ucscf->conf->upstream);
    ucscf->data->request_body   = ngx_shm_copy_string(ucscf->shpool, ucscf->conf->request_body);
    ucscf->data->response_body  = ngx_shm_copy_string(ucscf->shpool, ucscf->conf->response_body);


    rc = rc && (ucscf->data->upstream.data      || NULL == ucscf->conf->upstream.data);
    rc = rc && (ucscf->data->request_body.data  || NULL == ucscf->conf->request_body.data);
    rc = rc && (ucscf->data->response_body.data || NULL == ucscf->conf->response_body.data);

    if (!rc) {
        return NGX_ERROR;
    }

    shm_zone->data = ucscf->data;

    return NGX_OK;
}


const ngx_str_t
shared_zone_stream_prefix = {
    .data = (u_char *) "ngx_stream_dynamic_upstream_lua_module",
    .len = sizeof("ngx_stream_dynamic_upstream_lua_module") - 1
};


static ngx_shm_zone_t *
ngx_stream_create_shm_zone(ngx_conf_t *cf,
                           ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf)
{
    ngx_shm_zone_t *shm_zone;

    shm_zone = ngx_shared_create_zone(cf, 2048000, shared_zone_stream_prefix, ucscf->conf->upstream, &ngx_stream_dynamic_upstream_lua_module);
    if (shm_zone == NULL) {
        return NULL;
    }

    shm_zone->init = ngx_stream_init_shm_zone;
    shm_zone->noreuse = 1;
    shm_zone->data = ucscf;

    return shm_zone;
}


static void *
ngx_stream_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_dynamic_upstream_lua_srv_conf_t));
    if (ucscf == NULL) {
        return NULL;
    }

    ucscf->conf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_upstream_check_opts_t));
    if (ucscf->conf == NULL) {
        return NULL;
    }

    ucscf->conf->fall    = NGX_CONF_UNSET_UINT;
    ucscf->conf->rise    = NGX_CONF_UNSET_UINT;
    ucscf->conf->timeout = NGX_CONF_UNSET_MSEC;

    return ucscf;
}


static char *
ngx_stream_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_uint_t i;

    ucscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_dynamic_upstream_lua_module);

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; ++i)
    {
        if (ngx_strncmp(value[i].data, "timeout=", 8) == 0) {
            ucscf->conf->timeout = ngx_atoi(value[i].data + 8, value[i].len - 8);

            if (ucscf->conf->timeout == (ngx_msec_t) NGX_ERROR || ucscf->conf->timeout == 0) {
                goto invalid_check_parameter;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "rise=", 5) == 0) {
            ucscf->conf->rise = ngx_atoi(value[i].data + 5, value[i].len - 5);

            if (ucscf->conf->rise == (ngx_uint_t) NGX_ERROR || ucscf->conf->rise == 0) {
                goto invalid_check_parameter;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "fall=", 5) == 0) {
            ucscf->conf->fall = ngx_atoi(value[i].data + 5, value[i].len - 5);

            if (ucscf->conf->fall == (ngx_uint_t) NGX_ERROR || ucscf->conf->fall == 0) {
                goto invalid_check_parameter;
            }

            continue;
        }
    }

    if (ucscf->conf->fall == 0) {
        ucscf->conf->fall = 1;
    }

    if (ucscf->conf->rise == 0) {
        ucscf->conf->rise = 1;
    }

    if (ucscf->conf->timeout == 0) {
        ucscf->conf->timeout = 1000;
    }

    return NGX_CONF_OK;

invalid_check_parameter:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static char *
ngx_stream_dynamic_upstream_lua_disconnect_backup_if_primary_up(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;
    ucscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_dynamic_upstream_lua_module);
    ucscf->disconnect_backup = 1;
    return NGX_CONF_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_disconnect_if_market_down(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;
    ucscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_dynamic_upstream_lua_module);
    ucscf->disconnect_down = 1;
    return NGX_CONF_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_disconnect_on_exiting(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;
    ucscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_dynamic_upstream_lua_module);
    ucscf->disconnect_on_exiting = 1;
    return NGX_CONF_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_dynamic_upstream_lua_module);
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }

    ucscf->conf->request_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_stream_dynamic_upstream_lua_module);
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }

    ucscf->conf->response_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}
