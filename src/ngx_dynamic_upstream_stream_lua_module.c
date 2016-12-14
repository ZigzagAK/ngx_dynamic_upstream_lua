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


static ngx_shm_zone_t *
ngx_stream_create_shm_zone(ngx_conf_t *cf, ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf);


static ngx_command_t ngx_stream_dynamic_upstream_lua_commands[] = {

    { ngx_string("check"),
      NGX_STREAM_UPS_CONF|NGX_CONF_1MORE,
      ngx_stream_dynamic_upstream_lua_check,
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

    rc = rc && (ucscf->data->upstream.data || NULL == ucscf->conf->upstream.data);

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