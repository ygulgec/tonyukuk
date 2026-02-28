#include "tablo.h"
#include <string.h>

static unsigned int hash(const char *s) {
    unsigned int h = 2166136261u;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h;
}

Kapsam *kapsam_oluştur(Arena *a, Kapsam *ust) {
    Kapsam *k = (Kapsam *)arena_ayir(a, sizeof(Kapsam));
    memset(k->tablo, 0, sizeof(k->tablo));
    k->sembol_sayisi = 0;
    k->ust = ust;
    k->yerel_sayac = ust ? ust->yerel_sayac : 0;
    return k;
}

Sembol *sembol_ekle(Arena *a, Kapsam *k, const char *isim, TipTürü tip) {
    unsigned int idx = hash(isim) % TABLO_BOYUT;

    /* Çakışma durumunda lineer tarama */
    for (int i = 0; i < TABLO_BOYUT; i++) {
        int pos = (idx + i) % TABLO_BOYUT;
        if (k->tablo[pos] == NULL) {
            Sembol *s = (Sembol *)arena_ayir(a, sizeof(Sembol));
            s->isim = arena_strdup(a, isim);
            s->tip = tip;
            s->yerel_indeks = k->yerel_sayac++;
            s->parametre_mi = 0;
            s->global_mi = 0;  /* tüm değişkenler stack-based */
            k->tablo[pos] = s;
            k->sembol_sayisi++;
            return s;
        }
        if (strcmp(k->tablo[pos]->isim, isim) == 0) {
            return k->tablo[pos]; /* zaten var */
        }
    }
    return NULL; /* tablo dolu */
}

Sembol *sembol_ara(Kapsam *k, const char *isim) {
    while (k) {
        unsigned int idx = hash(isim) % TABLO_BOYUT;
        for (int i = 0; i < TABLO_BOYUT; i++) {
            int pos = (idx + i) % TABLO_BOYUT;
            if (k->tablo[pos] == NULL) break;
            if (strcmp(k->tablo[pos]->isim, isim) == 0) {
                return k->tablo[pos];
            }
        }
        k = k->ust;
    }
    return NULL;
}

TipTürü tip_adı_çevir(const char *isim) {
    if (!isim) return TİP_BİLİNMİYOR;
    /* UTF-8 byte karşılaştırması */
    if (strcmp(isim, "tam") == 0) return TİP_TAM;
    if (strcmp(isim, "ondalık") == 0 || strcmp(isim, "ondal\xc4\xb1k") == 0) return TİP_ONDALIK;
    if (strcmp(isim, "metin") == 0) return TİP_METİN;
    if (strcmp(isim, "mantık") == 0 || strcmp(isim, "mant\xc4\xb1k") == 0) return TİP_MANTIK;
    if (strcmp(isim, "dizi") == 0) return TİP_DİZİ;
    if (strcmp(isim, "boşluk") == 0 || strcmp(isim, "bo\xc5\x9fluk") == 0) return TİP_BOŞLUK;
    if (strcmp(isim, "s\xc3\xb6zl\xc3\xbck") == 0 || strcmp(isim, "sozluk") == 0) return TİP_SÖZLÜK;
    if (strcmp(isim, "k\xc3\xbcme") == 0 || strcmp(isim, "kume") == 0) return TİP_KÜME;
    /* Sonuç/Seçenek tipler (parametreli veya parametresiz) */
    if (strncmp(isim, "Sonu\xc3\xa7", 6) == 0 || strncmp(isim, "Sonuc", 5) == 0) return TİP_SONUÇ;
    if (strncmp(isim, "Se\xc3\xa7enek", 8) == 0 || strncmp(isim, "Secenek", 7) == 0) return TİP_SEÇENEK;
    return TİP_BİLİNMİYOR;
}

const char *tip_adı(TipTürü tip) {
    switch (tip) {
    case TİP_TAM:        return "tam";
    case TİP_ONDALIK:    return "ondalık";
    case TİP_METİN:      return "metin";
    case TİP_MANTIK:     return "mantık";
    case TİP_DİZİ:       return "dizi";
    case TİP_SINIF:      return "sınıf";
    case TİP_BOŞLUK:     return "boşluk";
    case TİP_İŞLEV:      return "işlev";
    case TİP_SÖZLÜK:     return "sözlük";
    case TİP_SAYIM:      return "sayım";
    case TİP_KÜME:       return "küme";
    case TİP_SONUÇ:      return "Sonuç";
    case TİP_SEÇENEK:    return "Seçenek";
    case TİP_BİLİNMİYOR: return "bilinmiyor";
    }
    return "bilinmiyor";
}

SinifBilgi *sınıf_bul(Kapsam *k, const char *isim) {
    Sembol *s = sembol_ara(k, isim);
    if (s && s->sınıf_bilgi) return s->sınıf_bilgi;
    return NULL;
}
