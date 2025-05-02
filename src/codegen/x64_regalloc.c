#include "x64_regalloc.h"

#include <string.h>

static void alloc_reg_for_instr(struct x64_regalloc *regalloc, struct x64_instr *instr);
struct x64_regalloc x64_regalloc(struct x64_fun *fun, struct abc_pool *allocator) {
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
        case X64_INSTR_ADDQ:
            alloc_reg_for_arg(regalloc, &instr->val.add.dst);
            alloc_reg_for_arg(regalloc, &instr->val.add.src);
            break;
        case X64_INSTR_SUBQ:
            alloc_reg_for_arg(regalloc, &instr->val.sub.dst);
            alloc_reg_for_arg(regalloc, &instr->val.sub.src);
            break;
        case X64_INSTR_IMULQ:
            alloc_reg_for_arg(regalloc, &instr->val.imul.mul);
            break;
        case X64_INSTR_IDIVQ:
            alloc_reg_for_arg(regalloc, &instr->val.idiv.div);
            break;
        case X64_INSTR_XORQ:
            alloc_reg_for_arg(regalloc, &instr->val.xor.dst);
            alloc_reg_for_arg(regalloc, &instr->val.xor.src);
            break;
        case X64_INSTR_MOVQ:
            alloc_reg_for_arg(regalloc, &instr->val.mov.dst);
            alloc_reg_for_arg(regalloc, &instr->val.mov.src);
            break;
        case X64_INSTR_MOVZBQ:
            alloc_reg_for_arg(regalloc, &instr->val.movzbq.dst);
            break;
        case X64_INSTR_PUSHQ:
            alloc_reg_for_arg(regalloc, &instr->val.push.src);
            break;
        case X64_INSTR_POPQ:
            alloc_reg_for_arg(regalloc, &instr->val.pop.dest);
            break;
        case X64_INSTR_LEAQ:
            alloc_reg_for_arg(regalloc, &instr->val.leaq.dest);
            break;
        case X64_INSTR_NEGQ:
            alloc_reg_for_arg(regalloc, &instr->val.neg.dest);
            break;
        case X64_INSTR_SETCC:
            break;
        case X64_INSTR_JMP:
            break;
        case X64_INSTR_CMPQ:
            alloc_reg_for_arg(regalloc, &instr->val.cmp.left);
            alloc_reg_for_arg(regalloc, &instr->val.cmp.right);
            break;
        case X64_INSTR_JMPCC:
            break;
        case X64_INSTR_CALLQ:
            break;
        case X64_INSTR_RETQ:
            break;
        case X64_INSTR_LEAVEQ:
            break;
    }
}

static void alloc_reg_for_arg(struct x64_regalloc *regalloc, struct x64_arg *arg) {
    if (arg->tag != X64_ARG_STR || x64_regalloc_get_arg(regalloc, arg->val.str.str) != NULL) {
        return;
    }
    int offset = (regalloc->num_spilled++) * X64_VAR_SIZE;
    struct x64_arg res = {.tag = X64_ARG_DEREF, .val.deref.reg = X64_REG_RBP, .val.deref.offset = offset};
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