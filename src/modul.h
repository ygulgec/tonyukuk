#ifndef MODÜL_H
#define MODÜL_H

#include "tablo.h"

/* Tek bir modül fonksiyonunun tanımı */
typedef struct {
    const char *isim;            /* Türkçe isim, UTF-8 (ör: "küpkök") */
    const char *ascii_isim;      /* ASCII alternatif, yoksa NULL (ör: "kupkok") */
    const char *runtime_isim;    /* C sembolü (ör: "_tr_kupkok") */
    TipTürü     param_tipleri[8];
    int         param_sayisi;
    TipTürü     dönüş_tipi;
} ModülFonksiyon;

/* Modül tanımı */
typedef struct {
    const char           *isim;            /* Modül adı (ör: "matematik") */
    const char           *ascii_isim;      /* ASCII alternatif, yoksa NULL */
    const ModülFonksiyon *fonksiyonlar;
    int                   fonksiyon_sayisi;
} ModülTanım;

/* Kayıtlı tüm modüller (modul_kayit_gen.c tarafından üretilir) */
extern const ModülTanım *tum_moduller[];
extern int modul_sayisi;

/* İsimle modül bul — NULL döndürürse bulunamadı */
const ModülTanım *modul_bul(const char *isim);

/* Tüm modüllerde fonksiyon ara (isim veya ascii_isim ile) */
const ModülFonksiyon *modul_fonksiyon_bul(const char *isim);

#endif
