#include "optimize.h"
#include "sozcuk.h"
#include <string.h>

/* ---- Sabit Katlama (Constant Folding) ---- */

/* İkili işlem düğümünü sabit katlama ile optimize et.
 * Her iki çocuk DÜĞÜM_TAM_SAYI ise sonucu hesapla ve düğümü dönüştür. */
static void sabit_katla(Düğüm *d) {
    if (d->tur != DÜĞÜM_İKİLİ_İŞLEM) return;
    if (d->çocuk_sayısı < 2) return;

    Düğüm *sol = d->çocuklar[0];
    Düğüm *sag = d->çocuklar[1];

    if (sol->tur != DÜĞÜM_TAM_SAYI || sag->tur != DÜĞÜM_TAM_SAYI) return;

    int64_t a = sol->veri.tam_deger;
    int64_t b = sag->veri.tam_deger;
    int64_t sonuç = 0;
    int hesaplandi = 1;

    switch (d->veri.islem.islem) {
    case TOK_ARTI:   sonuç = a + b; break;
    case TOK_EKSI:   sonuç = a - b; break;
    case TOK_ÇARPIM: sonuç = a * b; break;
    case TOK_BÖLME:
        if (b == 0) { hesaplandi = 0; break; }
        sonuç = a / b;
        break;
    case TOK_YÜZDE:
        if (b == 0) { hesaplandi = 0; break; }
        sonuç = a % b;
        break;
    default:
        hesaplandi = 0;
        break;
    }

    if (hesaplandi) {
        /* Düğümü DÜĞÜM_TAM_SAYI'ya dönüştür */
        d->tur = DÜĞÜM_TAM_SAYI;
        d->veri.tam_deger = sonuç;
        d->çocuk_sayısı = 0;
    }
}

/* ---- Güç Azaltma (Strength Reduction) ---- */

/* x * 2 -> x << 1, x * 4 -> x << 2, x / 2 -> x >> 1
 * İkili işlem düğümünü yerinde dönüştürür. */
static void guc_azalt(Düğüm *d) {
    if (d->tur != DÜĞÜM_İKİLİ_İŞLEM) return;
    if (d->çocuk_sayısı < 2) return;

    Düğüm *sol = d->çocuklar[0];
    Düğüm *sag = d->çocuklar[1];
    SözcükTürü op = d->veri.islem.islem;

    /* x * 2 -> x << 1 */
    if (op == TOK_ÇARPIM && sag->tur == DÜĞÜM_TAM_SAYI) {
        int64_t val = sag->veri.tam_deger;
        if (val == 2) {
            /* Sağ çocuğu 1 yap, işlemi kaydırma simüle et:
             * AST'de doğrudan kaydırma düğümü yok, bu yüzden
             * çarpma/bölme ile 2'nin kuvvetlerini katlama yaparız.
             * Sabit katlama ile aynı mantık: x * 2 = x + x
             * Ama daha iyi: DÜĞÜM_İKİLİ_İŞLEM olarak bırak,
             * sağ tarafı 1 yap, işlemi shift olarak işaretle.
             * Sorun: lexer'da shift token yok.
             * Alternatif: x * 2 -> x + x (kopyala)
             * En temiz: sabitten faydalanıp katlama yap.
             * Burada sağ tarafı 1 yapıp NOT shift,
             * assembly'de çarpma yerine shl kullanılmasını sağlayamayız.
             * O yüzden pratik güç azaltma: sabitin 2'nin kuvveti olup olmadığını
             * işaretle, üretici bunu okusun.
             * Basit yaklaşım: x * 2 -> x + x */

            /* x + x: sol düğümü kopyalamak yerine, 2'nin kuvveti çarpımları
             * sabit katlama ile zaten optimize olur. Assembly üreticide
             * bu bilgiyi kullanmak daha doğru.
             * Pragmatik çözüm: Sabit katlama yerine bırak, sadece
             * sağ taraf 1 olan çarpımları optimize et. */
            /* Basitçe: sag=2 ise sol+sol yap */
            d->veri.islem.islem = TOK_ARTI;
            d->çocuklar[1] = d->çocuklar[0]; /* x + x */
            return;
        }
    }

    /* 2 * x -> x + x */
    if (op == TOK_ÇARPIM && sol->tur == DÜĞÜM_TAM_SAYI && sol->veri.tam_deger == 2) {
        d->veri.islem.islem = TOK_ARTI;
        d->çocuklar[0] = d->çocuklar[1]; /* x + x */
        return;
    }

    /* x / 2 -> sağ kaydırma simülasyonu: bu mevcut AST'de temiz yapılamaz,
     * bırakıyoruz. Üretici seviyesinde yapılmalı. */
    (void)sol;
}

/* ---- Ölü Kod Eleme (Dead Code Elimination) ---- */

/* Bir blok içinde DÜĞÜM_DÖNDÜR'dan sonraki ifadeleri kaldır */
static void olu_kod_ele(Düğüm *d) {
    if (!d) return;

    /* Blok düğümlerinde DONDUR sonrası kodu kaldır */
    if (d->tur == DÜĞÜM_BLOK || d->tur == DÜĞÜM_PROGRAM) {
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            if (d->çocuklar[i]->tur == DÜĞÜM_DÖNDÜR) {
                /* Bu düğümden sonraki tüm çocukları kaldır */
                d->çocuk_sayısı = i + 1;
                break;
            }
        }
    }
}

/* ---- Özyinelemeli AST Yürüyüşü ---- */

static void optimize_dugum(Düğüm *d) {
    if (!d) return;

    /* Önce çocukları optimize et (alt-yukarı) */
    for (int i = 0; i < d->çocuk_sayısı; i++) {
        optimize_dugum(d->çocuklar[i]);
    }

    /* Sabit katlama */
    sabit_katla(d);

    /* Güç azaltma (sabit katlama'dan sonra, kalan çarpımlar için) */
    guc_azalt(d);

    /* Ölü kod eleme */
    olu_kod_ele(d);
}

void optimize_et(Düğüm *program) {
    if (!program) return;
    optimize_dugum(program);
}
