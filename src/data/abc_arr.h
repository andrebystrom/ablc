#ifndef ABC_ARR_H
#define ABC_ARR_H

#include <stdlib.h>

#include "abc_pool.h"

#define ABC_ARR_INIT_CAP 10

struct abc_arr {
  size_t len;
  size_t cap;
  size_t elem_size;
  struct abc_pool *pool;
  void *data;
};

void abc_arr_init(struct abc_arr *arr, size_t elem_size, struct abc_pool *pool);

void *abc_arr_push(struct abc_arr *arr, void *data);

/*
 * Migrate the array to use the specified pool.
 */
void abc_arr_migrate_pool(struct abc_arr *arr, struct abc_pool *pool);

/*
 * This is handled by destroying the pool.
 */
// void abc_arr_destroy(struct abc_arr *arr);

#endif //ABC_ARR_H
