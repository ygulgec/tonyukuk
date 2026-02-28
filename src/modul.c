#include "modul.h"
#include <string.h>

const ModülTanım *modul_bul(const char *isim) {
    if (!isim) return NULL;
    for (int i = 0; i < modul_sayisi; i++) {
        if (strcmp(isim, tum_moduller[i]->isim) == 0)
            return tum_moduller[i];
        if (tum_moduller[i]->ascii_isim &&
            strcmp(isim, tum_moduller[i]->ascii_isim) == 0)
            return tum_moduller[i];
    }
    return NULL;
}

const ModülFonksiyon *modul_fonksiyon_bul(const char *isim) {
    if (!isim) return NULL;
    for (int i = 0; i < modul_sayisi; i++) {
        const ModülTanım *m = tum_moduller[i];
        for (int j = 0; j < m->fonksiyon_sayisi; j++) {
            const ModülFonksiyon *f = &m->fonksiyonlar[j];
            if (strcmp(isim, f->isim) == 0) return f;
            if (f->ascii_isim && strcmp(isim, f->ascii_isim) == 0) return f;
        }
    }
    return NULL;
}
