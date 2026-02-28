#ifndef METİN_H
#define METİN_H

#include <stddef.h>

/* Dinamik string buffer */
typedef struct {
    char  *veri;
    int    uzunluk;
    int    kapasite;
} Metin;

void metin_baslat(Metin *m);
void metin_serbest(Metin *m);
void metin_ekle(Metin *m, const char *s);
void metin_ekle_n(Metin *m, const char *s, int n);
void metin_ekle_karakter(Metin *m, char c);
void metin_ekle_sayi(Metin *m, int sayi);
void metin_temizle(Metin *m);
/* Satır ekle (sonuna \n ekler) */
void metin_satir_ekle(Metin *m, const char *s);

#endif
