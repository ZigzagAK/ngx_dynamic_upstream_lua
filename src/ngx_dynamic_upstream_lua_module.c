#include <ngx_core.h>
#include <ngx_http.h>
#include <lauxlib.h>
#include "ngx_http_lua_api.h"


#include "ngx_dynamic_upstream_lua.h"
#include "ngx_dynamic_upstream_stream_lua.h"


ngx_module_t ngx_http_dynamic_upstream_lua_module;


static ngx_int_t
ngx_http_dynamic_upstream_lua_post_conf(ngx_conf_t *cf);

static char *
ngx_http_dynamic_upstream_lua_init_main_conf(ngx_conf_t *cf, void *conf);


static char *
ngx_http_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_http_dynamic_upstream_lua_check_request_uri(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_http_dynamic_upstream_lua_check_request_headers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_http_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_http_dynamic_upstream_lua_check_response_codes(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *
ngx_http_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static void *
ngx_http_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf);
static char *
ngx_http_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf, void *conf);


static ngx_command_t ngx_http_dynamic_upstream_lua_commands[] = {

    { ngx_string("check"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_upstream_lua_check,
      0,
      0,
      NULL },

    { ngx_string("check_request_uri"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE2,
      ngx_http_dynamic_upstream_lua_check_request_uri,
      0,
      0,
      NULL },

    { ngx_string("check_request_headers"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_upstream_lua_check_request_headers,
      0,
      0,
      NULL },

    { ngx_string("check_request_body"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_http_dynamic_upstream_lua_check_request_body,
      0,
      0,
      NULL },

    { ngx_string("check_response_codes"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_upstream_lua_check_response_codes,
      0,
      0,
      NULL },

    { ngx_string("check_response_body"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_http_dynamic_upstream_lua_check_response_body,
      0,
      0,
      NULL },

    ngx_null_command

};


static ngx_http_module_t ngx_http_dynamic_upstream_lua_ctx = {
    NULL,                                         /* preconfiguration */
    ngx_http_dynamic_upstream_lua_post_conf,      /* postconfiguration */
    NULL,                                         /* create main configuration */
    ngx_http_dynamic_upstream_lua_init_main_conf, /* init main configuration */
    ngx_http_dynamic_upstream_lua_create_srv_conf,/* create server configuration */
    NULL,                                         /* merge server configuration */
    NULL,                                         /* create location configuration */
    NULL                                          /* merge location configuration */
};


ngx_module_t ngx_http_dynamic_upstream_lua_module = {
    NGX_MODULE_V1,
    &ngx_http_dynamic_upstream_lua_ctx,     /* module context */
    ngx_http_dynamic_upstream_lua_commands, /* module directives */
    NGX_HTTP_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    NULL,                                   /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    NULL,                                   /* exit process */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};


ngx_int_t
ngx_http_dynamic_upstream_lua_post_conf(ngx_conf_t *cf)
{
    if (ngx_http_dynamic_upstream_lua_init(cf)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_http_lua_add_package_preload(cf, "ngx.dynamic_upstream.stream",
                                         ngx_stream_dynamic_upstream_lua_create_module)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


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


static char *
ngx_http_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
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

    ucscf->initialized = 1;

    return NGX_CONF_OK;

invalid_check_parameter:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static char *
ngx_http_dynamic_upstream_lua_check_request_uri(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_str_t                                *value;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);

    value = cf->args->elts;

    ucscf->request_method = value[1];
    ucscf->request_uri = value[2];

    return NGX_CONF_OK;
}


static char *
ngx_http_dynamic_upstream_lua_check_request_headers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
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


static char *
ngx_http_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);

    ucscf->request_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}


static char *
ngx_http_dynamic_upstream_lua_check_response_codes(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
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


static char *
ngx_http_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);

    ucscf->response_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}