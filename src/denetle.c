/*
 * Tonyukuk Denetleyici (Linter)
 * Kullanım: ./denetle dosya.tr
 *
 * Denetim kuralları:
 *   1. Kullanılmayan değişkenler
 *   2. Erişilmez kod (döndür/kır sonrası)
 *   3. Boş bloklar (boş eğer/iken gövdeleri)
 *   4. Döndür eksik (void olmayan işlevlerde)
 *   5. Değişken gölgeleme uyarıları
 */

#define _GNU_SOURCE
#include "sozcuk.h"
#include "cozumleyici.h"
#include "agac.h"
#include "anlam.h"
#include "hata.h"
#include "bellek.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== Denetim bağlamı ========== */

#define MAKS_DEGISKEN 256
#define MAKS_KAPSAM   64

typedef struct {
    char *isim;
    int   satir;
    int   sutun;
    int   kullanildi;     /* Değişken okundu mu? */
    int   kapsam_derinlik;
} DegiskenKaydi;

typedef struct {
    DegiskenKaydi degiskenler[MAKS_DEGISKEN];
    int           degisken_sayisi;
    int           kapsam_derinlik;
    int           uyari_sayisi;
    int           hata_sayisi_lint;
    const char   *dosya_adi;
} DenetimBaglami;

static void lint_uyari(DenetimBaglami *ctx, int satir, int sutun, const char *mesaj) {
    fprintf(stderr, "%s:%d:%d: uyarı: %s\n",
            ctx->dosya_adi ? ctx->dosya_adi : "?", satir, sutun, mesaj);
    ctx->uyari_sayisi++;
}

static void lint_bilgi(DenetimBaglami *ctx, int satir, int sutun, const char *mesaj) {
    fprintf(stderr, "%s:%d:%d: bilgi: %s\n",
            ctx->dosya_adi ? ctx->dosya_adi : "?", satir, sutun, mesaj);
}

/* Değişken kaydet */
static void degisken_kaydet(DenetimBaglami *ctx, const char *isim, int satir, int sutun) {
    if (!isim) return;

    /* Gölgeleme kontrolü: aynı isimde daha üst kapsamda var mı? */
    for (int i = 0; i < ctx->degisken_sayisi; i++) {
        if (strcmp(ctx->degiskenler[i].isim, isim) == 0 &&
            ctx->degiskenler[i].kapsam_derinlik < ctx->kapsam_derinlik) {
            char buf[256];
            snprintf(buf, sizeof(buf), "'%s' değişkeni üst kapsamdaki tanımı gölgeliyor", isim);
            lint_uyari(ctx, satir, sutun, buf);
            char buf2[256];
            snprintf(buf2, sizeof(buf2), "'%s' önceki tanım burada", isim);
            lint_bilgi(ctx, ctx->degiskenler[i].satir, ctx->degiskenler[i].sutun, buf2);
        }
    }

    if (ctx->degisken_sayisi < MAKS_DEGISKEN) {
        DegiskenKaydi *dk = &ctx->degiskenler[ctx->degisken_sayisi++];
        dk->isim = strdup(isim);
        dk->satir = satir;
        dk->sutun = sutun;
        dk->kullanildi = 0;
        dk->kapsam_derinlik = ctx->kapsam_derinlik;
    }
}

/* Değişken kullanımını işaretle */
static void degisken_kullan(DenetimBaglami *ctx, const char *isim) {
    if (!isim) return;
    /* En yakın kapsamdan başla (son eklenen) */
    for (int i = ctx->degisken_sayisi - 1; i >= 0; i--) {
        if (strcmp(ctx->degiskenler[i].isim, isim) == 0) {
            ctx->degiskenler[i].kullanildi = 1;
            return;
        }
    }
}

/* Kapsam çıkışında kullanılmayan değişkenleri raporla */
static void kapsam_cik_rapor(DenetimBaglami *ctx) {
    int i = ctx->degisken_sayisi - 1;
    while (i >= 0 && ctx->degiskenler[i].kapsam_derinlik == ctx->kapsam_derinlik) {
        DegiskenKaydi *dk = &ctx->degiskenler[i];
        if (!dk->kullanildi) {
            char buf[256];
            snprintf(buf, sizeof(buf), "'%s' değişkeni tanımlandı ama hiç kullanılmadı", dk->isim);
            lint_uyari(ctx, dk->satir, dk->sutun, buf);
        }
        free(dk->isim);
        ctx->degisken_sayisi--;
        i--;
    }
    if (ctx->kapsam_derinlik > 0) ctx->kapsam_derinlik--;
}

/* ========== AST traversal ========== */

static void dugum_denetle(DenetimBaglami *ctx, Düğüm *d);
static void blok_denetle(DenetimBaglami *ctx, Düğüm *d);

/* Bir ifadedeki tüm tanımlayıcı kullanımlarını işaretle */
static void ifade_kullanim_tara(DenetimBaglami *ctx, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
    case DÜĞÜM_TANIMLAYICI:
        degisken_kullan(ctx, d->veri.tanimlayici.isim);
        break;
    case DÜĞÜM_İKİLİ_İŞLEM:
    case DÜĞÜM_ÇAĞRI:
    case DÜĞÜM_DİZİ_DEĞERİ:
    case DÜĞÜM_DİZİ_ERİŞİM:
    case DÜĞÜM_BORU:
    case DÜĞÜM_ERİŞİM:
    case DÜĞÜM_TEKLİ_İŞLEM:
    case DÜĞÜM_ATAMA:
    case DÜĞÜM_DİZİ_ATAMA:
    case DÜĞÜM_ERİŞİM_ATAMA:
    case DÜĞÜM_SÖZLÜK_DEĞERİ:
    case DÜĞÜM_SÖZLÜK_ERİŞİM:
    case DÜĞÜM_LAMBDA:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_kullanim_tara(ctx, d->çocuklar[i]);
        }
        break;
    default:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_kullanim_tara(ctx, d->çocuklar[i]);
        }
        break;
    }
}

/* Blokta döndür var mı kontrol et (sadece son ifade) */
static int blok_dondur_var_mi(Düğüm *d) {
    if (!d) return 0;
    if (d->tur == DÜĞÜM_DÖNDÜR) return 1;
    if (d->çocuk_sayısı == 0) return 0;

    Düğüm *son = d->çocuklar[d->çocuk_sayısı - 1];
    if (son->tur == DÜĞÜM_DÖNDÜR) return 1;

    /* eğer/yoksa her iki dalda da döndür varsa tamam */
    if (son->tur == DÜĞÜM_EĞER && son->çocuk_sayısı >= 3) {
        /* çocuklar[0] = koşul, çocuklar[1] = doğru blok, çocuklar[2] = yanlış blok */
        int dogru_dondur = blok_dondur_var_mi(son->çocuklar[1]);
        int yanlis_dondur = (son->çocuk_sayısı > 2) ? blok_dondur_var_mi(son->çocuklar[2]) : 0;
        return dogru_dondur && yanlis_dondur;
    }

    return 0;
}

/* Erişilmez kod kontrolü: döndür/kır sonrası bildirim var mı? */
static void erisilmez_kod_kontrol(DenetimBaglami *ctx, Düğüm *blok) {
    if (!blok) return;
    for (int i = 0; i < blok->çocuk_sayısı; i++) {
        Düğüm *d = blok->çocuklar[i];
        if ((d->tur == DÜĞÜM_DÖNDÜR || d->tur == DÜĞÜM_KIR || d->tur == DÜĞÜM_DEVAM) &&
            i + 1 < blok->çocuk_sayısı) {
            Düğüm *sonraki = blok->çocuklar[i + 1];
            char buf[256];
            const char *tip = (d->tur == DÜĞÜM_DÖNDÜR) ? "döndür" :
                              (d->tur == DÜĞÜM_KIR) ? "kır" : "devam";
            snprintf(buf, sizeof(buf), "'%s' ifadesinden sonra erişilmez kod", tip);
            lint_uyari(ctx, sonraki->satir, sonraki->sutun, buf);
            break;  /* Bir kez uyar yeter */
        }
    }
}

/* Boş blok kontrolü */
static void bos_blok_kontrol(DenetimBaglami *ctx, Düğüm *d, const char *tip_adı) {
    if (!d) return;
    if (d->çocuk_sayısı == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "boş %s bloğu", tip_adı);
        lint_uyari(ctx, d->satir, d->sutun, buf);
    }
}

static void blok_denetle(DenetimBaglami *ctx, Düğüm *d) {
    if (!d) return;
    ctx->kapsam_derinlik++;

    erisilmez_kod_kontrol(ctx, d);

    for (int i = 0; i < d->çocuk_sayısı; i++) {
        dugum_denetle(ctx, d->çocuklar[i]);
    }

    kapsam_cik_rapor(ctx);
}

static void dugum_denetle(DenetimBaglami *ctx, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
    case DÜĞÜM_PROGRAM:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            dugum_denetle(ctx, d->çocuklar[i]);
        }
        break;

    case DÜĞÜM_DEĞİŞKEN:
        degisken_kaydet(ctx, d->veri.değişken.isim, d->satir, d->sutun);
        /* Başlangıç değeri ifadesindeki kullanımları tara */
        if (d->çocuk_sayısı > 0) {
            ifade_kullanim_tara(ctx, d->çocuklar[0]);
        }
        break;

    case DÜĞÜM_İŞLEV: {
        /* İşlev adını kaydetmek yerine, parametreleri bir alt kapsamda kaydet */
        ctx->kapsam_derinlik++;

        /* Parametreleri kaydet (çocuklar[0..n-2] parametre, çocuklar[son] gövde) */
        int govde_idx = d->çocuk_sayısı - 1;
        for (int i = 0; i < govde_idx; i++) {
            Düğüm *param = d->çocuklar[i];
            if (param->tur == DÜĞÜM_DEĞİŞKEN) {
                degisken_kaydet(ctx, param->veri.değişken.isim, param->satir, param->sutun);
            }
        }

        /* Gövdeyi denetle */
        if (govde_idx >= 0) {
            Düğüm *govde = d->çocuklar[govde_idx];

            /* Boş gövde kontrolü */
            bos_blok_kontrol(ctx, govde, "işlev");

            /* Erişilmez kod kontrolü */
            erisilmez_kod_kontrol(ctx, govde);

            /* Döndür eksikliği kontrolü (void olmayan işlevler) */
            if (d->veri.islev.dönüş_tipi != NULL &&
                strcmp(d->veri.islev.dönüş_tipi, "") != 0) {
                if (!blok_dondur_var_mi(govde)) {
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "'%s' işlevi '%s' dönüş tipi belirtiyor ama tüm yollar döndür içermiyor",
                             d->veri.islev.isim, d->veri.islev.dönüş_tipi);
                    lint_uyari(ctx, d->satir, d->sutun, buf);
                }
            }

            for (int i = 0; i < govde->çocuk_sayısı; i++) {
                dugum_denetle(ctx, govde->çocuklar[i]);
            }
        }

        kapsam_cik_rapor(ctx);
        break;
    }

    case DÜĞÜM_SINIF:
        ctx->kapsam_derinlik++;
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            dugum_denetle(ctx, d->çocuklar[i]);
        }
        kapsam_cik_rapor(ctx);
        break;

    case DÜĞÜM_EĞER:
        /* çocuklar[0] = koşul, çocuklar[1] = doğru blok, çocuklar[2] = yoksa blok (opsiyonel) */
        if (d->çocuk_sayısı > 0) ifade_kullanim_tara(ctx, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) {
            bos_blok_kontrol(ctx, d->çocuklar[1], "eğer");
            blok_denetle(ctx, d->çocuklar[1]);
        }
        if (d->çocuk_sayısı > 2) {
            blok_denetle(ctx, d->çocuklar[2]);
        }
        break;

    case DÜĞÜM_İKEN:
        if (d->çocuk_sayısı > 0) ifade_kullanim_tara(ctx, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) {
            bos_blok_kontrol(ctx, d->çocuklar[1], "iken");
            blok_denetle(ctx, d->çocuklar[1]);
        }
        break;

    case DÜĞÜM_DÖNGÜ:
        ctx->kapsam_derinlik++;
        /* Döngü değişkeni */
        if (d->veri.dongu.isim) {
            degisken_kaydet(ctx, d->veri.dongu.isim, d->satir, d->sutun);
        }
        /* çocuklar[0] = başlangıç, çocuklar[1] = bitiş, çocuklar[2] = gövde */
        if (d->çocuk_sayısı > 0) ifade_kullanim_tara(ctx, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) ifade_kullanim_tara(ctx, d->çocuklar[1]);
        if (d->çocuk_sayısı > 2) {
            bos_blok_kontrol(ctx, d->çocuklar[2], "döngü");
            for (int i = 0; i < d->çocuklar[2]->çocuk_sayısı; i++) {
                dugum_denetle(ctx, d->çocuklar[2]->çocuklar[i]);
            }
        }
        kapsam_cik_rapor(ctx);
        break;

    case DÜĞÜM_HER_İÇİN:
        ctx->kapsam_derinlik++;
        /* Döngü değişkeni */
        if (d->veri.dongu.isim) {
            degisken_kaydet(ctx, d->veri.dongu.isim, d->satir, d->sutun);
        }
        if (d->çocuk_sayısı > 0) ifade_kullanim_tara(ctx, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) {
            for (int i = 0; i < d->çocuklar[1]->çocuk_sayısı; i++) {
                dugum_denetle(ctx, d->çocuklar[1]->çocuklar[i]);
            }
        }
        kapsam_cik_rapor(ctx);
        break;

    case DÜĞÜM_EŞLE:
        if (d->çocuk_sayısı > 0) ifade_kullanim_tara(ctx, d->çocuklar[0]);
        for (int i = 1; i < d->çocuk_sayısı; i++) {
            dugum_denetle(ctx, d->çocuklar[i]);
        }
        break;

    case DÜĞÜM_DENE_YAKALA:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            blok_denetle(ctx, d->çocuklar[i]);
        }
        break;

    case DÜĞÜM_DÖNDÜR:
    case DÜĞÜM_FIRLAT:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_kullanim_tara(ctx, d->çocuklar[i]);
        }
        break;

    case DÜĞÜM_İFADE_BİLDİRİMİ:
        if (d->çocuk_sayısı > 0) {
            ifade_kullanim_tara(ctx, d->çocuklar[0]);
        }
        break;

    case DÜĞÜM_ATAMA:
    case DÜĞÜM_DİZİ_ATAMA:
    case DÜĞÜM_ERİŞİM_ATAMA:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_kullanim_tara(ctx, d->çocuklar[i]);
        }
        break;

    case DÜĞÜM_BLOK:
        blok_denetle(ctx, d);
        break;

    case DÜĞÜM_KULLAN:
        /* Modül bildirimi: denetlenecek bir şey yok */
        break;

    default:
        /* Diğer düğüm türleri için çocukları tara */
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_kullanim_tara(ctx, d->çocuklar[i]);
        }
        break;
    }
}

/* ========== Ana program ========== */

static char *dosya_oku(const char *dosya_adi) {
    FILE *f = fopen(dosya_adi, "rb");
    if (!f) {
        fprintf(stderr, "denetle: dosya açılamadı: %s\n", dosya_adi);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long boyut = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *icerik = (char *)malloc(boyut + 1);
    if (!icerik) { fclose(f); return NULL; }
    fread(icerik, 1, boyut, f);
    icerik[boyut] = '\0';
    fclose(f);
    return icerik;
}

static void kullanim_goster(void) {
    fprintf(stderr, "Kullanım: denetle <dosya.tr>\n");
    fprintf(stderr, "\nTonyukuk kaynak kodu denetleyicisi (linter)\n");
    fprintf(stderr, "\nDenetim kuralları:\n");
    fprintf(stderr, "  - Kullanılmayan değişkenler\n");
    fprintf(stderr, "  - Erişilmez kod (döndür/kır sonrası)\n");
    fprintf(stderr, "  - Boş bloklar\n");
    fprintf(stderr, "  - Döndür eksikliği (void olmayan işlevlerde)\n");
    fprintf(stderr, "  - Değişken gölgeleme\n");
}

int main(int argc, char **argv) {
    const char *dosya_adi = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--yardım") == 0) {
            kullanim_goster();
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "denetle: bilinmeyen seçenek: %s\n", argv[i]);
            kullanim_goster();
            return 1;
        } else {
            dosya_adi = argv[i];
        }
    }

    if (!dosya_adi) {
        fprintf(stderr, "denetle: dosya adı belirtilmedi\n");
        kullanim_goster();
        return 1;
    }

    /* Dosyayı oku */
    char *kaynak = dosya_oku(dosya_adi);
    if (!kaynak) return 1;

    /* Hata raporlama */
    hata_dosya_adi = dosya_adi;
    hata_kaynak = kaynak;

    /* Sözcük çözümleme */
    SözcükÇözümleyici sc;
    sözcük_çözümle(&sc, kaynak);

    if (hata_sayisi > 0) {
        fprintf(stderr, "denetle: sözcük çözümleme hatası\n");
        sözcük_serbest(&sc);
        free(kaynak);
        return 1;
    }

    /* Ayrıştırma */
    Arena arena;
    arena_baslat(&arena);

    Cozumleyici coz;
    Düğüm *program = cozumle(&coz, sc.sozcukler, sc.sozcuk_sayisi, &arena);

    if (hata_sayisi > 0) {
        fprintf(stderr, "denetle: sözdizimi hatası\n");
        arena_serbest(&arena);
        sözcük_serbest(&sc);
        free(kaynak);
        return 1;
    }

    /* Lint denetimi */
    DenetimBaglami ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.dosya_adi = dosya_adi;

    dugum_denetle(&ctx, program);

    /* Kalan global kapsamdaki değişkenleri kontrol et */
    for (int i = 0; i < ctx.degisken_sayisi; i++) {
        DegiskenKaydi *dk = &ctx.degiskenler[i];
        if (!dk->kullanildi) {
            char buf[256];
            snprintf(buf, sizeof(buf), "'%s' değişkeni tanımlandı ama hiç kullanılmadı", dk->isim);
            lint_uyari(&ctx, dk->satir, dk->sutun, buf);
        }
        free(dk->isim);
    }

    /* Sonuç raporu */
    if (ctx.uyari_sayisi == 0) {
        fprintf(stderr, "denetle: %s — sorun bulunamadı\n", dosya_adi);
    } else {
        fprintf(stderr, "denetle: %s — %d uyarı bulundu\n", dosya_adi, ctx.uyari_sayisi);
    }

    /* Temizlik */
    arena_serbest(&arena);
    sözcük_serbest(&sc);
    free(kaynak);

    return (ctx.uyari_sayisi > 0) ? 1 : 0;
}
