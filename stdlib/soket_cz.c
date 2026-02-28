/* Soket modülü — çalışma zamanı implementasyonu */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "runtime.h"

/* ========== MODUL 10: Soket (POSIX TCP) ========== */

long long _tr_soket_olustur(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    return (long long)fd;
}

long long _tr_soket_baglan(long long fd, const char *adres_ptr, long long adres_len, long long port) {
    char adres[256];
    int n = (int)adres_len;
    if (n >= (int)sizeof(adres)) n = (int)sizeof(adres) - 1;
    memcpy(adres, adres_ptr, n);
    adres[n] = '\0';

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%lld", (long long)port);

    if (getaddrinfo(adres, port_str, &hints, &res) != 0) return 0;

    int rc = connect((int)fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return (rc == 0) ? 1 : 0;
}

long long _tr_soket_dinle(long long port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 128) < 0) {
        close(fd);
        return -1;
    }
    return (long long)fd;
}

long long _tr_soket_kabul(long long fd) {
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int client_fd = accept((int)fd, (struct sockaddr *)&client, &client_len);
    return (long long)client_fd;
}

long long _tr_soket_gonder(long long fd, const char *veri_ptr, long long veri_len) {
    ssize_t sent = send((int)fd, veri_ptr, (size_t)veri_len, 0);
    return (long long)sent;
}

TrMetin _tr_soket_al(long long fd) {
    TrMetin m = {NULL, 0};
    char buf[8192];
    ssize_t n = recv((int)fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return m;
    m.ptr = (char *)malloc(n);
    if (m.ptr) memcpy(m.ptr, buf, n);
    m.len = (long long)n;
    return m;
}

void _tr_soket_kapat(long long fd) {
    close((int)fd);
}
