/* Sözlük modülü — çalışma zamanı implementasyonu */
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

/* Sozluk: basit hash map implementasyonu
   Her sozluk heap'te bir blok: [kapasite, eleman_sayisi, girdi_ptr]
   Her girdi: [anahtar_ptr, anahtar_len, deger (long long)]
   Basitlik icin: lineer arama, max 256 anahtar */

typedef struct {
    long long *veri;     /* heap blogu: [kap, say, anahtar_ptr, anahtar_len, deger, ...] */
    long long kapasite;
} TrSozluk;
long long _tr_sozluk_yeni(void) {
    /* 2 meta + 256 * 3 (anahtar_ptr, anahtar_len, deger) = 770 long long */
    long long boyut = 770 * sizeof(long long);
    long long *blok = (long long *)_tr_nesne_olustur(NESNE_TIP_SOZLUK, boyut);
    if (!blok) return 0;
    blok[0] = 256;  /* kapasite */
    blok[1] = 0;    /* eleman sayisi */
    return (long long)blok;
}

/* Sozluk'e deger ekle/guncelle: sozluk_ptr, anahtar_ptr, anahtar_len, deger */
long long _tr_sozluk_ekle(long long sozluk_ptr,
                            const char *anahtar_ptr, long long anahtar_len,
                            long long deger) {
    long long *blok = (long long *)sozluk_ptr;
    long long say = blok[1];
    /* Mevcut anahtari ara */
    for (long long i = 0; i < say; i++) {
        char *k = (char *)blok[2 + i * 3];
        long long kl = blok[2 + i * 3 + 1];
        if (kl == anahtar_len && memcmp(k, anahtar_ptr, anahtar_len) == 0) {
            blok[2 + i * 3 + 2] = deger;  /* guncelle */
            return sozluk_ptr;
        }
    }
    /* Yeni anahtar ekle */
    if (say >= blok[0]) return sozluk_ptr;  /* dolu */
    char *kopyala = (char *)malloc(anahtar_len);
    if (kopyala) memcpy(kopyala, anahtar_ptr, anahtar_len);
    blok[2 + say * 3] = (long long)kopyala;
    blok[2 + say * 3 + 1] = anahtar_len;
    blok[2 + say * 3 + 2] = deger;
    blok[1] = say + 1;
    return sozluk_ptr;
}

/* Sozluk'ten deger oku */
long long _tr_sozluk_oku(long long sozluk_ptr,
                           const char *anahtar_ptr, long long anahtar_len) {
    long long *blok = (long long *)sozluk_ptr;
    long long say = blok[1];
    for (long long i = 0; i < say; i++) {
        char *k = (char *)blok[2 + i * 3];
        long long kl = blok[2 + i * 3 + 1];
        if (kl == anahtar_len && memcmp(k, anahtar_ptr, anahtar_len) == 0) {
            return blok[2 + i * 3 + 2];
        }
    }
    return 0;  /* bulunamadi */
}

/* Sozluk eleman sayisi */
long long _tr_sozluk_uzunluk(long long sozluk_ptr) {
    long long *blok = (long long *)sozluk_ptr;
    return blok[1];
}

/* Sozluk anahtar var mi? */
long long _tr_sozluk_var_mi(long long sozluk_ptr,
                              const char *anahtar_ptr, long long anahtar_len) {
    long long *blok = (long long *)sozluk_ptr;
    long long say = blok[1];
    for (long long i = 0; i < say; i++) {
        char *k = (char *)blok[2 + i * 3];
        long long kl = blok[2 + i * 3 + 1];
        if (kl == anahtar_len && memcmp(k, anahtar_ptr, anahtar_len) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Sozluk anahtarlari dizi olarak dondur */
TrDizi _tr_sozluk_anahtarlar(long long sozluk_ptr) {
    long long *blok = (long long *)sozluk_ptr;
    long long say = blok[1];
    TrDizi d = {NULL, 0};
    if (say == 0) return d;

    long long boyut = say * 2 * sizeof(long long);
    long long *sonuç = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!sonuç) return d;

    for (long long i = 0; i < say; i++) {
        sonuç[i * 2] = blok[2 + i * 3];       /* anahtar ptr */
        sonuç[i * 2 + 1] = blok[2 + i * 3 + 1]; /* anahtar len */
    }
    d.ptr = sonuç;
    d.count = say;
    return d;
}
