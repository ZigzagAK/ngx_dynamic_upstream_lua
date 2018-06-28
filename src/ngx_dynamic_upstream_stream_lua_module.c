#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_stream.h>
#include <lauxlib.h>


#ifndef NO_NGX_STREAM_LUA_MODULE
#include "ngx_stream_lua_request.h"
#include "ngx_stream_lua_api.h"
#endif

#include "ngx_dynamic_shm.h"
#include "ngx_dynamic_upstream_stream_lua.h"


ngx_module_t ngx_stream_dynamic_upstream_lua_module;


static char *
ngx_stream_dynamic_upstream_lua_init_main_conf(ngx_conf_t *cf, void *conf);

static ngx_int_t
ngx_stream_dynamic_upstream_lua_post_conf(ngx_conf_t *cf);

static void *
ngx_stream_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf);

static char *
ngx_stream_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf,
    ngx_stream_upstream_srv_conf_t *uscf);

static char *
ngx_stream_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static char *
ngx_stream_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_stream_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_stream_dynamic_upstream_lua_disconnect_backup_if_primary_up(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_stream_dynamic_upstream_lua_disconnect_if_market_down(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_stream_dynamic_upstream_lua_disconnect_on_exiting(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);


static ngx_int_t ngx_stream_dynamic_upstream_write_filter(ngx_stream_session_t *s,
    ngx_chain_t *in, ngx_uint_t from_upstream);


static ngx_command_t ngx_stream_dynamic_upstream_lua_commands[] = {

    { ngx_string("check"),
      NGX_STREAM_UPS_CONF|NGX_CONF_ANY,
      ngx_stream_dynamic_upstream_lua_check,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_body"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE1,
      ngx_stream_dynamic_upstream_lua_check_request_body,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_response_body"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE1,
      ngx_stream_dynamic_upstream_lua_check_response_body,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

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
    ngx_stream_dynamic_upstream_lua_init_main_conf,  /* init main         */
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


const ngx_str_t
zone_stream_prefix = ngx_string("ngx_stream_dynamic_upstream_lua_module");


static ngx_int_t
ngx_stream_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf = shm_zone->data;
    ngx_stream_upstream_check_opts_t           *sh, *cf;
    ngx_int_t                                   rc = 1;

    ucscf->shm_zone = shm_zone;
    ucscf->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (data) {
        ucscf->data = data;
        return NGX_OK;
    }

    ngx_shmtx_lock(&ucscf->shpool->mutex);

    sh = ngx_slab_calloc_locked(ucscf->shpool,
        sizeof(ngx_stream_upstream_check_opts_t));

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
    sh->request_body   = ngx_shm_copy_string(ucscf->shpool, cf->request_body);
    sh->response_body  = ngx_shm_copy_string(ucscf->shpool, cf->response_body);

    rc = rc && (sh->upstream.data      || NULL == cf->upstream.data);
    rc = rc && (sh->request_body.data  || NULL == cf->request_body.data);
    rc = rc && (sh->response_body.data || NULL == cf->response_body.data);

    ngx_shmtx_unlock(&ucscf->shpool->mutex);

    if (!rc) {
        return NGX_ERROR;
    }

    ucscf->data = sh;
    shm_zone->data = sh;

    return NGX_OK;
}


static ngx_shm_zone_t *
ngx_stream_create_shm_zone(ngx_conf_t *cf,
                           ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf)
{
    ngx_shm_zone_t *shm_zone;

    shm_zone = ngx_shared_create_zone(cf, 2048000, zone_stream_prefix,
        ucscf->conf->upstream, &ngx_stream_dynamic_upstream_lua_module);

    if (shm_zone == NULL) {
        return NULL;
    }

    shm_zone->init = ngx_stream_init_shm_zone;
    shm_zone->noreuse = 1;
    shm_zone->data = ucscf;

    return shm_zone;
}


static ngx_uint_t
ngx_stream_dynamic_upstream_alive_primary(ngx_stream_upstream_rr_peers_t *peers,
    ngx_stream_upstream_rr_peer_t *current)
{
    ngx_stream_upstream_rr_peer_t  *peer;
    int                             alive = 0;

    for (peer = peers->peer; peer; peer = peer->next) {
        if (current == peer) {
            return 0;
        }
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


static ngx_int_t
ngx_stream_dynamic_upstream_write_filter(ngx_stream_session_t *s,
    ngx_chain_t *in, ngx_uint_t from_upstream)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;
    ngx_stream_upstream_srv_conf_t             *uscf;
    ngx_stream_upstream_rr_peer_t              *curr;
    ngx_stream_upstream_rr_peers_t             *peers = NULL;

    if (!ngx_have_upstream(s)) {
        goto skip;
    }

    uscf = s->upstream->upstream;
    peers = uscf->peer.data;

    ngx_http_upstream_rr_peers_rlock(peers);

    curr = ngx_stream_get_module_ctx(s, ngx_stream_dynamic_upstream_lua_module);

    if (curr == NULL) {
        curr = ngx_stream_dynamic_upstream_get_peer(peers,
                                                    s->upstream->state->peer);
        if (curr) {
            ngx_stream_set_ctx(s, curr, ngx_stream_dynamic_upstream_lua_module);
        } else {
            goto skip;
        }
    }

    ucscf = ngx_stream_conf_upstream_srv_conf(uscf,
        ngx_stream_dynamic_upstream_lua_module);

    if (ucscf->disconnect_down && curr->down) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
            "[disconnect_if_market_down] disconnect from peer=%V upstream=%V",
            &curr->name, &uscf->host);
        ngx_http_upstream_rr_peers_unlock(peers);
        return NGX_ERROR;
    }

    if (ucscf->disconnect_on_exiting && ngx_exiting) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
            "[disconnect_on_exiting] disconnect from peer=%V upstream=%V",
            &curr->name, &uscf->host);
        ngx_http_upstream_rr_peers_unlock(peers);
        return NGX_ERROR;
    }

    if (ucscf->disconnect_backup &&
        ngx_stream_dynamic_upstream_alive_primary(peers, curr)) {
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
            "[disconnect_backup_if_primary_up] disconnect from peer=%V "
            "upstream=%V", &curr->name, &uscf->host);
        ngx_http_upstream_rr_peers_unlock(peers);
        return NGX_ERROR;
    }

skip:

    if (peers) {
        ngx_http_upstream_rr_peers_unlock(peers);
    }

    if (ngx_stream_next_filter) {
        return ngx_stream_next_filter(s, in, from_upstream);
    }

    return NGX_OK; 
}


ngx_int_t
ngx_stream_dynamic_upstream_lua_post_conf(ngx_conf_t *cf)
{
#ifndef NO_NGX_STREAM_LUA_MODULE
    if (ngx_stream_lua_add_package_preload(cf, "ngx.dynamic_upstream.stream",
        ngx_stream_dynamic_upstream_lua_create_module) != NGX_OK) {
        return NGX_ERROR;
    }
#endif

    ngx_stream_next_filter = ngx_stream_top_filter;
    ngx_stream_top_filter = ngx_stream_dynamic_upstream_write_filter;

    return NGX_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_stream_upstream_srv_conf_t     **b, **e;
    ngx_stream_upstream_main_conf_t     *umcf;

    umcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_upstream_module);

    b = umcf->upstreams.elts;
    e = b + umcf->upstreams.nelts;

    for (; b < e; ++b) {
        if (ngx_stream_dynamic_upstream_lua_init_srv_conf(cf, *b) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    ngx_log_error(NGX_LOG_NOTICE, cf->log, 0,
                  "stream dynamic upstream lua module loaded");

    return NGX_CONF_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_init_srv_conf(ngx_conf_t *cf,
    ngx_stream_upstream_srv_conf_t *uscf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;

    if (uscf->srv_conf == NULL) {
        return NGX_CONF_OK;
    }

    ucscf = ngx_stream_conf_upstream_srv_conf(uscf,
        ngx_stream_dynamic_upstream_lua_module);

    ucscf->conf->upstream = uscf->host;
    ucscf->shm_zone = ngx_stream_create_shm_zone(cf, ucscf);

    if (ucscf->shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static void *
ngx_stream_dynamic_upstream_lua_create_srv_conf(ngx_conf_t *cf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf;

    ucscf = ngx_pcalloc(cf->pool,
                        sizeof(ngx_stream_dynamic_upstream_lua_srv_conf_t));
    if (ucscf == NULL) {
        return NULL;
    }

    ucscf->conf = ngx_pcalloc(cf->pool,
                              sizeof(ngx_stream_upstream_check_opts_t));
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
ngx_stream_dynamic_upstream_lua_check(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_str_t                                  *value;
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf = conf;
    ngx_uint_t                                  i;
    ngx_str_t                                   arg;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; ++i)
    {
        arg = value[i];

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

    return NGX_CONF_OK;

fail:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &arg);

    return NGX_CONF_ERROR;
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


static char *
ngx_stream_dynamic_upstream_lua_check_request_body(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf = conf;

    ucscf->conf->request_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}


static char *
ngx_stream_dynamic_upstream_lua_check_response_body(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_stream_dynamic_upstream_lua_srv_conf_t *ucscf = conf;

    ucscf->conf->response_body = ((ngx_str_t *) cf->args->elts) [1];

    return NGX_CONF_OK;
}
