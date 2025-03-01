#include <stdio.h>
#include <string.h>

#include "abc_lexer.h"

void doLex(char *file);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <compile | lex | parse> <file>\n", argv[0]);
    }
    else if (strcmp(argv[1], "compile") == 0) {
        // Do compile
    }
    else if (strcmp(argv[1], "lex") == 0) {
        doLex(argv[2]);
    }
    return 0;
}

void doLex(char *file) {
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
        abc_lexer_token_free(&token);
    }
    printf("lexer err: %s\n", lexer.has_error ? "yes" : "no");
    abc_lexer_destroy(&lexer);
}
