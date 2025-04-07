#include "abc_pool.h"

#include <stdio.h>

struct abc_pool *abc_pool_create(void) {
    struct abc_pool *pool = malloc(sizeof(struct abc_pool));
    if (pool == NULL) {
        fprintf(stderr, "pool allocation failed %s\n", __FILE__);
        exit(EXIT_FAILURE);
    }
    pool->next = NULL;
    pool->data = NULL;
    pool->offset = 0;
    pool->capacity = 0;
    return pool;
}

void *abc_pool_alloc(struct abc_pool *pool, size_t size) {
    return abc_pool_alloc_aligned(pool, size, FALLBACK_ALIGNMENT);
}

/*
 * Get the last pool page.
 */
static struct abc_pool *find_page(struct abc_pool *pool) {
    for (; pool != NULL; pool = pool->next) {
        if (pool->next == NULL) {
            break;
        }
    }
    return pool;
}

void *abc_pool_alloc_aligned(struct abc_pool *pool, size_t size, size_t alignment) {
    pool = find_page(pool);
    return NULL;
}

void abc_pool_destroy(struct abc_pool *pool) {
    for (struct abc_pool *p = pool; p != NULL; /* none */) {
        free(p->data);
        struct abc_pool *next = p->next;
        free(p);
        p = next;
    }
}
