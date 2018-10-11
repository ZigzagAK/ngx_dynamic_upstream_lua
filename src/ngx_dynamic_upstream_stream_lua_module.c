#include <ngx_core.h>
#include <ngx_stream.h>
#include "ngx_stream_lua_api.h"


ngx_module_t ngx_stream_dynamic_upstream_lua_module;


static ngx_int_t
ngx_stream_dynamic_upstream_lua_post_conf(ngx_conf_t *cf);

static void *
ngx_stream_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf);


static ngx_int_t ngx_stream_dynamic_upstream_write_filter
    (ngx_stream_session_t *s, ngx_chain_t *in, ngx_uint_t from_upstream);


typedef struct {
    ngx_flag_t  disconnect_backup;
    ngx_flag_t  disconnect_down;
    ngx_flag_t  disconnect_on_exiting;
} ngx_stream_dynamic_upstream_lua_srv_conf_t;


static char *
ngx_stream_dynamic_upstream_lua_disconnect_backup_if_primary_up(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_stream_dynamic_upstream_lua_disconnect_if_market_down(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_stream_dynamic_upstream_lua_disconnect_on_exiting(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);


static ngx_command_t ngx_stream_dynamic_upstream_lua_commands[] = {

    { ngx_string("disconnect_backup_if_primary_up"),
      NGX_STREAM_UPS_CONF|NGX_CONF_NOARGS,
      ngx_stream_dynamic_upstream_lua_disconnect_backup_if_primary_up,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("disconnect_if_market_down"),
      NGX_STREAM_UPS_CONF|NGX_CONF_NOARGS,
      ngx_stream_dynamic_upstream_lua_disconnect_if_market_down,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("disconnect_on_exiting"),
      NGX_STREAM_UPS_CONF|NGX_CONF_NOARGS,
      ngx_stream_dynamic_upstream_lua_disconnect_on_exiting,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command

};


static ngx_stream_module_t ngx_stream_dynamic_upstream_lua_ctx = {
    NULL,                                            /* preconfiguration  */
    ngx_stream_dynamic_upstream_lua_post_conf,       /* postconfiguration */
    NULL,                                            /* create main       */
    NULL,                                            /* init main         */
    ngx_stream_dynamic_upstream_lua_create_srv_conf, /* create server     */
    NULL                                             /* merge server      */
};


ngx_module_t ngx_stream_dynamic_upstream_lua_module = {
    NGX_MODULE_V1,
    &ngx_stream_dynamic_upstream_lua_ctx,     /* module context    */
    ngx_stream_dynamic_upstream_lua_commands, /* module directives */
    NGX_STREAM_MODULE,                        /* module type       */
    NULL,                                     /* init master       */
    NULL,                                     /* init module       */
    NULL,                                     /* init process      */
    NULL,                                     /* init thread       */
    NULL,                                     /* exit thread       */
    NULL,                                     /* exit process      */
    NULL,                                     /* exit master       */
    NGX_MODULE_V1_PADDING
};


static ngx_stream_filter_pt
ngx_stream_next_filter;


static ngx_uint_t
ngx_stream_dynamic_upstream_alive_primary(ngx_stream_upstream_rr_peers_t *peers,
    ngx_stream_upstream_rr_peer_t *current)
{
    ngx_stream_upstream_rr_peer_t  *peer;
    int                             alive = 0;

    for (peer = peers->peer; peer; peer = peer->next) {

        if (current == peer)
            return 0;

        alive = alive + (peer->down == 0 ? 1 : 0);
    }

    return alive;
}


static ngx_stream_upstream_rr_peer_t *
ngx_stream_dynamic_upstream_get_peer(ngx_stream_upstream_rr_peers_t *primary,
    ngx_str_t *name)
{
    ngx_stream_upstream_rr_peer_t   *peer;
    ngx_stream_upstream_rr_peers_t  *peers;

    for (peers = primary; peers; peers = peers->next) {

        for (peer = peers->peer; peer; peer = peer->next) {

            if (peer->name.len == name->len &&
                ngx_strncmp(peer->name.data, name->data, name->len) == 0) {
                return peer;
            }
        }
    }

    return NULL;
}


static ngx_int_t
ngx_have_upstream(ngx_stream_session_t *s)
{
    return s->upstream &&
           s->upstream->upstream &&
           s->upstream->upstream->srv_conf &&
           s->upstream->state &&
           s->upstream->state->peer;
}


typedef struct {
    ngx_stream_upstream_rr_peer_t *peer;
    ngx_msec_t                     check_ms;
} context_t;


static ngx_int_t
ngx_stream_dynamic_upstream_write_filter(ngx_stream_session_t *s,
    ngx_chain_t *in, ngx_uint_t from_upstream)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t  *ucscf;
    ngx_stream_upstream_srv_conf_t              *uscf;
    ngx_stream_upstream_rr_peers_t              *peers = NULL;
    context_t                                   *ctx;

    if (!ngx_have_upstream(s))
        goto skip;

    uscf = s->upstream->upstream;
    peers = uscf->peer.data;

    ngx_stream_upstream_rr_peers_rlock(peers);

    ctx = ngx_stream_get_module_ctx(s, ngx_stream_dynamic_upstream_lua_module);

    if (ctx == NULL) {
        ctx = ngx_pcalloc(s->connection->pool, sizeof(context_t));
        if (ctx == NULL)
            goto skip;

        ngx_stream_set_ctx(s, ctx, ngx_stream_dynamic_upstream_lua_module);

        ctx->peer = ngx_stream_dynamic_upstream_get_peer(peers,
            s->upstream->state->peer);

        if (ctx->peer == NULL)
            goto skip;
    }

    if (ctx->peer == NULL)
        goto skip;

    if (ngx_current_msec - ctx->check_ms < 1000)
        goto skip;

    ucscf = ngx_stream_conf_upstream_srv_conf(uscf,
        ngx_stream_dynamic_upstream_lua_module);

    if (ucscf->disconnect_down && ctx->peer->down) {

        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
            "[disconnect_if_market_down] peer=%V upstream=%V",
            &ctx->peer->name, &uscf->host);

        ngx_stream_upstream_rr_peers_unlock(peers);
        return NGX_ERROR;
    }

    if (ucscf->disconnect_on_exiting
        && (ngx_exiting || ngx_quit || ngx_terminate)) {

        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
            "[disconnect_on_exiting] peer=%V upstream=%V",
            &ctx->peer->name, &uscf->host);

        ngx_stream_upstream_rr_peers_unlock(peers);
        return NGX_ERROR;
    }

    if (ucscf->disconnect_backup &&
        ngx_stream_dynamic_upstream_alive_primary(peers, ctx->peer)) {

        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
            "[disconnect_backup_if_primary_up] peer=%V "
            "upstream=%V", &ctx->peer->name, &uscf->host);

        ngx_stream_upstream_rr_peers_unlock(peers);
        return NGX_ERROR;
    }

    ctx->check_ms = ngx_current_msec;

skip:

    if (peers)
        ngx_stream_upstream_rr_peers_unlock(peers);

    if (ngx_stream_next_filter)
        return ngx_stream_next_filter(s, in, from_upstream);

    return NGX_OK; 
}


extern int
ngx_stream_dynamic_upstream_lua_create_module(lua_State *L);


ngx_int_t
ngx_stream_dynamic_upstream_lua_post_conf(ngx_conf_t *cf)
{
#ifndef NO_NGX_STREAM_LUA_MODULE
    if (ngx_stream_lua_add_package_preload(cf, "ngx.dynamic_upstream.stream",
        ngx_stream_dynamic_upstream_lua_create_module) != NGX_OK)
        return NGX_ERROR;
#endif

    ngx_stream_next_filter = ngx_stream_top_filter;
    ngx_stream_top_filter = ngx_stream_dynamic_upstream_write_filter;

    return NGX_OK;
}


static void *
ngx_stream_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t  *ucscf;

    ucscf = ngx_palloc(cf->pool,
                       sizeof(ngx_stream_dynamic_upstream_lua_srv_conf_t));
    if (ucscf == NULL) {
        return NULL;
    }

    ucscf->disconnect_backup = NGX_CONF_UNSET;
    ucscf->disconnect_down = NGX_CONF_UNSET;
    ucscf->disconnect_on_exiting = NGX_CONF_UNSET;

    return ucscf;
}


static char *
ngx_stream_dynamic_upstream_lua_disconnect_backup_if_primary_up(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf = conf;

    ucscf->disconnect_backup = 1;

    return NGX_CONF_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_disconnect_if_market_down(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf = conf;

    ucscf->disconnect_down = 1;

    return NGX_CONF_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_disconnect_on_exiting(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf = conf;

    ucscf->disconnect_on_exiting = 1;

    return NGX_CONF_OK;
}
