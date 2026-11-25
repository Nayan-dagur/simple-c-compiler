#include "compiler.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

static char *read_entire_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, (size_t)size, fp);
    buf[read_bytes] = '\0';
    fclose(fp);
    return buf;
}

static void print_cli_result(const CompileResult *res) {
    if (res->tokens_text[0] != '\0') {
        printf("=== 1. LEXICAL ANALYSIS (TOKENS) ===\n%s\n", res->tokens_text);
    }

    if (!res->success) {
        printf("=== ERROR ===\n%s\n", res->error_text);
        return;
    }

    printf("=== 2 & 3. SYNTAX & SEMANTIC ANALYSIS ===\n%s\n", res->semantic_text);
    printf("=== 4. INTERMEDIATE CODE (THREE-ADDRESS CODE) ===\n%s\n", res->tac_text);
    printf("=== 5. PROGRAM OUTPUT ===\n%s", res->output_text);
}

static int run_cli(const char *filename) {
    char *source = read_entire_file(filename);
    if (!source) {
        fprintf(stderr, "Error: Could not open file '%s'.\n", filename);
        return 1;
    }

    CompileResult *res = (CompileResult *)calloc(1, sizeof(CompileResult));
    if (!res) {
        free(source);
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    compile_and_run(source, res);
    print_cli_result(res);
    int exit_code = res->success ? 0 : 1;

    free(res);
    free(source);
    return exit_code;
}

/* ---------------- Tiny Built-in HTTP Server (for UI) ---------------- */

static void send_http_response(int client_fd, const char *status,
                               const char *content_type, const char *body) {
    char header[512];
    int body_len = (int)strlen(body);
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %s\r\n"
                              "Content-Type: %s; charset=utf-8\r\n"
                              "Content-Length: %d\r\n"
                              "Connection: close\r\n\r\n",
                              status, content_type, body_len);
    send(client_fd, header, (size_t)header_len, 0);
    send(client_fd, body, (size_t)body_len, 0);
}

static void serve_static_file(int client_fd, const char *path, const char *content_type) {
    char *content = read_entire_file(path);
    if (!content) {
        send_http_response(client_fd, "404 Not Found", "text/plain", "File not found.\n");
        return;
    }
    send_http_response(client_fd, "200 OK", content_type, content);
    free(content);
}

static int parse_content_length(const char *headers) {
    const char *p = headers;
    while (*p) {
        if (strncasecmp(p, "Content-Length:", 15) == 0) {
            return atoi(p + 15);
        }
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    return 0;
}

static void append_json_string(char *dst, int dst_size, int *pos, const char *src) {
    if (*pos < dst_size - 1) dst[(*pos)++] = '"';
    for (int i = 0; src[i] != '\0' && *pos < dst_size - 6; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            dst[(*pos)++] = '\\';
            dst[(*pos)++] = (char)c;
        } else if (c == '\n') {
            dst[(*pos)++] = '\\';
            dst[(*pos)++] = 'n';
        } else if (c == '\r') {
            dst[(*pos)++] = '\\';
            dst[(*pos)++] = 'r';
        } else if (c == '\t') {
            dst[(*pos)++] = '\\';
            dst[(*pos)++] = 't';
        } else if (c >= 32) {
            dst[(*pos)++] = (char)c;
        }
    }
    if (*pos < dst_size - 1) dst[(*pos)++] = '"';
    dst[*pos] = '\0';
}

static void handle_compile_post(int client_fd, const char *source) {
    CompileResult *res = (CompileResult *)calloc(1, sizeof(CompileResult));
    char *json = (char *)malloc(MAX_TEXT_BUF * 5);
    if (!res || !json) {
        free(res);
        free(json);
        send_http_response(client_fd, "500 Internal Server Error", "text/plain", "Out of memory");
        return;
    }

    compile_and_run(source, res);

    int pos = 0;
    int cap = MAX_TEXT_BUF * 5;
    pos += snprintf(json + pos, cap - pos, "{\"success\":%s,\"error\":",
                    res->success ? "true" : "false");
    append_json_string(json, cap, &pos, res->error_text);
    pos += snprintf(json + pos, cap - pos, ",\"tokens\":");
    append_json_string(json, cap, &pos, res->tokens_text);
    pos += snprintf(json + pos, cap - pos, ",\"semantic\":");
    append_json_string(json, cap, &pos, res->semantic_text);
    pos += snprintf(json + pos, cap - pos, ",\"tac\":");
    append_json_string(json, cap, &pos, res->tac_text);
    pos += snprintf(json + pos, cap - pos, ",\"output\":");
    append_json_string(json, cap, &pos, res->output_text);
    snprintf(json + pos, cap - pos, "}");

    send_http_response(client_fd, "200 OK", "application/json", json);

    free(json);
    free(res);
}

static void handle_client(int client_fd) {
    char req[65536];
    int total = 0;
    char *header_end = NULL;

    /* Read until end of HTTP headers */
    while (total < (int)sizeof(req) - 1) {
        ssize_t n = recv(client_fd, req + total, sizeof(req) - 1 - (size_t)total, 0);
        if (n <= 0) break;
        total += (int)n;
        req[total] = '\0';
        header_end = strstr(req, "\r\n\r\n");
        if (header_end) break;
    }

    if (!header_end) {
        close(client_fd);
        return;
    }

    int header_bytes = (int)(header_end - req) + 4;
    int content_len = parse_content_length(req);
    if (content_len > (int)sizeof(req) - header_bytes - 1) {
        content_len = (int)sizeof(req) - header_bytes - 1;
    }

    /* Read remaining body bytes if needed */
    while (total < header_bytes + content_len) {
        ssize_t n = recv(client_fd, req + total, sizeof(req) - 1 - (size_t)total, 0);
        if (n <= 0) break;
        total += (int)n;
        req[total] = '\0';
    }

    if (strncmp(req, "GET / ", 6) == 0 || strncmp(req, "GET /index.html ", 16) == 0) {
        serve_static_file(client_fd, "index.html", "text/html");
    } else if (strncmp(req, "GET /style.css ", 15) == 0) {
        serve_static_file(client_fd, "style.css", "text/css");
    } else if (strncmp(req, "POST /compile ", 14) == 0) {
        char *body = req + header_bytes;
        body[content_len] = '\0';
        handle_compile_post(client_fd, body);
    } else {
        send_http_response(client_fd, "404 Not Found", "text/plain", "Not found.\n");
    }

    close(client_fd);
}

static int run_web_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Simple C Compiler UI running at http://127.0.0.1:%d\n", port);
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        handle_client(client_fd);
    }

    close(server_fd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc >= 2 && strcmp(argv[1], "--web") == 0) {
        int port = 8080;
        if (argc >= 3) {
            port = atoi(argv[2]);
            if (port <= 0) port = 8080;
        }
        return run_web_server(port);
    }

    if (argc == 2) {
        return run_cli(argv[1]);
    }

    printf("Usage:\n");
    printf("  %s <source_file.c>   Compile and run a file in terminal\n", argv[0]);
    printf("  %s --web [port]      Start local web interface (default port 8080)\n", argv[0]);
    return 0;
}
