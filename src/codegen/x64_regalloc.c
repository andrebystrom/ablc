/**
 * Very basic 'register allocator' that assigns registers to variables or spills them to the stack.
 * At the moment all variables are assigned stack locations, but the register allocator is free
 * to be extended to allocate registers according to the API.
 *
 * num_params is passed so arguments passed via the stack, that is spilled, can be avoided to occur a move from
 * the passed stack location to the current functions stack.
 *
*/

#include "x64_regalloc.h"

#include <string.h>

static void alloc_reg_for_instr(struct x64_regalloc *regalloc, struct x64_instr *instr);
struct x64_regalloc x64_regalloc(struct x64_fun *fun, struct abc_pool *allocator, int num_params) {
    // init
    struct x64_regalloc regalloc;
    abc_arr_init(&regalloc.allocs, sizeof(struct x64_alloc), allocator);
    abc_arr_init(&regalloc.callee_saved_allocs, sizeof(struct x64_arg), allocator);
    regalloc.num_spilled = 0;

    for (int i = 0; i < fun->x64_blocks.len; i++) {
        struct x64_block *block = (struct x64_block *)fun->x64_blocks.data + i;
        for (int j = 0; j < block->x64_instrs.len; j++) {
            struct x64_instr *instr = (struct x64_instr *)block->x64_instrs.data + j;
            alloc_reg_for_instr(&regalloc, instr);
        }
    }
    return regalloc;
}

static void alloc_reg_for_arg(struct x64_regalloc *regalloc, struct x64_arg *arg);
static void alloc_reg_for_instr(struct x64_regalloc *regalloc, struct x64_instr *instr) {

    switch (instr->tag) {
        case X64_INSTR_BIN:
            alloc_reg_for_arg(regalloc, &instr->val.bin.left);
            alloc_reg_for_arg(regalloc, &instr->val.bin.right);
            break;
        case X64_INSTR_FAC:
            alloc_reg_for_arg(regalloc, &instr->val.fac.right);
            break;
        case X64_INSTR_STACK:
            alloc_reg_for_arg(regalloc, &instr->val.stack.arg);
            break;
        case X64_INSTR_NEGQ:
            alloc_reg_for_arg(regalloc, &instr->val.neg.dest);
            break;
        case X64_INSTR_SETCC:
        case X64_INSTR_JMP:
        case X64_INSTR_JMPCC:
        case X64_INSTR_NOARG:
        case X64_INSTR_MOVZBQ:
        case X64_INSTR_LEAQ:
        case X64_INSTR_CALLQ:
            break;
    }
}

static void alloc_reg_for_arg(struct x64_regalloc *regalloc, struct x64_arg *arg) {
    if (arg->tag != X64_ARG_STR || x64_regalloc_get_arg(regalloc, arg->val.str.str) != NULL) {
        return;
    }
    int offset = (regalloc->num_spilled++) * X64_VAR_SIZE + X64_VAR_SIZE;
    struct x64_arg res = {.tag = X64_ARG_DEREF, .val.deref.reg = X64_REG_RBP, .val.deref.offset = -offset};
    struct x64_alloc alloc = {.label = arg->val.str.str, .arg = res};
    abc_arr_push(&regalloc->allocs, &alloc);
}

struct x64_arg *x64_regalloc_get_arg(struct x64_regalloc *regalloc, char *label) {
    for (int i = 0; i < regalloc->allocs.len; i++) {
        struct x64_alloc *alloc = (struct x64_alloc *)regalloc->allocs.data + i;
        if (strcmp(label, alloc->label) == 0) {
            return &alloc->arg;
        }
    }
    return NULL;
}