#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abc_lexer.h"
#include "abc_parser.h"
#include "abc_typechecker.h"
#include "codegen/ir.h"
#include "codegen/x64.h"

#define OUTPUT_FILE_MAX_LEN 100

struct compile_options {
    bool print_ast;
    bool print_ir;
    bool print_x64;
    bool skip_output;
    char *input_file;
    char *output_file;
};

void do_compile(struct compile_options *options);

void usage(void) {
    fprintf(stderr, "usage ./ablc <input_file.al> [--print-ast] [--print-ir] [--print-asm] "
                    "<--skip-output | --output outputfile>\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    struct compile_options compile_options = {0};
    struct option options[] = {{.flag = NULL, .val = 'a', .has_arg = false, .name = "print-ast"},
                               {.flag = NULL, .val = 'i', .has_arg = false, .name = "print-ir"},
                               {.flag = NULL, .val = 'x', .has_arg = false, .name = "print-asm"},
                               {.flag = NULL, .val = 's', .has_arg = false, .name = "skip-output"},
                               {.flag = NULL, .val = 'o', .has_arg = required_argument, .name = "output"},
                               {0, 0, 0, 0}};
    int c;
    while ((c = getopt_long(argc, argv, "aixso:", options, NULL)) != -1) {
        switch (c) {
            case 'a':
                compile_options.print_ast = true;
                break;
            case 'i':
                compile_options.print_ir = true;
                break;
            case 'x':
                compile_options.print_x64 = true;
                break;
            case 's':
                compile_options.skip_output = true;
                break;
            case 'o':
                compile_options.output_file = optarg;
                break;
            default:
                usage();
        }
    }
    if (optind >= argc || (compile_options.output_file == NULL && !compile_options.skip_output)) {
        usage();
    }
    compile_options.input_file = argv[optind];
    do_compile(&compile_options);

    return 0;
}

void do_compile(struct compile_options *options) {
    // parsing and lexing
    struct abc_lexer lexer;
    if (!abc_lexer_init(&lexer, options->input_file)) {
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
    if (options->print_ast) {
        abc_parser_print(&program, stdout);
    }

    // typechecker
    if (!abc_typechecker_typecheck(&program)) {
        fprintf(stderr, "typecheck failed, exiting\n");
        exit(EXIT_FAILURE);
    }
    if (parser.has_error || lexer.has_error) {
        fprintf(stderr, "skipping code generation because of parser/lexer errors\n");
        exit(EXIT_FAILURE);
    }

    // ir
    struct ir_translator ir_translator;
    ir_translator_init(&ir_translator);
    struct ir_program ir_program = ir_translate(&ir_translator, &program);
    if (options->print_ir) {
        ir_program_print(&ir_program, stdout);
    }

    // codegen
    struct x64_translator x64_translator;
    x64_translator_init(&x64_translator);
    struct x64_program x64_program = x64_translate(&x64_translator, &ir_program);
    if (options->print_x64) {
        x64_program_print(&x64_program, stdout);
    }
    if (!options->skip_output) {
        FILE *f = fopen(options->output_file, "w+");
        if (f == NULL) {
            fprintf(stderr, "failed to open output file\n");
            exit(EXIT_FAILURE);
        }
        x64_program_print(&x64_program, f);
        fclose(f);
    }

    abc_parser_destroy(&parser);
    abc_lexer_destroy(&lexer);
    ir_translator_destroy(&ir_translator);
    x64_translator_destroy(&x64_translator);
}
