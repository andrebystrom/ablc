#ifndef X64_REGALLOC_H
#define X64_REGALLOC_H

#include "../data/abc_arr.h"
#include "../data/abc_pool.h"
#include "x64.h"

struct x64_alloc {
    char *label;
    struct x64_arg arg;
};

struct x64_regalloc {
    struct abc_arr allocs; // x64_alloc
    struct abc_arr callee_saved_allocs; // x64_arg (registers)
    int num_spilled; // does not include spilled arguments passed through stack
};

// allocate registers for a function
// RAX/RBP is never used. We also guarantee that registers are not assigned variables with a live range
// overlapping with a function call. num_params are the number of function parameters, and these
// are assigned to names in the first block. With n params, the first n instructions are moves for these.
struct x64_regalloc x64_regalloc(struct x64_fun *program, struct abc_pool *allocator, int num_params);

struct x64_arg *x64_regalloc_get_arg(struct x64_regalloc *regalloc, char *label);

#endif //X64_REGALLOC_H
