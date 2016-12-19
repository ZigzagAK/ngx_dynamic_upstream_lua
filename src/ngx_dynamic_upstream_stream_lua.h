#ifndef _ngx_dynamic_upstream_stream_lua_h_
#define _ngx_dynamic_upstream_stream_lua_h_


#include <ngx_core.h>
#include <ngx_stream.h>


struct ngx_stream_upstream_check_opts_s {
    ngx_str_t upstream;
    ngx_uint_t fall;
    ngx_uint_t rise;
    ngx_msec_t timeout;
};
typedef struct ngx_stream_upstream_check_opts_s ngx_stream_upstream_check_opts_t;


struct ngx_stream_dynamic_upstream_lua_srv_conf_s {
    ngx_shm_zone_t                   *shm_zone;
    ngx_slab_pool_t                  *shpool;
    ngx_stream_upstream_check_opts_t *data;
    ngx_stream_upstream_check_opts_t *conf;
    ngx_uint_t                        disconnect_backup;
    ngx_uint_t                        disconnect_on_exiting;
};
typedef struct ngx_stream_dynamic_upstream_lua_srv_conf_s ngx_stream_dynamic_upstream_lua_srv_conf_t;


int
ngx_stream_dynamic_upstream_lua_create_module(lua_State * L);


#endif