#ifndef COMPILER_H
#define COMPILER_H

#include "lexer.h"
#include "parser.h"

#define MAX_VARS 256
#define MAX_TAC 2048
#define MAX_TEXT_BUF 16384

typedef struct {
    char names[MAX_VARS][MAX_LEXEME_LEN];
    int count;
} SymbolTable;

typedef enum {
    TAC_ASSIGN,
    TAC_NEG,
    TAC_BINOP,
    TAC_LABEL,
    TAC_GOTO,
    TAC_IF_FALSE,
    TAC_PRINT
} TACOp;

typedef struct {
    TACOp op;
    TokenType bin_op;
    char dst[MAX_LEXEME_LEN];
    char arg1[MAX_LEXEME_LEN];
    char arg2[MAX_LEXEME_LEN];
} TACInstr;

typedef struct {
    TACInstr code[MAX_TAC];
    int count;
    int temp_count;
    int label_count;
    int has_error;
    char error_msg[256];
} TACList;

typedef struct {
    int success;
    char tokens_text[MAX_TEXT_BUF];
    char semantic_text[MAX_TEXT_BUF];
    char tac_text[MAX_TEXT_BUF];
    char output_text[MAX_TEXT_BUF];
    char error_text[512];
} CompileResult;

/* Runs all compiler stages (Lex, Parse, Semantic, TAC, Execute) on source code. */
void compile_and_run(const char *source, CompileResult *res);

#endif
