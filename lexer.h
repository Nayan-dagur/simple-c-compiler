#ifndef LEXER_H
#define LEXER_H

#define MAX_TOKENS 2048
#define MAX_LEXEME_LEN 64

typedef enum {
    TOK_INT,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_PRINTF,
    TOK_IDENT,
    TOK_NUMBER,
    TOK_STRING,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MUL,
    TOK_DIV,
    TOK_ASSIGN,
    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMI,
    TOK_COMMA,
    TOK_EOF,
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[MAX_LEXEME_LEN];
    int value;
    int line;
} Token;

typedef struct {
    Token tokens[MAX_TOKENS];
    int count;
    int has_error;
    char error_msg[256];
} TokenList;

/* Scans source text and fills TokenList. Returns 1 on success, 0 on error. */
int tokenize(const char *src, TokenList *list);

/* Returns a readable string name for a token type. */
const char *token_type_name(TokenType type);

#endif
