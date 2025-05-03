/**
 * Intermediate representation to make translation to X64 easier.
 */

#include "ir.h"

#include <assert.h>
#include <string.h>

static char *fun_label(struct ir_translator *tr, char *fun_name) {
    (void) tr;
    return fun_name;
}

static char *fun_inner_label(struct ir_translator *tr) {
    static char *curr_fun = NULL;
    static int counter = 0;

    if (curr_fun == NULL) {
        curr_fun = tr->curr_fun->label;
        counter = 0;
    } else if (strcmp(curr_fun, tr->curr_fun->label) != 0) {
        counter = 0;
        curr_fun = tr->curr_fun->label;
    }
    counter++;
    int len = snprintf(NULL, 0, "%s_lab_%d", curr_fun, counter);
    char *res = abc_pool_alloc(tr->pool, len + 1, 1);
    snprintf(res, len + 1, "%s_lab_%d", curr_fun, counter);
    return res;
}

static char *fun_var_label(struct ir_translator *tr) {
    static char *curr_fun = NULL;
    static int counter = 0;

    if (curr_fun == NULL) {
        curr_fun = tr->curr_fun->label;
        counter = 0;
    } else if (strcmp(curr_fun, tr->curr_fun->label) != 0) {
        counter = 0;
        curr_fun = tr->curr_fun->label;
    }
    int len = snprintf(NULL, 0, "%s_var_%d", curr_fun, counter);
    char *res = abc_pool_alloc(tr->pool, len + 1, 1);
    snprintf(res, len + 1, "%s_var_%d", curr_fun, counter++);
    return res;
}

static void insert_ir_var_data(struct ir_translator *tr, struct ir_var_data *data) {
    abc_arr_push(&tr->ir_vars, data);
}

static char *lookup_ir_var(struct ir_translator *tr, char *og_name) {
    for (size_t i = 0; i < tr->ir_vars.len; i++) {
        struct ir_var_data var_data = ((struct ir_var_data *) tr->ir_vars.data)[tr->ir_vars.len - (i + 1)];
        if (var_data.marker) {
            continue;
        }
        if (strcmp(og_name, var_data.original_name) == 0) {
            return var_data.label;
        }
    }
    assert(0);
}

static void push_ir_var_scope(struct ir_translator *tr) {
    struct ir_var_data data = {.marker = true};
    abc_arr_push(&tr->ir_vars, &data);
}

static void pop_ir_var_scope(struct ir_translator *tr) {
    for (size_t i = 0; i < tr->ir_vars.len; i++) {
        struct ir_var_data data = ((struct ir_var_data *) tr->ir_vars.data)[tr->ir_vars.len - (i + 1)];
        if (data.marker) {
            tr->ir_vars.len = tr->ir_vars.len - (i + 1);
            return;
        }
    }
    assert(0);
}

static char *lookup_ir_fun(struct ir_translator *tr, char *og_name) {
    for (size_t i = 0; i < tr->ir_funs.len; i++) {
        struct ir_fun_data ir_fun_data = ((struct ir_fun_data *) tr->ir_funs.data)[i];
        if (strcmp(og_name, ir_fun_data.original_name) == 0) {
            return ir_fun_data.label;
        }
    }
    assert(0);
}

void ir_translator_init(struct ir_translator *translator) {
    translator->pool = abc_pool_create();
    translator->curr_fun = NULL;
    translator->curr_block = NULL;
    translator->has_error = false;
    abc_arr_init(&translator->ir_funs, sizeof(struct ir_fun_data), translator->pool);
    abc_arr_init(&translator->ir_vars, sizeof(struct ir_var_data), translator->pool);
}

void ir_translator_destroy(struct ir_translator *translator) { abc_pool_destroy(translator->pool); }

static struct ir_fun init_ir_fun(struct ir_translator *tr, struct abc_fun_decl *fun_decl) {
    char *label = fun_label(tr, fun_decl->name.lexeme);
    struct ir_fun fun = {.label = label, .num_var_labels = 0, .type = (enum abc_type) fun_decl->type};
    abc_arr_init(&fun.args, sizeof(struct ir_param), tr->pool);
    abc_arr_init(&fun.blocks, sizeof(struct ir_block), tr->pool);
    struct ir_fun_data ir_fun_data = {.label = label, .original_name = fun_decl->name.lexeme};
    abc_arr_push(&tr->ir_funs, &ir_fun_data);

    tr->curr_fun = &fun; // hack for fun_var_label to work
    for (size_t i = 0; i < fun_decl->params.len; i++) {
        struct abc_param param = ((struct abc_param *) fun_decl->params.data)[i];
        struct ir_param ir_param = {.type = (enum abc_type) param.type};
        char *param_label = fun_var_label(tr);
        ir_param.label = param_label;
        abc_arr_push(&fun.args, &ir_param);

        struct ir_var_data ir_param_data = {.original_name = param.token.lexeme, .label = param_label, .marker = false};
        insert_ir_var_data(tr, &ir_param_data);
    }

    struct ir_block start_block = {.label = fun_inner_label(tr), .has_tail = false};
    tr->curr_fun = NULL;
    abc_arr_init(&start_block.stmts, sizeof(struct ir_stmt), tr->pool);
    tr->curr_block = abc_arr_push(&fun.blocks, &start_block);

    return fun;
}

static void ir_translate_fun(struct ir_translator *tr, struct abc_fun_decl *fun_decl, struct ir_fun *fun);
static void ir_translate_decl(struct ir_translator *tr, struct abc_decl *decl);
static void ir_translate_stmt(struct ir_translator *tr, struct abc_stmt *stmt);
static void ir_translate_block_stmt(struct ir_translator *tr, struct abc_block_stmt *block);
static void ir_translate_expr_stmt(struct ir_translator *tr, struct abc_expr_stmt *stmt);
static void ir_translate_if_stmt(struct ir_translator *tr, struct abc_if_stmt *stmt);
static void ir_translate_while_stmt(struct ir_translator *tr, struct abc_while_stmt *stmt);
static void ir_translate_print_stmt(struct ir_translator *tr, struct abc_print_stmt *stmt);
static void ir_translate_return_stmt(struct ir_translator *tr, struct abc_return_stmt *stmt);
struct ir_expr ir_translate_expr(struct ir_translator *translator, struct abc_expr *expr);
struct ir_atom ir_atomize_expr(struct ir_translator *translator, struct ir_expr *expr);

struct ir_program ir_translate(struct ir_translator *translator, struct abc_program *program) {
    struct ir_program ir_prog;
    abc_arr_init(&ir_prog.ir_funs, sizeof(struct ir_fun), translator->pool);
    for (size_t i = 0; i < program->fun_decls.len; i++) {
        translator->ir_vars.len = 0; // reset var list
        struct abc_fun_decl fun_decl = ((struct abc_fun_decl *) program->fun_decls.data)[i];
        struct ir_fun fun = init_ir_fun(translator, &fun_decl);
        translator->curr_fun = &fun;
        ir_translate_fun(translator, &fun_decl, &fun);
        abc_arr_push(&ir_prog.ir_funs, &fun);
        translator->curr_fun = NULL;
    }
    return ir_prog;
}

static void ir_translate_fun(struct ir_translator *tr, struct abc_fun_decl *fun_decl, struct ir_fun *fun) {
    // parameters and return type is handled in init_ir_fun
    ir_translate_block_stmt(tr, &fun_decl->body);
}

static void ir_translate_decl(struct ir_translator *tr, struct abc_decl *decl) {
    if (decl->tag != ABC_DECL_VAR) {
        ir_translate_stmt(tr, &decl->val.stmt.stmt);
        return;
    }
    char *label = fun_var_label(tr);
    struct ir_stmt_decl ir_decl = {
            .label = label, .type = (enum abc_type) decl->val.var.type, .has_init = decl->val.var.has_init};
    if (ir_decl.has_init) {
        struct ir_expr init = ir_translate_expr(tr, decl->val.var.init);
        ir_decl.init = init;
    }
    struct ir_stmt stmt = {.tag = IR_STMT_DECL, .val = {ir_decl}};
    abc_arr_push(&tr->curr_block->stmts, &stmt);
    // TODO: Update num vars for ir_fun

    // Update environment
    struct ir_var_data ir_var_data = {.label = label, .original_name = decl->val.var.name.lexeme, .marker = false};
    insert_ir_var_data(tr, &ir_var_data);
}

static void ir_translate_stmt(struct ir_translator *tr, struct abc_stmt *stmt) {
    switch (stmt->tag) {
        case ABC_STMT_EXPR:
            ir_translate_expr_stmt(tr, &stmt->val.expr_stmt);
            break;
        case ABC_STMT_IF:
            ir_translate_if_stmt(tr, &stmt->val.if_stmt);
            break;
        case ABC_STMT_WHILE:
            ir_translate_while_stmt(tr, &stmt->val.while_stmt);
            break;
        case ABC_STMT_BLOCK:
            ir_translate_block_stmt(tr, &stmt->val.block_stmt);
            break;
        case ABC_STMT_PRINT:
            ir_translate_print_stmt(tr, &stmt->val.print_stmt);
            break;
        case ABC_STMT_RETURN:
            ir_translate_return_stmt(tr, &stmt->val.return_stmt);
            break;
    }
}

static void ir_translate_pred(struct ir_translator *tr, struct abc_expr *pred, char *success, char *fail) {
    if (pred->tag == ABC_EXPR_BINARY && pred->val.bin_expr.op.type == TOKEN_AND) {
        char *new_success = fun_inner_label(tr);
        ir_translate_pred(tr, pred->val.bin_expr.left, new_success, fail);

        struct ir_block new_success_block = {.label = new_success, .has_tail = false};
        abc_arr_init(&new_success_block.stmts, sizeof(struct ir_stmt), tr->pool);
        tr->curr_block = abc_arr_push(&tr->curr_fun->blocks, &new_success_block);

        ir_translate_pred(tr, pred->val.bin_expr.right, success, fail);
    } else if (pred->tag == ABC_EXPR_BINARY && pred->val.bin_expr.op.type == TOKEN_OR) {
        char *new_fail = fun_inner_label(tr);
        ir_translate_pred(tr, pred->val.bin_expr.left, success, new_fail);
        struct ir_block new_fail_block = {.label = new_fail, .has_tail = false};
        abc_arr_init(&new_fail_block.stmts, sizeof(struct ir_stmt), tr->pool);
        tr->curr_block = abc_arr_push(&tr->curr_fun->blocks, &new_fail_block);
        ir_translate_pred(tr, pred->val.bin_expr.right, success, fail);
    } else {
        struct ir_expr expr;
        struct ir_atom atom;
        struct ir_tail tail;
        expr = ir_translate_expr(tr, pred);
        atom = ir_atomize_expr(tr, &expr);
        tail.tag = IR_TAIL_IF;
        tail.val.if_then_else.atom = atom;
        tail.val.if_then_else.then_label = success;
        tail.val.if_then_else.else_label = fail;
        tr->curr_block->has_tail = true;
        tr->curr_block->tail = tail;
    }
}

static void ir_translate_if_stmt(struct ir_translator *tr, struct abc_if_stmt *stmt) {
    // success -> then block, fail = else/continue block
    // set tail to continuation in whatever the current block is.
    char *cont_label = fun_inner_label(tr);
    char *then_label = fun_inner_label(tr);
    char *else_label = fun_inner_label(tr);

    // cond
    ir_translate_pred(tr, stmt->cond, then_label, else_label);

    // then
    struct ir_block then_block = {.label = then_label, .has_tail = false};
    abc_arr_init(&then_block.stmts, sizeof(struct ir_stmt), tr->pool);
    tr->curr_block = abc_arr_push(&tr->curr_fun->blocks, &then_block);
    ir_translate_stmt(tr, stmt->then_stmt);
    tr->curr_block->has_tail = true;
    tr->curr_block->tail = (struct ir_tail) {.tag = IR_TAIL_GOTO, .val.go_to.label = cont_label};

    // else
    struct ir_block else_block = {.label = else_label, .has_tail = false};
    abc_arr_init(&else_block.stmts, sizeof(struct ir_stmt), tr->pool);
    tr->curr_block = abc_arr_push(&tr->curr_fun->blocks, &else_block);
    if (stmt->has_else) {
        ir_translate_stmt(tr, stmt->then_stmt);
    }
    tr->curr_block->has_tail = true;
    tr->curr_block->tail = (struct ir_tail) {.tag = IR_TAIL_GOTO, .val.go_to.label = cont_label};

    // setup continuation
    struct ir_block cont = {.label = cont_label, .has_tail = false};
    abc_arr_init(&cont.stmts, sizeof(struct ir_stmt), tr->pool);
    tr->curr_block = abc_arr_push(&tr->curr_fun->blocks, &cont);
}

static void ir_translate_while_stmt(struct ir_translator *tr, struct abc_while_stmt *stmt) {
    // success -> back to while block, fail = continue block
    char *loop_start_label = fun_inner_label(tr);
    char *loop_body_label = fun_inner_label(tr);
    char *cont_label = fun_inner_label(tr);
    tr->curr_block->has_tail = true;
    tr->curr_block->tail.tag = IR_TAIL_GOTO;
    tr->curr_block->tail.val.go_to.label = loop_start_label;

    // cond
    struct ir_block loop = {.label = loop_start_label, .has_tail = false};
    abc_arr_init(&loop.stmts, sizeof(struct ir_stmt), tr->pool);
    tr->curr_block = abc_arr_push(&tr->curr_fun->blocks, &loop);
    ir_translate_pred(tr, stmt->cond, loop_body_label, cont_label);

    // body
    struct ir_block body = {.label = loop_body_label, .has_tail = false};
    abc_arr_init(&body.stmts, sizeof(struct ir_stmt), tr->pool);
    tr->curr_block = abc_arr_push(&tr->curr_fun->blocks, &body);
    ir_translate_stmt(tr, stmt->body);
    tr->curr_block->has_tail = true;
    tr->curr_block->tail.tag = IR_TAIL_GOTO;
    tr->curr_block->tail.val.go_to.label = loop_start_label;

    // cont
    struct ir_block cont = {.label = cont_label, .has_tail = false};
    abc_arr_init(&cont.stmts, sizeof(struct ir_stmt), tr->pool);
    tr->curr_block = abc_arr_push(&tr->curr_fun->blocks, &cont);
}

static void ir_translate_expr_stmt(struct ir_translator *tr, struct abc_expr_stmt *stmt) {
    struct ir_expr expr = ir_translate_expr(tr, stmt->expr);
    struct ir_stmt ir_stmt = {.tag = IR_STMT_EXPR, .val.expr = {expr}};
    abc_arr_push(&tr->curr_block->stmts, &ir_stmt);
}

static void ir_translate_print_stmt(struct ir_translator *tr, struct abc_print_stmt *stmt) {
    struct ir_expr expr = ir_translate_expr(tr, stmt->expr);
    struct ir_atom atom = ir_atomize_expr(tr, &expr);
    struct ir_stmt_print print = {.atom = atom};
    struct ir_stmt res = {.tag = IR_STMT_PRINT, .val.print = print};
    abc_arr_push(&tr->curr_block->stmts, &res);
}

static void ir_translate_return_stmt(struct ir_translator *tr, struct abc_return_stmt *stmt) {
    // TODO: this needs to be considered. Returns within a scope means that no further stmts for that block
    // can be executed. Maybe make sure that return is the last statement of a block in the typechecker?
    // could also be handled by creating an insert_ir_stmt function that checks if return has been
    // encountered, and if that case simply does not append any more statements.
    struct ir_tail_ret ret = {.has_atom = stmt->has_expr};
    struct ir_tail tail = {.tag = IR_TAIL_RET};
    if (ret.has_atom) {
        struct ir_expr expr = ir_translate_expr(tr, stmt->expr);
        struct ir_atom atom = ir_atomize_expr(tr, &expr);
        ret.atom = atom;
    }
    tr->curr_block->tail = tail;
    tr->curr_block->has_tail = true;
}

static void ir_translate_block_stmt(struct ir_translator *tr, struct abc_block_stmt *block) {
    push_ir_var_scope(tr);
    for (size_t i = 0; i < block->decls.len; i++) {
        struct abc_decl decl = ((struct abc_decl *) block->decls.data)[i];
        ir_translate_decl(tr, &decl);
    }
    pop_ir_var_scope(tr);
}

static enum ir_bin_op to_ir_bin_op(enum abc_token_type type) {
    switch (type) {
        case TOKEN_PLUS:
            return IR_BIN_PLUS;
        case TOKEN_MINUS:
            return IR_BIN_MINUS;
        case TOKEN_STAR:
            return IR_BIN_MUL;
        case TOKEN_SLASH:
            return IR_BIN_DIV;
        default:
            assert(0);
    }
}

static enum ir_cmp to_ir_cmp(enum abc_token_type type) {
    switch (type) {
        case TOKEN_EQUALS_EQUALS:
            return IR_CMP_EQ;
        case TOKEN_BANG_EQUALS:
            return IR_CMP_NE;
        case TOKEN_LESS:
            return IR_CMP_LT;
        case TOKEN_GREATER:
            return IR_CMP_GT;
        case TOKEN_GREATER_EQUALS:
            return IR_CMP_GE;
        case TOKEN_LESS_EQUALS:
            return IR_CMP_LE;
        default:
            assert(0);
    }
}

static struct ir_atom ir_translate_and_atomize_expr(struct ir_translator *tr, struct abc_expr *expr) {
    struct ir_expr ir_expr = ir_translate_expr(tr, expr);
    return ir_atomize_expr(tr, &ir_expr);
}

struct ir_expr ir_translate_expr(struct ir_translator *tr, struct abc_expr *expr) {
    // Short-circuiting logic for these is handled in translate_pred.
    // Since the only valid place for these is in if stmts/while stmts (typechecker), we only need to worry about
    // them there. This means that ir_translate_expr will never need to create a new basic block.
    assert(expr->tag != ABC_EXPR_BINARY || expr->val.bin_expr.op.type != TOKEN_AND);
    assert(expr->tag != ABC_EXPR_BINARY || expr->val.bin_expr.op.type != TOKEN_OR);

    struct ir_atom lhs;
    struct ir_atom rhs;
    struct ir_expr ir_expr = {.type = expr->type};
    struct ir_expr *ir_expr_ptr;
    struct ir_expr tmp;
    char *label;

    switch (expr->tag) {
        case ABC_EXPR_BINARY:
            lhs = ir_translate_and_atomize_expr(tr, expr->val.bin_expr.left);
            rhs = ir_translate_and_atomize_expr(tr, expr->val.bin_expr.right);
            if (expr->val.bin_expr.op.type == TOKEN_PLUS || expr->val.bin_expr.op.type == TOKEN_MINUS ||
                expr->val.bin_expr.op.type == TOKEN_STAR || expr->val.bin_expr.op.type == TOKEN_SLASH) {
                ir_expr.tag = IR_EXPR_BIN;
                ir_expr.val.bin.lhs = lhs;
                ir_expr.val.bin.rhs = rhs;
                ir_expr.val.bin.op = to_ir_bin_op(expr->val.bin_expr.op.type);
            } else {
                ir_expr.tag = IR_EXPR_CMP;
                ir_expr.val.cmp.lhs = lhs;
                ir_expr.val.cmp.rhs = rhs;
                ir_expr.val.cmp.cmp = to_ir_cmp(expr->val.bin_expr.op.type);
            }
            break;
        case ABC_EXPR_UNARY:
            ir_expr.tag = IR_EXPR_UNARY;
            ir_expr.val.unary.op = expr->val.unary_expr.op.type == TOKEN_BANG ? IR_UNARY_BANG : IR_UNARY_MINUS;
            lhs = ir_translate_and_atomize_expr(tr, expr->val.unary_expr.expr);
            ir_expr.val.unary.atom = lhs;
            break;
        case ABC_EXPR_CALL:
            ir_expr.tag = IR_EXPR_CALL;
            abc_arr_init(&ir_expr.val.call.args, sizeof(struct ir_param), tr->pool);
            for (size_t i = 0; i < expr->val.call_expr.args.len; i++) {
                struct abc_expr *arg = ((struct abc_expr **) expr->val.call_expr.args.data)[i];
                struct ir_atom atom = ir_translate_and_atomize_expr(tr, arg);
                abc_arr_push(&ir_expr.val.call.args, &atom);
            }
            label = lookup_ir_fun(tr, expr->val.call_expr.callee.val.identifier.lexeme);
            ir_expr.val.call.label = label;
            break;
        case ABC_EXPR_LITERAL:
            ir_expr.tag = IR_EXPR_ATOM;
            if (expr->val.lit_expr.lit.tag == ABC_LITERAL_INT) {
                ir_expr.val.atom.atom.tag = IR_ATOM_INT_LIT;
                ir_expr.val.atom.atom.val.int_lit = expr->val.lit_expr.lit.val.integer;
            } else {
                label = lookup_ir_var(tr, expr->val.lit_expr.lit.val.identifier.lexeme);
                ir_expr.val.atom.atom.tag = IR_ATOM_IDENTIFIER;
                ir_expr.val.atom.atom.val.label = label;
            }
            break;
        case ABC_EXPR_ASSIGN:
            ir_expr.tag = IR_EXPR_ASSIGN;
            tmp = ir_translate_expr(tr, expr->val.assign_expr.expr);
            ir_expr_ptr = abc_pool_alloc(tr->pool, sizeof(struct ir_expr), 1);
            *ir_expr_ptr = tmp;
            ir_expr.val.assign.value = ir_expr_ptr;
            label = lookup_ir_var(tr, expr->val.assign_expr.lit.val.identifier.lexeme);
            ir_expr.val.assign.label = label;
            break;
        case ABC_EXPR_GROUPING:
            return ir_translate_expr(tr, expr->val.grouping_expr.expr);
        default:
            assert(0);
    }

    return ir_expr;
}

struct ir_atom ir_atomize_expr(struct ir_translator *translator, struct ir_expr *expr) {
    if (expr->tag == IR_EXPR_ATOM) {
        return expr->val.atom.atom;
    }
    char *label = fun_var_label(translator);
    struct ir_stmt_decl ir_decl = {.has_init = true, .type = expr->type, .init = *expr, .label = label};
    struct ir_stmt stmt = {.tag = IR_STMT_DECL, .val = {ir_decl}};
    abc_arr_push(&translator->curr_block->stmts, &stmt);
    // TODO: Update num vars for ir_fun
    return (struct ir_atom) {.tag = IR_ATOM_IDENTIFIER, .val.label = label};
}

/* PRINTING */

static char *type_to_str(enum abc_type type) {
    switch (type) {
        case ABC_TYPE_VOID:
            return "void";
        case ABC_TYPE_INT:
            return "int";
        case ABC_TYPE_BOOL:
            return "bool";
        default:
            assert(0);
    }
}

static char *bin_op_to_str(enum ir_bin_op op) {
    switch (op) {
        case IR_BIN_PLUS:
            return "+";
        case IR_BIN_MINUS:
            return "-";
        case IR_BIN_MUL:
            return "*";
        case IR_BIN_DIV:
            return "/";
        default:
            assert(0);
    }
}

static char *cmp_to_str(enum ir_cmp op) {
    switch (op) {
        case IR_CMP_EQ:
            return "==";
        case IR_CMP_NE:
            return "!=";
        case IR_CMP_LT:
            return "<";
        case IR_CMP_GT:
            return ">";
        case IR_CMP_LE:
            return "<=";
        case IR_CMP_GE:
            return ">=";
        default:
            assert(0);
    }
}

static char *unary_to_str(enum ir_unary_op op) {
    switch (op) {
        case IR_UNARY_MINUS:
            return "-";
        case IR_UNARY_BANG:
            return "!";
        default:
            assert(0);
    }
}

static void ir_program_print_atom(struct ir_atom *atom, FILE *out) {
    switch (atom->tag) {
        case IR_ATOM_INT_LIT:
            fprintf(out, "%ld", atom->val.int_lit);
            break;
        case IR_ATOM_IDENTIFIER:
            fprintf(out, "%s", atom->val.label);
            break;
        default:
            assert(0);
    }
}

static void ir_program_print_expr(struct ir_expr *expr, FILE *out) {
    switch (expr->tag) {
        case IR_EXPR_BIN:
            ir_program_print_atom(&expr->val.bin.lhs, out);
            fprintf(out, " %s ", bin_op_to_str(expr->val.bin.op));
            ir_program_print_atom(&expr->val.bin.rhs, out);
            break;
        case IR_EXPR_UNARY:
            fprintf(out, "%s", unary_to_str(expr->val.unary.op));
            ir_program_print_atom(&expr->val.unary.atom, out);
            break;
        case IR_EXPR_ATOM:
            ir_program_print_atom(&expr->val.atom.atom, out);
            break;
        case IR_EXPR_CMP:
            ir_program_print_atom(&expr->val.cmp.lhs, out);
            fprintf(out, " %s ", cmp_to_str(expr->val.cmp.cmp));
            ir_program_print_atom(&expr->val.cmp.rhs, out);
            break;
        case IR_EXPR_CALL:
            fprintf(out, "%s(", expr->val.call.label);
            for (size_t i = 0; i < expr->val.call.args.len; i++) {
                struct ir_atom arg = ((struct ir_atom *) expr->val.call.args.data)[i];
                ir_program_print_atom(&arg, out);
                if (i < expr->val.call.args.len - 1) {
                    fprintf(out, ", ");
                }
            }
            fprintf(out, ")");
            break;
        case IR_EXPR_ASSIGN:
            fprintf(out, "%s = ", expr->val.assign.label);
            ir_program_print_expr(expr->val.assign.value, out);
            break;
    }
}

static void ir_program_print_stmt(struct ir_stmt *stmt, FILE *out) {
    switch (stmt->tag) {
        case IR_STMT_DECL:
            fprintf(out, "%s %s", type_to_str(stmt->val.decl.type), stmt->val.decl.label);
            if (stmt->val.decl.has_init) {
                fprintf(out, " = ");
                ir_program_print_expr(&stmt->val.decl.init, out);
            }
            fprintf(out, "\n");
            break;
        case IR_STMT_EXPR:
            ir_program_print_expr(&stmt->val.expr.expr, out);
            fprintf(out, "\n");
            break;
        case IR_STMT_PRINT:
            fprintf(out, "print ");
            ir_program_print_atom(&stmt->val.print.atom, out);
            fprintf(out, "\n");
            break;
    }
}

static void ir_program_print_block(struct ir_block *block, FILE *out) {
    fprintf(out, "%s:\n", block->label);
    for (size_t i = 0; i < block->stmts.len; i++) {
        struct ir_stmt stmt = ((struct ir_stmt *)block->stmts.data)[i];
        ir_program_print_stmt(&stmt, out);
    }
    if (!block->has_tail) {
        return;
    }
    switch (block->tail.tag) {
        case IR_TAIL_GOTO:
            fprintf(out, "goto %s\n", block->tail.val.go_to.label);
            break;
        case IR_TAIL_RET:
            fprintf(out, "return");
            if (block->tail.val.ret.has_atom) {
                ir_program_print_atom(&block->tail.val.ret.atom, out);
            }
            fprintf(out, "\n");
            break;
        case IR_TAIL_IF:
            fprintf(out, "if ");
            ir_program_print_atom(&block->tail.val.if_then_else.atom, out);
            fprintf(out, " goto %s else goto %s\n",
                block->tail.val.if_then_else.then_label, block->tail.val.if_then_else.else_label);
            break;
    }
}

void ir_program_print(struct ir_program *program, FILE *out) {
    for (size_t i = 0; i < program->ir_funs.len; i++) {
        struct ir_fun fun = ((struct ir_fun *)program->ir_funs.data)[i];
        fprintf(out, "%s (", fun.label);
        for (size_t j = 0; j < fun.args.len; j++) {
            struct ir_param param = ((struct ir_param *)fun.args.data)[j];
            fprintf(out, "%s %s ", type_to_str(param.type), param.label);
            if (j < fun.args.len - 1) {
                fprintf(out, ", ");
            }
        }
        fprintf(out, ") -> %s:\n", type_to_str(fun.type));

        for (size_t j = 0; j < fun.blocks.len; j++) {
            struct ir_block block = ((struct ir_block *)fun.blocks.data)[j];
            ir_program_print_block(&block, out);
        }
        fprintf(out, "\n");
    }
}
