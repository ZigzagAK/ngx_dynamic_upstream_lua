#ifndef _ngx_dynamic_shm_h_
#define _ngx_dynamic_shm_h_


#include <ngx_core.h>


struct ngx_pair_s {
    ngx_str_t name;
    ngx_str_t value;
};
typedef struct ngx_pair_s ngx_pair_t;


ngx_shm_zone_t *
ngx_shared_memory_find(volatile ngx_cycle_t *cycle, ngx_str_t name, void *tag);


ngx_str_t
ngx_shm_copy_string(ngx_slab_pool_t *shpool, ngx_str_t src);


ngx_pair_t *
ngx_shm_copy_pairs(ngx_slab_pool_t *shpool, ngx_pair_t *src, ngx_uint_t count);


ngx_uint_t *
ngx_shm_copy_uint_vec(ngx_slab_pool_t *shpool, ngx_uint_t *src, ngx_uint_t count);


void
ngx_safe_slab_free(ngx_slab_pool_t *pool, void **p);


#endif