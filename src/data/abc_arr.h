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

// insertion functions that inserts a value directly before/after the index 'where' is located at
void *abc_arr_insert_before_ptr(struct abc_arr *arr, void *where, void *data);
void *abc_arr_insert_after_ptr(struct abc_arr *arr, void *where, void *data);
void abc_arr_remove_at_ptr(struct abc_arr *arr, void *where);

/*
 * Migrate the array to use the specified pool.
 */
void abc_arr_migrate_pool(struct abc_arr *arr, struct abc_pool *pool);

/*
 * This is handled by destroying the pool.
 */
// void abc_arr_destroy(struct abc_arr *arr);

#endif //ABC_ARR_H
