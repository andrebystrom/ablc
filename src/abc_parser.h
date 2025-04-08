/**
 * parse tree where only expressions, the branches of if statements, and the body of while loops are heap
 * allocated.
*/
#ifndef ABC_PARSER_H
#define ABC_PARSER_H

#include <stdbool.h>

#include "data/abc_arr.h"
#include "abc_lexer.h"

// Forward declarations needed for cyclic references.
struct abc_expr;

// misc

static inline void *abc_malloc(const size_t size) {
    void *p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

enum abc_type {
    ABC_TYPE_VOID,
    ABC_TYPE_INT
};

struct abc_param {
    enum abc_type type;
    struct abc_token token;
};

// BEGIN LIT

enum abc_lit_tag {
    ABC_LITERAL_INT,
    ABC_LITERAL_ID
};

struct abc_lit {
	enum abc_lit_tag tag;
    union {
        struct abc_token identifier;
        long integer;
    } val;
};

// END LIT

// BEGIN EXPR

enum abc_expr_tag {
    ABC_EXPR_BINARY,
    ABC_EXPR_UNARY,
    ABC_EXPR_CALL,
    ABC_EXPR_LITERAL,
    ABC_EXPR_ASSIGN,
    ABC_EXPR_GROUPING
};

struct abc_bin_expr {
    struct abc_expr *left;
    struct abc_expr *right;
    struct abc_token op;
};

struct abc_unary_expr {
    struct abc_token op;
    struct abc_expr *expr;
};

struct abc_call_expr {
    struct abc_lit callee;
    struct abc_arr args;
};

struct abc_grouping_expr {
    struct abc_expr *expr;
};

struct abc_lit_expr {
    struct abc_lit lit;
};

struct abc_assign_expr {
    struct abc_lit lit;
    struct abc_expr *expr;
};

struct abc_expr {
    enum abc_expr_tag tag;
    union {
        struct abc_bin_expr bin_expr;
        struct abc_unary_expr unary_expr;
        struct abc_call_expr call_expr;
        struct abc_grouping_expr grouping_expr;
        struct abc_lit_expr lit_expr;
        struct abc_assign_expr assign_expr;
    } val;
};

static inline struct abc_expr *abc_expr(void) {
    return abc_malloc(sizeof(struct abc_expr));
}

void free_expr(struct abc_expr *expr);

static inline struct abc_expr *abc_bin_expr(struct abc_token op, struct abc_expr *left, struct abc_expr *right) {
    struct abc_expr *ret = abc_malloc(sizeof(struct abc_expr));
    ret->tag = ABC_EXPR_BINARY;
    ret->val.bin_expr.op = op;
    ret->val.bin_expr.left = left;
    ret->val.bin_expr.right = right;
    return ret;
}

static inline struct abc_expr *abc_unary_expr(struct abc_token op, struct abc_expr *expr) {
    struct abc_expr *ret = abc_malloc(sizeof(struct abc_expr));
    ret->tag = ABC_EXPR_UNARY;
    ret->val.unary_expr.op = op;
    ret->val.unary_expr.expr = expr;
    return ret;
}

static inline struct abc_expr *abc_call_expr(struct abc_lit callee, struct abc_arr args) {
    struct abc_expr *ret = abc_malloc(sizeof(struct abc_expr));
    ret->tag = ABC_EXPR_CALL;
    ret->val.call_expr.callee = callee;
    ret->val.call_expr.args = args;
    return ret;
}

static inline struct abc_expr *abc_grouping_expr(struct abc_expr *expr) {
    struct abc_expr *ret = abc_malloc(sizeof(struct abc_expr));
    ret->tag = ABC_EXPR_GROUPING;
    ret->val.grouping_expr.expr = expr;
    return ret;
}

static inline struct abc_expr *abc_lit_expr(struct abc_lit lit) {
    struct abc_expr *ret = abc_malloc(sizeof(struct abc_expr));
    ret->tag = ABC_EXPR_LITERAL;
    ret->val.lit_expr.lit = lit;
    return ret;
}

static inline struct abc_expr *abc_assign_expr(struct abc_lit lit, struct abc_expr *expr) {
    struct abc_expr *ret = abc_malloc(sizeof(struct abc_expr));
    ret->tag = ABC_EXPR_ASSIGN;
    ret->val.assign_expr.lit = lit;
    ret->val.assign_expr.expr = expr;
    return ret;
}

// END EXPR

// BEGIN STMT

enum abc_stmt_tag {
    ABC_STMT_EXPR,
    ABC_STMT_IF,
    ABC_STMT_WHILE,
    ABC_STMT_BLOCK,
    ABC_STMT_PRINT,
    ABC_STMT_RETURN
};

struct abc_expr_stmt {
    struct abc_expr *expr;
};

struct abc_if_stmt {
    struct abc_expr *cond;
    struct abc_stmt *then_stmt;
    bool has_else;
    struct abc_stmt *else_stmt;
};

struct abc_while_stmt {
    struct abc_expr *cond;
    struct abc_stmt *body;
};

struct abc_block_stmt {
    struct abc_arr decls;
};

struct abc_print_stmt {
    struct abc_expr *expr;
};

struct abc_return_stmt {
    bool has_expr;
    struct abc_expr *expr;
};

struct abc_stmt {
    enum abc_stmt_tag tag;
    union {
		struct abc_expr_stmt expr_stmt;
        struct abc_if_stmt if_stmt;
        struct abc_while_stmt while_stmt;
        struct abc_block_stmt block_stmt;
        struct abc_print_stmt print_stmt;
        struct abc_return_stmt return_stmt;
    } val;
};

static inline struct abc_stmt *abc_stmt(void) {
    return abc_malloc(sizeof(struct abc_stmt));
}

// END STMT

// BEGIN DECL

enum abc_decl_tag {
    ABC_DECL_VAR,
    ABC_DECL_STMT
};

struct abc_var_decl {
    enum abc_type type;
    struct abc_token name;
    bool has_init;
    // valid if has_init == true
	struct abc_expr *init;
};

struct abc_stmt_decl {
    struct abc_stmt stmt;
};

struct abc_decl {
	enum abc_decl_tag tag;
    union {
		struct abc_var_decl var;
		struct abc_stmt_decl stmt;
    } val;
};

// END DECL

// BEGIN ROOT
struct abc_fun_decl {
    enum abc_type type;
    struct abc_token name;
    // list of abc_param
    struct abc_arr params;
    struct abc_block_stmt body;
};

struct abc_program {
    // List of abc_fun_decl
    struct abc_arr fun_decls;
};

// END ROOT

struct abc_parser {
    struct abc_lexer *lexer;
    bool has_error;
    struct abc_pool *pool;
};

void abc_parser_init(struct abc_parser *parser, struct abc_lexer *lexer);

struct abc_program abc_parser_parse(struct abc_parser *parser);

#endif //ABC_PARSER_H
