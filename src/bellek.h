#ifndef BELLEK_H
#define BELLEK_H

#include <stddef.h>

/* Arena bellek yönetimi */
#define ARENA_BLOK_BOYUT (64 * 1024)

typedef struct ArenaBlok {
    char *veri;
    int   kapasite;
    int   kullanilan;
    struct ArenaBlok *onceki;
} ArenaBlok;

typedef struct {
    ArenaBlok *mevcut;
} Arena;

void  arena_baslat(Arena *a);
void *arena_ayir(Arena *a, int boyut);
char *arena_strdup(Arena *a, const char *s);
char *arena_strndup(Arena *a, const char *s, int n);
void  arena_serbest(Arena *a);

#endif
