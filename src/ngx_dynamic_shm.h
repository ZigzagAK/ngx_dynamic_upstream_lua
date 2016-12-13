#ifndef _ngx_dynamic_shm_h_
#define _ngx_dynamic_shm_h_


#include <ngx_core.h>


ngx_shm_zone_t *
ngx_shared_memory_find(volatile ngx_cycle_t *cycle, u_char *zone_name, void *tag);


#endif