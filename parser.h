#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef enum {
    AST_PROGRAM,
    AST_BLOCK,
    AST_VAR_DECL,
    AST_ASSIGN,
    AST_IF,
    AST_WHILE,
    AST_PRINTF,
    AST_BINOP,
    AST_UNARY_NEG,
    AST_NUMBER,
    AST_VAR
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    int line;

    /* Used by VAR_DECL, ASSIGN, VAR */
    char name[MAX_LEXEME_LEN];

    /* Used by NUMBER */
    int value;

    /* Used by BINOP (stores TokenType like TOK_PLUS, TOK_LT, etc.) */
    TokenType op;

    /* Child pointers used depending on node type */
    struct ASTNode *left;       /* BINOP left, UNARY operand, VAR_DECL/ASSIGN expr, PRINTF expr */
    struct ASTNode *right;      /* BINOP right */
    struct ASTNode *cond;       /* IF / WHILE condition */
    struct ASTNode *then_body;  /* IF then / WHILE body / BLOCK / PROGRAM first statement */
    struct ASTNode *else_body;  /* IF else (optional) */

    /* Next statement in a block or program */
    struct ASTNode *next;
} ASTNode;

typedef struct {
    const TokenList *tokens;
    int pos;
    int has_error;
    char error_msg[256];
} Parser;

/* Parses tokens into an AST_PROGRAM node. Returns NULL on syntax error. */
ASTNode *parse_program(const TokenList *tokens, char *error_out, int error_out_size);

/* Frees all nodes in an AST. */
void free_ast(ASTNode *node);

#endif
