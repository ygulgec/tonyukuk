/* Argüman modülü — çalışma zamanı implementasyonu */
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

/* ========== MODUL 9: Arguman (argc/argv) ========== */

/* Global argc/argv - main'den ayarlanacak */
int _tr_argc = 0;
char **_tr_argv = NULL;

long long _tr_arguman_sayisi(void) {
    return (long long)_tr_argc;
}

TrMetin _tr_arguman_al(long long indeks) {
    TrMetin m = {NULL, 0};
    if (indeks < 0 || indeks >= _tr_argc) return m;
    const char *arg = _tr_argv[indeks];
    int alen = (int)strlen(arg);
    m.ptr = (char *)malloc(alen);
    if (m.ptr) memcpy(m.ptr, arg, alen);
    m.len = alen;
    return m;
}

TrDizi _tr_arguman_hepsi(void) {
    TrDizi sonuç = {NULL, 0};
    if (_tr_argc <= 0) return sonuç;

    long long boyut = _tr_argc * 2 * sizeof(long long);
    long long *veri = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!veri) return sonuç;

    for (int i = 0; i < _tr_argc; i++) {
        int alen = (int)strlen(_tr_argv[i]);
        char *kopya = (char *)malloc(alen);
        if (kopya) memcpy(kopya, _tr_argv[i], alen);
        veri[i * 2] = (long long)(intptr_t)kopya;
        veri[i * 2 + 1] = alen;
    }
    sonuç.ptr = veri;
    sonuç.count = _tr_argc;
    return sonuç;
}
