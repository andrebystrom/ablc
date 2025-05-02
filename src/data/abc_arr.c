#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "abc_arr.h"

#include <stddef.h>


void abc_arr_init(struct abc_arr *arr, size_t elem_size, struct abc_pool *pool) {
    arr->len = 0;
    arr->cap = ABC_ARR_INIT_CAP;
    arr->elem_size = elem_size;
    arr->pool = pool;
    arr->data = abc_pool_alloc(arr->pool, elem_size, arr->cap);
}

void *abc_arr_push(struct abc_arr *arr, void *data) {
    if (arr->len == arr->cap) {
        // Grow
        arr->cap *= 2;
        void *tmp = abc_pool_alloc(arr->pool, arr->elem_size, arr->cap);
        memcpy(tmp, arr->data, arr->elem_size * arr->len);
        arr->data = tmp;
    }
    memmove((char *) arr->data + arr->elem_size * arr->len, data, arr->elem_size);
    return (char *) arr->data + arr->elem_size * (arr->len++);
}

static unsigned long get_ptr_index(struct abc_arr *arr, void *ptr) {
    char *b = (char *) ptr;
    uintptr_t diff = (uintptr_t) b - (uintptr_t)arr->data;
    diff = diff / arr->elem_size;
    return (unsigned long) diff;
}

void *abc_arr_insert_before_ptr(struct abc_arr *arr, void *where, void *data) {
    abc_arr_push(arr, data);
    unsigned long index = get_ptr_index(arr, where);
    unsigned long n = arr->len - 1 - index;
    memmove((char *) arr->data + (index + 1) * arr->elem_size, (char *) arr->data + index * arr->elem_size, n);
    memmove((char *) arr->data + index * arr->elem_size, data, arr->elem_size);
    return (char *) arr->data + index * arr->elem_size;
}

void *abc_arr_insert_after_ptr(struct abc_arr *arr, void *where, void *data) {
    abc_arr_push(arr, data);
    unsigned long index = get_ptr_index(arr, where);
    unsigned long n = arr->len - 2 - index;
    memmove((char *) arr->data + (index + 2) * arr->elem_size + 2, (char *) arr->data + (index + 1) * arr->elem_size, n);
    memmove((char *) arr->data + (index + 1) * arr->elem_size, data, arr->elem_size);
    return (char *) arr->data + (index + 1) * arr->elem_size;
}

void abc_arr_migrate_pool(struct abc_arr *arr, struct abc_pool *pool) {
    void *new_data = abc_pool_alloc(pool, arr->elem_size, arr->cap);
    memcpy(new_data, arr->data, arr->len * arr->elem_size);
    abc_pool_destroy(arr->pool);
    arr->pool = pool;
    arr->data = new_data;
}
