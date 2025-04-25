#include "abc_parser.h"

#include <assert.h>
#include <stdarg.h>

#include "abc_lexer.h"
#include "data/abc_arr.h"

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
static struct abc_expr *parse_expr_postfix(struct abc_parser *parser, struct abc_expr *lhs);
struct abc_expr *parse_infix_expr(struct abc_parser *parser, struct abc_expr *lhs, int precedence);

static void synchronize(struct abc_parser *parser) {
    struct abc_token token = abc_lexer_peek(parser->lexer);
    while (token.type != TOKEN_EOF && token.type != TOKEN_LBRACE) {
        abc_lexer_next_token(parser->lexer);
        token = abc_lexer_peek(parser->lexer);
    }
}

static void report_error(struct abc_parser *parser, int line, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Error at line %d:", line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    parser->has_error = true;
}

static bool match_token(struct abc_parser *parser, enum abc_token_type type) {
    struct abc_token token = abc_lexer_next_token(parser->lexer);
    if (token.type == type) {
        return true;
    }
    // TODO: get a str instead of the int value for the token type...
    report_error(parser, token.line, "expect %s got %s",
        abc_lexer_token_type_str(type), abc_lexer_token_type_str(token.type));
    return false;
}

void abc_parser_init(struct abc_parser *parser, struct abc_lexer *lexer) {
    parser->lexer = lexer;
    parser->has_error = false;
    parser->pool = abc_pool_create();
}

void abc_parser_destroy(struct abc_parser *parser) {
    abc_pool_destroy(parser->pool);
}

struct abc_program abc_parser_parse(struct abc_parser *parser) {
    struct abc_program program;
    abc_arr_init(&program.fun_decls, sizeof(struct abc_fun_decl), parser->pool);

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

/* FUNCTIONS */

static bool parse_fun_decl(struct abc_parser *parser, struct abc_fun_decl *fun_decl) {
    struct abc_token type_token = abc_lexer_next_token(parser->lexer);
    if (type_token.type != TOKEN_INT_TYPE && type_token.type != TOKEN_VOID_TYPE) {
        report_error(parser, type_token.line, "Expected int or void, got %s", type_token.lexeme);
        return false;
    }
    fun_decl->type = type_token.type == TOKEN_INT_TYPE ? ABC_TYPE_INT : ABC_TYPE_VOID;

    struct abc_token id_token = abc_lexer_next_token(parser->lexer);
    if (id_token.type != TOKEN_IDENTIFIER) {
        report_error(parser, id_token.line, "Expected an identifier after type, got %s", id_token.lexeme);
        return false;
    }
    fun_decl->name = id_token;

    if (!match_token(parser, TOKEN_LPAREN)) {
        return false;
    }

    struct abc_token tmp_token = abc_lexer_next_token(parser->lexer);
    bool has_err = false;
    abc_arr_init(&fun_decl->params, sizeof(struct abc_param), abc_pool_create());
    while (tmp_token.type != TOKEN_RPAREN) {
        struct abc_param param;
        if (tmp_token.type != TOKEN_INT_TYPE && tmp_token.type != TOKEN_VOID_TYPE) {
            report_error(parser, tmp_token.line, "Expected int or void, got %s", tmp_token.lexeme);
            has_err = true;
            break;
        }
        param.type = tmp_token.type == TOKEN_INT_TYPE ? ABC_TYPE_INT : ABC_TYPE_VOID;
        tmp_token = abc_lexer_next_token(parser->lexer);
        if (tmp_token.type != TOKEN_IDENTIFIER) {
            report_error(parser, tmp_token.line, "Expected an identifier, got %s", tmp_token.lexeme);
            has_err = true;
            break;
        }
        param.token = tmp_token;
        tmp_token = abc_lexer_next_token(parser->lexer);
        abc_arr_push(&fun_decl->params, &param);
        if (tmp_token.type == TOKEN_COMMA) {
            tmp_token = abc_lexer_next_token(parser->lexer);
        }
    }
    if (has_err || !parse_block_stmt(parser, &fun_decl->body)) {
        abc_pool_destroy(fun_decl->params.pool);
        return false;
    }

    abc_arr_migrate_pool(&fun_decl->params, parser->pool);
    return true;
}

/* DECLARATIONS */

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
    if (type_token.type != TOKEN_INT_TYPE && type_token.type != TOKEN_VOID_TYPE) {
        report_error(parser, type_token.line, "Expected int or void, got %s", type_token.lexeme);
        return false;
    }
    decl->val.var.type = type_token.type == TOKEN_INT_TYPE ? ABC_TYPE_INT : ABC_TYPE_VOID;

    struct abc_token id = abc_lexer_peek(parser->lexer);
    if (id.type != TOKEN_IDENTIFIER) {
        report_error(parser, id.line, "Expected an identifier, got %s", id.lexeme);
        return false;
    }
    abc_lexer_next_token(parser->lexer);
    decl->val.var.name = id;

    if (abc_lexer_peek(parser->lexer).type != TOKEN_EQUALS) {
        decl->val.var.has_init = false;
        if (!match_token(parser, TOKEN_SEMICOLON)) {
            return false;
        }
        return true;
    }
    match_token(parser, TOKEN_EQUALS);

    decl->val.var.has_init = true;
    decl->val.var.init = parse_expr(parser, 0);
    if (decl->val.var.init == NULL) {
        report_error(parser, id.line, "unable to parse var decl initializer");
        return false;
    }
    if (!match_token(parser, TOKEN_SEMICOLON)) {
        return false;
    }
    return true;
}

static bool parse_stmt_decl(struct abc_parser *parser, struct abc_decl *decl) {
    decl->tag = ABC_DECL_STMT;
    return parse_stmt(parser, &decl->val.stmt.stmt);
}

/* STATEMENTS */

static bool parse_stmt(struct abc_parser *parser, struct abc_stmt *stmt) {
    switch (abc_lexer_peek(parser->lexer).type) {
        case TOKEN_IF:
            stmt->tag = ABC_STMT_IF;
            return parse_if_stmt(parser, &stmt->val.if_stmt);
        case TOKEN_WHILE:
            stmt->tag = ABC_STMT_WHILE;
            return parse_while_stmt(parser, &stmt->val.while_stmt);
        case TOKEN_LBRACE:
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
        return false;
    }

    abc_arr_init(&block->decls, sizeof(struct abc_decl), abc_pool_create());
    struct abc_token token = abc_lexer_peek(parser->lexer);
    bool has_err = false;
    while (token.type != TOKEN_EOF && token.type != TOKEN_RBRACE) {
        struct abc_decl decl;
        if (!parse_decl(parser, &decl)) {
            report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse line in block statement");
            has_err = true;
            break;
        }
        abc_arr_push(&block->decls, &decl);
        token = abc_lexer_peek(parser->lexer);
    }
    if (has_err || !match_token(parser, TOKEN_RBRACE)) {
        abc_pool_destroy(block->decls.pool);
        return false;
    }
    abc_arr_migrate_pool(&block->decls, parser->pool);
    return true;
}

static bool parse_expr_stmt(struct abc_parser *parser, struct abc_expr_stmt *stmt) {
    stmt->expr = parse_expr(parser, 0);
    if (stmt->expr == NULL) {
        report_error(parser, abc_lexer_peek(parser->lexer).line, "unable to parse expression stmt");
        return false;
    }
    if (!match_token(parser, TOKEN_SEMICOLON)) {
        return false;
    }
    return true;
}

static bool parse_if_stmt(struct abc_parser *parser, struct abc_if_stmt *stmt) {
    if (!match_token(parser, TOKEN_IF)) {
        return false;
    }
    if (!match_token(parser, TOKEN_LPAREN)) {
        return false;
    }
    struct abc_expr *expr;
    if ((expr = parse_expr(parser, 0)) == NULL) {
        report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse if cond");
        return false;
    }
    stmt->cond = expr;
    if (!match_token(parser, TOKEN_RPAREN)) {
        return false;
    }

    struct abc_stmt *body = abc_stmt(parser->pool);
    if (!parse_stmt(parser, body)) {
        report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse if statement body");
        return false;
    }
    stmt->then_stmt = body;

    // check for else
    const struct abc_token token = abc_lexer_peek(parser->lexer);
    if (token.type != TOKEN_ELSE) {
        stmt->has_else = false;
        stmt->else_stmt = NULL;
        return true;
    }
    stmt->has_else = true;
    struct abc_stmt *else_stmt = abc_stmt(parser->pool);
    if (!parse_stmt(parser, else_stmt)) {
        report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse else statement body");
        return false;
    }
    stmt->else_stmt = else_stmt;
    return true;
}

static bool parse_while_stmt(struct abc_parser *parser, struct abc_while_stmt *stmt) {
    // while(cond)
    if (!match_token(parser, TOKEN_WHILE)) {
        return false;
    }
    if (!match_token(parser, TOKEN_LPAREN)) {
        return false;
    }
    struct abc_expr *cond;
    if ((cond = parse_expr(parser, 0)) == NULL) {
        report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse while cond");
        return false;
    }
    if (!match_token(parser, TOKEN_RPAREN)) {
        return false;
    }
    // body
    struct abc_stmt *body = abc_stmt(parser->pool);
    if (!parse_stmt(parser, body)) {
        report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse while body");
        return false;
    }

    stmt->cond = cond;
    stmt->body = body;
    return true;
}

static bool parse_print_stmt(struct abc_parser *parser, struct abc_print_stmt *stmt) {
    if (!match_token(parser, TOKEN_PRINT)) {
        return false;
    }
    if (!match_token(parser, TOKEN_LPAREN)) {
        return false;
    }
    struct abc_expr *expr;
    if ((expr = parse_expr(parser, 0)) == NULL) {
        report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse print stmt expr");
        return false;
    }
    if (!match_token(parser, TOKEN_RPAREN)) {
        return false;
    }
    if (!match_token(parser, TOKEN_SEMICOLON)) {
        return false;
    }
    stmt->expr = expr;
    return true;
}

static bool parse_return_stmt(struct abc_parser *parser, struct abc_return_stmt *stmt) {
    if (!match_token(parser, TOKEN_RETURN)) {
        return false;
    }
    const struct abc_token token = abc_lexer_peek(parser->lexer);
    if (token.type == TOKEN_SEMICOLON) {
        (void) match_token(parser, TOKEN_SEMICOLON);
        stmt->has_expr = false;
        stmt->expr = NULL;
        return true;
    }

    struct abc_expr *expr;
    if ((expr = parse_expr(parser, 0)) == NULL) {
        report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse return stmt expr");
        return false;
    }
    if (!match_token(parser, TOKEN_SEMICOLON)) {
        return false;
    }
    stmt->expr = expr;
    return true;
}

/* EXPRESSIONS */

// bigger number => higher precedence, 0 => not applicable.
static struct binding_power {
    int left;
    int right;
} binding_powers[TOKEN_EOF] = {
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
static int right_binding_powers[TOKEN_EOF] = {[TOKEN_LPAREN] = 16};

static struct abc_expr *parse_expr(struct abc_parser *parser, int precedence) {
    assert(precedence >= 0);
    struct abc_expr *lhs;
    if ((lhs = parse_expr_lhs(parser)) == NULL) {
        return NULL;
    }

    int right_bp;
    struct binding_power binding_power;
    while (1) {
        struct abc_token token = abc_lexer_peek(parser->lexer);
        if ((right_bp = right_binding_powers[token.type]) > 0) {
            if (right_bp < precedence) {
                break;
            }
            struct abc_expr *tmp = parse_expr_postfix(parser, lhs);
            if (tmp == NULL) {
                return NULL;
            }
            lhs = tmp;
            continue;
        }
        if ((binding_power = binding_powers[token.type]).left <= 0 || binding_power.left < precedence) {
            break;
        }
        struct abc_expr *tmp = parse_infix_expr(parser, lhs, binding_power.right);
        if (tmp == NULL) {
            report_error(parser, token.line, "failed to parse infix expr");
            return NULL;
        }
        lhs = tmp;
    }

    return lhs;
}

static struct abc_expr *parse_expr_lhs(struct abc_parser *parser) {
    struct abc_token token = abc_lexer_peek(parser->lexer);
    struct abc_expr *expr = abc_expr(parser->pool);
    switch (token.type) {
        case TOKEN_INT:
            expr->tag = ABC_EXPR_LITERAL;
            expr->val.lit_expr.lit.tag = ABC_LITERAL_INT;
            expr->val.lit_expr.lit.val.integer = (long) token.data;
            (void) match_token(parser, token.type);
            return expr;
        case TOKEN_IDENTIFIER:
            expr->tag = ABC_EXPR_LITERAL;
            expr->val.lit_expr.lit.tag = ABC_LITERAL_ID;
            expr->val.lit_expr.lit.val.identifier = token;
            (void) abc_lexer_next_token(parser->lexer);
            return expr;
        case TOKEN_LPAREN:
            (void) match_token(parser, token.type);
            expr->tag = ABC_EXPR_GROUPING;
            expr->val.grouping_expr.expr = parse_expr(parser, 0);
            if (expr->val.grouping_expr.expr == NULL) {
                report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse grouping expr");
                return NULL;
            }
            if (!match_token(parser, TOKEN_RPAREN)) {
                return NULL;
            }
            return expr;
        case TOKEN_BANG:
            // fall through
        case TOKEN_MINUS:
            expr->tag = ABC_EXPR_UNARY;
            expr->val.unary_expr.op = token;
            expr->val.unary_expr.expr = parse_expr(parser, left_binding_powers[token.type]);
            if (expr->val.unary_expr.expr == NULL) {
                (void) match_token(parser, token.type);
                report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse unary expr");
                return NULL;
            }
            (void) abc_lexer_next_token(parser->lexer);
            return expr;
        default:
            report_error(parser, token.line, "unexpected token to start expr: %s", token.lexeme);
            (void) match_token(parser, token.type);
            return NULL;
    }
    assert(0); // unreachable
    return NULL;
}

// lhs is freed on success, on failure it is not.
static struct abc_expr *parse_expr_postfix(struct abc_parser *parser, struct abc_expr *lhs) {
    // only function calls currently
    if (lhs->tag != ABC_EXPR_LITERAL || lhs->val.lit_expr.lit.tag != ABC_LITERAL_ID) {
        report_error(parser, abc_lexer_peek(parser->lexer).line, "expect identifier as func name");
        return NULL;
    }
    if (!match_token(parser, TOKEN_LPAREN)) {
        return NULL;
    }
    struct abc_token token = abc_lexer_peek(parser->lexer);
    struct abc_arr args;
    abc_arr_init(&args, sizeof(struct abc_expr *), abc_pool_create());
    bool has_err = false;
    while (token.type != TOKEN_RPAREN) {
        struct abc_expr *arg = parse_expr(parser, 0);
        if (arg == NULL) {
            has_err = true;
            break;
        }
        abc_arr_push(&args, &arg);
        token = abc_lexer_peek(parser->lexer);
        if (token.type == TOKEN_COMMA) {
            (void) match_token(parser, token.type);
            token = abc_lexer_peek(parser->lexer);
        }
    }

    if (!has_err) {
        (void) match_token(parser, TOKEN_RPAREN);
        struct abc_expr *res = abc_expr(parser->pool);
        abc_arr_migrate_pool(&args, parser->pool);
        res->tag = ABC_EXPR_CALL;
        res->val.call_expr.args = args;
        res->val.call_expr.callee = lhs->val.lit_expr.lit;
        return res;
    }
    // cleanup
    abc_pool_destroy(args.pool);
    return NULL;
}

// lhs might be freed on success, but never on error.
struct abc_expr *parse_infix_expr(struct abc_parser *parser, struct abc_expr *lhs, int precedence) {
    struct abc_token op = abc_lexer_next_token(parser->lexer);
    if (op.type == TOKEN_EQUALS) {
        // assign expr
        if (lhs->tag != ABC_EXPR_LITERAL || lhs->val.lit_expr.lit.tag != ABC_LITERAL_ID) {
            report_error(parser, abc_lexer_peek(parser->lexer).line, "expect identifier as lhs of assign");
            return NULL;
        }
        struct abc_expr *rhs = parse_expr(parser, precedence);
        if (rhs == NULL) {
            report_error(parser, abc_lexer_peek(parser->lexer).line, "failed to parse assign expr");
            return NULL;
        }
        struct abc_expr *res = abc_expr(parser->pool);
        res->tag = ABC_EXPR_ASSIGN;
        res->val.assign_expr.lit.tag = ABC_LITERAL_ID;
        res->val.assign_expr.lit.val.identifier = lhs->val.lit_expr.lit.val.identifier;
        res->val.assign_expr.expr = rhs;
        return res;
    }
    if (op.type == TOKEN_PLUS || op.type == TOKEN_MINUS || op.type == TOKEN_STAR || op.type == TOKEN_SLASH ||
        op.type == TOKEN_AND || op.type == TOKEN_OR || op.type == TOKEN_GREATER || op.type == TOKEN_GREATER_EQUALS ||
        op.type == TOKEN_LESS || op.type == TOKEN_LESS_EQUALS || op.type == TOKEN_EQUALS_EQUALS ||
        op.type == TOKEN_BANG_EQUALS) {
        // binary expr
        struct abc_expr *rhs = parse_expr(parser, precedence);
        if (!rhs) {
            report_error(parser, op.line, "failed to parse binary expr");
            return NULL;
        }
        struct abc_expr *res = abc_expr(parser->pool);
        res->tag = ABC_EXPR_BINARY;
        res->val.bin_expr.op = op;
        res->val.bin_expr.left = lhs;
        res->val.bin_expr.right = rhs;
        return res;
    }
    // invalid op
    report_error(parser, op.line, "unexpected binary expression operation token '%s'", op.lexeme);
    return NULL;
}

/* MISC */
static void print_stmt(struct abc_stmt *stmt, FILE *f, int indent);
static void print_expr(struct abc_expr *expr, FILE *f);

static void print_indent(int indent, FILE *f) {
    for (int i = 0; i < indent * 4; i++) {
        fputc(' ', f);
    }
}

static void print_decl(struct abc_decl *decl, FILE *f, int indent) {
    if (decl->tag == ABC_DECL_STMT) {
        print_stmt(&decl->val.stmt.stmt, f, indent);
    } else {
        struct abc_var_decl var_decl = decl->val.var;
        print_indent(indent, f);
        fprintf(f, "%s ", var_decl.type == ABC_TYPE_INT ? "int" : "void");
        fprintf(f, "%s", var_decl.name.lexeme);
        if (!var_decl.has_init) {
            fprintf(f, ";\n");
            return;
        }
        fprintf(f, " = ");
        print_expr(var_decl.init, f);
        fprintf(f, ";\n");
    }
}

static void print_block_stmt(struct abc_block_stmt *block_stmt, FILE *f, int indent) {
    fprintf(f, "{\n");
    for (size_t i = 0; i < block_stmt->decls.len; i++) {
        struct abc_decl decl = ((struct abc_decl *)block_stmt->decls.data)[i];
        print_decl(&decl, f, indent);
    }
    print_indent(indent - 1, f);
    fprintf(f, "}\n");
}

static void print_stmt(struct abc_stmt *stmt, FILE *f, int indent) {
    switch (stmt->tag) {
        case ABC_STMT_EXPR:
            print_indent(indent, f);
            print_expr(stmt->val.expr_stmt.expr, f);
            fprintf(f, ";\n");
            break;
        case ABC_STMT_IF:
            print_indent(indent, f);
            fprintf(f, "if (");
            print_expr(stmt->val.if_stmt.cond, f);
            fprintf(f, ")");
            print_stmt(stmt->val.if_stmt.then_stmt, f, indent);
            if (stmt->val.if_stmt.has_else) {
                print_indent(indent, f);
                fprintf(f, "else ");
                print_stmt(stmt->val.if_stmt.else_stmt, f, indent);
            }
            break;
        case ABC_STMT_WHILE:
            print_indent(indent, f);
            fprintf(f, "while (");
            print_expr(stmt->val.while_stmt.cond, f);
            fprintf(f, ")");
            print_stmt(stmt->val.while_stmt.body, f, indent + 1);
            break;
        case ABC_STMT_BLOCK:
            print_block_stmt(&stmt->val.block_stmt, f, indent + 1);
            break;
        case ABC_STMT_PRINT:
            print_indent(indent, f);
            fprintf(f, "print(");
            print_expr(stmt->val.print_stmt.expr, f);
            fprintf(f, ");\n");
            break;
        case ABC_STMT_RETURN:
            print_indent(indent, f);
            fprintf(f, "return");
            if (stmt->val.return_stmt.has_expr) {
                fprintf(f, " ");
                print_expr(stmt->val.return_stmt.expr, f);
            }
            fprintf(f, ";\n");
            break;
    }
}

static void print_expr(struct abc_expr *expr, FILE *f) {
    switch (expr->tag) {
        case ABC_EXPR_BINARY:
            fprintf(f, "(");
            print_expr(expr->val.bin_expr.left, f);
            fprintf(f, " %s ", expr->val.bin_expr.op.lexeme);
            print_expr(expr->val.bin_expr.right, f);
            fprintf(f, ")");
            break;
        case ABC_EXPR_UNARY:
            fprintf(f, "(");
            fprintf(f, "%s", expr->val.unary_expr.op.lexeme);
            print_expr(expr->val.unary_expr.expr, f);
            fprintf(f, ")");
            break;
        case ABC_EXPR_CALL:
            fprintf(f, "%s", expr->val.call_expr.callee.val.identifier.lexeme);
            fprintf(f, "(");
            for (size_t i = 0; i < expr->val.call_expr.args.len; i++) {
                struct abc_expr *arg = ((struct abc_expr **) expr->val.call_expr.args.data)[i];
                print_expr(arg, f);
                if (i < expr->val.call_expr.args.len - 1) {
                    fprintf(f, ", ");
                }
            }
            fprintf(f, ")");
            break;
        case ABC_EXPR_LITERAL:
            if (expr->val.lit_expr.lit.tag == ABC_LITERAL_INT) {
                fprintf(f, "%ld", expr->val.lit_expr.lit.val.integer);
            } else {
                fprintf(f, "%s", expr->val.lit_expr.lit.val.identifier.lexeme);
            }
            break;
        case ABC_EXPR_ASSIGN:
            fprintf(f, "(");
            fprintf(f, "%s", expr->val.assign_expr.lit.val.identifier.lexeme);
            fprintf(f, " = ");
            print_expr(expr->val.assign_expr.expr, f);
            fprintf(f, ")");
            break;
        case ABC_EXPR_GROUPING:
            fprintf(f, "(");
            print_expr(expr->val.grouping_expr.expr, f);
            fprintf(f, ")");
            break;
    }
}

static void print_fun_decl(struct abc_fun_decl *fun_decl, FILE *f) {
    fprintf(f, "%s ", fun_decl->type == ABC_TYPE_INT ? "int" : "void");
    fprintf(f, "%s(", fun_decl->name.lexeme);
    for (size_t i = 0; i < fun_decl->params.len; i++) {
        struct abc_param param = ((struct abc_param *)fun_decl->params.data)[i];
        fprintf(f, "%s ", param.type == ABC_TYPE_INT ? "int" : "void");
        fprintf(f, "%s", param.token.lexeme);
        if (i < fun_decl->params.len - 1) {
            fprintf(f, ", ");
        }
    }
    fprintf(f, ") ");
    print_block_stmt(&fun_decl->body, f, 1);
}

void abc_parser_print(struct abc_program *program, FILE *f) {
    for (size_t i = 0; i < program->fun_decls.len; i++) {
        struct abc_fun_decl fun_decl = ((struct abc_fun_decl *)program->fun_decls.data)[i];
        print_fun_decl(&fun_decl, f);
    }
}
