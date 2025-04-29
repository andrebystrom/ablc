#include "ir.h"

#include <assert.h>

static char *fun_label(struct ir_translator *tr, char *fun_name) { return NULL; }

static char *fun_inner_label(struct ir_translator *tr) { return NULL; }

static char *fun_var_label(struct ir_translator *tr) { return NULL; }

static void insert_ir_var_data(struct ir_translator *tr, struct ir_var_data *data) {}

static char *lookup_ir_var(struct ir_translator *tr, char *og_name) { return NULL; }

static void push_ir_var_scope(struct ir_translator *tr) {}

static void pop_ir_var_scope(struct ir_translator *tr) {}

void ir_translator_init(struct ir_translator *translator) {
    translator->pool = abc_pool_create();
    translator->curr_fun_label = NULL;
    translator->curr_block = NULL;
    translator->has_error = false;
    abc_arr_init(&translator->ir_funs, sizeof(struct ir_fun_data), translator->pool);
    abc_arr_init(&translator->ir_vars, sizeof(struct ir_var_data), translator->pool);
}

void ir_translator_destroy(struct ir_translator *translator) { abc_pool_destroy(translator->pool); }

static struct ir_fun init_ir_fun(struct ir_translator *tr, struct abc_fun_decl *fun_decl) {
    char *label = fun_label(tr, fun_decl->name.lexeme);
    struct ir_fun fun = {.label = label, .num_var_labels = 0, .type = (enum abc_type) fun_decl->type};
    tr->curr_fun_label = label;
    abc_arr_init(&fun.args, sizeof(struct ir_param), tr->pool);
    abc_arr_init(&fun.blocks, sizeof(struct ir_block), tr->pool);
    struct ir_fun_data ir_fun_data = {.label = label, .original_name = fun_decl->name.lexeme};
    abc_arr_push(&tr->ir_funs, &ir_fun_data);

    for (int i = 0; i < fun_decl->params.len; i++) {
        struct abc_param param = ((struct abc_param *) fun_decl->params.data)[i];
        struct ir_param ir_param = {.type = (enum abc_param) param.type};
        char *param_label = fun_var_label(tr);
        ir_param.label = param_label;
        abc_arr_push(&fun.args, &ir_param);
        struct ir_var_data ir_param_data = {.original_name = param.token.lexeme, .label = param_label, .marker = false};
        insert_ir_var_data(tr, &ir_param_data);
    }

    struct ir_block start_block = {.label = fun_inner_label(tr)};
    abc_arr_init(&start_block.stmts, sizeof(struct ir_stmt), tr->pool);
    tr->curr_block = abc_arr_push(&fun.blocks, &start_block);

    return fun;
}

static void ir_translate_fun(struct ir_translator *tr, struct abc_fun_decl *fun_decl, struct ir_fun *fun);
static void ir_translate_decl(struct ir_translator *tr, struct abc_decl *decl);
static void ir_translate_stmt(struct ir_translator *tr, struct abc_stmt *stmt);
static void ir_translate_block_stmt(struct ir_translator *tr, struct abc_block_stmt *block);
struct ir_expr ir_translate_expr(struct ir_translator *translator, struct abc_expr *expr);
struct ir_atom ir_atomize_expr(struct ir_translator *translator, struct abc_expr *expr);

struct ir_program ir_translate(struct ir_translator *translator, struct abc_program *program) {
    for (size_t i = 0; i < program->fun_decls.len; i++) {
        translator->ir_vars.len = 0; // reset var list
        struct abc_fun_decl fun_decl = ((struct abc_fun_decl *) program->fun_decls.data)[i];
        struct ir_fun fun = init_ir_fun(translator, &fun_decl);
        ir_translate_fun(translator, &fun_decl, &fun);
        abc_arr_push(&translator->ir_funs, &fun);
    }
    return (struct ir_program) {.ir_funs = translator->ir_funs};
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
    struct ir_stmt stmt = {.tag = IR_STMT_DECL, .val = ir_decl};
    abc_arr_push(&tr->curr_block->stmts, &stmt);

    // Update environment
    struct ir_var_data ir_var_data = {.label = label, .original_name = decl->val.var.name.lexeme, .marker = false};
    insert_ir_var_data(tr, &ir_var_data);
}

static void ir_translate_stmt(struct ir_translator *tr, struct abc_stmt *stmt) {
    switch (stmt->tag) {
        case ABC_STMT_EXPR:
            break;
        case ABC_STMT_IF:
            break;
        case ABC_STMT_WHILE:
            break;
        case ABC_STMT_BLOCK:
            ir_translate_block_stmt(tr, &stmt->val.block_stmt);
            break;
        case ABC_STMT_PRINT:
            break;
        case ABC_STMT_RETURN:
            break;
    }
}

static void ir_translate_block_stmt(struct ir_translator *tr, struct abc_block_stmt *block) {
    push_ir_var_scope(tr);
    for (size_t i = 0; i < block->decls.len; i++) {
        struct abc_decl decl = ((struct abc_decl *) block->decls.data)[i];
        ir_translate_decl(tr, &decl);
    }
    pop_ir_var_scope(tr);
}

struct ir_expr ir_translate_expr(struct ir_translator *translator, struct abc_expr *expr) {
    return (struct ir_expr) {0};
}

enum ir_bin_op to_ir_bin_op(enum abc_token_type type) {
    switch (type) {
        case TOKEN_PLUS:
            return IR_BIN_PLUS;
        case TOKEN_MINUS:
            return IR_BIN_MINUS;
        case TOKEN_STAR:
            return IR_BIN_MUL;
        case TOKEN_SLASH:
            return IR_BIN_DIV;
    }
    assert(0);
}

enum ir_bin_op to_ir_cmp(enum abc_token_type type) {
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

struct ir_atom ir_atomize_expr(struct ir_translator *translator, struct abc_expr *expr) {
    // Short-circuiting logic for these is handled in translate_pred.
    // Since the only valid place for these is in if stmts/while stmts (typechecker), we only need to worry about
    // them there. This means that ir_translate_expr will never need to create a new basic block.
    assert(expr->tag != ABC_EXPR_BINARY || expr->val.bin_expr.op.type != TOKEN_AND);
    assert(expr->tag != ABC_EXPR_BINARY || expr->val.bin_expr.op.type != TOKEN_OR);

    // TODO
    // 1. translate to atomic ir_expr that is heap allocated
    // 2. create decl for new variable (atom) to equal the atomic ir_expr
    // 3. insert the decl into the block
    // 4. return the atom
    struct ir_atom lhs;
    struct ir_atom rhs;
    struct ir_expr *ir_expr = abc_pool_alloc(translator->pool, sizeof(struct ir_expr), 1);
    ir_expr->type = expr->type;

    switch (expr->tag) {
        case ABC_EXPR_BINARY:
            lhs = ir_atomize_expr(translator, expr->val.bin_expr.left);
            rhs = ir_atomize_expr(translator, expr->val.bin_expr.right);
            if (expr->val.bin_expr.op.type == TOKEN_PLUS || expr->val.bin_expr.op.type == TOKEN_MINUS ||
                expr->val.bin_expr.op.type == TOKEN_STAR || expr->val.bin_expr.op.type == TOKEN_SLASH) {
                ir_expr->tag = IR_EXPR_BIN;
                ir_expr->val.bin.lhs = lhs;
                ir_expr->val.bin.rhs = rhs;
                ir_expr->val.bin.op = to_ir_bin_op(expr->val.bin_expr.op.type);
            } else {
                ir_expr->tag = IR_EXPR_CMP;
                ir_expr->val.cmp.lhs = lhs;
                ir_expr->val.cmp.rhs = rhs;
                ir_expr->val.cmp.cmp = to_ir_cmp(expr->val.bin_expr.op.type);
            }
            break;
        case ABC_EXPR_UNARY:
            ir_expr->tag = IR_EXPR_UNARY;
            ir_expr->val.unary.op = expr->val.unary_expr.op.type == TOKEN_BANG ? IR_UNARY_BANG : IR_UNARY_MINUS;
            lhs = ir_atomize_expr(translator, expr->val.unary_expr.expr);
            ir_expr->val.unary.atom = lhs;
            break;
        case ABC_EXPR_CALL:
            break;
        case ABC_EXPR_LITERAL:
            break;
        case ABC_EXPR_ASSIGN:
            break;
        case ABC_EXPR_GROUPING:
            break;
        default:
            assert(0);
    }


    return (struct ir_atom) {0};
}
