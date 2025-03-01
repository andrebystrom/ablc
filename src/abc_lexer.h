#ifndef ABC_LEXER_H
#define ABC_LEXER_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "data/abc_arr.h"

#define ABC_LEXER_BUFFER_SIZE 1024

struct abc_lexer {
    FILE *file;
    int line;

    uint8_t buf[ABC_LEXER_BUFFER_SIZE];
    int buf_pos;

    bool has_error;
};

// Order for TOKEN_X, TOKEN_X_EQUALS matter for lexer.
enum abc_token_type {
    TOKEN_EQUALS,
    TOKEN_EQUALS_EQUALS,
    TOKEN_LESS,
    TOKEN_LESS_EQUALS,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUALS,
    TOKEN_BANG,
    TOKEN_BANG_EQUALS,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMICOLON,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_PRINT,
    TOKEN_RETURN,
    TOKEN_INT_TYPE,
    TOKEN_VOID_TYPE,
    TOKEN_INT,
    TOKEN_IDENTIFIER,
    TOKEN_ERROR,
    TOKEN_EOF
};

struct abc_token {
	enum abc_token_type type;
    int line;
    // Not present for TOKEN_ERROR
    char *lexeme;
    // For integers, the integer value is stored as a long.
    void *data;
};

bool abc_lexer_init(struct abc_lexer *lexer, const char *filename);

struct abc_token abc_lexer_next_token(struct abc_lexer *lexer);

void abc_lexer_token_free(struct abc_token *token);

void abc_lexer_destroy(struct abc_lexer *lexer);

#endif //ABC_LEXER_H
