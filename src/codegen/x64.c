/**
 * Translates IR to x64 assembly.
 *
 */

#include "x64.h"
#include "x64_regalloc.h"

#include <assert.h>
#include <string.h>

void x64_translator_init(struct x64_translator *t) {
    t->pool = abc_pool_create();
    t->curr_fun = NULL;
    t->curr_block = NULL;
}

void x64_translator_destroy(struct x64_translator *t) { abc_pool_destroy(t->pool); }

static void x64_program_translate_fun(struct x64_translator *t, struct ir_fun *ir_fun);

struct x64_program x64_translate(struct x64_translator *t, struct ir_program *program) {
    struct x64_program result;
    abc_arr_init(&result.x64_funs, sizeof(struct x64_fun), t->pool);

    for (size_t i = 0; i < program->ir_funs.len; i++) {
        struct ir_fun *ir_fun = &((struct ir_fun *) program->ir_funs.data)[i];
        struct x64_fun fun;
        t->curr_fun = abc_arr_push(&result.x64_funs, &fun);
        x64_program_translate_fun(t, ir_fun);
        t->curr_fun = NULL;
        t->curr_block = NULL;
    }

    return result;
}

/* FUNCTIONS */

static void create_init_block(struct x64_translator *t, struct ir_fun *ir_fun) {
    int len = snprintf(NULL, 0, "%s_init", ir_fun->label);
    char *label = abc_pool_alloc(t->pool, len + 1, 1);
    snprintf(label, len + 1, "%s_init", ir_fun->label);
    struct x64_block block = {.label = label};
    abc_arr_init(&block.x64_instrs, sizeof(struct x64_instr), t->pool);
    t->curr_block = abc_arr_push(&t->curr_fun->x64_blocks, &block);

    for (size_t i = 0; i < ir_fun->args.len; i++) {
        struct ir_param *arg = ((struct ir_param *) ir_fun->args.data) + i;
        if (i < 4) {
            // rdi to rcx
            struct x64_instr mov = {.tag = X64_INSTR_BIN, .val.bin.tag = X64_BIN_MOVQ};
            mov.val.bin.right.tag = X64_ARG_STR;
            mov.val.bin.right.val.str.str = arg->label;
            mov.val.bin.left = X64_REGS[X64_REG_RDI - i];
            abc_arr_push(&t->curr_block->x64_instrs, &mov);
        } else if (i < 6) {
            // r8 to r9
            struct x64_instr mov = {.tag = X64_INSTR_BIN, .val.bin.tag = X64_BIN_MOVQ};
            mov.val.bin.right.tag = X64_ARG_STR;
            mov.val.bin.right.val.str.str = arg->label;
            mov.val.bin.left = X64_REGS[X64_REG_R8 + (i - 4)];
            abc_arr_push(&t->curr_block->x64_instrs, &mov);
        } else {
            int num_spilled = (int) ir_fun->args.len - 6;
            int offset = X64_STACK_PARAM_OFFSET + (num_spilled - (i - 6 + 1)) * X64_VAR_SIZE;
            struct x64_instr mov = {.tag = X64_INSTR_BIN, .val.bin.tag = X64_BIN_MOVQ};
            mov.val.bin.right.tag = X64_ARG_STR;
            mov.val.bin.right.val.str.str = arg->label;
            mov.val.bin.left.tag = X64_ARG_DEREF;
            mov.val.bin.left.val.deref.offset = offset;
            mov.val.bin.left.val.deref.reg = X64_REG_RBP;
            abc_arr_push(&t->curr_block->x64_instrs, &mov);
        }
    }
}

static char *create_prelude_label(struct x64_translator *t, char *fun_name) {
    int len = snprintf(NULL, 0, "%s_prelude", fun_name);
    char *prelude = abc_pool_alloc(t->pool, len + 1, 1);
    snprintf(prelude, len + 1, "%s_prelude", fun_name);
    return prelude;
}

static void create_prelude(struct x64_translator *t, struct ir_fun *ir_fun, struct x64_regalloc *regalloc) {
    char *label = create_prelude_label(t, ir_fun->label);
    struct x64_block block = {.label = label};
    abc_arr_init(&block.x64_instrs, sizeof(struct x64_instr), t->pool);
    // insert block at start
    t->curr_block = abc_arr_insert_before_ptr(&t->curr_fun->x64_blocks, t->curr_fun->x64_blocks.data, &block);

    struct x64_instr instr = {.tag = X64_INSTR_STACK, .val.stack.tag = X64_STACK_PUSHQ, .val.stack.arg = X64_RBP};
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
    instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ, instr.val.bin.left = X64_RSP,
    instr.val.bin.right = X64_RBP;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);

    // space for spilled variables (and alignment)
    long offset = regalloc->num_spilled * X64_VAR_SIZE;
    int total = (regalloc->num_spilled + (int) regalloc->callee_saved_allocs.len + 1) * X64_VAR_SIZE;
    if (total % 16 != 0) {
        offset += X64_VAR_SIZE;
    }
    instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_SUBQ;
    instr.val.bin.right = X64_RSP;
    instr.val.bin.left.tag = X64_ARG_IMM;
    instr.val.bin.left.val.imm.imm = offset;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);

    // Save all callee saved regs
    for (size_t i = 0; i < regalloc->callee_saved_allocs.len; i++) {
        struct x64_arg *saved = (struct x64_arg *) regalloc->callee_saved_allocs.data + i;
        instr.tag = X64_INSTR_STACK;
        instr.val.stack.tag = X64_STACK_PUSHQ;
        instr.val.stack.arg = *saved;
        abc_arr_push(&t->curr_block->x64_instrs, &instr);
    }
}

static char *create_epilogue_label(struct x64_translator *t, char *fun_name) {
    static char *curr_fun_name = NULL;
    static char *cached = NULL;
    if (curr_fun_name == NULL || strcmp(curr_fun_name, fun_name) != 0) {
        curr_fun_name = fun_name;
    } else {
        return cached;
    }
    int len = snprintf(NULL, 0, "%s_epilogue", fun_name);
    char *epilogue = abc_pool_alloc(t->pool, len + 1, 1);
    snprintf(epilogue, len + 1, "%s_epilogue", fun_name);
    cached = epilogue;
    return epilogue;
}

static void create_epilogue(struct x64_translator *t, char *fun_name, struct x64_regalloc *regalloc) {
    char *label = create_epilogue_label(t, fun_name);
    struct x64_block block = {.label = label};
    abc_arr_init(&block.x64_instrs, sizeof(struct x64_instr), t->pool);
    // insert at end
    t->curr_block = abc_arr_push(&t->curr_fun->x64_blocks, &block);

    // restore callee saved, in reverse order
    for (size_t i = 0; i < regalloc->callee_saved_allocs.len; i++) {
        struct x64_arg *saved = regalloc->callee_saved_allocs.data + (regalloc->callee_saved_allocs.len - (i + 1));
        struct x64_instr instr = {.tag = X64_INSTR_STACK, .val.stack.tag = X64_STACK_POPQ};
        instr.val.stack.arg = *saved;
        abc_arr_push(&t->curr_block->x64_instrs, &instr);
    }

    // space for spilled variables (and alignment)
    struct x64_instr restore_instr = {.tag = X64_INSTR_STACK, .val.stack.tag = X64_STACK_POPQ};
    long offset = regalloc->num_spilled * X64_VAR_SIZE;
    int total = (regalloc->num_spilled + (int) regalloc->callee_saved_allocs.len + 1) * X64_VAR_SIZE;
    if (total % 16 != 0) {
        offset += X64_VAR_SIZE;
    }
    restore_instr.tag = X64_INSTR_BIN, restore_instr.val.bin.tag = X64_BIN_ADDQ;
    restore_instr.val.bin.right = X64_RSP;
    restore_instr.val.bin.left.tag = X64_ARG_IMM;
    restore_instr.val.bin.left.val.imm.imm = offset;
    abc_arr_push(&t->curr_block->x64_instrs, &restore_instr);

    // restore base pointer and return
    struct x64_instr instr = {.tag = X64_INSTR_STACK, .val.stack.tag = X64_STACK_POPQ};
    instr.val.stack.arg = X64_RBP;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
    instr.tag = X64_INSTR_NOARG;
    instr.val.noarg.tag = X64_NOARG_RETQ;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
}

/* REGISTER ALLOCATION */

static void x64_assign_homes_arg(struct x64_translator *t, struct x64_regalloc *regalloc, struct x64_arg *arg) {
    if (arg->tag == X64_ARG_STR) {
        struct x64_arg *new_arg = x64_regalloc_get_arg(regalloc, arg->val.str.str);
        assert(new_arg != NULL);
        *arg = *new_arg;
    }
}

static void x64_assign_homes_instr(struct x64_translator *t, struct x64_regalloc *regalloc, struct x64_instr *instr) {
    switch (instr->tag) {
        case X64_INSTR_BIN:
            x64_assign_homes_arg(t, regalloc, &instr->val.bin.left);
            x64_assign_homes_arg(t, regalloc, &instr->val.bin.right);
            break;
        case X64_INSTR_FAC:
            x64_assign_homes_arg(t, regalloc, &instr->val.fac.right);
            break;
        case X64_INSTR_STACK:
            x64_assign_homes_arg(t, regalloc, &instr->val.stack.arg);
            break;
        case X64_INSTR_NEGQ:
            x64_assign_homes_arg(t, regalloc, &instr->val.neg.dest);
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

static void x64_assign_homes(struct x64_translator *t, struct x64_regalloc *regalloc) {
    for (int i = 0; i < t->curr_fun->x64_blocks.len; i++) {
        struct x64_block *block = (struct x64_block *) t->curr_fun->x64_blocks.data + i;
        for (int j = 0; j < block->x64_instrs.len; j++) {
            struct x64_instr *instr = (struct x64_instr *) block->x64_instrs.data + j;
            x64_assign_homes_instr(t, regalloc, instr);
        }
    }
}

/* INSTRUCTION PATCHING */

static void x64_program_patch_instr(struct x64_translator *t, struct x64_block *block, struct x64_instr *instr) {
    if (instr->tag == X64_INSTR_BIN && instr->val.bin.left.tag == X64_ARG_DEREF &&
            instr->val.bin.right.tag == X64_ARG_DEREF) {
        struct x64_instr patch = {.tag = X64_INSTR_BIN, .val.bin.tag = X64_BIN_MOVQ};
        patch.val.bin.left = instr->val.bin.left;
        patch.val.bin.right = X64_RAX;
        abc_arr_insert_before_ptr(&block->x64_instrs, instr, &patch);
        (instr + 1)->val.bin.left = X64_RAX;
    }
}

static void x64_program_patch_fun(struct x64_translator *t, struct x64_fun *fun) {
    for (int i = 0; i < t->curr_fun->x64_blocks.len; i++) {
        struct x64_block *block = (struct x64_block *) t->curr_fun->x64_blocks.data + i;
        size_t len = block->x64_instrs.len;
        for (int j = 0; j < len; j++) {
            struct x64_instr *instr = (struct x64_instr *) block->x64_instrs.data + j;
            x64_program_patch_instr(t, block, instr);
            len = block->x64_instrs.len;
        }
    }
}

static void x64_program_translate_block(struct x64_translator *t, struct ir_block *ir_block);
static void x64_program_translate_fun(struct x64_translator *t, struct ir_fun *ir_fun) {
    // init
    t->curr_fun->label = ir_fun->label;
    abc_arr_init(&t->curr_fun->x64_blocks, sizeof(struct x64_block), t->pool);
    create_init_block(t, ir_fun);

    // fun translation
    for (size_t i = 0; i < ir_fun->blocks.len; i++) {
        struct ir_block *ir_block = ((struct ir_block *) ir_fun->blocks.data) + i;
        struct x64_block block = {.label = ir_block->label};
        abc_arr_init(&block.x64_instrs, sizeof(struct x64_instr), t->pool);
        t->curr_block = abc_arr_push(&t->curr_fun->x64_blocks, &block);
        x64_program_translate_block(t, ir_block);
    }

    // register allocation
    struct abc_pool *allocator = abc_pool_create();
    struct x64_regalloc regalloc = x64_regalloc(t->curr_fun, allocator, (int) ir_fun->args.len);
    x64_assign_homes(t, &regalloc);

    // patch instructions
    x64_program_patch_fun(t, t->curr_fun);

    // end
    create_prelude(t, ir_fun, &regalloc);
    create_epilogue(t, ir_fun->label, &regalloc);
    abc_pool_destroy(allocator);
}

/* STATEMENT TRANSLATION */

static void x64_program_translate_stmt(struct x64_translator *t, struct ir_stmt *ir_stmt);
static void x64_program_translate_tail(struct x64_translator *t, struct ir_tail *ir_tail);
static void x64_program_translate_block(struct x64_translator *t, struct ir_block *ir_block) {
    for (size_t i = 0; i < ir_block->stmts.len; i++) {
        struct ir_stmt *stmt = ((struct ir_stmt *) ir_block->stmts.data) + i;
        x64_program_translate_stmt(t, stmt);
    }
    if (ir_block->has_tail) {
        x64_program_translate_tail(t, &ir_block->tail);
    }
}

static void x64_program_translate_expr(struct x64_translator *t, struct ir_expr *expr);
static struct x64_arg x64_program_translate_atom(struct x64_translator *t, struct ir_atom *atom);
static void x64_program_translate_stmt(struct x64_translator *t, struct ir_stmt *ir_stmt) {
    struct x64_instr instr;
    struct x64_arg arg;
    switch (ir_stmt->tag) {
        case IR_STMT_DECL:
            if (ir_stmt->val.decl.has_init) {
                x64_program_translate_expr(t, &ir_stmt->val.decl.init);
                instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
                instr.val.bin.right.tag = X64_ARG_STR;
                instr.val.bin.right.val.str.str = ir_stmt->val.decl.label;
                instr.val.bin.left.tag = X64_ARG_REG;
                instr.val.bin.left.val.reg.reg = X64_REG_RAX;
                abc_arr_push(&t->curr_block->x64_instrs, &instr);
            }
            break;
        case IR_STMT_EXPR:
            x64_program_translate_expr(t, &ir_stmt->val.expr.expr);
            break;
        case IR_STMT_PRINT:
            // align
            instr.tag = X64_INSTR_STACK, instr.val.stack.tag = X64_STACK_PUSHQ;
            instr.val.stack.arg = X64_RBP;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            // load fmt string
            instr.tag = X64_INSTR_LEAQ;
            instr.val.leaq.label = X64_FORMAT_STR_LABEL;
            instr.val.leaq.dest.tag = X64_ARG_REG;
            instr.val.leaq.dest.val.reg.reg = X64_REG_RDI;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            // move atom to rsi
            instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
            arg = x64_program_translate_atom(t, &ir_stmt->val.print.atom);
            instr.val.bin.left = arg;
            instr.val.bin.right = X64_RSI;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            // zero al for varargs
            instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
            instr.val.bin.left.tag = X64_ARG_IMM;
            instr.val.bin.left.val.imm.imm = 0;
            instr.val.bin.right = X64_RAX;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            // issue call
            instr.tag = X64_INSTR_CALLQ;
            instr.val.callq.label = "printf";
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            // remove alignment
            instr.tag = X64_INSTR_STACK, instr.val.stack.tag = X64_STACK_POPQ;
            instr.val.stack.arg = X64_RBP;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            break;
    }
}

static void x64_program_translate_tail(struct x64_translator *t, struct ir_tail *ir_tail) {
    struct x64_instr instr;
    struct x64_arg arg;
    switch (ir_tail->tag) {
        case IR_TAIL_GOTO:
            instr.tag = X64_INSTR_JMP;
            instr.val.jmp.label = ir_tail->val.go_to.label;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            break;
        case IR_TAIL_RET:
            if (ir_tail->val.ret.has_atom) {
                arg = x64_program_translate_atom(t, &ir_tail->val.ret.atom);
                instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
                instr.val.bin.right = X64_RAX;
                instr.val.bin.left = arg;
                abc_arr_push(&t->curr_block->x64_instrs, &instr);
            }
            instr.tag = X64_INSTR_JMP;
            instr.val.jmp.label = create_epilogue_label(t, t->curr_fun->label);
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            break;
        case IR_TAIL_IF:
            arg = x64_program_translate_atom(t, &ir_tail->val.if_then_else.atom);
            instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_CMPQ;
            instr.val.bin.left.tag = X64_ARG_IMM, instr.val.bin.left.val.imm.imm = 1;
            instr.val.bin.right = arg;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            instr.tag = X64_INSTR_JMPCC;
            instr.val.jmpcc.code = X64_CC_E;
            instr.val.jmpcc.label = ir_tail->val.if_then_else.then_label;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            instr.tag = X64_INSTR_JMP;
            instr.val.jmp.label = ir_tail->val.if_then_else.else_label;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            break;
        default:
            assert(0);
    }
}

/* EXPRESSION TRANSLATION, result of expressions always stored in $rax */

static void x64_program_translate_bin_expr(struct x64_translator *t, struct ir_expr_bin *expr);
static void x64_program_translate_unary_expr(struct x64_translator *t, struct ir_expr_unary *expr);
static void x64_program_translate_cmp_expr(struct x64_translator *t, struct ir_expr_cmp *expr);
static void x64_program_translate_call_expr(struct x64_translator *t, struct ir_expr_call *expr);

static void x64_program_translate_expr(struct x64_translator *t, struct ir_expr *expr) {
    struct x64_instr instr;
    struct x64_arg lhs;
    switch (expr->tag) {
        case IR_EXPR_BIN:
            x64_program_translate_bin_expr(t, &expr->val.bin);
            break;
        case IR_EXPR_UNARY:
            x64_program_translate_unary_expr(t, &expr->val.unary);
            break;
        case IR_EXPR_ATOM:
            lhs = x64_program_translate_atom(t, &expr->val.unary.atom);
            instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
            instr.val.bin.right = X64_RAX;
            instr.val.bin.left = lhs;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            break;
        case IR_EXPR_CMP:
            x64_program_translate_cmp_expr(t, &expr->val.cmp);
            break;
        case IR_EXPR_CALL:
            x64_program_translate_call_expr(t, &expr->val.call);
            break;
        case IR_EXPR_ASSIGN:
            x64_program_translate_expr(t, expr->val.assign.value);
            instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
            instr.val.bin.right.tag = X64_ARG_STR, instr.val.bin.right.val.str.str = expr->val.assign.label;
            instr.val.bin.left = X64_RAX;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            break;
    }
}

static void x64_program_translate_bin_expr(struct x64_translator *t, struct ir_expr_bin *expr) {
    struct x64_arg lhs, rhs;
    struct x64_instr instr;
    lhs = x64_program_translate_atom(t, &expr->lhs);
    rhs = x64_program_translate_atom(t, &expr->rhs);
    if (expr->op == IR_BIN_PLUS || expr->op == IR_BIN_MINUS) {
        instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
        instr.val.bin.right = X64_RAX;
        instr.val.bin.left = lhs;
        abc_arr_push(&t->curr_block->x64_instrs, &instr);

        instr.tag = X64_INSTR_BIN, instr.val.bin.tag = expr->op == IR_BIN_PLUS ? X64_BIN_ADDQ : X64_BIN_SUBQ;
        instr.val.bin.left = rhs;
        instr.val.bin.right = X64_RAX;
        abc_arr_push(&t->curr_block->x64_instrs, &instr);
        return;
    }

    // rdx:rax [* / /] arg
    instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
    instr.val.bin.left.tag = X64_ARG_IMM;
    instr.val.bin.left.val.imm.imm = 0;
    instr.val.bin.right = X64_RDX;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);

    instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
    instr.val.bin.left = lhs;
    instr.val.bin.right = X64_RAX;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);

    instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
    instr.val.bin.left = rhs;
    instr.val.bin.right = X64_R15;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
    instr.tag = X64_INSTR_FAC, instr.val.fac.tag = expr->op == IR_BIN_MUL ? X64_FAC_IMULQ : X64_FAC_IDIVQ;
    instr.val.fac.right = X64_R15;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
}

static void x64_program_translate_unary_expr(struct x64_translator *t, struct ir_expr_unary *expr) {
    struct x64_arg rhs = x64_program_translate_atom(t, &expr->atom);
    struct x64_instr instr;
    instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
    instr.val.bin.right = X64_RAX;
    instr.val.bin.left = rhs;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);

    if (expr->op == IR_UNARY_BANG) {
        instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_XORQ;
        instr.val.bin.left.tag = X64_ARG_IMM, instr.val.bin.left.val.imm.imm = 1;
        instr.val.bin.right = X64_RAX;
        abc_arr_push(&t->curr_block->x64_instrs, &instr);
    } else {
        instr.tag = X64_INSTR_NEGQ;
        instr.val.neg.dest.tag = X64_ARG_REG;
        instr.val.neg.dest.val.reg.reg = X64_REG_RAX;
        abc_arr_push(&t->curr_block->x64_instrs, &instr);
    }
}

static void x64_program_translate_cmp_expr(struct x64_translator *t, struct ir_expr_cmp *expr) {
    struct x64_arg lhs, rhs;
    struct x64_instr instr;
    lhs = x64_program_translate_atom(t, &expr->lhs);
    rhs = x64_program_translate_atom(t, &expr->rhs);

    instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
    instr.val.bin.right = X64_RAX;
    instr.val.bin.left = lhs;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);

    instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_CMPQ;
    instr.val.bin.right = X64_RAX;
    instr.val.bin.left = rhs;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);

    instr.tag = X64_INSTR_SETCC;
    switch (expr->cmp) {
        case IR_CMP_EQ:
            instr.val.setcc.code = X64_CC_E;
            break;
        case IR_CMP_NE:
            instr.val.setcc.code = X64_CC_NE;
            break;
        case IR_CMP_LT:
            instr.val.setcc.code = X64_CC_L;
            break;
        case IR_CMP_GT:
            instr.val.setcc.code = X64_CC_G;
            break;
        case IR_CMP_LE:
            instr.val.setcc.code = X64_CC_LE;
            break;
        case IR_CMP_GE:
            instr.val.setcc.code = X64_CC_GE;
            break;
    }
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
    instr.tag = X64_INSTR_MOVZBQ;
    instr.val.movzbq.dst = X64_RAX;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
}

static void x64_program_translate_call_expr(struct x64_translator *t, struct ir_expr_call *expr) {
    struct x64_instr instr;
    bool need_align = expr->args.len <= 6 || ((int) expr->args.len - 6 + 1) * X64_VAR_SIZE % 16 != 0;

    if (need_align) {
        instr.tag = X64_INSTR_STACK, instr.val.stack.tag = X64_STACK_PUSHQ;
        instr.val.stack.arg = X64_RBP;
        abc_arr_push(&t->curr_block->x64_instrs, &instr);
    }

    // pass args
    for (int i = 0; i < expr->args.len; i++) {
        struct x64_arg src = x64_program_translate_atom(t, (struct ir_atom *) expr->args.data + i);
        if (i < 4) {
            struct x64_arg dest = X64_REGS[X64_REG_RDI - i];
            instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
            instr.val.bin.left = src;
            instr.val.bin.right = dest;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
        } else if (i < 6) {
            struct x64_arg dest = X64_REGS[X64_REG_R8 + (i - 4)];
            instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_MOVQ;
            instr.val.bin.left = src;
            instr.val.bin.right = dest;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
        } else {
            instr.tag = X64_INSTR_STACK, instr.val.stack.tag = X64_STACK_PUSHQ;
            instr.val.stack.arg = src;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
        }
    }

    // call
    instr.tag = X64_INSTR_CALLQ, instr.val.callq.label = expr->label;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);

    // restore stack
    int num_pushed = ((int) expr->args.len - 6 > 0) ? (int) expr->args.len - 6 : 0 + (need_align ? 1 : 0);
    if (num_pushed > 0) {
        instr.tag = X64_INSTR_BIN, instr.val.bin.tag = X64_BIN_ADDQ;
        instr.val.bin.left.tag = X64_ARG_IMM;
        instr.val.bin.left.val.imm.imm = num_pushed * X64_VAR_SIZE;
        instr.val.bin.right = X64_RSP;
        abc_arr_push(&t->curr_block->x64_instrs, &instr);
    }
}

static struct x64_arg x64_program_translate_atom(struct x64_translator *t, struct ir_atom *atom) {
    struct x64_arg arg = {0};
    switch (atom->tag) {
        case IR_ATOM_INT_LIT:
            arg.tag = X64_ARG_IMM;
            arg.val.imm.imm = atom->val.int_lit;
            break;
        case IR_ATOM_IDENTIFIER:
            arg.tag = X64_ARG_STR;
            arg.val.str.str = atom->val.label;
            break;
        default:
            assert(0);
    }
    return arg;
}

/* PRINTING */

static void x64_program_print_indent(FILE *f) { fprintf(f, "    "); }

static void x64_program_print_label(FILE *f, char *label) {
#ifdef __APPLE__
    fprintf(f, "_%s", label);
#else
    fprintf(f, "%s", label);
#endif
}

static void x64_program_print_reg(enum x64_reg reg, FILE *f) {
    switch (reg) {
        case X64_REG_RAX:
            fprintf(f, "rax");
            break;
        case X64_REG_RBX:
            fprintf(f, "rbx");
            break;
        case X64_REG_RCX:
            fprintf(f, "rcx");
            break;
        case X64_REG_RDX:
            fprintf(f, "rdx");
            break;
        case X64_REG_RSI:
            fprintf(f, "rsi");
            break;
        case X64_REG_RDI:
            fprintf(f, "rdi");
            break;
        case X64_REG_RSP:
            fprintf(f, "rsp");
            break;
        case X64_REG_RBP:
            fprintf(f, "rbp");
            break;
        case X64_REG_R8:
            fprintf(f, "r8");
            break;
        case X64_REG_R9:
            fprintf(f, "r9");
            break;
        case X64_REG_R10:
            fprintf(f, "r10");
            break;
        case X64_REG_R11:
            fprintf(f, "r11");
            break;
        case X64_REG_R12:
            fprintf(f, "r12");
            break;
        case X64_REG_R13:
            fprintf(f, "r13");
            break;
        case X64_REG_R14:
            fprintf(f, "r14");
            break;
        case X64_REG_R15:
            fprintf(f, "r15");
            break;
    }
}

static void x64_program_print_cc(enum x64_cc cc, FILE *f) {
    switch (cc) {
        case X64_CC_E:
            fprintf(f, "e");
            break;
        case X64_CC_NE:
            fprintf(f, "ne");
            break;
        case X64_CC_L:
            fprintf(f, "l");
            break;
        case X64_CC_LE:
            fprintf(f, "le");
            break;
        case X64_CC_G:
            fprintf(f, "g");
            break;
        case X64_CC_GE:
            fprintf(f, "ge");
            break;
    }
}

static void x64_program_print_arg(struct x64_arg *arg, FILE *f) {
    struct x64_arg tmp;
    switch (arg->tag) {
        case X64_ARG_STR:
            fprintf(f, "%s", arg->val.str.str);
            break;
        case X64_ARG_REG:
            fprintf(f, "%%");
            x64_program_print_reg(arg->val.reg.reg, f);
            break;
        case X64_ARG_IMM:
            fprintf(f, "$%ld", arg->val.imm.imm);
            break;
        case X64_ARG_DEREF:
            fprintf(f, "%ld(", arg->val.deref.offset);
            tmp.tag = X64_ARG_REG;
            tmp.val.reg.reg = arg->val.deref.reg;
            x64_program_print_arg(&tmp, f);
            fprintf(f, ")");
            break;
    }
}

static void x64_program_print_instr(struct x64_instr *instr, FILE *f) {

    switch (instr->tag) {
        case X64_INSTR_BIN:
            switch (instr->val.bin.tag) {
                case X64_BIN_ADDQ:
                    fprintf(f, "addq ");
                    break;
                case X64_BIN_SUBQ:
                    fprintf(f, "subq ");
                    break;
                case X64_BIN_XORQ:
                    fprintf(f, "xorq ");
                    break;
                case X64_BIN_MOVQ:
                    fprintf(f, "movq ");
                    break;
                case X64_BIN_CMPQ:
                    fprintf(f, "cmpq ");
                    break;
            }
            x64_program_print_arg(&instr->val.bin.left, f);
            fprintf(f, ", ");
            x64_program_print_arg(&instr->val.bin.right, f);
            break;
        case X64_INSTR_FAC:
            fprintf(f, "%s ", instr->val.fac.tag == X64_FAC_IMULQ ? "imulq" : "idivq");
            x64_program_print_arg(&instr->val.fac.right, f);
            break;
        case X64_INSTR_STACK:
            fprintf(f, "%s ", instr->val.stack.tag == X64_STACK_POPQ ? "popq" : "pushq");
            x64_program_print_arg(&instr->val.stack.arg, f);
            break;
        case X64_INSTR_NOARG:
            fprintf(f, "%s", instr->val.noarg.tag == X64_NOARG_RETQ ? "retq" : "leaveq");
            break;
        case X64_INSTR_MOVZBQ:
            fprintf(f, "movzbq %%al, ");
            x64_program_print_arg(&instr->val.movzbq.dst, f);
            break;
        case X64_INSTR_SETCC:
            fprintf(f, "set");
            x64_program_print_cc(instr->val.setcc.code, f);
            fprintf(f, " %%al");
            break;
        case X64_INSTR_LEAQ:
            fprintf(f, "leaq ");
            fprintf(f, "%s(%%rip), ", instr->val.leaq.label);
            x64_program_print_arg(&instr->val.leaq.dest, f);
            break;
        case X64_INSTR_NEGQ:
            fprintf(f, "negq ");
            x64_program_print_arg(&instr->val.neg.dest, f);
            break;
        case X64_INSTR_JMP:
            fprintf(f, "jmp ");
            x64_program_print_label(f, instr->val.jmp.label);
            break;
        case X64_INSTR_JMPCC:
            fprintf(f, "j");
            x64_program_print_cc(instr->val.jmpcc.code, f);
            fprintf(f, " ");
            x64_program_print_label(f, instr->val.jmpcc.label);
            break;
        case X64_INSTR_CALLQ:
            fprintf(f, "callq ");
            x64_program_print_label(f, instr->val.callq.label);
            break;
    }
}

static void x64_program_print_block(struct x64_block *block, FILE *f) {
    x64_program_print_label(f, block->label);
    fprintf(f, ":\n");
    for (size_t i = 0; i < block->x64_instrs.len; i++) {
        x64_program_print_indent(f);
        struct x64_instr *instr = ((struct x64_instr *) block->x64_instrs.data) + i;
        x64_program_print_instr(instr, f);
        fprintf(f, "\n");
    }
}

void x64_program_print(struct x64_program *prog, FILE *f) {
    // generate format string in data section and switch to text section
    fprintf(f, ".data\n" X64_FORMAT_STR_LABEL ": .asciz \"%%ld\\n\"\n\n");
    fprintf(f, ".text\n.global main\n\n");

    for (size_t i = 0; i < prog->x64_funs.len; i++) {
        struct x64_fun *fun = ((struct x64_fun *) prog->x64_funs.data) + i;
        x64_program_print_label(f, fun->label);
        fprintf(f, ":\n");
        for (size_t j = 0; j < fun->x64_blocks.len; j++) {
            struct x64_block *block = ((struct x64_block *) fun->x64_blocks.data) + j;
            x64_program_print_block(block, f);
        }
        fprintf(f, "\n");
    }
}
