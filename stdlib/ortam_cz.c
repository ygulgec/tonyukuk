/* Ortam modülü — çalışma zamanı implementasyonu */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

extern char **environ;

/* ========== MODUL 8: Ortam Degiskenleri ========== */

extern char **environ;

TrMetin _tr_ortam_al(const char *key_ptr, long long key_len) {
    TrMetin m = {NULL, 0};
    char key[512];
    int n = (int)key_len;
    if (n >= (int)sizeof(key)) n = (int)sizeof(key) - 1;
    memcpy(key, key_ptr, n);
    key[n] = '\0';

    const char *val = getenv(key);
    if (!val) return m;
    int vlen = (int)strlen(val);
    m.ptr = (char *)malloc(vlen);
    if (m.ptr) memcpy(m.ptr, val, vlen);
    m.len = vlen;
    return m;
}

void _tr_ortam_koy(const char *key_ptr, long long key_len,
                    const char *val_ptr, long long val_len) {
    char key[512], val[4096];
    int kn = (int)key_len;
    if (kn >= (int)sizeof(key)) kn = (int)sizeof(key) - 1;
    memcpy(key, key_ptr, kn); key[kn] = '\0';

    int vn = (int)val_len;
    if (vn >= (int)sizeof(val)) vn = (int)sizeof(val) - 1;
    memcpy(val, val_ptr, vn); val[vn] = '\0';

    setenv(key, val, 1);
}

void _tr_ortam_sil(const char *key_ptr, long long key_len) {
    char key[512];
    int n = (int)key_len;
    if (n >= (int)sizeof(key)) n = (int)sizeof(key) - 1;
    memcpy(key, key_ptr, n); key[n] = '\0';
    unsetenv(key);
}

TrDizi _tr_ortam_hepsi(void) {
    TrDizi sonuç = {NULL, 0};
    /* Count env vars */
    int count = 0;
    for (char **e = environ; *e; e++) count++;

    long long boyut = count * 2 * sizeof(long long);
    long long *veri = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!veri) return sonuç;

    for (int i = 0; i < count; i++) {
        int elen = (int)strlen(environ[i]);
        char *kopya = (char *)malloc(elen);
        if (kopya) memcpy(kopya, environ[i], elen);
        veri[i * 2] = (long long)(intptr_t)kopya;
        veri[i * 2 + 1] = elen;
    }
    sonuç.ptr = veri;
    sonuç.count = count;
    return sonuç;
}
