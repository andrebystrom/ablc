#include "ir.h"

static char *fun_label(struct ir_translator *tr, char *fun_name) { return NULL; }

static char *fun_inner_label(struct ir_translator *tr) { return NULL; }

static char *fun_var_label(struct ir_translator *tr) { return NULL; }

static void insert_ir_var_data(struct ir_translator *tr, struct ir_var_data *data) {

}

static char *lookup_ir_var(struct ir_translator *tr, char *og_name) {
    return NULL;
}

static void push_ir_var_scope(struct ir_translator *tr, struct ir_var_data *data) {

}

static void pop_ir_var_scope(struct ir_translator *tr, struct ir_var_data *data) {

}

void ir_translator_init(struct ir_translator *translator) {
    translator->pool = abc_pool_create();
    translator->curr_fun_label = NULL;
    translator->curr_block = NULL;
    translator->has_error = false;
    abc_arr_init(&translator->ir_funs, sizeof(struct ir_fun_data), translator->pool);
}

void ir_translator_destroy(struct ir_translator *translator) { abc_pool_destroy(translator->pool); }

static struct ir_fun init_ir_fun(struct ir_translator *tr, struct abc_fun_decl *fun_decl) {
    char *label = fun_label(tr, fun_decl->name.lexeme);
    struct ir_fun fun = {.label = label, .num_var_labels = 0, .type = (enum abc_type) fun_decl->type};
    tr->curr_fun_label = label;
    abc_arr_init(&fun.args, sizeof(struct ir_param), tr->pool);
    abc_arr_init(&fun.blocks, sizeof(struct ir_block), tr->pool);
    struct ir_fun_data ir_fun_data = {.label = label, .original_name = fun_decl->name.lexeme};
    abc_arr_init(&ir_fun_data.var_data, sizeof(struct ir_var_data), tr->pool);
    abc_arr_push(&tr->ir_funs, &ir_fun_data);

    for (int i = 0; i < fun_decl->params.len; i++) {
        struct abc_param param = ((struct abc_param *) fun_decl->params.data)[i];
        struct ir_param ir_param = {.type = (enum abc_param) param.type};
        char *param_label = fun_var_label(tr);
        ir_param.label = param_label;
        abc_arr_push(&fun.args, &ir_param);
        struct ir_var_data ir_param_data = {
                .original_name = param.token.lexeme, .label = param_label, .marker = false};
        insert_ir_var_data(tr, &ir_param_data);
    }

    struct ir_block start_block = {.label = fun_inner_label(tr)};
    abc_arr_init(&start_block.stms, sizeof(struct ir_stmt), tr->pool);
    tr->curr_block = abc_arr_push(&fun.blocks, &start_block);

    return fun;
}

static void translate_ir_fun(struct ir_translator *tr, struct abc_fun_decl *fun_decl, struct ir_fun *fun);

struct ir_program ir_translate(struct ir_translator *translator, struct abc_program *program) {
    for (size_t i = 0; i < program->fun_decls.len; i++) {
        struct abc_fun_decl fun_decl = ((struct abc_fun_decl *) program->fun_decls.data)[i];
        struct ir_fun fun = init_ir_fun(translator, &fun_decl);
        translate_ir_fun(translator, &fun_decl, &fun);
        abc_arr_push(&translator->ir_funs, &fun);
    }
    return (struct ir_program) {.ir_funs = translator->ir_funs};
}

static void translate_ir_fun(struct ir_translator *tr, struct abc_fun_decl *fun_decl, struct ir_fun *fun) {
    // parameters and return type is handled in init_ir_fun
}

struct ir_atom translate_expr(struct ir_translator *translator, struct abc_expr *expr) {
    struct ir_block *curr_block = translator->curr_block;
}
