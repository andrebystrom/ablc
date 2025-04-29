#include "abc_pool.h"

#include <assert.h>
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

void *abc_pool_alloc(struct abc_pool *pool, size_t size, size_t count) {
    return abc_pool_alloc_aligned(pool, size, count, FALLBACK_ALIGNMENT);
}

/*
 * Get the last pool page.
 */
static struct abc_pool *find_page(struct abc_pool *pool) {
    while (pool->next != NULL) {
        pool = pool->next;
    }
    return pool;
}

static void alloc_page(struct abc_pool *pool, size_t size, size_t count) {
    size_t alloc_size = size * count > ABC_POOL_SIZE ? size * count : ABC_POOL_SIZE;
    pool->data = malloc(alloc_size);
    if (pool->data == NULL) {
        fprintf(stderr, "pool allocation failed %s\n", __FILE__);
        exit(EXIT_FAILURE);
    }
    pool->capacity = alloc_size;
    pool->offset = 0;
}

void *abc_pool_alloc_aligned(struct abc_pool *pool, size_t size, size_t count, size_t alignment) {
    assert(pool != NULL);
    pool = find_page(pool);
    // init page if not already
    if (pool->capacity == 0) {
        alloc_page(pool, size, count);
    }
    // get offset that we need
    // TODO: also take start address of data into consideration, is however malloc'd so probably fine.
    size_t start = (pool->offset + alignment) & ~(alignment - 1);
    if (start + size * count >= pool->capacity) {
        size_t offset = (size * count + alignment) & ~(alignment - 1);
        // does not fit current page, create new and put it there
        pool->next = malloc(sizeof(struct abc_pool));
        pool = pool->next;
        pool->next = NULL;
        if (pool == NULL) {
            fprintf(stderr, "pool allocation failed %s\n", __FILE__);
            exit(EXIT_FAILURE);
        }
        alloc_page(pool, size, count);
        pool->offset = offset;
        return pool->data;
    }
    pool->offset = start + size * count;
    return pool->data + start;
}

void abc_pool_destroy(struct abc_pool *pool) {
    for (struct abc_pool *p = pool; p != NULL; /* none */) {
        free(p->data);
        struct abc_pool *next = p->next;
        free(p);
        p = next;
    }
}
