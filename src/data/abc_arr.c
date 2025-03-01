#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "abc_arr.h"


void abc_arr_init(struct abc_arr *arr, size_t elem_size) {
    arr->len = 0;
    arr->cap = ABC_ARR_INIT_CAP;
    arr->elem_size = elem_size;
    arr->data = malloc(elem_size * arr->cap);
    if(!arr->data) {
        // We do not bother to try to recover
        fprintf(stderr, "%s: Init failed, out of memory\n", __FILE__);
        exit(EXIT_FAILURE);
    }
}

void *abc_arr_push(struct abc_arr *arr, void *data) {
    if(arr->len == arr->cap) {
        // Grow
        arr->cap *= 2;
        arr->data = realloc(arr->data, arr->cap * arr->elem_size);
        if(!arr->data) {
            fprintf(stderr, "%s: Expansion failed, out of memory\n", __FILE__);
            exit(EXIT_FAILURE);
        }
    }
    memmove((char *) arr->data + arr->len, data, arr->elem_size);

    return (char *) arr->data + (arr->len++);
}

void abc_arr_destroy(struct abc_arr *arr) {
    free(arr->data);
}
