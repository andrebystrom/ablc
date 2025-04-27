#include "ir.h"

void ir_translator_init(struct ir_translator *translator) {
    translator->pool = abc_pool_create();
    translator->curr_fun_label = NULL;
    translator->curr_block = NULL;
    translator->has_error = false;
    abc_arr_init(&translator->ir_funs, sizeof(struct ir_fun_data), translator->pool);
    abc_arr_init(&translator->ir_params, sizeof(struct ir_param_data), translator->pool);
}

void ir_translator_destroy(struct ir_translator *translator) {
    abc_pool_destroy(translator->pool);
}

struct ir_program ir_translate(struct ir_translator *translator, struct abc_program *program) {
    return (struct ir_program) {0};
}

struct ir_atom translate_expr(struct ir_translator *translator, struct abc_expr *expr) {
    struct ir_block *curr_block = translator->curr_block;
}