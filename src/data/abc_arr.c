#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "abc_arr.h"


void abc_arr_init(struct abc_arr *arr, size_t elem_size, struct abc_pool *pool) {
    arr->len = 0;
    arr->cap = ABC_ARR_INIT_CAP;
    arr->elem_size = elem_size;
    arr->pool = pool;
    arr->data = abc_pool_alloc(arr->pool, elem_size, arr->cap);
}

void *abc_arr_push(struct abc_arr *arr, void *data) {
    if(arr->len == arr->cap) {
        // Grow
        arr->cap *= 2;
        void *tmp = abc_pool_alloc(arr->pool, arr->elem_size, arr->cap);
        memcpy(tmp, data, arr->len);
        arr->data = tmp;
    }
    memmove((char *) arr->data + arr->len, data, arr->elem_size);
    return (char *) arr->data + (arr->len++);
}

void abc_arr_migrate_pool(struct abc_arr *arr, struct abc_pool *pool) {
    (void) arr;
    (void) pool;
    // TODO: change pool to new, maybe also destroy current pool (or flag) (?)
    // allocate and copy data to new pool.
}

void abc_arr_destroy(struct abc_arr *arr) {
    free(arr->data);
}
