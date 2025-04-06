#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abc_lexer.h"

#include <errno.h>
#include <limits.h>

static void max_munch(const struct abc_lexer *lexer, enum abc_token_type type, char start, struct abc_token *token);
static bool match(const struct abc_lexer *lexer, char c);
static void lex_int(struct abc_lexer *lexer, uint8_t start, struct abc_token *token);
static void lex_keyword_or_identifier(struct abc_lexer *lexer, uint8_t start, struct abc_token *token);

static char *my_strdup(const uint8_t *src, int len) {
    char *result = malloc(len + 1);
    if (!result) {
        perror(__FILE__ ": strndup");
        exit(EXIT_FAILURE);
    }
    strncpy(result, (char *) src, len);
    result[len] = '\0';
    return result;
}

void abc_lexer_token_free(struct abc_token *token) {
    free(token->lexeme);
}

void abc_lexer_destroy(struct abc_lexer *lexer) { fclose(lexer->file); }

bool abc_lexer_init(struct abc_lexer *lexer, const char *filename) {
    lexer->file = fopen(filename, "r");
    if (!lexer->file) {
        perror("Error opening file");
        lexer->has_error = true;
        return false;
    }
    lexer->line = 1;
    lexer->has_error = false;

    lexer->buf_pos = 0;

    lexer->has_peek = lexer->is_eof = false;

    return true;
}

struct abc_token abc_lexer_next_token(struct abc_lexer *lexer) {
    struct abc_token result = {0};
    uint8_t ch;
    int tmpch;

    if (lexer->is_eof) {
        result.type = TOKEN_EOF;
        result.line = lexer->line;
        return result;
    }
    if (lexer->has_peek) {
        lexer->has_peek = false;
        return lexer->peeked;
    }

    while ((tmpch = fgetc(lexer->file)) != EOF) {
        ch = (uint8_t) tmpch;
        lexer->buf_pos = 0;

        // Skip whitespace
        if (ch == '\n') {
            lexer->line++;
            continue;
        }
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            continue;
        }
        // Basic tokens
        if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
            ch == ',' || ch == ';') {
            result.line = lexer->line;
            result.lexeme = my_strdup(&ch, 1);
            if (ch == '+')
                result.type = TOKEN_PLUS;
            else if (ch == '-')
                result.type = TOKEN_MINUS;
            else if (ch == '*')
                result.type = TOKEN_STAR;
            else if (ch == '/')
                result.type = TOKEN_SLASH;
            else if (ch == '(')
                result.type = TOKEN_LPAREN;
            else if (ch == ')')
                result.type = TOKEN_RPAREN;
            else if (ch == '{')
                result.type = TOKEN_LBRACE;
            else if (ch == '}')
                result.type = TOKEN_RBRACE;
            else if (ch == ',')
                result.type = TOKEN_COMMA;
            else
                result.type = TOKEN_SEMICOLON;
            return result;
        }
        if (ch == '>') {
            max_munch(lexer, TOKEN_GREATER, '>', &result);
            return result;
        }
        if (ch == '<') {
            max_munch(lexer, TOKEN_LESS, '<', &result);
            return result;
        }
        if (ch == '=') {
            max_munch(lexer, TOKEN_EQUALS, '=', &result);
            return result;
        }
        if (ch == '!') {
            max_munch(lexer, TOKEN_BANG, '!', &result);
            return result;
        }
        if (ch >= '0' && ch <= '9') {
            lex_int(lexer, ch, &result);
            return result;
        }
        if (isalpha(ch)) {
            // keyword or identifier
            lex_keyword_or_identifier(lexer, ch, &result);
            return result;
        }
        result.type = TOKEN_ERROR;
        result.line = lexer->line;
        lexer->has_error = true;
        fprintf(stderr, "Failed to lex at line %d unexpected character: %c\n", lexer->line, ch);
        return result;
    }

    lexer->is_eof = true;
    result.line = lexer->line;
    result.type = TOKEN_EOF;
    return result;
}

struct abc_token abc_lexer_peek(struct abc_lexer *lexer) {
    if (lexer->has_peek) {
        return lexer->peeked;
    }
    struct abc_token res = lexer->peeked = abc_lexer_next_token(lexer);
    lexer->has_peek = true;
    return res;
}

static void max_munch(const struct abc_lexer *lexer, const enum abc_token_type type, const char start,
                      struct abc_token *token) {
    char buf[2] = {start};
    token->line = lexer->line;
    if (match(lexer, '=')) {
        buf[1] = '=';
        token->type = type + 1;
        token->lexeme = my_strdup((uint8_t *) buf, 2);
    } else {
        token->type = type;
        token->lexeme = my_strdup((uint8_t *) buf, 1);
    }
}

static bool match(const struct abc_lexer *lexer, const char c) {
    const int ch = fgetc(lexer->file);
    if (ch == c) {
        return true;
    }
    ungetc(ch, lexer->file);
    return false;
}

static void lex_int(struct abc_lexer *lexer, uint8_t start, struct abc_token *token) {
    int errnocpy = errno;
    int ch = -1;
    lexer->buf[lexer->buf_pos++] = start;
    while (ch = fgetc(lexer->file), lexer->buf_pos < ABC_LEXER_BUFFER_SIZE && isdigit(ch)) {
        lexer->buf[lexer->buf_pos++] = ch;
    }
    ungetc(ch, lexer->file);
    if (lexer->buf_pos == ABC_LEXER_BUFFER_SIZE - 1) {
        goto err;
    }
    lexer->buf[lexer->buf_pos] = '\0';


    errno = 0;
    char *endptr;
    long res = strtol((char *) lexer->buf, &endptr, 10);
    if (*endptr != '\0' || errno != 0) {
        goto err;
    }
    token->line = lexer->line;
    token->type = TOKEN_INT;
    token->lexeme = my_strdup(lexer->buf, lexer->buf_pos);
    token->data = (void *) res;
    errno = errnocpy;
    return;

err:
    lexer->has_error = true;
    token->type = TOKEN_ERROR;
    token->lexeme = my_strdup((uint8_t *)"invalid integer token", sizeof("invalid integer token") - 1);
    token->line = lexer->line;
    fprintf(stderr, "Failed to lex int at line %d\n", lexer->line);
    errno = errnocpy;
}

static void lex_keyword_or_identifier(struct abc_lexer *lexer, uint8_t start, struct abc_token *token) {
    int ch;
    lexer->buf[lexer->buf_pos++] = start;
    bool err = false;
    while ((ch = fgetc(lexer->file)) != EOF && isalnum(ch)) {
        if (lexer->buf_pos < ABC_LEXER_BUFFER_SIZE) {
            lexer->buf[lexer->buf_pos++] = ch;
        } else {
            err = true;
        }
    }
    ungetc(ch, lexer->file);

    token->line = lexer->line;
    if (err) {
        token->type = TOKEN_ERROR;
        token->lexeme = my_strdup(lexer->buf, lexer->buf_pos);
        lexer->has_error = true;
        fprintf(stderr, "Failed to lex keyword or identifier at line %d: %.*s\n", lexer->line, lexer->buf_pos,
                (char *) lexer->buf);
        return;
    }

    token->lexeme = my_strdup(lexer->buf, lexer->buf_pos);
    char *buf = (char *) lexer->buf;
    if (lexer->buf_pos == 2 && strncmp(buf, "if", lexer->buf_pos) == 0) {
        token->type = TOKEN_IF;
    } else if (lexer->buf_pos == 4 && strncmp(buf, "else", lexer->buf_pos) == 0) {
        token->type = TOKEN_ELSE;
    } else if (lexer->buf_pos == 5 && strncmp(buf, "while", lexer->buf_pos) == 0) {
        token->type = TOKEN_WHILE;
    } else if (lexer->buf_pos == 5 && strncmp(buf, "print", lexer->buf_pos) == 0) {
        token->type = TOKEN_PRINT;
    } else if (lexer->buf_pos == 3 && strncmp(buf, "int", lexer->buf_pos) == 0) {
        token->type = TOKEN_INT_TYPE;
    } else if (lexer->buf_pos == 4 && strncmp(buf, "void", lexer->buf_pos) == 0) {
        token->type = TOKEN_VOID_TYPE;
    } else if (lexer->buf_pos == 6 && strncmp(buf, "return", lexer->buf_pos) == 0) {
        token->type = TOKEN_RETURN;
    } else if (lexer->buf_pos == 3 && strncmp(buf, "and", lexer->buf_pos) == 0) {
        token->type = TOKEN_AND;
    } else if (lexer->buf_pos == 2 && strncmp(buf, "or", lexer->buf_pos) == 0) {
        token->type = TOKEN_OR;
    } else {
        token->type = TOKEN_IDENTIFIER;
    }
}
