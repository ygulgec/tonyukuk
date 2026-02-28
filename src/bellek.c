#include "bellek.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArenaBlok *yeni_blok(int boyut, ArenaBlok *onceki) {
    ArenaBlok *blok = (ArenaBlok *)malloc(sizeof(ArenaBlok));
    if (!blok) { fprintf(stderr, "bellek yetersiz\n"); abort(); }
    blok->kapasite = boyut;
    blok->kullanilan = 0;
    blok->veri = (char *)malloc(boyut);
    if (!blok->veri) { fprintf(stderr, "bellek yetersiz\n"); free(blok); abort(); }
    blok->onceki = onceki;
    return blok;
}

void arena_baslat(Arena *a) {
    a->mevcut = yeni_blok(ARENA_BLOK_BOYUT, NULL);
}

void *arena_ayir(Arena *a, int boyut) {
    /* 8-byte hizalama */
    boyut = (boyut + 7) & ~7;

    if (a->mevcut->kullanilan + boyut > a->mevcut->kapasite) {
        int yeni_boyut = ARENA_BLOK_BOYUT;
        if (boyut > yeni_boyut) yeni_boyut = boyut;
        a->mevcut = yeni_blok(yeni_boyut, a->mevcut);
    }

    void *ptr = a->mevcut->veri + a->mevcut->kullanilan;
    a->mevcut->kullanilan += boyut;
    memset(ptr, 0, boyut);
    return ptr;
}

char *arena_strdup(Arena *a, const char *s) {
    int len = (int)strlen(s);
    char *kopya = (char *)arena_ayir(a, len + 1);
    memcpy(kopya, s, len + 1);
    return kopya;
}

char *arena_strndup(Arena *a, const char *s, int n) {
    char *kopya = (char *)arena_ayir(a, n + 1);
    memcpy(kopya, s, n);
    kopya[n] = '\0';
    return kopya;
}

void arena_serbest(Arena *a) {
    ArenaBlok *blok = a->mevcut;
    while (blok) {
        ArenaBlok *onceki = blok->onceki;
        free(blok->veri);
        free(blok);
        blok = onceki;
    }
    a->mevcut = NULL;
}
