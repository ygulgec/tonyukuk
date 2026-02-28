#ifndef ANLAM_H
#define ANLAM_H

#include "agac.h"
#include "tablo.h"
#include "bellek.h"

/* Generic fonksiyon özelleştirmesi */
typedef struct {
    char   *orijinal_isim;     /* Orijinal generic fonksiyon adı */
    char   *ozel_isim;         /* Özelleştirilmiş fonksiyon adı (örn: degistir_tam) */
    char   *tip_parametre;     /* Tip parametresi adı (örn: "T") */
    char   *somut_tip;         /* Somut tip adı (örn: "tam") */
    Düğüm  *orijinal_dugum;    /* Orijinal AST düğümü */
    Düğüm  *ozel_dugum;        /* Özelleştirilmiş AST düğümü */
    int     uretildi;          /* Kod üretildi mi? */
} GenericÖzelleştirme;

typedef struct {
    Arena  *arena;
    Kapsam *kapsam;
    int     islev_icinde;
    int     dongu_icinde;
    char   *mevcut_sinif;   /* mevcut sınıf adı (metot analizi sırasında) */

    /* Etiketli kır/devam için döngü değişken yığını */
    #define MAKS_DONGU_DERINLIK 32
    char   *dongu_degiskenleri[MAKS_DONGU_DERINLIK];
    int     dongu_derinligi;

    /* Monomorphization için */
    Düğüm **generic_islevler;          /* Generic fonksiyon AST'leri */
    int     generic_islev_sayisi;
    int     generic_islev_kapasite;

    GenericÖzelleştirme *ozellestirilmisler;  /* Özelleştirilmiş versiyonlar */
    int     ozellestirme_sayisi;
    int     ozellestirme_kapasite;

    /* Mevcut generic bağlam (tip parametresi çözümleme için) */
    char   *mevcut_tip_parametre;      /* Mevcut tip parametresi adı */
    char   *mevcut_somut_tip;          /* Mevcut somut tip */
} AnlamÇözümleyici;

/* Semantik analiz çalıştır. Hata varsa 0 olmayan değer döndürür.
 * hedef: derleme hedefi (ör: "wasm", "x86_64"). NULL olabilir.
 * WASM hedefinde donanım fonksiyonları otomatik kaydedilir. */
int anlam_çözümle(AnlamÇözümleyici *ac, Düğüm *program, Arena *arena, const char *hedef);

#endif
