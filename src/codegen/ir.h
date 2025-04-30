#ifndef IR_H
#define IR_H

#include <stdbool.h>

#include "../data/abc_arr.h"
#include "../data/abc_pool.h"
#include "../abc_type.h"
#include "../abc_parser.h"

enum ir_cmp { IR_CMP_EQ, IR_CMP_NE, IR_CMP_LT, IR_CMP_GT, IR_CMP_LE, IR_CMP_GE };

enum ir_bin_op {
    IR_BIN_PLUS,
    IR_BIN_MINUS,
    IR_BIN_MUL,
    IR_BIN_DIV,
};

enum ir_unary_op {
    IR_UNARY_MINUS,
    IR_UNARY_BANG,
};

enum ir_atom_tag { IR_ATOM_INT_LIT, IR_ATOM_IDENTIFIER };

struct ir_atom {
    enum ir_atom_tag tag;
    union {
        long int_lit;
        char *label;
    } val;
};

enum ir_tail_tag { IR_TAIL_GOTO, IR_TAIL_RET, IR_TAIL_IF };

struct ir_tail_goto {
    char *label;
};

struct ir_tail_ret {
    bool has_atom;
    struct ir_atom atom;
};

struct ir_tail_if {
    struct ir_atom atom;
    char *then_label;
    char *else_label;
};

struct ir_tail {
    enum ir_tail_tag tag;
    union {
        struct ir_tail_goto go_to;
        struct ir_tail_ret ret;
        struct ir_tail_if if_then_else;
    } val;
};

enum ir_expr_tag { IR_EXPR_BIN, IR_EXPR_UNARY, IR_EXPR_ATOM, IR_EXPR_CMP, IR_EXPR_CALL, IR_EXPR_ASSIGN };

struct ir_expr_bin {
    struct ir_atom lhs;
    struct ir_atom rhs;
    enum ir_bin_op op;
};

struct ir_expr_unary {
    struct ir_atom atom;
    enum ir_unary_op op;
};

struct ir_expr_atom {
    struct ir_atom atom;
};

struct ir_expr_cmp {
    struct ir_atom lhs;
    struct ir_atom rhs;
    enum ir_cmp cmp;
};

struct ir_expr_call {
    char *label;
    struct abc_arr args; //ir_atom
};

struct ir_expr_assign {
    char *label; // assign to
    struct ir_expr *value;
};

struct ir_expr {
    enum ir_expr_tag tag;
    enum abc_type type;
    union {
        struct ir_expr_bin bin;
        struct ir_expr_unary unary;
        struct ir_expr_atom atom;
        struct ir_expr_cmp cmp;
        struct ir_expr_call call;
        struct ir_expr_assign assign;
    } val;
};

enum ir_stmt_tag { IR_STMT_DECL, IR_STMT_EXPR, IR_STMT_PRINT };

struct ir_stmt_decl {
    char *label;
    enum abc_type type;
    bool has_init;
    struct ir_expr init;
};

struct ir_stmt_expr {
    struct ir_expr expr;
};

struct ir_stmt_print {
    struct ir_atom atom;
};

struct ir_stmt {
    enum ir_stmt_tag tag;
    union {
        struct ir_stmt_decl decl;
        struct ir_stmt_expr expr;
        struct ir_stmt_print print;
    } val;
};

struct ir_block {
    char *label;
    struct abc_arr stmts; // ir_stmt
    struct ir_tail tail;
};

struct ir_param {
    char *label;
    enum abc_type type;
};

struct ir_fun {
    int num_var_labels;
    char *label;
    enum abc_type type;
    struct abc_arr args; // ir_param
    struct abc_arr blocks; // ir_block
};

struct ir_program {
    struct abc_arr ir_funs; // ir_fun
};

// TRANSLATOR

// Used to map variable names to their labels
// Marker is used to handle scopes during translation
struct ir_var_data {
    char *original_name;
    char *label;
    bool marker;
};

// Used to map function names to their labels.
struct ir_fun_data {
    char *original_name;
    char *label;
};

struct ir_translator {
    bool has_error;
    struct abc_arr ir_funs; // ir_fun_data
    struct abc_arr ir_vars; // ir_var_data, new for each function
    // label of the current function we are in
    char *curr_fun_label;
    // current (active) basic block of the function we are in
    struct ir_block *curr_block;
    struct abc_pool *pool;
};

void ir_translator_init(struct ir_translator *translator);
void ir_translator_destroy(struct ir_translator *translator);
struct ir_program ir_translate(struct ir_translator *translator, struct abc_program *program);
void ir_program_print(struct ir_program *program, FILE *out);

#endif // IR_H
