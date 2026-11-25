#include "lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_token(TokenList *list, TokenType type, const char *lexeme, int value, int line) {
    if (list->count >= MAX_TOKENS) {
        list->has_error = 1;
        snprintf(list->error_msg, sizeof(list->error_msg),
                 "Lexer Error (line %d): Too many tokens (max %d).", line, MAX_TOKENS);
        return;
    }
    Token *t = &list->tokens[list->count++];
    t->type = type;
    t->value = value;
    t->line = line;
    snprintf(t->lexeme, MAX_LEXEME_LEN, "%s", lexeme);
}

static TokenType keyword_or_ident(const char *word) {
    if (strcmp(word, "int") == 0) return TOK_INT;
    if (strcmp(word, "if") == 0) return TOK_IF;
    if (strcmp(word, "else") == 0) return TOK_ELSE;
    if (strcmp(word, "while") == 0) return TOK_WHILE;
    if (strcmp(word, "printf") == 0) return TOK_PRINTF;
    return TOK_IDENT;
}

int tokenize(const char *src, TokenList *list) {
    list->count = 0;
    list->has_error = 0;
    list->error_msg[0] = '\0';

    int i = 0;
    int line = 1;

    while (src[i] != '\0') {
        char c = src[i];

        /* Skip whitespace and track line numbers */
        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            continue;
        }
        if (c == '\n') {
            line++;
            i++;
            continue;
        }

        /* Skip single-line comments: // ... */
        if (c == '/' && src[i + 1] == '/') {
            i += 2;
            while (src[i] != '\0' && src[i] != '\n') {
                i++;
            }
            continue;
        }

        /* Skip multi-line comments: */
        if (c == '/' && src[i + 1] == '*') {
            int start_line = line;
            i += 2;
            while (src[i] != '\0' && !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n') line++;
                i++;
            }
            if (src[i] == '\0') {
                list->has_error = 1;
                snprintf(list->error_msg, sizeof(list->error_msg),
                         "Lexer Error (line %d): Unterminated block comment.", start_line);
                return 0;
            }
            i += 2;
            continue;
        }

        /* Identifiers and keywords */
        if (isalpha((unsigned char)c) || c == '_') {
            char buf[MAX_LEXEME_LEN];
            int len = 0;
            while (isalnum((unsigned char)src[i]) || src[i] == '_') {
                if (len < MAX_LEXEME_LEN - 1) {
                    buf[len++] = src[i];
                }
                i++;
            }
            buf[len] = '\0';
            add_token(list, keyword_or_ident(buf), buf, 0, line);
            if (list->has_error) return 0;
            continue;
        }

        /* Integer numbers */
        if (isdigit((unsigned char)c)) {
            char buf[MAX_LEXEME_LEN];
            int len = 0;
            while (isdigit((unsigned char)src[i])) {
                if (len < MAX_LEXEME_LEN - 1) {
                    buf[len++] = src[i];
                }
                i++;
            }
            buf[len] = '\0';
            add_token(list, TOK_NUMBER, buf, atoi(buf), line);
            if (list->has_error) return 0;
            continue;
        }

        /* String literals (used for printf format strings) */
        if (c == '"') {
            char buf[MAX_LEXEME_LEN];
            int len = 0;
            int start_line = line;
            i++; /* skip opening quote */
            while (src[i] != '\0' && src[i] != '"') {
                if (src[i] == '\n') {
                    list->has_error = 1;
                    snprintf(list->error_msg, sizeof(list->error_msg),
                             "Lexer Error (line %d): Newline inside string literal.", start_line);
                    return 0;
                }
                if (src[i] == '\\' && src[i + 1] != '\0') {
                    if (len < MAX_LEXEME_LEN - 2) {
                        buf[len++] = src[i];
                        buf[len++] = src[i + 1];
                    }
                    i += 2;
                    continue;
                }
                if (len < MAX_LEXEME_LEN - 1) {
                    buf[len++] = src[i];
                }
                i++;
            }
            if (src[i] != '"') {
                list->has_error = 1;
                snprintf(list->error_msg, sizeof(list->error_msg),
                         "Lexer Error (line %d): Unterminated string literal.", start_line);
                return 0;
            }
            i++; /* skip closing quote */
            buf[len] = '\0';
            add_token(list, TOK_STRING, buf, 0, line);
            if (list->has_error) return 0;
            continue;
        }

        /* Two-character comparison operators */
        if (c == '=' && src[i + 1] == '=') {
            add_token(list, TOK_EQ, "==", 0, line);
            i += 2;
            continue;
        }
        if (c == '!' && src[i + 1] == '=') {
            add_token(list, TOK_NEQ, "!=", 0, line);
            i += 2;
            continue;
        }
        if (c == '<' && src[i + 1] == '=') {
            add_token(list, TOK_LE, "<=", 0, line);
            i += 2;
            continue;
        }
        if (c == '>' && src[i + 1] == '=') {
            add_token(list, TOK_GE, ">=", 0, line);
            i += 2;
            continue;
        }

        /* Single-character operators and punctuation */
        switch (c) {
            case '+': add_token(list, TOK_PLUS, "+", 0, line); break;
            case '-': add_token(list, TOK_MINUS, "-", 0, line); break;
            case '*': add_token(list, TOK_MUL, "*", 0, line); break;
            case '/': add_token(list, TOK_DIV, "/", 0, line); break;
            case '=': add_token(list, TOK_ASSIGN, "=", 0, line); break;
            case '<': add_token(list, TOK_LT, "<", 0, line); break;
            case '>': add_token(list, TOK_GT, ">", 0, line); break;
            case '(': add_token(list, TOK_LPAREN, "(", 0, line); break;
            case ')': add_token(list, TOK_RPAREN, ")", 0, line); break;
            case '{': add_token(list, TOK_LBRACE, "{", 0, line); break;
            case '}': add_token(list, TOK_RBRACE, "}", 0, line); break;
            case ';': add_token(list, TOK_SEMI, ";", 0, line); break;
            case ',': add_token(list, TOK_COMMA, ",", 0, line); break;
            default:
                list->has_error = 1;
                snprintf(list->error_msg, sizeof(list->error_msg),
                         "Lexer Error (line %d): Unexpected character '%c'.", line, c);
                add_token(list, TOK_ERROR, "?", 0, line);
                return 0;
        }
        i++;
        if (list->has_error) return 0;
    }

    add_token(list, TOK_EOF, "EOF", 0, line);
    return !list->has_error;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_INT:    return "INT";
        case TOK_IF:     return "IF";
        case TOK_ELSE:   return "ELSE";
        case TOK_WHILE:  return "WHILE";
        case TOK_PRINTF: return "PRINTF";
        case TOK_IDENT:  return "IDENT";
        case TOK_NUMBER: return "NUMBER";
        case TOK_STRING: return "STRING";
        case TOK_PLUS:   return "PLUS";
        case TOK_MINUS:  return "MINUS";
        case TOK_MUL:    return "MUL";
        case TOK_DIV:    return "DIV";
        case TOK_ASSIGN: return "ASSIGN";
        case TOK_EQ:     return "EQ";
        case TOK_NEQ:    return "NEQ";
        case TOK_LT:     return "LT";
        case TOK_GT:     return "GT";
        case TOK_LE:     return "LE";
        case TOK_GE:     return "GE";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_SEMI:   return "SEMI";
        case TOK_COMMA:  return "COMMA";
        case TOK_EOF:    return "EOF";
        case TOK_ERROR:  return "ERROR";
    }
    return "UNKNOWN";
}
