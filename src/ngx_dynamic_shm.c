#include "ngx_dynamic_shm.h"


ngx_shm_zone_t *
ngx_shared_memory_find(volatile ngx_cycle_t *cycle, u_char *zone_name, void *tag)
{
    ngx_uint_t        i;
    ngx_shm_zone_t   *shm_zone;
    ngx_list_part_t  *part;
    ngx_str_t         name = { .data = zone_name, .len = ngx_strlen(zone_name) };

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

        if (name.len != shm_zone[i].shm.name.len) {
            continue;
        }

        if (ngx_strncmp(name.data, shm_zone[i].shm.name.data, name.len) != 0)
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