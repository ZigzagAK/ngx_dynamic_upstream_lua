#include <ngx_core.h>
#include <ngx_http.h>
#include <lauxlib.h>
#include "ngx_http_lua_api.h"


#include "ngx_dynamic_shm.h"
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


static ngx_shm_zone_t *
ngx_http_create_shm_zone(ngx_conf_t *cf, ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf);


static ngx_command_t ngx_http_dynamic_upstream_lua_commands[] = {

    { ngx_string("check"),
      NGX_HTTP_UPS_CONF|NGX_CONF_ANY,
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
    &ngx_http_dynamic_upstream_lua_ctx,        /* module context */
    ngx_http_dynamic_upstream_lua_commands,    /* module directives */
    NGX_HTTP_MODULE,                           /* module type */
    NULL,                                      /* init master */
    NULL,                                      /* init module */
    NULL,                                      /* init process */
    NULL,                                      /* init thread */
    NULL,                                      /* exit thread */
    NULL,                                      /* exit process */
    NULL,                                      /* exit master */
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


static char *
ngx_http_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_upstream_srv_conf_t             *uscf = conf;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    if (uscf->srv_conf == NULL) {
        return NGX_CONF_OK;
    }

    ucscf = ngx_http_conf_upstream_srv_conf(uscf, ngx_http_dynamic_upstream_lua_module);
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }

    ucscf->conf->upstream = uscf->host;
    ucscf->shm_zone = ngx_http_create_shm_zone(cf, ucscf);

    if (ucscf->shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf = shm_zone->data;
    ngx_int_t                                 rc = 1;

    ucscf->shm_zone = shm_zone;
    ucscf->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (data) {
        ucscf->data = data;
        return NGX_OK;
    }

    ucscf->data = ngx_slab_calloc(ucscf->shpool, sizeof(ngx_http_upstream_check_opts_t));
    if (ucscf->data == NULL) {
        return NGX_ERROR;
    }

    ucscf->data->fall     = ucscf->conf->fall;
    ucscf->data->rise     = ucscf->conf->rise;
    ucscf->data->timeout  = ucscf->conf->timeout;
    ucscf->data->interval = ucscf->conf->interval;

    ucscf->data->upstream       = ngx_shm_copy_string(ucscf->shpool, ucscf->conf->upstream);
    ucscf->data->type           = ngx_shm_copy_string(ucscf->shpool, ucscf->conf->type);
    ucscf->data->request_uri    = ngx_shm_copy_string(ucscf->shpool, ucscf->conf->request_uri);
    ucscf->data->request_method = ngx_shm_copy_string(ucscf->shpool, ucscf->conf->request_method);
    ucscf->data->request_body   = ngx_shm_copy_string(ucscf->shpool, ucscf->conf->request_body);
    ucscf->data->response_body  = ngx_shm_copy_string(ucscf->shpool, ucscf->conf->response_body);

    ucscf->data->request_headers_count = ucscf->conf->request_headers_count;
    ucscf->data->response_codes_count  = ucscf->conf->response_codes_count;

    ucscf->data->request_headers = ngx_shm_copy_pairs(ucscf->shpool, ucscf->conf->request_headers, ucscf->conf->request_headers_count);
    ucscf->data->response_codes = ngx_shm_copy_uint_vec(ucscf->shpool, ucscf->conf->response_codes, ucscf->conf->response_codes_count);

    rc = rc && (ucscf->data->upstream.data        || NULL == ucscf->conf->upstream.data);
    rc = rc && (ucscf->data->type.data            || NULL == ucscf->conf->type.data);
    rc = rc && (ucscf->data->request_uri.data     || NULL == ucscf->conf->request_uri.data);
    rc = rc && (ucscf->data->request_method.data  || NULL == ucscf->conf->request_method.data);
    rc = rc && (ucscf->data->request_body.data    || NULL == ucscf->conf->request_body.data);
    rc = rc && (ucscf->data->response_body.data   || NULL == ucscf->conf->response_body.data);

    rc = rc && (ucscf->data->request_headers || NULL == ucscf->conf->request_headers);
    rc = rc && (ucscf->data->response_codes  || NULL == ucscf->conf->response_codes);

    if (!rc) {
        return NGX_ERROR;
    }

    shm_zone->data = ucscf->data;

    return NGX_OK;
}


const ngx_str_t
shared_zone_http_prefix = {
    .data = (u_char *) "ngx_http_dynamic_upstream_lua_module",
    .len = sizeof("ngx_http_dynamic_upstream_lua_module") - 1
};


static ngx_shm_zone_t *
ngx_http_create_shm_zone(ngx_conf_t *cf,
                         ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf)
{
    ngx_shm_zone_t *shm_zone;

    shm_zone = ngx_shared_create_zone(cf, 2048000, shared_zone_http_prefix, ucscf->conf->upstream, &ngx_http_dynamic_upstream_lua_module);
    if (shm_zone == NULL) {
        return NULL;
    }

    shm_zone->init = ngx_http_init_shm_zone;
    shm_zone->noreuse = 1;
    shm_zone->data = ucscf;

    return shm_zone;
}


static void *
ngx_http_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_dynamic_upstream_lua_srv_conf_t));
    if (ucscf == NULL) {
        return NULL;
    }

    ucscf->conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_check_opts_t));
    if (ucscf->conf == NULL) {
        return NULL;
    }

    ucscf->conf->fall     = 1;
    ucscf->conf->rise     = 1;
    ucscf->conf->timeout  = 1000;
    ucscf->conf->interval = NGX_CONF_UNSET_UINT;

    return ucscf;
}


static char *
ngx_http_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_uint_t i;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; ++i)
    {
        if (ngx_strncmp(value[i].data, "type=", 5) == 0) {
            ucscf->conf->type.data = value[i].data + 5;
            ucscf->conf->type.len = value[i].len - 5;

            if (ngx_strncmp(ucscf->conf->type.data, "http", 4) != 0 && ngx_strncmp(ucscf->conf->type.data, "tcp", 3) != 0) {
                goto invalid_check_parameter;
            }

            continue;
        }

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

        if (ngx_strncmp(value[i].data, "interval=", 9) == 0) {
            ucscf->conf->interval = ngx_atoi(value[i].data + 9, value[i].len - 9);

            if (ucscf->conf->interval == (ngx_uint_t) NGX_ERROR || ucscf->conf->interval == 0) {
                goto invalid_check_parameter;
            }

            continue;
        }
    }

    if (ucscf->conf->type.data == NULL) {
        ucscf->conf->type.data = (u_char *) "tcp";
        ucscf->conf->type.len = 3;
    }

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
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ucscf->conf->request_method = value[1];
    ucscf->conf->request_uri = value[2];

    return NGX_CONF_OK;
}


static char *
ngx_http_dynamic_upstream_lua_check_request_headers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;
    char                                     *sep;
    ngx_uint_t                                i;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ucscf->conf->request_headers_count = cf->args->nelts - 1;
    ucscf->conf->request_headers = ngx_pcalloc(cf->pool, ucscf->conf->request_headers_count * sizeof(ngx_pair_t));

    if (ucscf->conf->request_headers == NULL)
    {
        return NULL;
    }

    for (i = 1; i < cf->args->nelts; ++i)
    {
        sep = ngx_strchr(value[i].data, '=');
        if (sep == NULL)
        {
            goto invalid_check_parameter;
        }

        ucscf->conf->request_headers[i-1].name.len = (u_char *) sep - value[i].data;
        ucscf->conf->request_headers[i-1].name.data = value[i].data;

        ucscf->conf->request_headers[i-1].value.len = (ngx_uint_t) ((value[i].data + value[i].len - (u_char *) sep) - 1);
        ucscf->conf->request_headers[i-1].value.data = (u_char *) sep + 1;
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
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }

    ucscf->conf->request_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}


static char *
ngx_http_dynamic_upstream_lua_check_response_codes(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_uint_t                                i;

    ucscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_dynamic_upstream_lua_module);
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }
    
    value = cf->args->elts;

    ucscf->conf->response_codes_count = cf->args->nelts - 1;
    ucscf->conf->response_codes = ngx_pcalloc(cf->pool, ucscf->conf->response_codes_count * sizeof(ngx_uint_t));

    if (ucscf->conf->response_codes == NULL) {
        return NULL;
    }

    for (i = 1; i < cf->args->nelts; ++i)
    {
        ucscf->conf->response_codes[i-1] = ngx_atoi(value[i].data, value[i].len);
        if (ucscf->conf->response_codes[i-1] == (ngx_uint_t) NGX_ERROR || ucscf->conf->response_codes[i-1] == 0) {
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
    if (ucscf == NULL) {
        return NGX_CONF_ERROR;
    }
   
    ucscf->conf->response_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}