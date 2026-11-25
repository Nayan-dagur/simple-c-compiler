# Simple C Compiler

A simple, educational C compiler built entirely in C for a small subset of the C programming language. It includes both a terminal CLI runner and a minimal HTML/CSS web interface served directly by a tiny C HTTP server.

## Compiler Stages

1. **Lexical Analysis (`lexer.c`, `lexer.h`)** — Tokenizes source code into keywords, identifiers, integer literals, string literals, operators, and punctuation with line numbers.
2. **Syntax Analysis (`parser.c`, `parser.h`)** — Recursive-descent parser that enforces grammar rules and operator precedence, building an Abstract Syntax Tree (AST).
3. **Semantic Analysis (`compiler.c`, `compiler.h`)** — Walks the AST with a symbol table to catch undeclared variables and duplicate declarations.
4. **Intermediate Code Generation & Execution (`compiler.c`)** — Generates Three-Address Code (TAC) with temporary variables (`t1`, `t2`, ...) and labels (`L1`, `L2`, ...), then executes the TAC instructions to display `printf` output.

## Supported C Subset

- `int` variable declarations (e.g. `int x;` or `int x = 10;`)
- Assignment statements (`x = a + b * 2;`)
- Arithmetic expressions (`+`, `-`, `*`, `/`) and parentheses `( ... )`
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`)
- `if` / `else` statements and `{ ... }` blocks
- `while` loops
- `printf("%d\n", expr);` or `printf(expr);` for integer output
- Optional `int main() { ... }` wrapper
- Single-line (`//`) and multi-line (`/* ... */`) comments

## Project Structure

```text
simple-c-compiler/
├── main.c        # CLI runner and built-in POSIX socket HTTP server
├── lexer.h       # Token definitions and lexer declarations
├── lexer.c       # Stage 1: Lexical analyzer
├── parser.h      # AST node definitions and parser declarations
├── parser.c      # Stage 2: Recursive-descent parser
├── compiler.h    # Symbol table, TAC structs, and compiler API
├── compiler.c    # Stages 3 & 4: Semantic check, TAC generation, execution
├── Makefile      # Build rules
├── index.html    # Minimal HTML interface
├── style.css     # Plain, customizable CSS stylesheet
└── sample.c      # Sample input program
```

## How to Build and Run

### Build
```bash
make
```

### 1. Terminal (CLI) Mode
Compile and run a source file directly in the terminal:
```bash
./mycc sample.c
```

### 2. Web UI Mode
Start the built-in C web server:
```bash
./mycc --web
```
Then open `http://127.0.0.1:8080` in your browser. (You can pass a custom port, e.g. `./mycc --web 9000`.)

### Clean Build Files
```bash
make clean
```

## Customizing the UI

The interface uses plain HTML and CSS with no external libraries:
- Edit `style.css` to change fonts, font sizes, colors, borders, spacing, buttons, editor styling, and output boxes.
- Edit `index.html` to adjust layout or default sample code.
