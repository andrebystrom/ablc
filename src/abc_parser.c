/**
 * Error model is that only the 'root' allocation needs to be handled/freed by the caller, any allocations made
 * by the callee is freed by the callee on error. Only expressions, the branches of if statements, and the body of while
 * loops are heap allocated.
 */

#include "abc_parser.h"

#include <assert.h>

#include "abc_lexer.h"
#include "data/abc_arr.h"

// TODO: make sure to call token_free where appropriate
// TODO: move error handling to central function that sets error flag.

static bool parse_fun_decl(struct abc_parser *parser, struct abc_fun_decl *fun_decl);
static bool parse_decl(struct abc_parser *parser, struct abc_decl *decl);
static bool parse_var_decl(struct abc_parser *parser, struct abc_decl *decl);
static bool parse_stmt_decl(struct abc_parser *parser, struct abc_decl *decl);
static bool parse_stmt(struct abc_parser *parser, struct abc_stmt *stmt);
static bool parse_expr_stmt(struct abc_parser *parser, struct abc_expr_stmt *stmt);
static bool parse_if_stmt(struct abc_parser *parser, struct abc_if_stmt *stmt);
static bool parse_while_stmt(struct abc_parser *parser, struct abc_while_stmt *stmt);
static bool parse_block_stmt(struct abc_parser *parser, struct abc_block_stmt *block);
static bool parse_print_stmt(struct abc_parser *parser, struct abc_print_stmt *stmt);
static bool parse_return_stmt(struct abc_parser *parser, struct abc_return_stmt *stmt);
static struct abc_expr *parse_expr(struct abc_parser *parser, int precedence);
static struct abc_expr *parse_expr_lhs(struct abc_parser *parser);

static void synchronize(struct abc_parser *parser) {
    struct abc_token token = abc_lexer_peek(parser->lexer);
    while (token.type != TOKEN_EOF && token.type != TOKEN_LBRACE) {
        abc_lexer_next_token(parser->lexer);
        abc_lexer_token_free(&token);
        token = abc_lexer_peek(parser->lexer);
    }
}

static bool match_token(struct abc_parser *parser, enum abc_token_type type) {
    struct abc_token token = abc_lexer_next_token(parser->lexer);
    const enum abc_token_type tmp = token.type;
    abc_lexer_token_free(&token);
    return tmp == type;
}

void abc_parser_init(struct abc_parser *parser, struct abc_lexer *lexer) {
    parser->lexer = lexer;
    parser->has_error = false;
}

struct abc_program abc_parser_parse(struct abc_parser *parser) {
    struct abc_program program;
    abc_arr_init(&program.fun_decls, sizeof(struct abc_fun_decl));

    struct abc_token token;
    while ((token = abc_lexer_peek(parser->lexer)).type != TOKEN_EOF) {
        struct abc_fun_decl fun_decl;
        if (parse_fun_decl(parser, &fun_decl)) {
            abc_arr_push(&program.fun_decls, &fun_decl);
        } else {
            synchronize(parser);
        }
    }

    return program;
}

static bool parse_fun_decl(struct abc_parser *parser, struct abc_fun_decl *fun_decl) {
    abc_arr_init(&fun_decl->params, sizeof(struct abc_param));

    struct abc_token type_token = abc_lexer_next_token(parser->lexer);
    if (type_token.type != TOKEN_INT_TYPE && type_token.type != TOKEN_VOID_TYPE) {
        fprintf(stderr, "expected int or void, got %s\n", type_token.lexeme);
        parser->has_error = true;
        return false;
    }

    struct abc_token id_token = abc_lexer_next_token(parser->lexer);
    if (id_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "expected identifier, got %s\n", id_token.lexeme);
        parser->has_error = true;
        return false;
    }

    if (!match_token(parser, TOKEN_LPAREN)) {
        fprintf(stderr, "expected '(' after identifier");
        parser->has_error = true;
        return false;
    }

    struct abc_token tmp_token = abc_lexer_next_token(parser->lexer);
    while (tmp_token.type != TOKEN_RPAREN) {
        if (tmp_token.type != TOKEN_INT_TYPE) {
            fprintf(stderr, "expected int type in parameter list, got %s\n", tmp_token.lexeme);
            parser->has_error = true;
            return false;
        }
        tmp_token = abc_lexer_next_token(parser->lexer);
        if (tmp_token.type != TOKEN_IDENTIFIER) {
            fprintf(stderr, "expected identifier in parameter after type, got %s\n", tmp_token.lexeme);
            parser->has_error = true;
            return false;
        }
        tmp_token = abc_lexer_next_token(parser->lexer);
    }

    return parse_block_stmt(parser, &fun_decl->body);
}

static bool parse_decl(struct abc_parser *parser, struct abc_decl *decl) {
    struct abc_token token = abc_lexer_peek(parser->lexer);
    if (token.type == TOKEN_VOID_TYPE || token.type == TOKEN_INT_TYPE) {
        return parse_var_decl(parser, decl);
    }
    return parse_stmt_decl(parser, decl);
}

static bool parse_var_decl(struct abc_parser *parser, struct abc_decl *decl) {
    decl->tag = ABC_DECL_VAR;

    struct abc_token type_token = abc_lexer_next_token(parser->lexer);
    struct abc_token id = abc_lexer_peek(parser->lexer);
    if (id.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "expected identifier, got %s\n", id.lexeme);
        return false;
    }
    abc_lexer_next_token(parser->lexer);

    decl->val.var.type = type_token.type == TOKEN_INT_TYPE ? ABC_TYPE_INT : ABC_TYPE_VOID;
    decl->val.var.name = id;

    if (abc_lexer_peek(parser->lexer).type != TOKEN_EQUALS) {
        decl->val.var.has_init = false;
        return true;
    }
    abc_lexer_next_token(parser->lexer);

    decl->val.var.has_init = true;
    decl->val.var.init = parse_expr(parser, 0);
    if (decl->val.var.init == NULL) {
        parser->has_error = true;
        fprintf(stderr, "unable to parse var declaration initializer\n");
        return false;
    }
    return match_token(parser, TOKEN_SEMICOLON);
}

static bool parse_stmt_decl(struct abc_parser *parser, struct abc_decl *decl) {
    decl->tag = ABC_DECL_STMT;
    return parse_stmt(parser, &decl->val.stmt.stmt);
}


static bool parse_stmt(struct abc_parser *parser, struct abc_stmt *stmt) {
    switch (abc_lexer_peek(parser->lexer).type) {
        case TOKEN_IF:
            stmt->tag = ABC_STMT_IF;
            return parse_if_stmt(parser, &stmt->val.if_stmt);
        case TOKEN_WHILE:
            stmt->tag = ABC_STMT_WHILE;
            return parse_while_stmt(parser, &stmt->val.while_stmt);
        case TOKEN_RBRACE:
            stmt->tag = ABC_STMT_BLOCK;
            return parse_block_stmt(parser, &stmt->val.block_stmt);
        case TOKEN_PRINT:
            stmt->tag = ABC_STMT_PRINT;
            return parse_print_stmt(parser, &stmt->val.print_stmt);
        case TOKEN_RETURN:
            stmt->tag = ABC_STMT_RETURN;
            return parse_return_stmt(parser, &stmt->val.return_stmt);
        default:
            stmt->tag = ABC_STMT_EXPR;
            return parse_expr_stmt(parser, &stmt->val.expr_stmt);
    }
}

static bool parse_block_stmt(struct abc_parser *parser, struct abc_block_stmt *block) {
    if (!match_token(parser, TOKEN_LBRACE)) {
        fprintf(stderr, "expected '{' to start block statement\n");
        return false;
    }
    abc_arr_init(&block->decls, sizeof(struct abc_decl));

    struct abc_token token = abc_lexer_peek(parser->lexer);
    while (token.type != TOKEN_EOF && token.type != TOKEN_RBRACE) {
        struct abc_decl decl;
        if (!parse_decl(parser, &decl)) {
            fprintf(stderr, "failed to parse line in block statement\n");
            return false;
        }
        abc_arr_push(&block->decls, &decl);
        token = abc_lexer_peek(parser->lexer);
    }
    if (token.type != TOKEN_EOF) {
        abc_lexer_token_free(&token);
        return true;
    }
    fprintf(stderr, "expected '}' to end block statement\n");
    return false;
}

static bool parse_expr_stmt(struct abc_parser *parser, struct abc_expr_stmt *stmt) {
    stmt->expr = parse_expr(parser, 0);
    return stmt->expr != NULL;
}

static bool parse_if_stmt(struct abc_parser *parser, struct abc_if_stmt *stmt) {
    if (!match_token(parser, TOKEN_IF)) {
        fprintf(stderr, "expected 'if' to start if statement\n");
        return false;
    }
    if (!match_token(parser, TOKEN_LPAREN)) {
        fprintf(stderr, "expected '(' after 'if'\n");
        return false;
    }
    struct abc_expr *expr;
    if ((expr = parse_expr(parser, 0)) == NULL) {
        fprintf(stderr, "failed to parse if condition\n");
        return false;
    }
    stmt->cond = expr;
    if (!match_token(parser, TOKEN_RPAREN)) {
        free(expr);
        fprintf(stderr, "expected ')' after 'if' condition\n");
        return false;
    }

    struct abc_stmt *body = abc_stmt();
    if (!parse_stmt(parser, body)) {
        free(expr);
        free(body);
        fprintf(stderr, "failed to parse if statement body\n");
        return false;
    }
    stmt->then_stmt = body;

    // check for else
    const struct abc_token token = abc_lexer_peek(parser->lexer);
    if (token.type != TOKEN_ELSE) {
        stmt->has_else = false;
        stmt->then_stmt = NULL;
        return true;
    }
    stmt->has_else = true;
    struct abc_stmt *else_stmt = abc_stmt();
    if (!parse_stmt(parser, else_stmt)) {
        free(else_stmt);
        free(body);
        free(expr);
        fprintf(stderr, "failed to parse else statement body\n");
        return false;
    }
    stmt->else_stmt = else_stmt;

    return true;
}

static bool parse_while_stmt(struct abc_parser *parser, struct abc_while_stmt *stmt) {
    // while(cond)
    if (!match_token(parser, TOKEN_WHILE)) {
        fprintf(stderr, "expected 'while' to start while statement\n");
        return false;
    }
    if (!match_token(parser, TOKEN_LPAREN)) {
        fprintf(stderr, "expected '(' after 'while'\n");
        return false;
    }
    struct abc_expr *cond;
    if ((cond = parse_expr(parser, 0)) == NULL) {
        fprintf(stderr, "failed to parse while condition\n");
        return false;
    }
    if (!match_token(parser, TOKEN_RPAREN)) {
        free(cond);
        fprintf(stderr, "expected ')' after 'while' condition\n");
        return false;
    }
    // body
    struct abc_stmt *body = abc_stmt();
    if (!parse_stmt(parser, body)) {
        free(cond);
        free(body);
        fprintf(stderr, "failed to parse while statement body\n");
        return false;
    }

    stmt->cond = cond;
    stmt->body = body;
    return true;
}

static bool parse_print_stmt(struct abc_parser *parser, struct abc_print_stmt *stmt) {
    if (!match_token(parser, TOKEN_PRINT)) {
        fprintf(stderr, "expected 'print' to start print statement\n");
        return false;
    }
    if (!match_token(parser, TOKEN_LPAREN)) {
        fprintf(stderr, "expected '(' after 'print'\n");
        return false;
    }
    struct abc_expr *expr;
    if ((expr = parse_expr(parser, 0)) == NULL) {
        fprintf(stderr, "failed to parse print statement expr\n");
        return false;
    }
    if (!match_token(parser, TOKEN_RPAREN)) {
        free(expr);
        fprintf(stderr, "expected ')' after 'print'\n");
        return false;
    }
    if (!match_token(parser, TOKEN_SEMICOLON)) {
        free(expr);
        fprintf(stderr, "expected ';' after 'print'\n");
        return false;
    }
    stmt->expr = expr;
    return true;
}

static bool parse_return_stmt(struct abc_parser *parser, struct abc_return_stmt *stmt) {
    if (!match_token(parser, TOKEN_RETURN)) {
        fprintf(stderr, "expected 'return' to start return statement\n");
        return false;
    }
    const struct abc_token token = abc_lexer_peek(parser->lexer);
    if (token.type == TOKEN_SEMICOLON) {
        (void) match_token(parser, TOKEN_SEMICOLON);
        stmt->has_expr = false;
        stmt->expr = NULL;
    }

    struct abc_expr *expr;
    if ((expr = parse_expr(parser, 0)) == NULL) {
        fprintf(stderr, "failed to parse return statement expr\n");
        return false;
    }
    if (!match_token(parser, TOKEN_SEMICOLON)) {
        free(expr);
        fprintf(stderr, "expected ';' after 'return'\n");
        return false;
    }
    stmt->expr = expr;
    return true;
}

// bigger number => higher precedence, 0 => not applicable.
static struct binding_power {
    int left;
    int right;
} binding_powers[TOKEN_EOF] {
    [TOKEN_EQUALS] = {.left = 2, .right = 1},
    [TOKEN_OR] = {.left = 3, .right = 4},
    [TOKEN_AND] = {.left = 5, .right = 6},
    [TOKEN_EQUALS_EQUALS] = {.left = 7, .right = 8},
    [TOKEN_BANG_EQUALS] = {.left = 7, .right = 8},
    // Same for all comparisons
    [TOKEN_GREATER] = {.left = 9, .right = 10},
    [TOKEN_GREATER_EQUALS] = {.left = 9, .right = 10},
    [TOKEN_LESS] = {.left = 9, .right = 10},
    [TOKEN_LESS_EQUALS] = {.left = 9, .right = 10},

    [TOKEN_PLUS] = {.left = 11, .right = 12},
    [TOKEN_MINUS] = {.left = 11, .right = 12},
    [TOKEN_STAR] = {.left = 13, .right = 14},
    [TOKEN_SLASH] = {.left = 13, .right = 14},
};
static int left_binding_powers[TOKEN_EOF] = {[TOKEN_BANG] = 15, [TOKEN_MINUS] = 15};

static struct abc_expr *parse_expr(struct abc_parser *parser, int precedence) {
    assert(precedence >= 0);
    struct abc_expr *lhs;
    if ((lhs = parse_expr_lhs(parser)) == NULL) {
        return NULL;
    }

    // TODO LOOP

    // TODO check prefix (call)
    // TODO check infix

    return (void *)parser->has_error; // TODO tmp
}

static struct abc_expr *parse_expr_lhs(struct abc_parser *parser) {
    switch (token.type) {
        case TOKEN_INT:
        case TOKEN_IDENTIFIER:
            break;
        case TOKEN_LPAREN:
            break;
        case TOKEN_BANG:
        case TOKEN_MINUS:
            break;
        default:
            fprintf(stderr, "unexpected token to start expr: '%s'\n", token.lexeme);
            (void) match_token(parser, token.type);
            return NULL;
    }
    return NULL;
}
