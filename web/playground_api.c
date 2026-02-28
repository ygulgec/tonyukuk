/*
 * Tonyukuk Playground API - sandboxed compile & run service.
 * Pure C replacement for playground_api.py
 *
 * Listens on 127.0.0.1:8081, accepts POST /run with form-encoded "kod" parameter.
 * Compiles and runs code in a sandbox, returns output.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>

#define PORT            8081
#define BACKLOG         16
#define MAX_REQUEST     16384
#define MAX_CODE_SIZE   8192      /* 8 KB */
#define COMPILE_TIMEOUT 10        /* seconds */
#define RUN_TIMEOUT     5         /* seconds */
#define MAX_OUTPUT      32768     /* 32 KB */

static const char *COMPILER = "/var/www/tonyukuktr.com/derleyici/tonyukuk-derle";
static const char *SANDBOX_USER = "tonyukuktr";
static const char *CORS_ORIGIN = "https://tonyukuktr.com";

/* --- Utility functions --- */

static void remove_dir_recursive(const char *path) {
    DIR *d = opendir(path);
    if (!d) return;

    struct dirent *entry;
    char filepath[512];
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        struct stat st;
        if (lstat(filepath, &st) == 0 && S_ISDIR(st.st_mode))
            remove_dir_recursive(filepath);
        else
            unlink(filepath);
    }
    closedir(d);
    rmdir(path);
}

/* Decode a single %XX hex sequence. Returns decoded char or -1. */
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* URL-decode in place. Returns new length. */
static size_t url_decode(char *buf, size_t len) {
    size_t i = 0, j = 0;
    while (i < len) {
        if (buf[i] == '+') {
            buf[j++] = ' ';
            i++;
        } else if (buf[i] == '%' && i + 2 < len) {
            int hi = hex_val(buf[i + 1]);
            int lo = hex_val(buf[i + 2]);
            if (hi >= 0 && lo >= 0) {
                buf[j++] = (char)((hi << 4) | lo);
                i += 3;
            } else {
                buf[j++] = buf[i++];
            }
        } else {
            buf[j++] = buf[i++];
        }
    }
    buf[j] = '\0';
    return j;
}

/* Find value of "kod=" in form-encoded body. Returns malloc'd string or NULL. */
static char *extract_kod(const char *body, size_t body_len) {
    const char *p = body;
    const char *end = body + body_len;

    while (p < end) {
        /* Check if this parameter starts with "kod=" */
        if (p + 4 <= end && strncmp(p, "kod=", 4) == 0) {
            p += 4;
            const char *val_start = p;
            while (p < end && *p != '&') p++;
            size_t val_len = (size_t)(p - val_start);
            char *val = malloc(val_len + 1);
            if (!val) return NULL;
            memcpy(val, val_start, val_len);
            val[val_len] = '\0';
            url_decode(val, val_len);
            return val;
        }
        /* Skip to next parameter */
        while (p < end && *p != '&') p++;
        if (p < end) p++; /* skip '&' */
    }
    return NULL;
}

/* Run a command with timeout, capture stdout+stderr. Returns exit code. */
static int run_command(char *const argv[], int timeout_sec, const char *cwd,
                       char *out_buf, size_t out_buf_size, size_t *out_len) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        if (cwd && chdir(cwd) < 0) _exit(127);

        execvp(argv[0], argv);
        _exit(127);
    }

    /* Parent */
    close(pipefd[1]);

    /* Set read timeout */
    struct timeval tv;
    tv.tv_sec = timeout_sec + 2;
    tv.tv_usec = 0;
    setsockopt(pipefd[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    size_t total = 0;
    ssize_t n;
    while (total < out_buf_size - 1) {
        n = read(pipefd[0], out_buf + total, out_buf_size - 1 - total);
        if (n <= 0) break;
        total += (size_t)n;
    }
    out_buf[total] = '\0';
    *out_len = total;
    close(pipefd[0]);

    /* Wait with timeout */
    int status = 0;
    int elapsed = 0;
    while (elapsed < timeout_sec + 2) {
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
        if (w < 0) return -1;
        usleep(100000); /* 100ms */
        elapsed++;
    }

    /* Timeout - kill the process */
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    return 124; /* timeout exit code */
}

/* --- HTTP handling --- */

static void send_response(int fd, int status_code, const char *status_text,
                          const char *body, size_t body_len) {
    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, body_len, CORS_ORIGIN);

    (void)!write(fd, header, (size_t)hlen);
    if (body && body_len > 0)
        (void)!write(fd, body, body_len);
}

static void send_cors_preflight(int fd) {
    char resp[512];
    int len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 200 OK\r\n"
        "Access-Control-Allow-Origin: %s\r\n"
        "Access-Control-Allow-Methods: POST\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n",
        CORS_ORIGIN);
    (void)!write(fd, resp, (size_t)len);
}

static void send_error(int fd, int code, const char *text) {
    const char *status = code == 400 ? "Bad Request" :
                         code == 404 ? "Not Found" :
                         code == 405 ? "Method Not Allowed" :
                         "Internal Server Error";
    send_response(fd, code, status, text, strlen(text));
}

static void handle_request(int client_fd) {
    /* Restore SIGCHLD so waitpid works in run_command */
    signal(SIGCHLD, SIG_DFL);

    char request[MAX_REQUEST];
    ssize_t total = 0;
    ssize_t n;

    /* Read the full request with a timeout */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (total < (ssize_t)sizeof(request) - 1) {
        n = read(client_fd, request + total, (size_t)(sizeof(request) - 1 - total));
        if (n <= 0) break;
        total += n;

        /* Check if we have the full request (headers + body) */
        request[total] = '\0';
        char *header_end = strstr(request, "\r\n\r\n");
        if (header_end) {
            /* Check Content-Length to see if body is complete */
            char *cl = strcasestr(request, "Content-Length:");
            if (cl) {
                /* Güvenli integer parsing - overflow koruması */
                char *endptr;
                long content_len_long = strtol(cl + 15, &endptr, 10);
                /* Negatif veya çok büyük değerleri reddet */
                if (content_len_long < 0 || content_len_long > MAX_REQUEST) {
                    break; /* Geçersiz Content-Length */
                }
                size_t content_len = (size_t)content_len_long;
                size_t header_size = (size_t)(header_end + 4 - request);
                if ((size_t)total >= header_size + content_len)
                    break;
            } else {
                break; /* No body expected */
            }
        }
    }

    if (total <= 0) {
        close(client_fd);
        return;
    }
    request[total] = '\0';

    /* Parse method and path */
    char method[16] = {0};
    char path[256] = {0};
    sscanf(request, "%15s %255s", method, path);

    /* OPTIONS - CORS preflight */
    if (strcmp(method, "OPTIONS") == 0) {
        send_cors_preflight(client_fd);
        close(client_fd);
        return;
    }

    /* Only POST /run */
    if (strcmp(method, "POST") != 0) {
        send_error(client_fd, 405, "Sadece POST desteklenir");
        close(client_fd);
        return;
    }

    if (strcmp(path, "/run") != 0) {
        send_error(client_fd, 404, "Bulunamadi");
        close(client_fd);
        return;
    }

    /* Find body */
    char *body = strstr(request, "\r\n\r\n");
    if (!body) {
        send_error(client_fd, 400, "Gecersiz istek");
        close(client_fd);
        return;
    }
    body += 4;
    size_t body_len = (size_t)(total - (body - request));

    if (body_len == 0) {
        send_error(client_fd, 400, "Bos istek");
        close(client_fd);
        return;
    }

    if (body_len > MAX_CODE_SIZE) {
        send_error(client_fd, 400, "Kod boyutu cok buyuk (max 8KB)");
        close(client_fd);
        return;
    }

    /* Extract "kod" parameter */
    char *code = extract_kod(body, body_len);
    if (!code || strlen(code) == 0) {
        free(code);
        send_error(client_fd, 400, "Bos kod");
        close(client_fd);
        return;
    }

    /* Create temp directory */
    char work_dir[] = "/tmp/play_XXXXXX";
    if (!mkdtemp(work_dir)) {
        free(code);
        send_error(client_fd, 500, "Gecici dizin olusturulamadi");
        close(client_fd);
        return;
    }
    chmod(work_dir, 0755);

    char src_file[512], bin_file[512];
    snprintf(src_file, sizeof(src_file), "%s/program.tr", work_dir);
    snprintf(bin_file, sizeof(bin_file), "%s/program", work_dir);

    /* Write source code to file */
    FILE *f = fopen(src_file, "w");
    if (!f) {
        free(code);
        remove_dir_recursive(work_dir);
        send_error(client_fd, 500, "Dosya yazma hatasi");
        close(client_fd);
        return;
    }
    fputs(code, f);
    fclose(f);
    free(code);

    char output[MAX_OUTPUT];
    size_t out_len = 0;

    /* Compile */
    char *compile_argv[] = {(char *)COMPILER, src_file, "-o", bin_file, NULL};
    int ret = run_command(compile_argv, COMPILE_TIMEOUT, work_dir, output, sizeof(output), &out_len);

    if (ret != 0) {
        if (out_len == 0) {
            const char *msg = "Derleme hatasi";
            send_response(client_fd, 200, "OK", msg, strlen(msg));
        } else {
            send_response(client_fd, 200, "OK", output, out_len);
        }
        remove_dir_recursive(work_dir);
        close(client_fd);
        return;
    }

    /* Make binary executable */
    chmod(bin_file, 0755);

    /* Run as unprivileged user with timeout */
    char timeout_str[16];
    snprintf(timeout_str, sizeof(timeout_str), "%d", RUN_TIMEOUT);

    char *run_argv[] = {
        "timeout", timeout_str,
        "sudo", "-u", (char *)SANDBOX_USER,
        "env", "-i", "PATH=/usr/bin:/bin",
        bin_file,
        NULL
    };

    memset(output, 0, sizeof(output));
    out_len = 0;
    ret = run_command(run_argv, RUN_TIMEOUT + 2, work_dir, output, sizeof(output), &out_len);

    if (ret == 124) {
        /* Timeout */
        const char *timeout_msg = "\n[Zaman asimi]";
        size_t tmlen = strlen(timeout_msg);
        if (out_len + tmlen < sizeof(output)) {
            memcpy(output + out_len, timeout_msg, tmlen);
            out_len += tmlen;
            output[out_len] = '\0';
        }
    }

    if (out_len == 0) {
        const char *empty_msg = "(Cikti yok)";
        send_response(client_fd, 200, "OK", empty_msg, strlen(empty_msg));
    } else {
        send_response(client_fd, 200, "OK", output, out_len);
    }

    /* Cleanup */
    remove_dir_recursive(work_dir);
    close(client_fd);
}

/* --- Main server --- */

int main(int argc, char *argv[]) {
    int port = PORT;
    if (argc > 1) {
        /* Güvenli port parsing - geçerli aralık kontrolü */
        char *endptr;
        long port_long = strtol(argv[1], &endptr, 10);
        if (*endptr != '\0' || port_long < 1 || port_long > 65535) {
            fprintf(stderr, "Hata: Geçersiz port numarası (1-65535 arası olmalı)\n");
            return 1;
        }
        port = (int)port_long;
    }

    /* Ignore SIGCHLD to prevent zombie processes */
    signal(SIGCHLD, SIG_IGN);
    /* Ignore broken pipe */
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)port),
    };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    fprintf(stderr, "Playground API listening on 127.0.0.1:%d\n", port);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            /* Fork failed - handle in parent */
            send_error(client_fd, 500, "Sunucu hatasi");
            close(client_fd);
        } else if (pid == 0) {
            /* Child */
            close(server_fd);
            handle_request(client_fd);
            _exit(0);
        } else {
            /* Parent */
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}
