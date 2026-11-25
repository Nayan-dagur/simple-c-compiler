#include "compiler.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- Helper for building output strings ---------------- */

static void append_str(char *buf, int buf_size, const char *text) {
    int cur = (int)strlen(buf);
    if (cur < buf_size - 1) {
        snprintf(buf + cur, buf_size - cur, "%s", text);
    }
}

/* ---------------- Stage 3: Semantic Analysis ---------------- */

static int sym_exists(const SymbolTable *sym, const char *name) {
    for (int i = 0; i < sym->count; i++) {
        if (strcmp(sym->names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int sym_add(SymbolTable *sym, const char *name, int line, char *err, int err_size) {
    if (sym_exists(sym, name)) {
        snprintf(err, err_size, "Semantic Error (line %d): Variable '%s' already declared.", line, name);
        return 0;
    }
    if (sym->count >= MAX_VARS) {
        snprintf(err, err_size, "Semantic Error (line %d): Too many variables (max %d).", line, MAX_VARS);
        return 0;
    }
    snprintf(sym->names[sym->count++], MAX_LEXEME_LEN, "%s", name);
    return 1;
}

static int check_node_semantics(const ASTNode *node, SymbolTable *sym, char *err, int err_size) {
    if (!node) return 1;

    switch (node->type) {
        case AST_VAR_DECL:
            if (node->left && !check_node_semantics(node->left, sym, err, err_size)) {
                return 0;
            }
            if (!sym_add(sym, node->name, node->line, err, err_size)) {
                return 0;
            }
            break;

        case AST_ASSIGN:
            if (!sym_exists(sym, node->name)) {
                snprintf(err, err_size, "Semantic Error (line %d): Undeclared variable '%s'.",
                         node->line, node->name);
                return 0;
            }
            if (!check_node_semantics(node->left, sym, err, err_size)) return 0;
            break;

        case AST_VAR:
            if (!sym_exists(sym, node->name)) {
                snprintf(err, err_size, "Semantic Error (line %d): Undeclared variable '%s'.",
                         node->line, node->name);
                return 0;
            }
            break;

        default:
            if (!check_node_semantics(node->left, sym, err, err_size)) return 0;
            if (!check_node_semantics(node->right, sym, err, err_size)) return 0;
            if (!check_node_semantics(node->cond, sym, err, err_size)) return 0;
            if (!check_node_semantics(node->then_body, sym, err, err_size)) return 0;
            if (!check_node_semantics(node->else_body, sym, err, err_size)) return 0;
            break;
    }

    return check_node_semantics(node->next, sym, err, err_size);
}

/* ---------------- Stage 4: Three-Address Code (TAC) ---------------- */

static void new_temp(TACList *tac, char *out) {
    tac->temp_count++;
    snprintf(out, MAX_LEXEME_LEN, "t%d", tac->temp_count);
}

static void new_label(TACList *tac, char *out) {
    tac->label_count++;
    snprintf(out, MAX_LEXEME_LEN, "L%d", tac->label_count);
}

static void emit_tac(TACList *tac, TACOp op, TokenType bin_op,
                     const char *dst, const char *arg1, const char *arg2) {
    if (tac->count >= MAX_TAC) {
        tac->has_error = 1;
        snprintf(tac->error_msg, sizeof(tac->error_msg),
                 "Compiler Error: Too many intermediate instructions (max %d).", MAX_TAC);
        return;
    }
    TACInstr *ins = &tac->code[tac->count++];
    ins->op = op;
    ins->bin_op = bin_op;
    snprintf(ins->dst, MAX_LEXEME_LEN, "%s", dst ? dst : "");
    snprintf(ins->arg1, MAX_LEXEME_LEN, "%s", arg1 ? arg1 : "");
    snprintf(ins->arg2, MAX_LEXEME_LEN, "%s", arg2 ? arg2 : "");
}

static const char *op_symbol(TokenType op) {
    switch (op) {
        case TOK_PLUS:  return "+";
        case TOK_MINUS: return "-";
        case TOK_MUL:   return "*";
        case TOK_DIV:   return "/";
        case TOK_EQ:    return "==";
        case TOK_NEQ:   return "!=";
        case TOK_LT:    return "<";
        case TOK_GT:    return ">";
        case TOK_LE:    return "<=";
        case TOK_GE:    return ">=";
        default:        return "?";
    }
}

static void gen_expr(const ASTNode *node, TACList *tac, char *result_name) {
    if (!node || tac->has_error) {
        result_name[0] = '\0';
        return;
    }

    if (node->type == AST_NUMBER) {
        snprintf(result_name, MAX_LEXEME_LEN, "%d", node->value);
        return;
    }

    if (node->type == AST_VAR) {
        snprintf(result_name, MAX_LEXEME_LEN, "%s", node->name);
        return;
    }

    if (node->type == AST_UNARY_NEG) {
        char operand[MAX_LEXEME_LEN];
        gen_expr(node->left, tac, operand);
        new_temp(tac, result_name);
        emit_tac(tac, TAC_NEG, TOK_MINUS, result_name, operand, "");
        return;
    }

    if (node->type == AST_BINOP) {
        char left_name[MAX_LEXEME_LEN];
        char right_name[MAX_LEXEME_LEN];
        gen_expr(node->left, tac, left_name);
        gen_expr(node->right, tac, right_name);
        new_temp(tac, result_name);
        emit_tac(tac, TAC_BINOP, node->op, result_name, left_name, right_name);
        return;
    }
}

static void gen_stmt(const ASTNode *node, TACList *tac) {
    if (!node || tac->has_error) return;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK:
            gen_stmt(node->then_body, tac);
            break;

        case AST_VAR_DECL:
            if (node->left) {
                char val[MAX_LEXEME_LEN];
                gen_expr(node->left, tac, val);
                emit_tac(tac, TAC_ASSIGN, TOK_ASSIGN, node->name, val, "");
            } else {
                emit_tac(tac, TAC_ASSIGN, TOK_ASSIGN, node->name, "0", "");
            }
            break;

        case AST_ASSIGN: {
            char val[MAX_LEXEME_LEN];
            gen_expr(node->left, tac, val);
            emit_tac(tac, TAC_ASSIGN, TOK_ASSIGN, node->name, val, "");
            break;
        }

        case AST_PRINTF: {
            char val[MAX_LEXEME_LEN];
            gen_expr(node->left, tac, val);
            emit_tac(tac, TAC_PRINT, TOK_PRINTF, "", val, "");
            break;
        }

        case AST_IF: {
            char cond_val[MAX_LEXEME_LEN];
            char else_lbl[MAX_LEXEME_LEN];
            char end_lbl[MAX_LEXEME_LEN];
            gen_expr(node->cond, tac, cond_val);
            new_label(tac, else_lbl);

            emit_tac(tac, TAC_IF_FALSE, TOK_IF, else_lbl, cond_val, "");
            gen_stmt(node->then_body, tac);

            if (node->else_body) {
                new_label(tac, end_lbl);
                emit_tac(tac, TAC_GOTO, TOK_ELSE, end_lbl, "", "");
                emit_tac(tac, TAC_LABEL, TOK_ELSE, else_lbl, "", "");
                gen_stmt(node->else_body, tac);
                emit_tac(tac, TAC_LABEL, TOK_ELSE, end_lbl, "", "");
            } else {
                emit_tac(tac, TAC_LABEL, TOK_IF, else_lbl, "", "");
            }
            break;
        }

        case AST_WHILE: {
            char start_lbl[MAX_LEXEME_LEN];
            char end_lbl[MAX_LEXEME_LEN];
            char cond_val[MAX_LEXEME_LEN];
            new_label(tac, start_lbl);
            new_label(tac, end_lbl);

            emit_tac(tac, TAC_LABEL, TOK_WHILE, start_lbl, "", "");
            gen_expr(node->cond, tac, cond_val);
            emit_tac(tac, TAC_IF_FALSE, TOK_WHILE, end_lbl, cond_val, "");
            gen_stmt(node->then_body, tac);
            emit_tac(tac, TAC_GOTO, TOK_WHILE, start_lbl, "", "");
            emit_tac(tac, TAC_LABEL, TOK_WHILE, end_lbl, "", "");
            break;
        }

        default:
            break;
    }

    gen_stmt(node->next, tac);
}

static void format_tac(const TACList *tac, char *out, int out_size) {
    out[0] = '\0';
    if (tac->count == 0) {
        snprintf(out, out_size, "(no intermediate instructions)\n");
        return;
    }

    char line[256];
    for (int i = 0; i < tac->count; i++) {
        const TACInstr *ins = &tac->code[i];
        switch (ins->op) {
            case TAC_ASSIGN:
                snprintf(line, sizeof(line), "  %s = %s\n", ins->dst, ins->arg1);
                break;
            case TAC_NEG:
                snprintf(line, sizeof(line), "  %s = -%s\n", ins->dst, ins->arg1);
                break;
            case TAC_BINOP:
                snprintf(line, sizeof(line), "  %s = %s %s %s\n",
                         ins->dst, ins->arg1, op_symbol(ins->bin_op), ins->arg2);
                break;
            case TAC_LABEL:
                snprintf(line, sizeof(line), "%s:\n", ins->dst);
                break;
            case TAC_GOTO:
                snprintf(line, sizeof(line), "  goto %s\n", ins->dst);
                break;
            case TAC_IF_FALSE:
                snprintf(line, sizeof(line), "  if_false %s goto %s\n", ins->arg1, ins->dst);
                break;
            case TAC_PRINT:
                snprintf(line, sizeof(line), "  print %s\n", ins->arg1);
                break;
        }
        append_str(out, out_size, line);
    }
}

/* ---------------- Simple TAC Executor (for printf output) ---------------- */

#define MAX_RUNTIME_VARS 1024
#define MAX_EXEC_STEPS 100000

typedef struct {
    char name[MAX_LEXEME_LEN];
    int value;
} RuntimeVar;

typedef struct {
    RuntimeVar vars[MAX_RUNTIME_VARS];
    int count;
} RuntimeEnv;

static int is_int_literal(const char *s) {
    if (!s || s[0] == '\0') return 0;
    int i = (s[0] == '-') ? 1 : 0;
    if (s[i] == '\0') return 0;
    for (; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

static int get_val(const RuntimeEnv *env, const char *operand) {
    if (is_int_literal(operand)) {
        return atoi(operand);
    }
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->vars[i].name, operand) == 0) {
            return env->vars[i].value;
        }
    }
    return 0;
}

static void set_val(RuntimeEnv *env, const char *name, int value) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->vars[i].name, name) == 0) {
            env->vars[i].value = value;
            return;
        }
    }
    if (env->count < MAX_RUNTIME_VARS) {
        snprintf(env->vars[env->count].name, MAX_LEXEME_LEN, "%s", name);
        env->vars[env->count].value = value;
        env->count++;
    }
}

static int find_label(const TACList *tac, const char *label) {
    for (int i = 0; i < tac->count; i++) {
        if (tac->code[i].op == TAC_LABEL && strcmp(tac->code[i].dst, label) == 0) {
            return i;
        }
    }
    return -1;
}

static int execute_tac(const TACList *tac, char *output_out, int output_size,
                       char *err_out, int err_size) {
    RuntimeEnv env;
    env.count = 0;
    output_out[0] = '\0';

    int pc = 0;
    int steps = 0;

    while (pc >= 0 && pc < tac->count) {
        if (++steps > MAX_EXEC_STEPS) {
            snprintf(err_out, err_size,
                     "Runtime Error: Execution limit (%d steps) exceeded (possible infinite loop).",
                     MAX_EXEC_STEPS);
            return 0;
        }

        const TACInstr *ins = &tac->code[pc];
        switch (ins->op) {
            case TAC_ASSIGN:
                set_val(&env, ins->dst, get_val(&env, ins->arg1));
                pc++;
                break;

            case TAC_NEG:
                set_val(&env, ins->dst, -get_val(&env, ins->arg1));
                pc++;
                break;

            case TAC_BINOP: {
                int a = get_val(&env, ins->arg1);
                int b = get_val(&env, ins->arg2);
                int r = 0;
                switch (ins->bin_op) {
                    case TOK_PLUS:  r = a + b; break;
                    case TOK_MINUS: r = a - b; break;
                    case TOK_MUL:   r = a * b; break;
                    case TOK_DIV:
                        if (b == 0) {
                            snprintf(err_out, err_size, "Runtime Error: Division by zero.");
                            return 0;
                        }
                        r = a / b;
                        break;
                    case TOK_EQ:  r = (a == b); break;
                    case TOK_NEQ: r = (a != b); break;
                    case TOK_LT:  r = (a < b); break;
                    case TOK_GT:  r = (a > b); break;
                    case TOK_LE:  r = (a <= b); break;
                    case TOK_GE:  r = (a >= b); break;
                    default: break;
                }
                set_val(&env, ins->dst, r);
                pc++;
                break;
            }

            case TAC_LABEL:
                pc++;
                break;

            case TAC_GOTO:
                pc = find_label(tac, ins->dst);
                break;

            case TAC_IF_FALSE:
                if (get_val(&env, ins->arg1) == 0) {
                    pc = find_label(tac, ins->dst);
                } else {
                    pc++;
                }
                break;

            case TAC_PRINT: {
                char line[64];
                snprintf(line, sizeof(line), "%d\n", get_val(&env, ins->arg1));
                append_str(output_out, output_size, line);
                pc++;
                break;
            }
        }
    }

    if (output_out[0] == '\0') {
        snprintf(output_out, output_size, "(program finished with no printf output)\n");
    }
    return 1;
}

/* ---------------- Public Pipeline Entry Point ---------------- */

void compile_and_run(const char *source, CompileResult *res) {
    memset(res, 0, sizeof(CompileResult));

    /* Stage 1: Lexical Analysis */
    TokenList tokens;
    if (!tokenize(source, &tokens)) {
        snprintf(res->error_text, sizeof(res->error_text), "%s", tokens.error_msg);
        return;
    }

    char line_buf[160];
    for (int i = 0; i < tokens.count; i++) {
        snprintf(line_buf, sizeof(line_buf), "Line %-3d | %-8s | %s\n",
                 tokens.tokens[i].line,
                 token_type_name(tokens.tokens[i].type),
                 tokens.tokens[i].lexeme);
        append_str(res->tokens_text, MAX_TEXT_BUF, line_buf);
    }

    /* Stage 2: Syntax Analysis (Parser) */
    ASTNode *ast = parse_program(&tokens, res->error_text, sizeof(res->error_text));
    if (!ast) {
        return;
    }

    /* Stage 3: Semantic Analysis */
    SymbolTable sym;
    sym.count = 0;
    if (!check_node_semantics(ast, &sym, res->error_text, sizeof(res->error_text))) {
        free_ast(ast);
        return;
    }

    append_str(res->semantic_text, MAX_TEXT_BUF, "Syntax OK.\nSemantic Analysis OK.\nDeclared variables: ");
    if (sym.count == 0) {
        append_str(res->semantic_text, MAX_TEXT_BUF, "(none)\n");
    } else {
        for (int i = 0; i < sym.count; i++) {
            append_str(res->semantic_text, MAX_TEXT_BUF, sym.names[i]);
            if (i < sym.count - 1) append_str(res->semantic_text, MAX_TEXT_BUF, ", ");
        }
        append_str(res->semantic_text, MAX_TEXT_BUF, "\n");
    }

    /* Stage 4: Intermediate Code Generation (TAC) */
    TACList tac;
    memset(&tac, 0, sizeof(tac));
    gen_stmt(ast, &tac);
    free_ast(ast);

    if (tac.has_error) {
        snprintf(res->error_text, sizeof(res->error_text), "%s", tac.error_msg);
        return;
    }

    format_tac(&tac, res->tac_text, MAX_TEXT_BUF);

    /* Execute generated TAC for printf output */
    if (!execute_tac(&tac, res->output_text, MAX_TEXT_BUF, res->error_text, sizeof(res->error_text))) {
        return;
    }

    res->success = 1;
}
