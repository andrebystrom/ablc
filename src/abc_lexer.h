/***
 * abc_lexer.h
 *
 * Lex a file into tokens.
 */

#ifndef ABC_LEXER_H
#define ABC_LEXER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "data/abc_pool.h"

#define ABC_LEXER_BUFFER_SIZE 1024

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
    TOKEN_COMMA,
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
    TOKEN_EOF // this needs to be last, relied upon for lookup tables.
};

const char *abc_lexer_token_type_str(enum abc_token_type type);

struct abc_token {
    enum abc_token_type type;
    int line;
    // Not present for TOKEN_ERROR
    char *lexeme;
    // For integers, the integer value is stored as a long.
    void *data;
};

struct abc_lexer {
    FILE *file;
    int line;

    // Scratch area.
    uint8_t buf[ABC_LEXER_BUFFER_SIZE];
    int buf_pos;

    bool has_error;

    bool has_peek;
    struct abc_token peeked;

    bool is_eof;
    struct abc_pool *pool;
};

/**
 * Initializes the lexer.
 * @param lexer the lexer.
 * @param filename the file to lex.
 * @return true on success, false otherwise.
 */
bool abc_lexer_init(struct abc_lexer *lexer, const char *filename);

/**
 * Lexes the next token.
 * @param lexer the lexer.
 * @return a token, when EOF is reached the type is TOKEN_EOF.
 */
struct abc_token abc_lexer_next_token(struct abc_lexer *lexer);

/**
 * Peeks 1 token. Subsequent calls to peek will return the same token
 * until it is consumed by abc_lexer_next_token.
 * @param lexer the lexer.
 * @return the peeked token.
 */
struct abc_token abc_lexer_peek(struct abc_lexer *lexer);

/**
 * Destroys a token, called when ready to discard a token.
 * @param token the token.
 */
void abc_lexer_token_free(struct abc_token *token);

/**
 * Destroys the lexer, closing the file etc.
 * @param lexer the lexer to destroy.
 */
void abc_lexer_destroy(struct abc_lexer *lexer);

#endif // ABC_LEXER_H
