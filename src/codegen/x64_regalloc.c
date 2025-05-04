/**
 * Very basic 'register allocator' that assigns registers to variables or spills them to the stack.
 *
 * num_params is passed so arguments passed via the stack, that is spilled, can be avoided to occur a move from
 * the passed stack location to the current functions stack. The first block always contains moves from the
 * registers/stack location the arguments were passed in.
 *
 */

#include "x64_regalloc.h"
#include "x64.h"

#include <assert.h>
#include <string.h>

struct live_range {
    const struct x64_arg *arg;
    int start;
    int end;
    enum x64_reg reg; // only for elements in the active list
};

struct reg_pool {
    enum x64_reg reg;
    bool in_use;
    bool saved;
};

static void init_reg_pool(struct abc_arr *reg_pool) {
    struct reg_pool entries[] = {
            {.reg = X64_REG_RDI, .in_use = false, .saved = false},
            {.reg = X64_REG_RSI, .in_use = false, .saved = false},
            {.reg = X64_REG_RDX, .in_use = false, .saved = false},
            {.reg = X64_REG_RCX, .in_use = false, .saved = false},
            {.reg = X64_REG_R8, .in_use = false, .saved = false},
            {.reg = X64_REG_R9, .in_use = false, .saved = false},
            {.reg = X64_REG_R10, .in_use = false, .saved = false},
            {.reg = X64_REG_R11, .in_use = false, .saved = false},
            {.reg = X64_REG_RBX, .in_use = false, .saved = false},
            {.reg = X64_REG_R12, .in_use = false, .saved = true},
            {.reg = X64_REG_R13, .in_use = false, .saved = true},
            {.reg = X64_REG_R14, .in_use = false, .saved = true},
    };
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        abc_arr_push(reg_pool, &entries[i]);
    }
}

static bool arg_eq(const struct x64_arg *arg, const struct x64_arg *arg2) {
    if (arg->tag != arg2->tag) {
        return false;
    }
    switch (arg->tag) {
        case X64_ARG_STR:
            return strcmp(arg->val.str.str, arg2->val.str.str) == 0;
        case X64_ARG_REG:
            return arg->val.reg.reg == arg2->val.reg.reg;
        default:
            assert(0);
    }
}

int live_range_cmp_start(const void *l, const void *r) {
    const struct live_range *l1 = l;
    const struct live_range *r1 = r;
    return l1->start - r1->start;
}

static void live_range_used(struct abc_arr *arr, const struct x64_arg *arg, int block) {
    if (arg->tag == X64_ARG_IMM || arg->tag == X64_ARG_DEREF) {
        return;
    }

    // search and update
    for (size_t i = 0; i < arr->len; i++) {
        struct live_range *r = (struct live_range *) arr->data + i;
        if (arg_eq(arg, r->arg)) {
            if (r->end < block) {
                r->end = block;
            }
            if (r->start > block) {
                r->start = block;
            }
            return;
        }
    }

    // we need to insert a new live range
    struct live_range r = {.start = block, .end = block, .arg = arg};
    abc_arr_push(arr, &r);
}

static void calculate_live_range(struct abc_arr *arr, struct x64_instr *instr, int block) {
    switch (instr->tag) {
        case X64_INSTR_BIN:
            live_range_used(arr, &instr->val.bin.left, block);
            live_range_used(arr, &instr->val.bin.right, block);
            break;
        case X64_INSTR_FAC:
            live_range_used(arr, &instr->val.fac.right, block);
            break;
        case X64_INSTR_STACK:
            live_range_used(arr, &instr->val.stack.arg, block);
            break;
        case X64_INSTR_LEAQ:
            live_range_used(arr, &instr->val.leaq.dest, block);
            break;
        case X64_INSTR_NEGQ:
            live_range_used(arr, &instr->val.neg.dest, block);
            break;
        case X64_INSTR_CALLQ:
            for (size_t i = 0; i < X64_REG_R15; i++) {
                if (i < 6 || (i > 7 && i < 12)) {
                    live_range_used(arr, &X64_REGS[i], block);
                }
            }

        case X64_INSTR_SETCC:
        case X64_INSTR_MOVZBQ:
        case X64_INSTR_NOARG:
        case X64_INSTR_JMP:
        case X64_INSTR_JMPCC:
            break;
    }
}

static void move_reg_constraints(struct abc_arr *ranges, struct abc_arr *dest) {
    for (int i = 0; i < (int) ranges->len; i++) {
        struct live_range *r = (struct live_range *) ranges->data + i;
        struct live_range cpy = *r;
        if (r->arg->tag == X64_ARG_REG) {
            abc_arr_remove_at_ptr(ranges, r);
            abc_arr_push(dest, &cpy);
            i = i - 1; // continue with same position next iteration.
        }
    }
}

static void remove_expired_ranges(struct abc_arr *active, struct live_range *current, struct abc_arr *regs) {
    for (size_t i = 0; i < active->len; i++) {
        struct live_range *r = (struct live_range *) active->data + i;
        if (r->end <= current->end) {
            for (size_t j = 0; i < regs->len; i++) {
                struct reg_pool *r_entry = (struct reg_pool *) regs->data + j;
                if (r->reg == r_entry->reg) {
                    r_entry->in_use = false;
                }
            }
            abc_arr_remove_at_ptr(active, r);
            i = i - 1;
        }
    }
}

static void insert_callee_saved(struct abc_arr *saved, enum x64_reg reg_tag) {
    for (size_t i = 0; i < saved->len; i++) {
        struct x64_arg *arg = (struct x64_arg *) saved->data + i;
        if (arg->val.reg.reg == reg_tag) {
            return;
        }
    }
    struct x64_arg arg = {.tag = X64_ARG_REG, .val.reg.reg = reg_tag};
    abc_arr_push(saved, &arg);
}

static void alloc_reg(struct x64_regalloc *regalloc, struct abc_arr *active, struct live_range *r,
                      struct abc_arr *constraints, struct abc_arr *regs) {
    for (size_t i = 0; i < regs->len; i++) {
        struct reg_pool *r_entry = (struct reg_pool *) regs->data + i;
        if (r_entry->in_use) {
            continue;
        }

        bool can_alloc = true;
        for (size_t j = 0; j < constraints->len; j++) {
            struct live_range *c_r = (struct live_range *) constraints->data + j;
            if (c_r->arg->val.reg.reg != r_entry->reg) {
                continue;
            }
            if (r->start <= c_r->end && c_r->start <= r->end) {
                can_alloc = false;
            }
            break;
        }
        if (!can_alloc) {
            continue;
        }
        // we can allocate this register!
        struct x64_arg arg = {.tag = X64_ARG_REG, .val.reg.reg = r_entry->reg};
        struct x64_alloc entry = {.label = r->arg->val.str.str, .arg = arg};
        abc_arr_push(&regalloc->allocs, &entry);
        if (r_entry->saved) {
            insert_callee_saved(&regalloc->callee_saved_allocs, r_entry->reg);
        }
        r_entry->in_use = true;
        r->reg = entry.arg.val.reg.reg;
        abc_arr_push(active, r);
        return;
    }
    // no register found, we have to spill...
    int offset = (regalloc->num_spilled++) * X64_VAR_SIZE + X64_VAR_SIZE;
    struct x64_arg res = {.tag = X64_ARG_DEREF, .val.deref.reg = X64_REG_RBP, .val.deref.offset = -offset};
    struct x64_alloc alloc = {.label = r->arg->val.str.str, .arg = res};
    abc_arr_push(&regalloc->allocs, &alloc);
}

struct x64_regalloc x64_regalloc(struct x64_fun *fun, struct abc_pool *allocator, int num_params) {
    // init
    (void) num_params;
    struct x64_regalloc regalloc;
    abc_arr_init(&regalloc.allocs, sizeof(struct x64_alloc), allocator);
    abc_arr_init(&regalloc.callee_saved_allocs, sizeof(struct x64_arg), allocator);
    regalloc.num_spilled = 0;

    // calculate live ranges
    struct abc_arr ranges;
    abc_arr_init(&ranges, sizeof(struct live_range), allocator);
    for (size_t i = 0; i < fun->x64_blocks.len; i++) {
        struct x64_block *block = (struct x64_block *) fun->x64_blocks.data + i;
        for (size_t j = 0; j < block->x64_instrs.len; j++) {
            struct x64_instr *instr = (struct x64_instr *) block->x64_instrs.data + j;
            calculate_live_range(&ranges, instr, (int) i);
        }
    }
    qsort(ranges.data, ranges.len, sizeof(struct live_range), live_range_cmp_start);
    struct abc_arr reg_constraints; // list of register live ranges
    abc_arr_init(&reg_constraints, sizeof(struct live_range), allocator);
    move_reg_constraints(&ranges, &reg_constraints); // ranges now only contains variables

    // assign registers
    struct abc_arr active;
    abc_arr_init(&active, sizeof(struct live_range), allocator);
    struct abc_arr regs;
    abc_arr_init(&regs, sizeof(struct reg_pool), allocator);
    init_reg_pool(&regs);
    for (size_t i = 0; i < ranges.len; i++) {
        struct live_range *r = (struct live_range *) ranges.data + i;
        remove_expired_ranges(&active, r, &regs);
        alloc_reg(&regalloc, &active, r, &reg_constraints, &regs);
    }

    // TODO: patch spills that are arguments passed through the stack to just refer to the stack location they
    // were passed in

    return regalloc;
}

struct x64_arg *x64_regalloc_get_arg(struct x64_regalloc *regalloc, char *label) {
    for (size_t i = 0; i < regalloc->allocs.len; i++) {
        struct x64_alloc *alloc = (struct x64_alloc *) regalloc->allocs.data + i;
        if (strcmp(label, alloc->label) == 0) {
            return &alloc->arg;
        }
    }
    return NULL;
}
