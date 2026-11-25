#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ASTNode *new_node(ASTNodeType type, int line) {
    ASTNode *n = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (n) {
        n->type = type;
        n->line = line;
    }
    return n;
}

static Token current_tok(Parser *p) {
    if (p->pos < p->tokens->count) {
        return p->tokens->tokens[p->pos];
    }
    return p->tokens->tokens[p->tokens->count - 1];
}

static Token peek_tok(Parser *p, int offset) {
    int idx = p->pos + offset;
    if (idx >= 0 && idx < p->tokens->count) {
        return p->tokens->tokens[idx];
    }
    return p->tokens->tokens[p->tokens->count - 1];
}

static void set_error(Parser *p, int line, const char *msg) {
    if (!p->has_error) {
        p->has_error = 1;
        snprintf(p->error_msg, sizeof(p->error_msg), "Syntax Error (line %d): %s", line, msg);
    }
}

static int expect(Parser *p, TokenType expected, const char *err_msg) {
    Token t = current_tok(p);
    if (t.type == expected) {
        p->pos++;
        return 1;
    }
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (found '%s').", err_msg, t.lexeme);
    set_error(p, t.line, buf);
    return 0;
}

/* Forward declarations for recursive descent */
static ASTNode *parse_statement(Parser *p);
static ASTNode *parse_expression(Parser *p);

static ASTNode *parse_factor(Parser *p) {
    if (p->has_error) return NULL;
    Token t = current_tok(p);

    if (t.type == TOK_NUMBER) {
        p->pos++;
        ASTNode *n = new_node(AST_NUMBER, t.line);
        n->value = t.value;
        return n;
    }

    if (t.type == TOK_IDENT) {
        p->pos++;
        ASTNode *n = new_node(AST_VAR, t.line);
        snprintf(n->name, MAX_LEXEME_LEN, "%s", t.lexeme);
        return n;
    }

    if (t.type == TOK_MINUS) {
        p->pos++;
        ASTNode *operand = parse_factor(p);
        if (!operand) return NULL;
        ASTNode *n = new_node(AST_UNARY_NEG, t.line);
        n->left = operand;
        return n;
    }

    if (t.type == TOK_PLUS) {
        p->pos++;
        return parse_factor(p);
    }

    if (t.type == TOK_LPAREN) {
        p->pos++;
        ASTNode *expr = parse_expression(p);
        if (!expr) return NULL;
        if (!expect(p, TOK_RPAREN, "Expected ')' after expression")) {
            free_ast(expr);
            return NULL;
        }
        return expr;
    }

    char buf[200];
    snprintf(buf, sizeof(buf), "Expected number, variable, or '(', found '%s'.", t.lexeme);
    set_error(p, t.line, buf);
    return NULL;
}

static ASTNode *parse_term(Parser *p) {
    ASTNode *left = parse_factor(p);
    if (!left) return NULL;

    while (!p->has_error) {
        Token t = current_tok(p);
        if (t.type == TOK_MUL || t.type == TOK_DIV) {
            p->pos++;
            ASTNode *right = parse_factor(p);
            if (!right) {
                free_ast(left);
                return NULL;
            }
            ASTNode *bin = new_node(AST_BINOP, t.line);
            bin->op = t.type;
            bin->left = left;
            bin->right = right;
            left = bin;
        } else {
            break;
        }
    }
    return left;
}

static ASTNode *parse_additive(Parser *p) {
    ASTNode *left = parse_term(p);
    if (!left) return NULL;

    while (!p->has_error) {
        Token t = current_tok(p);
        if (t.type == TOK_PLUS || t.type == TOK_MINUS) {
            p->pos++;
            ASTNode *right = parse_term(p);
            if (!right) {
                free_ast(left);
                return NULL;
            }
            ASTNode *bin = new_node(AST_BINOP, t.line);
            bin->op = t.type;
            bin->left = left;
            bin->right = right;
            left = bin;
        } else {
            break;
        }
    }
    return left;
}

static int is_comparison_op(TokenType type) {
    return type == TOK_EQ || type == TOK_NEQ ||
           type == TOK_LT || type == TOK_GT ||
           type == TOK_LE || type == TOK_GE;
}

static ASTNode *parse_expression(Parser *p) {
    ASTNode *left = parse_additive(p);
    if (!left) return NULL;

    while (!p->has_error && is_comparison_op(current_tok(p).type)) {
        Token t = current_tok(p);
        p->pos++;
        ASTNode *right = parse_additive(p);
        if (!right) {
            free_ast(left);
            return NULL;
        }
        ASTNode *bin = new_node(AST_BINOP, t.line);
        bin->op = t.type;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static ASTNode *parse_block(Parser *p) {
    Token open = current_tok(p);
    if (!expect(p, TOK_LBRACE, "Expected '{'")) return NULL;

    ASTNode *block = new_node(AST_BLOCK, open.line);
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    while (!p->has_error && current_tok(p).type != TOK_RBRACE && current_tok(p).type != TOK_EOF) {
        ASTNode *stmt = parse_statement(p);
        if (!stmt) {
            free_ast(head);
            free_ast(block);
            return NULL;
        }
        if (!head) {
            head = stmt;
            tail = stmt;
        } else {
            tail->next = stmt;
            tail = stmt;
        }
    }

    if (!expect(p, TOK_RBRACE, "Expected '}' to close block")) {
        free_ast(head);
        free_ast(block);
        return NULL;
    }

    block->then_body = head;
    return block;
}

static ASTNode *parse_var_decl(Parser *p) {
    Token kw = current_tok(p);
    p->pos++; /* consume 'int' */

    Token name_tok = current_tok(p);
    if (!expect(p, TOK_IDENT, "Expected variable name after 'int'")) return NULL;

    ASTNode *node = new_node(AST_VAR_DECL, kw.line);
    snprintf(node->name, MAX_LEXEME_LEN, "%s", name_tok.lexeme);

    if (current_tok(p).type == TOK_ASSIGN) {
        p->pos++;
        node->left = parse_expression(p);
        if (!node->left) {
            free_ast(node);
            return NULL;
        }
    }

    if (!expect(p, TOK_SEMI, "Expected ';' after variable declaration")) {
        free_ast(node);
        return NULL;
    }
    return node;
}

static ASTNode *parse_assign(Parser *p) {
    Token name_tok = current_tok(p);
    p->pos++; /* consume identifier */

    if (!expect(p, TOK_ASSIGN, "Expected '=' after variable name")) return NULL;

    ASTNode *expr = parse_expression(p);
    if (!expr) return NULL;

    if (!expect(p, TOK_SEMI, "Expected ';' after assignment")) {
        free_ast(expr);
        return NULL;
    }

    ASTNode *node = new_node(AST_ASSIGN, name_tok.line);
    snprintf(node->name, MAX_LEXEME_LEN, "%s", name_tok.lexeme);
    node->left = expr;
    return node;
}

static ASTNode *parse_if(Parser *p) {
    Token kw = current_tok(p);
    p->pos++; /* consume 'if' */

    if (!expect(p, TOK_LPAREN, "Expected '(' after 'if'")) return NULL;
    ASTNode *cond = parse_expression(p);
    if (!cond) return NULL;
    if (!expect(p, TOK_RPAREN, "Expected ')' after if condition")) {
        free_ast(cond);
        return NULL;
    }

    ASTNode *then_body = parse_statement(p);
    if (!then_body) {
        free_ast(cond);
        return NULL;
    }

    ASTNode *else_body = NULL;
    if (current_tok(p).type == TOK_ELSE) {
        p->pos++;
        else_body = parse_statement(p);
        if (!else_body) {
            free_ast(cond);
            free_ast(then_body);
            return NULL;
        }
    }

    ASTNode *node = new_node(AST_IF, kw.line);
    node->cond = cond;
    node->then_body = then_body;
    node->else_body = else_body;
    return node;
}

static ASTNode *parse_while(Parser *p) {
    Token kw = current_tok(p);
    p->pos++; /* consume 'while' */

    if (!expect(p, TOK_LPAREN, "Expected '(' after 'while'")) return NULL;
    ASTNode *cond = parse_expression(p);
    if (!cond) return NULL;
    if (!expect(p, TOK_RPAREN, "Expected ')' after while condition")) {
        free_ast(cond);
        return NULL;
    }

    ASTNode *body = parse_statement(p);
    if (!body) {
        free_ast(cond);
        return NULL;
    }

    ASTNode *node = new_node(AST_WHILE, kw.line);
    node->cond = cond;
    node->then_body = body;
    return node;
}

static ASTNode *parse_printf(Parser *p) {
    Token kw = current_tok(p);
    p->pos++; /* consume 'printf' */

    if (!expect(p, TOK_LPAREN, "Expected '(' after 'printf'")) return NULL;

    /* Support both printf("%d\n", expr); and printf(expr); */
    if (current_tok(p).type == TOK_STRING) {
        p->pos++;
        if (!expect(p, TOK_COMMA, "Expected ',' after printf format string")) return NULL;
    }

    ASTNode *expr = parse_expression(p);
    if (!expr) return NULL;

    if (!expect(p, TOK_RPAREN, "Expected ')' after printf argument")) {
        free_ast(expr);
        return NULL;
    }
    if (!expect(p, TOK_SEMI, "Expected ';' after printf statement")) {
        free_ast(expr);
        return NULL;
    }

    ASTNode *node = new_node(AST_PRINTF, kw.line);
    node->left = expr;
    return node;
}

static ASTNode *parse_statement(Parser *p) {
    if (p->has_error) return NULL;
    Token t = current_tok(p);

    if (t.type == TOK_INT)    return parse_var_decl(p);
    if (t.type == TOK_IDENT)  return parse_assign(p);
    if (t.type == TOK_IF)     return parse_if(p);
    if (t.type == TOK_WHILE)  return parse_while(p);
    if (t.type == TOK_PRINTF) return parse_printf(p);
    if (t.type == TOK_LBRACE) return parse_block(p);

    char buf[200];
    snprintf(buf, sizeof(buf), "Unexpected token '%s' at start of statement.", t.lexeme);
    set_error(p, t.line, buf);
    return NULL;
}

ASTNode *parse_program(const TokenList *tokens, char *error_out, int error_out_size) {
    Parser p;
    p.tokens = tokens;
    p.pos = 0;
    p.has_error = 0;
    p.error_msg[0] = '\0';

    ASTNode *prog = new_node(AST_PROGRAM, 1);

    /* Optional support for: int main() { ... } */
    if (peek_tok(&p, 0).type == TOK_INT &&
        peek_tok(&p, 1).type == TOK_IDENT &&
        strcmp(peek_tok(&p, 1).lexeme, "main") == 0 &&
        peek_tok(&p, 2).type == TOK_LPAREN) {
        p.pos += 3; /* consume int, main, ( */
        if (!expect(&p, TOK_RPAREN, "Expected ')' in main()")) {
            snprintf(error_out, error_out_size, "%s", p.error_msg);
            free_ast(prog);
            return NULL;
        }
        ASTNode *body = parse_block(&p);
        if (!body) {
            snprintf(error_out, error_out_size, "%s", p.error_msg);
            free_ast(prog);
            return NULL;
        }
        prog->then_body = body;
        if (current_tok(&p).type != TOK_EOF) {
            set_error(&p, current_tok(&p).line, "Unexpected tokens after main() block.");
            snprintf(error_out, error_out_size, "%s", p.error_msg);
            free_ast(prog);
            return NULL;
        }
        return prog;
    }

    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    while (!p.has_error && current_tok(&p).type != TOK_EOF) {
        ASTNode *stmt = parse_statement(&p);
        if (!stmt) {
            snprintf(error_out, error_out_size, "%s", p.error_msg);
            free_ast(head);
            free_ast(prog);
            return NULL;
        }
        if (!head) {
            head = stmt;
            tail = stmt;
        } else {
            tail->next = stmt;
            tail = stmt;
        }
    }

    prog->then_body = head;
    return prog;
}

void free_ast(ASTNode *node) {
    if (!node) return;
    free_ast(node->left);
    free_ast(node->right);
    free_ast(node->cond);
    free_ast(node->then_body);
    free_ast(node->else_body);
    free_ast(node->next);
    free(node);
}
