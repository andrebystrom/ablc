#include "abc_parser.h"
#include "abc_lexer.h"
#include "data/abc_arr.h"

static bool parse_fun_decl(struct abc_parser *parser, struct abc_fun_decl *fun_decl);
static bool parse_block_stmt(struct abc_parser *parser, struct abc_block_stmt *block);

static void synchronize(struct abc_parser *parser) {
    struct abc_token token = abc_lexer_peek(parser->lexer);

    while(token.type != TOKEN_EOF && token.type != TOKEN_LBRACE) {
        abc_lexer_next_token(parser->lexer);
        token = abc_lexer_peek(parser->lexer);
    }
}

static bool match_token(struct abc_parser *parser, enum abc_token_type type) {
    struct abc_token token = abc_lexer_next_token(parser->lexer);
    return token.type == type;
}

void abc_parser_init(struct abc_parser *parser, struct abc_lexer *lexer) {
    parser->lexer = lexer;
    parser->has_error = false;
}

struct abc_program abc_parser_parse(struct abc_parser *parser) {
    struct abc_program program;
    abc_arr_init(&program.fun_decls, sizeof (struct abc_fun_decl));

    struct abc_token token;
    while((token = abc_lexer_peek(parser->lexer)).type != TOKEN_EOF) {
		struct abc_fun_decl fun_decl;
        if(parse_fun_decl(parser, &fun_decl)) {
            abc_arr_push(&program.fun_decls, &fun_decl);
        } else {
            synchronize(parser);
        }
    }

    return program;
}

static bool parse_fun_decl(struct abc_parser *parser, struct abc_fun_decl *fun_decl) {
    abc_arr_init(&fun_decl->params, sizeof (struct abc_param));

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

static bool parse_block_stmt(struct abc_parser *parser, struct abc_block_stmt *block) {
    return false;
}