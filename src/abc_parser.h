#ifndef ABC_PARSER_H
#define ABC_PARSER_H

#include <stdbool.h>

#include "data/abc_arr.h"
#include "abc_lexer.h"

// Forward declarations needed for cyclic references.
struct abc_expr;

// misc

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
        int integer;
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

// END EXPR

// BEGIN STMT

enum abc_stmt_tag {
    ABC_STMT_EXPR,
    ABC_STMT_IF,
    ABC_STMT_WHILE,
    ABC_STMT_BLOCK,
    ABC_STMT_RETURN
};

struct abc_expr_stmt {
    struct abc_expr expr;
};

struct abc_if_stmt {
    struct abc_expr cond;
    struct abc_stmt *then_stmt;
    bool has_else;
    struct abc_stmt *else_stmt;
};

struct abc_while_stmt {
    struct abc_expr cond;
    struct abc_stmt *then;
};

struct abc_block_stmt {
    struct abc_arr stmts;
};

struct abc_print_stmt {
    struct abc_expr expr;
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
	struct abc_lit init;
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

struct abc_program {
    // List of abc_fun_decl
    struct abc_arr fun_decls;
};

struct abc_fun_decl {
    enum abc_type type;
    struct abc_token name;
    // list of abc_param
    struct abc_arr params;
    // list of abc_decl
    struct abc_arr decls;
};

// END ROOT

struct abc_parser {
    struct abc_lexer *lexer;
    bool has_error;
};

void abc_parser_init(struct abc_parser *parser, struct abc_lexer *lexer);

struct abc_program abc_parser_parse(struct abc_parser *parser);

#endif //ABC_PARSER_H
