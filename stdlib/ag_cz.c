/* Ağ/HTTP modülü — çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * Kapsamlı HTTP client desteği
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include "runtime.h"

/* ========== YARDIMCI YAPILAR ========== */

/* HTTP Yanıt yapısı */
typedef struct {
    int durum_kodu;
    char *basliklar;
    long long baslik_len;
    char *govde;
    long long govde_len;
} HttpYanit;

/* Son HTTP hatası */
static int son_http_durum = 0;
static char son_http_hata[256] = "";

/* ========== YARDIMCI FONKSİYONLAR ========== */

/* Boş metin döndür */
static TrMetin bos_metin(void) {
    TrMetin m = {NULL, 0};
    return m;
}

/* URL'den host, port, path çıkart */
static void url_parcala(const char *ptr, long long len,
                        char *host, int *port, char *path, int *https) {
    char url[4096];
    int n = (int)len;
    if (n >= (int)sizeof(url)) n = (int)sizeof(url) - 1;
    memcpy(url, ptr, n);
    url[n] = '\0';

    *port = 80;
    *https = 0;
    host[0] = '\0';
    snprintf(path, 2048, "/");

    char *p = url;

    /* https:// kontrol */
    if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        *port = 443;
        *https = 1;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }

    /* host ve path ayır */
    char *slash = strchr(p, '/');
    if (slash) {
        snprintf(path, 2048, "%s", slash);
        *slash = '\0';
    }

    /* host:port ayır */
    char *colon = strchr(p, ':');
    if (colon) {
        *port = atoi(colon + 1);
        *colon = '\0';
    }
    snprintf(host, 512, "%s", p);
}

/* Soket bağlantısı oluştur */
static int soket_baglan(const char *host, int port, int timeout_ms) {
    struct hostent *he = gethostbyname(host);
    if (!he) {
        snprintf(son_http_hata, sizeof(son_http_hata), "Host çözümlenemedi: %s", host);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        snprintf(son_http_hata, sizeof(son_http_hata), "Soket oluşturulamadı");
        return -1;
    }

    /* Non-blocking yap */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    int ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close(fd);
        snprintf(son_http_hata, sizeof(son_http_hata), "Bağlantı başarısız");
        return -1;
    }

    /* Bağlantıyı bekle */
    if (timeout_ms > 0) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        ret = poll(&pfd, 1, timeout_ms);
        if (ret <= 0) {
            close(fd);
            snprintf(son_http_hata, sizeof(son_http_hata), "Bağlantı zaman aşımı");
            return -1;
        }
    }

    /* Blocking'e geri dön */
    fcntl(fd, F_SETFL, flags);
    return fd;
}

/* HTTP yanıtını oku */
static HttpYanit http_yanit_oku(int fd, int timeout_ms) {
    HttpYanit yanit = {0, NULL, 0, NULL, 0};

    char *buf = (char *)malloc(65536);
    if (!buf) return yanit;

    int toplam = 0;
    int kap = 65536;

    /* Zaman aşımı ile oku */
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (1) {
        if (toplam >= kap - 1) {
            kap *= 2;
            char *yeni = (char *)realloc(buf, kap);
            if (!yeni) break;
            buf = yeni;
        }

        int ret = poll(&pfd, 1, timeout_ms > 0 ? timeout_ms : 30000);
        if (ret <= 0) break;

        int n = (int)recv(fd, buf + toplam, kap - toplam - 1, 0);
        if (n <= 0) break;
        toplam += n;
    }
    buf[toplam] = '\0';

    /* Durum kodunu çıkar */
    char *status_line = strstr(buf, "HTTP/");
    if (status_line) {
        char *space = strchr(status_line, ' ');
        if (space) {
            yanit.durum_kodu = atoi(space + 1);
            son_http_durum = yanit.durum_kodu;
        }
    }

    /* Body'yi bul */
    char *body = strstr(buf, "\r\n\r\n");
    if (body) {
        /* Başlıkları kaydet */
        long long baslik_len = body - buf;
        yanit.basliklar = (char *)malloc(baslik_len + 1);
        if (yanit.basliklar) {
            memcpy(yanit.basliklar, buf, baslik_len);
            yanit.basliklar[baslik_len] = '\0';
            yanit.baslik_len = baslik_len;
        }

        body += 4;
        long long body_len = toplam - (body - buf);
        yanit.govde = (char *)malloc(body_len + 1);
        if (yanit.govde) {
            memcpy(yanit.govde, body, body_len);
            yanit.govde[body_len] = '\0';
            yanit.govde_len = body_len;
        }
    }

    free(buf);
    return yanit;
}

/* ========== TEMEL HTTP FONKSİYONLARI ========== */

/* http_al(url) -> metin */
TrMetin _tr_http_al(const char *url_ptr, long long url_len) {
    TrMetin m = bos_metin();
    char host[512], path[2048];
    int port, https;
    url_parcala(url_ptr, url_len, host, &port, path, &https);

    if (https) {
        snprintf(son_http_hata, sizeof(son_http_hata), "HTTPS henüz desteklenmiyor");
        return m;
    }

    int fd = soket_baglan(host, port, 10000);
    if (fd < 0) return m;

    /* HTTP GET isteği */
    char istek[4096];
    int istek_len = snprintf(istek, sizeof(istek),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Tonyukuk/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n",
        path, host);
    send(fd, istek, istek_len, 0);

    HttpYanit yanit = http_yanit_oku(fd, 30000);
    close(fd);

    if (yanit.govde) {
        m.ptr = yanit.govde;
        m.len = yanit.govde_len;
    }
    free(yanit.basliklar);
    return m;
}

/* http_gonder(url, veri) -> metin (POST) */
TrMetin _tr_http_gonder(const char *url_ptr, long long url_len,
                         const char *veri_ptr, long long veri_len) {
    TrMetin m = bos_metin();
    char host[512], path[2048];
    int port, https;
    url_parcala(url_ptr, url_len, host, &port, path, &https);

    if (https) {
        snprintf(son_http_hata, sizeof(son_http_hata), "HTTPS henüz desteklenmiyor");
        return m;
    }

    int fd = soket_baglan(host, port, 10000);
    if (fd < 0) return m;

    /* HTTP POST isteği */
    char baslik[4096];
    int baslik_len = snprintf(baslik, sizeof(baslik),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Tonyukuk/1.0\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n\r\n",
        path, host, veri_len);
    send(fd, baslik, baslik_len, 0);
    if (veri_len > 0 && veri_ptr) {
        send(fd, veri_ptr, (size_t)veri_len, 0);
    }

    HttpYanit yanit = http_yanit_oku(fd, 30000);
    close(fd);

    if (yanit.govde) {
        m.ptr = yanit.govde;
        m.len = yanit.govde_len;
    }
    free(yanit.basliklar);
    return m;
}

/* ========== GELİŞMİŞ HTTP FONKSİYONLARI ========== */

/* http_json_gonder(url, json_veri) -> metin (POST JSON) */
TrMetin _tr_http_json_gonder(const char *url_ptr, long long url_len,
                              const char *veri_ptr, long long veri_len) {
    TrMetin m = bos_metin();
    char host[512], path[2048];
    int port, https;
    url_parcala(url_ptr, url_len, host, &port, path, &https);

    if (https) {
        snprintf(son_http_hata, sizeof(son_http_hata), "HTTPS henüz desteklenmiyor");
        return m;
    }

    int fd = soket_baglan(host, port, 10000);
    if (fd < 0) return m;

    /* HTTP POST JSON isteği */
    char baslik[4096];
    int baslik_len = snprintf(baslik, sizeof(baslik),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Tonyukuk/1.0\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n\r\n",
        path, host, veri_len);
    send(fd, baslik, baslik_len, 0);
    if (veri_len > 0 && veri_ptr) {
        send(fd, veri_ptr, (size_t)veri_len, 0);
    }

    HttpYanit yanit = http_yanit_oku(fd, 30000);
    close(fd);

    if (yanit.govde) {
        m.ptr = yanit.govde;
        m.len = yanit.govde_len;
    }
    free(yanit.basliklar);
    return m;
}

/* http_put(url, veri) -> metin */
TrMetin _tr_http_put(const char *url_ptr, long long url_len,
                      const char *veri_ptr, long long veri_len) {
    TrMetin m = bos_metin();
    char host[512], path[2048];
    int port, https;
    url_parcala(url_ptr, url_len, host, &port, path, &https);

    if (https) return m;

    int fd = soket_baglan(host, port, 10000);
    if (fd < 0) return m;

    char baslik[4096];
    int baslik_len = snprintf(baslik, sizeof(baslik),
        "PUT %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Tonyukuk/1.0\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n\r\n",
        path, host, veri_len);
    send(fd, baslik, baslik_len, 0);
    if (veri_len > 0 && veri_ptr) {
        send(fd, veri_ptr, (size_t)veri_len, 0);
    }

    HttpYanit yanit = http_yanit_oku(fd, 30000);
    close(fd);

    if (yanit.govde) {
        m.ptr = yanit.govde;
        m.len = yanit.govde_len;
    }
    free(yanit.basliklar);
    return m;
}

/* http_sil(url) -> metin (DELETE) */
TrMetin _tr_http_sil(const char *url_ptr, long long url_len) {
    TrMetin m = bos_metin();
    char host[512], path[2048];
    int port, https;
    url_parcala(url_ptr, url_len, host, &port, path, &https);

    if (https) return m;

    int fd = soket_baglan(host, port, 10000);
    if (fd < 0) return m;

    char istek[4096];
    int istek_len = snprintf(istek, sizeof(istek),
        "DELETE %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Tonyukuk/1.0\r\n"
        "Connection: close\r\n\r\n",
        path, host);
    send(fd, istek, istek_len, 0);

    HttpYanit yanit = http_yanit_oku(fd, 30000);
    close(fd);

    if (yanit.govde) {
        m.ptr = yanit.govde;
        m.len = yanit.govde_len;
    }
    free(yanit.basliklar);
    return m;
}

/* http_head(url) -> metin (HEAD - sadece başlıklar) */
TrMetin _tr_http_head(const char *url_ptr, long long url_len) {
    TrMetin m = bos_metin();
    char host[512], path[2048];
    int port, https;
    url_parcala(url_ptr, url_len, host, &port, path, &https);

    if (https) return m;

    int fd = soket_baglan(host, port, 10000);
    if (fd < 0) return m;

    char istek[4096];
    int istek_len = snprintf(istek, sizeof(istek),
        "HEAD %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Tonyukuk/1.0\r\n"
        "Connection: close\r\n\r\n",
        path, host);
    send(fd, istek, istek_len, 0);

    HttpYanit yanit = http_yanit_oku(fd, 30000);
    close(fd);

    if (yanit.basliklar) {
        m.ptr = yanit.basliklar;
        m.len = yanit.baslik_len;
    }
    free(yanit.govde);
    return m;
}

/* ========== ÖZEL BAŞLIKLI İSTEKLER ========== */

/* http_al_baslikli(url, basliklar) -> metin */
TrMetin _tr_http_al_baslikli(const char *url_ptr, long long url_len,
                              const char *baslik_ptr, long long baslik_len) {
    TrMetin m = bos_metin();
    char host[512], path[2048];
    int port, https;
    url_parcala(url_ptr, url_len, host, &port, path, &https);

    if (https) return m;

    int fd = soket_baglan(host, port, 10000);
    if (fd < 0) return m;

    /* Kullanıcı başlıklarını null-terminate et */
    char *basliklar = (char *)malloc(baslik_len + 1);
    if (!basliklar) { close(fd); return m; }
    memcpy(basliklar, baslik_ptr, baslik_len);
    basliklar[baslik_len] = '\0';

    char istek[8192];
    int istek_len = snprintf(istek, sizeof(istek),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Tonyukuk/1.0\r\n"
        "%s%s"
        "Connection: close\r\n\r\n",
        path, host, basliklar,
        (baslik_len > 0 && basliklar[baslik_len-1] != '\n') ? "\r\n" : "");
    free(basliklar);
    send(fd, istek, istek_len, 0);

    HttpYanit yanit = http_yanit_oku(fd, 30000);
    close(fd);

    if (yanit.govde) {
        m.ptr = yanit.govde;
        m.len = yanit.govde_len;
    }
    free(yanit.basliklar);
    return m;
}

/* http_gonder_baslikli(url, veri, basliklar) -> metin */
TrMetin _tr_http_gonder_baslikli(const char *url_ptr, long long url_len,
                                  const char *veri_ptr, long long veri_len,
                                  const char *baslik_ptr, long long baslik_len) {
    TrMetin m = bos_metin();
    char host[512], path[2048];
    int port, https;
    url_parcala(url_ptr, url_len, host, &port, path, &https);

    if (https) return m;

    int fd = soket_baglan(host, port, 10000);
    if (fd < 0) return m;

    char *basliklar = (char *)malloc(baslik_len + 1);
    if (!basliklar) { close(fd); return m; }
    memcpy(basliklar, baslik_ptr, baslik_len);
    basliklar[baslik_len] = '\0';

    char istek_baslik[8192];
    int len = snprintf(istek_baslik, sizeof(istek_baslik),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Tonyukuk/1.0\r\n"
        "Content-Length: %lld\r\n"
        "%s%s"
        "Connection: close\r\n\r\n",
        path, host, veri_len, basliklar,
        (baslik_len > 0 && basliklar[baslik_len-1] != '\n') ? "\r\n" : "");
    free(basliklar);
    send(fd, istek_baslik, len, 0);
    if (veri_len > 0 && veri_ptr) {
        send(fd, veri_ptr, (size_t)veri_len, 0);
    }

    HttpYanit yanit = http_yanit_oku(fd, 30000);
    close(fd);

    if (yanit.govde) {
        m.ptr = yanit.govde;
        m.len = yanit.govde_len;
    }
    free(yanit.basliklar);
    return m;
}

/* ========== DURUM BİLGİLERİ ========== */

/* http_durum_kodu() -> tam: Son HTTP durum kodu */
long long _tr_http_durum_kodu(void) {
    return son_http_durum;
}

/* http_hata_mesaji() -> metin: Son hata mesajı */
TrMetin _tr_http_hata_mesaji(void) {
    TrMetin m;
    int len = (int)strlen(son_http_hata);
    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, son_http_hata, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* ========== URL YARDIMCI FONKSİYONLAR ========== */

/* url_kodla(metin) -> metin: URL encode */
TrMetin _tr_url_kodla(const char *ptr, long long len) {
    TrMetin m = bos_metin();

    /* En kötü durumda 3 kat büyür */
    char *buf = (char *)malloc(len * 3 + 1);
    if (!buf) return m;

    long long pos = 0;
    for (long long i = 0; i < len; i++) {
        unsigned char c = (unsigned char)ptr[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            buf[pos++] = c;
        } else {
            sprintf(buf + pos, "%%%02X", c);
            pos += 3;
        }
    }
    buf[pos] = '\0';

    m.ptr = (char *)malloc(pos + 1);
    if (m.ptr) {
        memcpy(m.ptr, buf, pos + 1);
        m.len = pos;
    }
    free(buf);
    return m;
}

/* url_coz(metin) -> metin: URL decode */
TrMetin _tr_url_coz(const char *ptr, long long len) {
    TrMetin m = bos_metin();

    char *buf = (char *)malloc(len + 1);
    if (!buf) return m;

    long long pos = 0;
    for (long long i = 0; i < len; i++) {
        if (ptr[i] == '%' && i + 2 < len) {
            char hex[3] = {ptr[i+1], ptr[i+2], 0};
            buf[pos++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (ptr[i] == '+') {
            buf[pos++] = ' ';
        } else {
            buf[pos++] = ptr[i];
        }
    }
    buf[pos] = '\0';

    m.ptr = (char *)malloc(pos + 1);
    if (m.ptr) {
        memcpy(m.ptr, buf, pos + 1);
        m.len = pos;
    }
    free(buf);
    return m;
}

/* ========== BASİT HTTP SUNUCU ========== */

/* Global sunucu soket */
static int sunucu_soket = -1;

/* http_sunucu_baslat(port) -> tam (başarı: 0, hata: -1) */
long long _tr_http_sunucu_baslat(long long port) {
    if (sunucu_soket >= 0) {
        close(sunucu_soket);
    }

    sunucu_soket = socket(AF_INET, SOCK_STREAM, 0);
    if (sunucu_soket < 0) return -1;

    /* Adres yeniden kullanımı */
    int opt = 1;
    setsockopt(sunucu_soket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(sunucu_soket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sunucu_soket);
        sunucu_soket = -1;
        return -1;
    }

    if (listen(sunucu_soket, 10) < 0) {
        close(sunucu_soket);
        sunucu_soket = -1;
        return -1;
    }

    return 0;
}

/* http_sunucu_durdur() -> boşluk */
void _tr_http_sunucu_durdur(void) {
    if (sunucu_soket >= 0) {
        close(sunucu_soket);
        sunucu_soket = -1;
    }
}

/* http_istek_bekle() -> tam (client soket veya -1) */
long long _tr_http_istek_bekle(void) {
    if (sunucu_soket < 0) return -1;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(sunucu_soket, (struct sockaddr *)&client_addr, &client_len);

    return client_fd;
}

/* http_istek_oku(soket) -> metin (ham HTTP isteği) */
TrMetin _tr_http_istek_oku(long long soket) {
    TrMetin m = bos_metin();
    if (soket < 0) return m;

    char buf[8192];
    int n = (int)recv((int)soket, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return m;
    buf[n] = '\0';

    m.ptr = (char *)malloc(n + 1);
    if (m.ptr) {
        memcpy(m.ptr, buf, n + 1);
        m.len = n;
    }
    return m;
}

/* http_yanit_gonder(soket, durum_kodu, icerik, icerik_tipi) -> tam */
long long _tr_http_yanit_gonder(long long soket, long long durum_kodu,
                                 const char *icerik_ptr, long long icerik_len,
                                 const char *tip_ptr, long long tip_len) {
    if (soket < 0) return -1;

    char *tip = (char *)malloc(tip_len + 1);
    if (!tip) return -1;
    memcpy(tip, tip_ptr, tip_len);
    tip[tip_len] = '\0';

    const char *durum_metin = "OK";
    if (durum_kodu == 201) durum_metin = "Created";
    else if (durum_kodu == 204) durum_metin = "No Content";
    else if (durum_kodu == 400) durum_metin = "Bad Request";
    else if (durum_kodu == 401) durum_metin = "Unauthorized";
    else if (durum_kodu == 403) durum_metin = "Forbidden";
    else if (durum_kodu == 404) durum_metin = "Not Found";
    else if (durum_kodu == 500) durum_metin = "Internal Server Error";

    char baslik[1024];
    int baslik_len = snprintf(baslik, sizeof(baslik),
        "HTTP/1.1 %lld %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n\r\n",
        durum_kodu, durum_metin, tip, icerik_len);
    free(tip);

    send((int)soket, baslik, baslik_len, 0);
    if (icerik_len > 0 && icerik_ptr) {
        send((int)soket, icerik_ptr, (size_t)icerik_len, 0);
    }

    return 0;
}

/* http_baglanti_kapat(soket) -> boşluk */
void _tr_http_baglanti_kapat(long long soket) {
    if (soket >= 0) {
        close((int)soket);
    }
}
