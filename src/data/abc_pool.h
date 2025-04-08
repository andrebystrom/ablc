/**
 * basic memory pool/arena allocator.
 */

#ifndef ABC_POOL_H
#define ABC_POOL_H

#include <stdlib.h>

#define ABC_POOL_SIZE 4096 // default pool 'page' size
#define FALLBACK_ALIGNMENT 16

struct abc_pool {
  char *data;
  size_t capacity;
  size_t offset;
  struct abc_pool *next;
};

/*
 * Create an empty pool.
 */
struct abc_pool *abc_pool_create(void);

/*
 * Allocate an item with the default alignment (16 bytes).
 */
void *abc_pool_alloc(struct abc_pool *pool, size_t size, size_t count);

/*
 * Allocate with custom alignment.
 */
void *abc_pool_alloc_aligned(struct abc_pool *pool, size_t size, size_t count, size_t alignment);

/*
 * Destroy pool and all allocations made with it.
 */
void abc_pool_destroy(struct abc_pool *pool);

#endif //ABC_POOL_H
