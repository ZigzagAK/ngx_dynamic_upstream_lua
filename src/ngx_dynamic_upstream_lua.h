#ifndef _ngx_dynamic_upstream_lua_h_
#define _ngx_dynamic_upstream_lua_h_


#include <ngx_core.h>


#include "ngx_dynamic_shm.h"


struct ngx_http_upstream_check_opts_s {
    ngx_str_t        upstream;
    ngx_str_t        type;
    ngx_int_t        fall;
    ngx_int_t        rise;
    ngx_int_t        timeout;
    ngx_int_t        interval;
    ngx_str_t        request_uri;
    ngx_str_t        request_method;
    ngx_pair_t      *request_headers;
    ngx_uint_t       request_headers_count;
    ngx_str_t        request_body;
    ngx_int_t       *response_codes;
    ngx_uint_t       response_codes_count;
    ngx_str_t        response_body;
};
typedef struct ngx_http_upstream_check_opts_s ngx_http_upstream_check_opts_t;

struct ngx_http_dynamic_upstream_lua_srv_conf_s {
    ngx_shm_zone_t                 *shm_zone;
    ngx_slab_pool_t                *shpool;
    ngx_http_upstream_check_opts_t *data;
    ngx_http_upstream_check_opts_t *conf;
};
typedef struct ngx_http_dynamic_upstream_lua_srv_conf_s ngx_http_dynamic_upstream_lua_srv_conf_t;

ngx_int_t
ngx_http_dynamic_upstream_lua_init(ngx_conf_t *cf);


#endif