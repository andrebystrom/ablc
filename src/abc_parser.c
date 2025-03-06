#include "abc_parser.h"
#include "abc_lexer.h"
#include "data/abc_arr.h"

static struct abc_fun_decl parse_fun_decl(struct abc_parser *parser);

void abc_parser_init(struct abc_parser *parser, struct abc_lexer *lexer) {
    parser->lexer = lexer;
    parser->has_error = false;
}

struct abc_program abc_parser_parse(struct abc_parser *parser) {
    struct abc_program program;
    abc_arr_init(&program.fun_decls, sizeof (struct abc_fun_decl));

    struct abc_token token;
    while((token = abc_lexer_peek(parser->lexer)).type != TOKEN_EOF) {

    }

    return program;
}

static struct abc_fun_decl parse_fun_decl(struct abc_parser *parser) {
    struct abc_fun_decl fun_decl;
    abc_arr_init(&fun_decl.params, sizeof (struct abc_param));
    abc_arr_init(&fun_decl.decls, sizeof (struct abc_fun_decl));

    return fun_decl;
}
