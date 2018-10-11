#include <ngx_core.h>
#include <ngx_http.h>
#include <lauxlib.h>
#include "ngx_http_lua_api.h"


#include "ngx_dynamic_upstream_lua.h"


ngx_module_t ngx_http_dynamic_upstream_lua_module;


static ngx_int_t
ngx_http_dynamic_upstream_lua_post_conf(ngx_conf_t *cf);


static ngx_http_module_t ngx_http_dynamic_upstream_lua_ctx = {
    NULL,                                          /* preconfiguration  */
    ngx_http_dynamic_upstream_lua_post_conf,       /* postconfiguration */
    NULL,                                          /* create main       */
    NULL,                                          /* init main         */
    NULL,                                          /* create server     */
    NULL,                                          /* merge server      */
    NULL,                                          /* create location   */
    NULL                                           /* merge location    */
};


ngx_module_t ngx_http_dynamic_upstream_lua_module = {
    NGX_MODULE_V1,
    &ngx_http_dynamic_upstream_lua_ctx,        /* module context    */
    NULL,                                      /* module directives */
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

extern int
ngx_stream_dynamic_upstream_lua_create_module(lua_State *L);

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
