#include "x64.h"

#include <assert.h>
#include <string.h>

void x64_translator_init(struct x64_translator *t) {
    t->pool = abc_pool_create();
    t->curr_fun = NULL;
    t->curr_block = NULL;
    abc_arr_init(&t->var_specs, sizeof(struct x64_var_spec), t->pool);
    t->num_stacked = 0;
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

static void determine_var_spec(struct x64_translator *t, struct ir_fun *ir_fun) {
    t->var_specs.len = 0;

    // start by determining where args are to be placed.
    // stack params starts before return address
    // when entering a function, we push rbp to the stack
    // so the stack pointer points to x, rbp is at x + 8, return address at x + 16, so first param is at x + 24.

    int num_stacked = 0;
    for (size_t i = 0; i < ir_fun->args.len; i++) {
        struct ir_param param = ((struct ir_param *) ir_fun->args.data)[i];
        if (i < 6) {
            // passed as registers, we set these as x, x+8, ...
            struct x64_var_spec var_spec = {.label = param.label, .offset = -(X64_VAR_SIZE * (int) i)};
            abc_arr_push(&t->var_specs, &var_spec);
            num_stacked++;
        } else {
            // spilled arguments are passed in reverse order on the stack
            const int num_param = (int) ir_fun->args.len - 6;
            const int offset = X64_STACK_PARAM_OFFSET + (num_param - ((int) i + 1)) * X64_VAR_SIZE;
            struct x64_var_spec var_spec = {.label = param.label, .offset = offset};
            abc_arr_push(&t->var_specs, &var_spec);
        }
    }

    // now we need to place all local variables...
    for (size_t i = 0; i < ir_fun->blocks.len; i++) {
        struct ir_block block = ((struct ir_block *) ir_fun->blocks.data)[i];
        for (size_t j = 0; j < ir_fun->blocks.len; j++) {
            struct ir_stmt stmt = ((struct ir_stmt *) block.stmts.data)[j];
            if (stmt.tag == IR_STMT_DECL) {
                struct ir_stmt_decl decl = stmt.val.decl;
                struct x64_var_spec var_spec = {.label = decl.label, .offset = -(num_stacked++ * X64_VAR_SIZE)};
                abc_arr_push(&t->var_specs, &var_spec);
            }
        }
    }
    t->num_stacked = num_stacked;
}

static char *create_prelude_label(struct x64_translator *t, char *fun_name) {
    int len = snprintf(NULL, 0, "%s_prelude", fun_name);
    char *prelude = abc_pool_alloc(t->pool, len + 1, 1);
    snprintf(prelude, len + 1, "%s_prelude", fun_name);
    return prelude;
}

static void create_prelude(struct x64_translator *t, struct ir_fun *ir_fun) {
    char *label = create_prelude_label(t, ir_fun->label);
    struct x64_block block = {.label = label};
    abc_arr_init(&block.x64_instrs, sizeof(struct x64_instr), t->pool);
    t->curr_block = abc_arr_push(&t->curr_fun->x64_blocks, &block);

    struct x64_instr push_rbp = {.tag = X64_INSTR_PUSHQ,
                                 .val.push.src = (struct x64_arg) {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RBP}};
    struct x64_instr mov_rsp_rbp = {.tag = X64_INSTR_MOVQ,
        .val.mov.src = (struct x64_arg) {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RSP},
        .val.mov.dst = (struct x64_arg) {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RBP}};
    struct x64_instr sub_rsp = {.tag = X64_INSTR_SUBQ,
        .val.sub.src = (struct x64_arg) {.tag = X64_ARG_IMM, .val.imm.imm = t->num_stacked * X64_VAR_SIZE},
        .val.sub.dst = (struct x64_arg) {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RSP}};

    abc_arr_push(&t->curr_block->x64_instrs, &push_rbp);
    abc_arr_push(&t->curr_block->x64_instrs, &mov_rsp_rbp);
    // TODO: handle this after register allocation instead abc_arr_push(&t->curr_block->x64_instrs, &sub_rsp);

    // TODO: mov all incoming registers/stack params to the parameter names
    struct x64_instr mov = {.tag = X64_INSTR_MOVQ};
    for (size_t i = 0; i < ir_fun->args.len; i++) {
        struct ir_param *arg = ((struct ir_param *) ir_fun->args.data) + i;
        if (i < 4) {
            // rdi to rcx
            mov.val.mov.dst.tag = X64_ARG_STR;
            mov.val.mov.dst.val.str.str = arg->label;
            mov.val.mov.src.tag = X64_ARG_REG;
            mov.val.mov.src.val.reg.reg = X64_REG_RDI - i;
            abc_arr_push(&t->curr_block->x64_instrs, &mov);
        } else if (i < 6) {
            // r8 to r9
            mov.val.mov.dst.tag = X64_ARG_STR;
            mov.val.mov.dst.val.str.str = arg->label;
            mov.val.mov.src.tag = X64_ARG_REG;
            mov.val.mov.src.val.reg.reg = X64_REG_R8 + (i - 4);
            abc_arr_push(&t->curr_block->x64_instrs, &mov);
        }
        else {
            int num_spilled = (int) ir_fun->args.len - 6;
            int offset = X64_STACK_PARAM_OFFSET + (num_spilled - (i - 6 + 1)) * X64_VAR_SIZE;
            mov.val.mov.dst.tag = X64_ARG_STR;
            mov.val.mov.dst.val.str.str = arg->label;
            mov.val.mov.src.tag = X64_ARG_DEREF;
            mov.val.mov.src.val.deref.offset = offset;
            mov.val.mov.dst.val.deref.reg = X64_REG_RBP;
            abc_arr_push(&t->curr_block->x64_instrs, &mov);
        }
    }
}

static char *create_epilogue_label(struct x64_translator *t, char *fun_name) {
    static char *curr_fun_name = NULL;
    static char *cached = NULL;
    if (curr_fun_name == NULL) {
        curr_fun_name = fun_name;
    } else if (strcmp(curr_fun_name, fun_name) == 0) {
        return cached;
    } else {
        curr_fun_name = fun_name;
    }
    int len = snprintf(NULL, 0, "%s_epilogue", fun_name);
    char *epilogue = abc_pool_alloc(t->pool, len + 1, 1);
    snprintf(epilogue, len + 1, "%s_epilogue", fun_name);
    cached = epilogue;
    return epilogue;
}

static void create_epilogue(struct x64_translator *t, char *fun_name) {
    char *label = create_epilogue_label(t, fun_name);
    struct x64_block block = {.label = label};
    abc_arr_init(&block.x64_instrs, sizeof(struct x64_instr), t->pool);
    t->curr_block = abc_arr_push(&t->curr_fun->x64_blocks, &block);

    struct x64_instr leave = {.tag = X64_INSTR_LEAVEQ};
    struct x64_instr ret = {.tag = X64_INSTR_RETQ};
    abc_arr_push(&t->curr_block->x64_instrs, &leave);
    abc_arr_push(&t->curr_block->x64_instrs, &ret);
}

static void x64_program_translate_block(struct x64_translator *t, struct ir_block *ir_block);
static void x64_program_translate_fun(struct x64_translator *t, struct ir_fun *ir_fun) {
    // prelude
    determine_var_spec(t, ir_fun);
    t->curr_fun->label = ir_fun->label;
    abc_arr_init(&t->curr_fun->x64_blocks, sizeof(struct x64_block), t->pool);
    create_prelude(t, ir_fun);

    // fun translation
    for (size_t i = 0; i < ir_fun->blocks.len; i++) {
        struct ir_block *ir_block = ((struct ir_block *)ir_fun->blocks.data) + i;
        struct x64_block block = {.label = ir_block->label};
        abc_arr_init(&block.x64_instrs, sizeof(struct x64_instr), t->pool);
        t->curr_block = abc_arr_push(&t->curr_fun->x64_blocks, &block);
        x64_program_translate_block(t, ir_block);
    }

    // end
    create_epilogue(t, ir_fun->label);
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
                instr.tag = X64_INSTR_MOVQ;
                instr.val.mov.dst.tag = X64_ARG_STR;
                instr.val.mov.dst.val.str.str = ir_stmt->val.decl.label;
                instr.val.mov.src.tag = X64_ARG_REG;
                instr.val.mov.src.val.reg.reg = X64_REG_RAX;
                abc_arr_push(&t->curr_block->x64_instrs, &instr);
            }
            break;
        case IR_STMT_EXPR:
            x64_program_translate_expr(t, &ir_stmt->val.expr.expr);
            break;
        case IR_STMT_PRINT:
            // align
            instr.tag = X64_INSTR_PUSHQ;
            instr.val.push.src.tag = X64_ARG_REG;
            instr.val.push.src.val.reg.reg = X64_REG_RAX;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            // load fmt string
            instr.tag = X64_INSTR_LEAQ;
            instr.val.leaq.label = X64_FORMAT_STR_LABEL;
            instr.val.leaq.dest.tag = X64_ARG_REG;
            instr.val.leaq.dest.val.reg.reg = X64_REG_RDI;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            // move atom to rsi
            instr.tag = X64_INSTR_MOVQ;
            arg = x64_program_translate_atom(t, &ir_stmt->val.print.atom);
            instr.val.mov.src = arg;
            instr.val.mov.dst.tag = X64_ARG_REG;
            instr.val.mov.dst.val.reg.reg = X64_REG_RSI;
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            // issue call
            instr.tag = X64_INSTR_CALLQ;
            instr.val.callq.label = "printf";
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            // remove alignment
            instr.tag = X64_INSTR_POPQ;
            instr.val.pop.dest.tag = X64_ARG_REG;
            instr.val.pop.dest.val.reg.reg = X64_REG_RDI;
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
                instr.tag = X64_INSTR_MOVQ;
                instr.val.mov.dst.tag = X64_ARG_REG;
                instr.val.mov.dst.val.reg.reg = X64_REG_RAX;
                instr.val.mov.src = arg;
                abc_arr_push(&t->curr_block->x64_instrs, &instr);
            }
            instr.tag = X64_INSTR_JMP;
            instr.val.jmp.label = create_epilogue_label(t, t->curr_fun->label);
            abc_arr_push(&t->curr_block->x64_instrs, &instr);
            break;
        case IR_TAIL_IF:
            arg = x64_program_translate_atom(t, &ir_tail->val.if_then_else.atom);
            instr.tag = X64_INSTR_CMPQ;
            instr.val.cmp.left.tag = X64_ARG_IMM;
            instr.val.cmp.left.val.imm.imm = 1;
            instr.val.cmp.right = arg;
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
            instr.tag = X64_INSTR_MOVQ;
            instr.val.mov.dst.tag = X64_ARG_REG;
            instr.val.mov.dst.val.reg.reg = X64_REG_RAX;
            instr.val.mov.src = lhs;
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
            instr.tag = X64_INSTR_MOVQ;
            instr.val.mov.dst.tag = X64_ARG_STR;
            instr.val.mov.dst.val.str.str = expr->val.assign.label;
            instr.val.mov.src.tag = X64_ARG_REG;
            instr.val.mov.src.val.reg.reg = X64_REG_RAX;
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
        instr.tag = X64_INSTR_MOVQ;
        instr.val.mov.dst.tag = X64_ARG_REG;
        instr.val.mov.dst.val.reg.reg = X64_REG_RAX;
        instr.val.mov.src = lhs;
        abc_arr_push(&t->curr_block->x64_instrs, &instr);

        instr.tag = expr->op == IR_BIN_PLUS ? X64_INSTR_ADDQ : X64_INSTR_SUBQ;
        if (expr->op == IR_BIN_PLUS) {
            instr.val.add.src = rhs;
            instr.val.add.dst.tag = X64_ARG_REG;
            instr.val.add.dst.val.reg.reg = X64_REG_RAX;
        } else {
            instr.val.sub.src = rhs;
            instr.val.sub.dst.tag = X64_ARG_REG;
            instr.val.sub.dst.val.reg.reg = X64_REG_RAX;
        }
        abc_arr_push(&t->curr_block->x64_instrs, &instr);
        return;
    }
    instr.tag = X64_INSTR_MOVQ;
    instr.val.mov.src.tag = X64_ARG_IMM;
    instr.val.mov.src.val.imm.imm = 0;
    instr.val.mov.dst.tag = X64_ARG_REG;
    instr.val.mov.dst.val.reg.reg = X64_REG_RDX;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
    instr.tag = X64_INSTR_MOVQ;
    instr.val.mov.src = lhs;
    instr.val.mov.dst.tag = X64_ARG_REG;
    instr.val.mov.dst.val.reg.reg = X64_REG_RAX;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
    instr.tag = expr->op == IR_BIN_MUL ? X64_INSTR_IMULQ : X64_INSTR_IDIVQ;
    if (expr->op == IR_BIN_MUL) {
        instr.val.imul.mul = rhs;
    } else {
        instr.val.idiv.div = rhs;
    }
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
}

static void x64_program_translate_unary_expr(struct x64_translator *t, struct ir_expr_unary *expr) {
    struct x64_arg rhs = x64_program_translate_atom(t, &expr->atom);
    struct x64_instr instr;
    instr.tag = X64_INSTR_MOVQ;
    instr.val.mov.dst.tag = X64_ARG_REG;
    instr.val.mov.dst.val.reg.reg = X64_REG_RAX;
    instr.val.mov.src = rhs;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);

    if (expr->op == IR_UNARY_BANG) {
        instr.tag = X64_INSTR_XORQ;
        instr.val.xor.src.tag = X64_ARG_IMM;
        instr.val.xor.src.val.imm.imm = 1;
        instr.val.xor.dst.tag = X64_ARG_REG;
        instr.val.xor.dst.val.reg.reg = X64_REG_RAX;
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
    instr.tag = X64_INSTR_MOVQ;
    instr.val.mov.dst.tag = X64_ARG_REG;
    instr.val.mov.dst.val.reg.reg = X64_REG_RAX;
    instr.val.mov.src = lhs;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
    instr.tag = X64_INSTR_CMPQ;
    instr.val.mov.dst.tag = X64_ARG_REG;
    instr.val.mov.dst.val.reg.reg = X64_REG_RAX;
    instr.val.mov.src = rhs;
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
    instr.val.movzbq.dst.tag = X64_ARG_REG;
    instr.val.movzbq.dst.val.reg.reg = X64_REG_RAX;
    abc_arr_push(&t->curr_block->x64_instrs, &instr);
}

static void x64_program_translate_call_expr(struct x64_translator *t, struct ir_expr_call *expr) {

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

static void x64_program_print_indent(FILE *f) {
    fprintf(f, "    ");
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
        case X64_INSTR_ADDQ:
            fprintf(f, "addq ");
            x64_program_print_arg(&instr->val.add.src, f);
            fprintf(f, ", ");
            x64_program_print_arg(&instr->val.add.dst, f);
            break;
        case X64_INSTR_SUBQ:
            fprintf(f, "subq ");
            x64_program_print_arg(&instr->val.sub.src, f);
            fprintf(f, ", ");
            x64_program_print_arg(&instr->val.sub.dst, f);
            break;
        case X64_INSTR_IMULQ:
            fprintf(f, "imulq ");
            x64_program_print_arg(&instr->val.imul.mul, f);
            break;
        case X64_INSTR_IDIVQ:
            fprintf(f, "idivq ");
            x64_program_print_arg(&instr->val.idiv.div, f);
            break;
        case X64_INSTR_MOVQ:
            fprintf(f, "movq ");
            x64_program_print_arg(&instr->val.mov.src, f);
            fprintf(f, ", ");
            x64_program_print_arg(&instr->val.mov.dst, f);
            break;
        case X64_INSTR_PUSHQ:
            fprintf(f, "pushq ");
            x64_program_print_arg(&instr->val.push.src, f);
            break;
        case X64_INSTR_POPQ:
            fprintf(f, "popq ");
            x64_program_print_arg(&instr->val.pop.dest, f);
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
            fprintf(f, "jmp %s", instr->val.jmp.label);
            break;
        case X64_INSTR_CMPQ:
            fprintf(f, "cmpq ");
            x64_program_print_arg(&instr->val.cmp.left, f);
            fprintf(f, ", ");
            x64_program_print_arg(&instr->val.cmp.right, f);
            break;
        case X64_INSTR_JMPCC:
            fprintf(f, "jmp");
            x64_program_print_cc(instr->val.jmpcc.code, f);
            fprintf(f, " %s", instr->val.jmpcc.label);
            break;
        case X64_INSTR_CALLQ:
            fprintf(f, "callq %s", instr->val.callq.label);
            break;
        case X64_INSTR_RETQ:
            fprintf(f, "retq");
            break;
        case X64_INSTR_LEAVEQ:
            fprintf(f, "leaveq");
            break;
        case X64_INSTR_XORQ:
            fprintf(f, "xorq ");
            x64_program_print_arg(&instr->val.xor.src, f);
            fprintf(f, ", ");
            x64_program_print_arg(&instr->val.xor.dst, f);
            break;
        case X64_INSTR_MOVZBQ:
            fprintf(f, "movzbq $al, ");
            x64_program_print_arg(&instr->val.movzbq.dst, f);
            break;
        case X64_INSTR_SETCC:
            fprintf(f, "set");
            x64_program_print_cc(instr->val.setcc.code, f);
            fprintf(f, " $al");
            break;
    }
}

static void x64_program_print_block(struct x64_block *block, FILE *f) {
    fprintf(f, "%s:\n", block->label);
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
        struct x64_fun *fun = ((struct x64_fun *)prog->x64_funs.data) + i;
        fprintf(f, "%s:\n", fun->label);
        for (size_t j = 0; j < fun->x64_blocks.len; j++) {
            struct x64_block *block = ((struct x64_block *)fun->x64_blocks.data) + j;
            x64_program_print_block(block, f);
        }
        fprintf(f, "\n");
    }
}
