#ifndef _ngx_dynamic_upstream_lua_h_
#define _ngx_dynamic_upstream_lua_h_


#include <ngx_core.h>


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

    ngx_uint_t initialized;
};
typedef struct ngx_http_dynamic_upstream_lua_srv_conf_s ngx_http_dynamic_upstream_lua_srv_conf_t;


ngx_int_t
ngx_http_dynamic_upstream_lua_init(ngx_conf_t *cf);


#endif