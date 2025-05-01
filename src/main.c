#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "abc_lexer.h"
#include "abc_parser.h"
#include "abc_typechecker.h"
#include "codegen/ir.h"
#include "codegen/x64.h"

void do_lex(char *file);
void do_parse(char *file);
void do_typecheck(char *file);
void do_ir(char *file);
void do_x64(char *file);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <compile | lex | parse> <file>\n", argv[0]);
    }
    else if (strcmp(argv[1], "compile") == 0) {
        // Do compile
    }
    else if (strcmp(argv[1], "lex") == 0) {
        do_lex(argv[2]);
    }
    else if (strcmp(argv[1], "parse") == 0) {
        do_parse(argv[2]);
    }
    else if (strcmp(argv[1], "typecheck") == 0) {
        do_typecheck(argv[2]);
    }
    else if (strcmp(argv[1], "ir") == 0) {
        do_ir(argv[2]);
    }
    else if (strcmp(argv[1], "x64") == 0) {
        do_x64(argv[2]);
    }
    return 0;
}

void do_lex(char *file) {
    struct abc_lexer lexer;
    if (!abc_lexer_init(&lexer, file)) {
        exit(EXIT_FAILURE);
    }

    struct abc_token token;
    while ((token = abc_lexer_next_token(&lexer)).type != TOKEN_EOF) {
        printf("Token = %d line = %d\n", token.type, token.line);
        printf("Lexeme = %s\n", token.lexeme);
        if (token.data != NULL) {
            printf("%d\n", (int) (long) token.data);
        }
        printf("\n");
    }
    printf("lexer err: %s\n", lexer.has_error ? "yes" : "no");
    abc_lexer_destroy(&lexer);
}

void do_parse(char *file) {
    struct abc_lexer lexer;
    if (!abc_lexer_init(&lexer, file)) {
        fprintf(stderr, "failed to init lexer\n");
        exit(EXIT_FAILURE);
    }
    struct abc_parser parser;
    abc_parser_init(&parser, &lexer);
    struct abc_program program = abc_parser_parse(&parser);
    if (parser.has_error) {
        fprintf(stderr, "failed to parse program\n");
        exit(EXIT_FAILURE);
    }
    printf("parsed %lu fun decls\n", program.fun_decls.len);
    abc_parser_print(&program, stdout);
    abc_parser_destroy(&parser);
    abc_lexer_destroy(&lexer);
}

void do_typecheck(char *file) {
    struct abc_lexer lexer;
    if (!abc_lexer_init(&lexer, file)) {
        fprintf(stderr, "failed to init lexer\n");
        exit(EXIT_FAILURE);
    }
    struct abc_parser parser;
    abc_parser_init(&parser, &lexer);
    struct abc_program program = abc_parser_parse(&parser);
    if (parser.has_error) {
        fprintf(stderr, "failed to parse program\n");
        exit(EXIT_FAILURE);
    }
    printf("parsed %lu fun decls\n", program.fun_decls.len);
    abc_parser_print(&program, stdout);

    bool tc_res = abc_typechecker_typecheck(&program);
    printf("typecheck %s\n", tc_res ? "OK" : "FAILED");

    abc_parser_destroy(&parser);
    abc_lexer_destroy(&lexer);
}

void do_ir(char *file) {
    struct abc_lexer lexer;
    if (!abc_lexer_init(&lexer, file)) {
        fprintf(stderr, "failed to init lexer\n");
        exit(EXIT_FAILURE);
    }
    struct abc_parser parser;
    abc_parser_init(&parser, &lexer);
    struct abc_program program = abc_parser_parse(&parser);
    if (parser.has_error) {
        fprintf(stderr, "failed to parse program\n");
        exit(EXIT_FAILURE);
    }
    printf("parsed %lu fun decls\n", program.fun_decls.len);
    // abc_parser_print(&program, stdout);

    bool tc_res = abc_typechecker_typecheck(&program);
    printf("typecheck %s\n", tc_res ? "OK" : "FAILED");

    struct ir_translator ir_translator;
    ir_translator_init(&ir_translator);
    struct ir_program ir_program = ir_translate(&ir_translator, &program);
    ir_program_print(&ir_program, stdout);

    abc_parser_destroy(&parser);
    abc_lexer_destroy(&lexer);
    ir_translator_destroy(&ir_translator);
}

void do_x64(char *file) {
    struct abc_lexer lexer;
    if (!abc_lexer_init(&lexer, file)) {
        fprintf(stderr, "failed to init lexer\n");
        exit(EXIT_FAILURE);
    }
    struct abc_parser parser;
    abc_parser_init(&parser, &lexer);
    struct abc_program program = abc_parser_parse(&parser);
    if (parser.has_error) {
        fprintf(stderr, "failed to parse program\n");
        exit(EXIT_FAILURE);
    }
    printf("parsed %lu fun decls\n", program.fun_decls.len);
    // abc_parser_print(&program, stdout);

    bool tc_res = abc_typechecker_typecheck(&program);
    printf("typecheck %s\n", tc_res ? "OK" : "FAILED");

    struct ir_translator ir_translator;
    ir_translator_init(&ir_translator);
    struct ir_program ir_program = ir_translate(&ir_translator, &program);
    ir_program_print(&ir_program, stdout);

    struct x64_translator x64_translator;
    x64_translator_init(&x64_translator);
    struct x64_program x64_program = x64_translate(&x64_translator, &ir_program);
    x64_program_print(&x64_program, stdout);


    abc_parser_destroy(&parser);
    abc_lexer_destroy(&lexer);
    ir_translator_destroy(&ir_translator);
    x64_translator_destroy(&x64_translator);
}
