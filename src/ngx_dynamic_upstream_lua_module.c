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
ngx_http_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static char *
ngx_http_dynamic_upstream_lua_check_request_uri(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_http_dynamic_upstream_lua_check_request_headers(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_http_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_http_dynamic_upstream_lua_check_response_codes(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_http_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);


static void *
ngx_http_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf);

static char *
ngx_http_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *uscf);


static ngx_command_t ngx_http_dynamic_upstream_lua_commands[] = {

    { ngx_string("check"),
      NGX_HTTP_UPS_CONF|NGX_CONF_ANY,
      ngx_http_dynamic_upstream_lua_check,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_uri"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE2,
      ngx_http_dynamic_upstream_lua_check_request_uri,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_headers"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_upstream_lua_check_request_headers,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_body"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_http_dynamic_upstream_lua_check_request_body,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_response_codes"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_upstream_lua_check_response_codes,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_response_body"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_http_dynamic_upstream_lua_check_response_body,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command

};


static ngx_http_module_t ngx_http_dynamic_upstream_lua_ctx = {
    NULL,                                          /* preconfiguration  */
    ngx_http_dynamic_upstream_lua_post_conf,       /* postconfiguration */
    NULL,                                          /* create main       */
    ngx_http_dynamic_upstream_lua_init_main_conf,  /* init main         */
    ngx_http_dynamic_upstream_lua_create_srv_conf, /* create server     */
    NULL,                                          /* merge server      */
    NULL,                                          /* create location   */
    NULL                                           /* merge location    */
};


ngx_module_t ngx_http_dynamic_upstream_lua_module = {
    NGX_MODULE_V1,
    &ngx_http_dynamic_upstream_lua_ctx,        /* module context    */
    ngx_http_dynamic_upstream_lua_commands,    /* module directives */
    NGX_HTTP_MODULE,                           /* module type       */
    NULL,                                      /* init master       */
    NULL,                                      /* init module       */
    NULL,                                      /* init process      */
    NULL,                                      /* init thread       */
    NULL,                                      /* exit thread       */
    NULL,                                      /* exit process      */
    NULL,                                      /* exit master       */
    NGX_MODULE_V1_PADDING
};


const ngx_str_t
zone_http_prefix = ngx_string("ngx_http_dynamic_upstream_lua_module");


static ngx_int_t
ngx_http_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf = shm_zone->data;
    ngx_http_upstream_check_opts_t           *sh, *cf;
    ngx_int_t                                 rc = 1;

    ucscf->shm_zone = shm_zone;
    ucscf->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (data) {
        ucscf->data = data;
        return NGX_OK;
    }

    ngx_shmtx_lock(&ucscf->shpool->mutex);

    sh = ngx_slab_calloc_locked(ucscf->shpool,
        sizeof(ngx_http_upstream_check_opts_t));

    if (sh == NULL) {
        ngx_shmtx_unlock(&ucscf->shpool->mutex);
        return NGX_ERROR;
    }

    cf = ucscf->conf;

    sh->fall     = cf->fall;
    sh->rise     = cf->rise;
    sh->timeout  = cf->timeout;
    sh->interval = cf->interval;

    sh->upstream       = ngx_shm_copy_string(ucscf->shpool, cf->upstream);
    sh->type           = ngx_shm_copy_string(ucscf->shpool, cf->type);
    sh->request_uri    = ngx_shm_copy_string(ucscf->shpool, cf->request_uri);
    sh->request_method = ngx_shm_copy_string(ucscf->shpool, cf->request_method);
    sh->request_body   = ngx_shm_copy_string(ucscf->shpool, cf->request_body);
    sh->response_body  = ngx_shm_copy_string(ucscf->shpool, cf->response_body);

    sh->request_headers_count = cf->request_headers_count;
    sh->response_codes_count  = cf->response_codes_count;

    sh->request_headers = ngx_shm_copy_pairs(ucscf->shpool,
        cf->request_headers, cf->request_headers_count);
    sh->response_codes  = ngx_shm_copy_int_vec(ucscf->shpool,
        cf->response_codes, cf->response_codes_count);

    rc = rc && (sh->upstream.data       || NULL == cf->upstream.data);
    rc = rc && (sh->type.data           || NULL == cf->type.data);
    rc = rc && (sh->request_uri.data    || NULL == cf->request_uri.data);
    rc = rc && (sh->request_method.data || NULL == cf->request_method.data);
    rc = rc && (sh->request_body.data   || NULL == cf->request_body.data);
    rc = rc && (sh->response_body.data  || NULL == cf->response_body.data);
    rc = rc && (sh->request_headers     || NULL == cf->request_headers);
    rc = rc && (sh->response_codes      || NULL == cf->response_codes);

    ngx_shmtx_unlock(&ucscf->shpool->mutex);

    if (!rc) {
        return NGX_ERROR;
    }

    ucscf->data = sh;
    shm_zone->data = sh;

    return NGX_OK;
}


static ngx_shm_zone_t *
ngx_http_create_shm_zone(ngx_conf_t *cf,
                         ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf)
{
    ngx_shm_zone_t *shm_zone;

    shm_zone = ngx_shared_create_zone(cf, 2048000, zone_http_prefix,
        ucscf->conf->upstream, &ngx_http_dynamic_upstream_lua_module);

    if (shm_zone == NULL) {
        return NULL;
    }

    shm_zone->init = ngx_http_init_shm_zone;
    shm_zone->noreuse = 1;
    shm_zone->data = ucscf;

    return shm_zone;
}


ngx_int_t
ngx_http_dynamic_upstream_lua_post_conf(ngx_conf_t *cf)
{
    if (ngx_http_dynamic_upstream_lua_init(cf) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_http_lua_add_package_preload(cf, "ngx.dynamic_upstream.stream",
        ngx_stream_dynamic_upstream_lua_create_module) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static char *
ngx_http_dynamic_upstream_lua_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_upstream_srv_conf_t     **b, **e;
    ngx_http_upstream_main_conf_t     *umcf;

    umcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);

    b = umcf->upstreams.elts;
    e = b + umcf->upstreams.nelts;

    for (; b < e; ++b) {
        if (ngx_http_dynamic_upstream_lua_init_srv_conf(cf, *b) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *uscf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    if (uscf->srv_conf == NULL) {
        return NGX_CONF_OK;
    }

    ucscf = ngx_http_conf_upstream_srv_conf(uscf,
        ngx_http_dynamic_upstream_lua_module);

    ucscf->conf->upstream = uscf->host;
    ucscf->shm_zone = ngx_http_create_shm_zone(cf, ucscf);

    if (ucscf->shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_pcalloc(cf->pool,
                        sizeof(ngx_http_dynamic_upstream_lua_srv_conf_t));
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
    ucscf->conf->interval = 10;

    return ucscf;
}


static int
ngx_is_arg(const char *n, ngx_str_t arg)
{
    return arg.len > ngx_strlen(n) &&
           ngx_strncmp(arg.data, n, ngx_strlen(n)) == 0;
}


static char *
ngx_http_dynamic_upstream_lua_check(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf = conf;
    ngx_uint_t                                i;
    ngx_str_t                                 arg, type;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; ++i)
    {
        arg = value[i];

        if (ngx_is_arg("type=", arg)) {
            type.data = arg.data + 5;
            type.len = arg.len - 5;

            if (ngx_strncmp("http", type.data, type.len) != 0 &&
                ngx_strncmp("tcp", type.data, type.len) != 0) {
                goto fail;
            }

            ucscf->conf->type = type;

            continue;
        }

        if (ngx_is_arg("timeout=", arg)) {
            ucscf->conf->timeout = ngx_atoi(arg.data + 8, arg.len - 8);

            if (ucscf->conf->timeout <= 0) {
                goto fail;
            }

            continue;
        }

        if (ngx_is_arg("rise=", arg)) {
            ucscf->conf->rise = ngx_atoi(arg.data + 5, arg.len - 5);

            if (ucscf->conf->rise <= 0) {
                goto fail;
            }

            continue;
        }

        if (ngx_is_arg("fall=", arg)) {
            ucscf->conf->fall = ngx_atoi(arg.data + 5, arg.len - 5);

            if (ucscf->conf->fall <= 0) {
                goto fail;
            }

            continue;
        }

        if (ngx_is_arg("interval=", arg)) {
            ucscf->conf->interval = ngx_atoi(arg.data + 9, arg.len - 9);

            if (ucscf->conf->interval <= 0) {
                goto fail;
            }

            continue;
        }
    }

    if (ucscf->conf->type.data == NULL) {
        ucscf->conf->type.data = (u_char *) "tcp";
        ucscf->conf->type.len = 3;
    }

    return NGX_CONF_OK;

fail:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &arg);

    return NGX_CONF_ERROR;
}


static char *
ngx_http_dynamic_upstream_lua_check_request_uri(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf = conf;
    ngx_str_t                                *value;

    value = cf->args->elts;

    ucscf->conf->request_method = value[1];
    ucscf->conf->request_uri = value[2];

    return NGX_CONF_OK;
}


static char *
ngx_http_dynamic_upstream_lua_check_request_headers(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf = conf;
    ngx_http_upstream_check_opts_t           *o;
    char                                     *sep;
    ngx_uint_t                                i;

    value = cf->args->elts;
    o = ucscf->conf;

    o->request_headers_count = cf->args->nelts - 1;
    o->request_headers = ngx_pcalloc(cf->pool,
        o->request_headers_count * sizeof(ngx_pair_t));

    if (o->request_headers == NULL) {
        return NULL;
    }

    for (i = 1; i < cf->args->nelts; ++i) {
        sep = ngx_strchr(value[i].data, '=');
        if (sep == NULL) {
            goto fail;
        }

        o->request_headers[i-1].name.len = (u_char *) sep - value[i].data;
        o->request_headers[i-1].name.data = value[i].data;

        o->request_headers[i-1].value.len =
            (value[i].data + value[i].len - (u_char *) sep) - 1;
        o->request_headers[i-1].value.data = (u_char *) sep + 1;
    }

    return NGX_CONF_OK;

fail:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid header desc \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static char *
ngx_http_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf = conf;

    ucscf->conf->request_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}


static char *
ngx_http_dynamic_upstream_lua_check_response_codes(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                *value;
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf = conf;
    ngx_http_upstream_check_opts_t           *o;
    ngx_uint_t                                i;

    value = cf->args->elts;
    o = ucscf->conf;

    o->response_codes_count = cf->args->nelts - 1;
    o->response_codes = ngx_pcalloc(cf->pool,
        o->response_codes_count * sizeof(ngx_uint_t));

    if (o->response_codes == NULL) {
        return NULL;
    }

    for (i = 1; i < cf->args->nelts; ++i)
    {
        o->response_codes[i-1] = ngx_atoi(value[i].data, value[i].len);
        if (o->response_codes[i-1] <= 0) {
            goto fail;
        }
    }

    return NGX_CONF_OK;

fail:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid response code \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static char *
ngx_http_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_dynamic_upstream_lua_srv_conf_t *ucscf = conf;

    ucscf->conf->response_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}
