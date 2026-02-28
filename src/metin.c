#include "metin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void metin_baslat(Metin *m) {
    m->kapasite = 256;
    m->veri = (char *)malloc(m->kapasite);
    if (!m->veri) { fprintf(stderr, "bellek yetersiz\n"); abort(); }
    m->veri[0] = '\0';
    m->uzunluk = 0;
}

void metin_serbest(Metin *m) {
    free(m->veri);
    m->veri = NULL;
    m->uzunluk = 0;
    m->kapasite = 0;
}

static void metin_genislet(Metin *m, int ek) {
    while (m->uzunluk + ek + 1 > m->kapasite) {
        m->kapasite *= 2;
    }
    char *yeni = (char *)realloc(m->veri, m->kapasite);
    if (!yeni) { fprintf(stderr, "bellek yetersiz\n"); abort(); }
    m->veri = yeni;
}

void metin_ekle(Metin *m, const char *s) {
    int len = (int)strlen(s);
    metin_genislet(m, len);
    memcpy(m->veri + m->uzunluk, s, len);
    m->uzunluk += len;
    m->veri[m->uzunluk] = '\0';
}

void metin_ekle_n(Metin *m, const char *s, int n) {
    metin_genislet(m, n);
    memcpy(m->veri + m->uzunluk, s, n);
    m->uzunluk += n;
    m->veri[m->uzunluk] = '\0';
}

void metin_ekle_karakter(Metin *m, char c) {
    metin_genislet(m, 1);
    m->veri[m->uzunluk++] = c;
    m->veri[m->uzunluk] = '\0';
}

void metin_ekle_sayi(Metin *m, int sayi) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", sayi);
    metin_ekle(m, buf);
}

void metin_temizle(Metin *m) {
    m->uzunluk = 0;
    m->veri[0] = '\0';
}

void metin_satir_ekle(Metin *m, const char *s) {
    metin_ekle(m, s);
    metin_ekle_karakter(m, '\n');
}
