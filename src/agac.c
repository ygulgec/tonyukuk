#include "agac.h"
#include <string.h>

Düğüm *düğüm_oluştur(Arena *a, DüğümTürü tur, int satir, int sutun) {
    Düğüm *d = (Düğüm *)arena_ayir(a, sizeof(Düğüm));
    d->tur = tur;
    d->satir = satir;
    d->sutun = sutun;
    d->çocuklar = NULL;
    d->çocuk_sayısı = 0;
    d->çocuk_kapasite = 0;
    memset(&d->veri, 0, sizeof(d->veri));
    return d;
}

void düğüm_çocuk_ekle(Arena *a, Düğüm *ebeveyn, Düğüm *cocuk) {
    if (ebeveyn->çocuk_sayısı >= ebeveyn->çocuk_kapasite) {
        int yeni_kap = ebeveyn->çocuk_kapasite == 0 ? 4 : ebeveyn->çocuk_kapasite * 2;
        Düğüm **yeni = (Düğüm **)arena_ayir(a, yeni_kap * sizeof(Düğüm *));
        if (ebeveyn->çocuklar) {
            memcpy(yeni, ebeveyn->çocuklar, ebeveyn->çocuk_sayısı * sizeof(Düğüm *));
        }
        ebeveyn->çocuklar = yeni;
        ebeveyn->çocuk_kapasite = yeni_kap;
    }
    ebeveyn->çocuklar[ebeveyn->çocuk_sayısı++] = cocuk;
}
