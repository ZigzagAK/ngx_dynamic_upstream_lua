#ifndef _ngx_dynamic_upstream_stream_lua_h_
#define _ngx_dynamic_upstream_stream_lua_h_


#include <ngx_core.h>


struct ngx_stream_dynamic_upstream_lua_srv_conf_s {
    ngx_str_t upstream;

    ngx_uint_t fall;
    ngx_uint_t rise;
    ngx_msec_t timeout;

    ngx_uint_t initialized;
};
typedef struct ngx_stream_dynamic_upstream_lua_srv_conf_s ngx_stream_dynamic_upstream_lua_srv_conf_t;


ngx_int_t
ngx_stream_dynamic_upstream_lua_init(ngx_conf_t *cf);


#endif