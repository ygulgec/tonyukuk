#ifndef ÇÖZÜMLEYİCİ_H
#define ÇÖZÜMLEYİCİ_H

#include "sozcuk.h"
#include "agac.h"
#include "bellek.h"

typedef struct {
    Sözcük *sozcukler;
    int     sozcuk_sayisi;
    int     pos;
    Arena  *arena;
    int     panik_modu;     /* Panik modunda mı? (hata kurtarma) */
} Cozumleyici;

/* Sözcük dizisini parse et, AST kökünü döndür */
Düğüm *cozumle(Cozumleyici *c, Sözcük *sozcukler, int sozcuk_sayisi, Arena *arena);

#endif
