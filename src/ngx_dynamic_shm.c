#include "ngx_dynamic_shm.h"


ngx_shm_zone_t *
ngx_shared_create_zone(ngx_conf_t *cf, ngx_uint_t size, ngx_str_t prefix, ngx_str_t suffix, void *tag)
{
    ngx_shm_zone_t  *shm_zone;
    ngx_str_t        shm_zone_name;

    shm_zone_name.len = prefix.len + 1 + suffix.len;
    shm_zone_name.data = ngx_pcalloc(cf->pool, shm_zone_name.len + 1);
    ngx_snprintf(shm_zone_name.data, shm_zone_name.len + 1, "%s:%s\0", prefix.data, suffix.data);

    shm_zone = ngx_shared_memory_add(cf, &shm_zone_name, size, tag);

    if (shm_zone == NULL) {
        return NULL;
    }

    return shm_zone;
}


ngx_shm_zone_t *
ngx_shared_memory_find(volatile ngx_cycle_t *cycle, ngx_str_t prefix, ngx_str_t suffix, void *tag)
{
    ngx_uint_t        i;
    ngx_shm_zone_t   *shm_zone;
    ngx_list_part_t  *part;
    ngx_str_t         shm_zone_name;

    shm_zone_name.len = prefix.len + 1 + suffix.len;
    shm_zone_name.data = alloca(shm_zone_name.len + 1);
    bzero(shm_zone_name.data, shm_zone_name.len + 1);
    
    ngx_snprintf(shm_zone_name.data, shm_zone_name.len + 1, "%s:%s\0", prefix.data, suffix.data);

    part = (ngx_list_part_t *) &(cycle->shared_memory.part);
    shm_zone = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone = part->elts;
            i = 0;
        }

        if (shm_zone_name.len != shm_zone[i].shm.name.len) {
            continue;
        }

        if (ngx_strncmp(shm_zone_name.data, shm_zone[i].shm.name.data, shm_zone_name.len) != 0)
        {
            continue;
        }

        if (tag != shm_zone[i].tag) {
            continue;
        }

        return &shm_zone[i];
    }

    return NULL;
}


ngx_str_t
ngx_shm_copy_string(ngx_slab_pool_t *shpool, ngx_str_t src)
{
    ngx_str_t s = { .data = 0, .len = 0 };
    if (src.len) {
        s.data = ngx_slab_calloc_locked(shpool, src.len);
        if (s.data != NULL) {
            ngx_memcpy(s.data, src.data, src.len);
            s.len = src.len;
        }
    }
    return s;
}


ngx_pair_t *
ngx_shm_copy_pairs(ngx_slab_pool_t *shpool, ngx_pair_t *src, ngx_uint_t count)
{
    ngx_pair_t *pairs = NULL;
    ngx_uint_t    i;
    if (count) {
        pairs = ngx_slab_calloc(shpool, count * sizeof(ngx_pair_t));
        if (pairs != NULL) {
            for (i = 0; i < count; ++i) {
                pairs[i].name = ngx_shm_copy_string(shpool, src[i].name);
                pairs[i].value = ngx_shm_copy_string(shpool, src[i].value);
                if (pairs[i].name.data == NULL || pairs[i].value.data == NULL) {
                    return NULL;
                }
            }
        }
    }
    return pairs;
}


ngx_uint_t *
ngx_shm_copy_uint_vec(ngx_slab_pool_t *shpool, ngx_uint_t *src, ngx_uint_t count)
{
    ngx_uint_t *codes = NULL;
    if (count) {
        codes = ngx_slab_calloc(shpool, count * sizeof(ngx_uint_t));
        if (codes != NULL) {
            ngx_memcpy(codes, src, count * sizeof(ngx_uint_t));
        }
    }
    return codes;
}


void
ngx_safe_slab_free(ngx_slab_pool_t *pool, void **p)
{
    if (*p) {
        ngx_slab_free_locked(pool, *p);
        *p = NULL;
    }
}
