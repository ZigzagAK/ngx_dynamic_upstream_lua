#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_stream.h>
#include <lauxlib.h>


#include "ngx_dynamic_upstream_stream_lua.h"


ngx_module_t ngx_stream_dynamic_upstream_lua_module;


static char *
ngx_stream_dynamic_upstream_lua_init_main_conf(ngx_conf_t *cf, void *conf);


static void *
ngx_stream_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf);
static char *
ngx_stream_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf, void *conf);


static char *
ngx_stream_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


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
    NULL,                                           /* postconfiguration */
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


static void *
ngx_stream_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_dynamic_upstream_lua_srv_conf_t));
    if (ucscf == NULL) {
        return NULL;
    }

    bzero(ucscf, sizeof(ngx_stream_dynamic_upstream_lua_srv_conf_t));

    ucscf->fall    = NGX_CONF_UNSET_UINT;
    ucscf->rise    = NGX_CONF_UNSET_UINT;
    ucscf->timeout = NGX_CONF_UNSET_MSEC;

    return ucscf;
}


static char *
ngx_stream_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf, void *conf)
{
    ngx_stream_upstream_srv_conf_t             *us = conf;
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;

    if (us->srv_conf == NULL) {
        return NGX_CONF_OK;
    }

    ucscf = ngx_stream_conf_upstream_srv_conf(us, ngx_stream_dynamic_upstream_lua_module);

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

    ucscf->initialized = 1;

    return NGX_CONF_OK;

invalid_check_parameter:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}