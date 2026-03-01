/*
 * Tonyukuk Sanal Makinesi (TrSM)
 * Bytecode yorumlayıcı — .trbc dosyalarını çalıştırır
 *
 * Kullanım: trsm program.trbc
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "vm.h"

/* ═══════════════════════════════════════════════════════════════════
 *  BYTECODE DOSYA OKUMA
 * ═══════════════════════════════════════════════════════════════════ */

static uint8_t *dosya_oku(const char *yol, int *boyut) {
    FILE *f = fopen(yol, "rb");
    if (!f) {
        fprintf(stderr, "Hata: '%s' dosyası açılamadı\n", yol);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long uz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *veri = malloc(uz);
    if (!veri) {
        fclose(f);
        return NULL;
    }
    fread(veri, 1, uz, f);
    fclose(f);
    *boyut = (int)uz;
    return veri;
}

/* .trbc dosyasını ayrıştır ve TrSM yapısını doldur */
static int trbc_yukle(TrSM *sm, const uint8_t *veri, int boyut) {
    if (boyut < 8) {
        fprintf(stderr, "Hata: Dosya çok küçük\n");
        return -1;
    }

    const uint8_t *p = veri;

    /* Başlık kontrolü */
    uint32_t sihirli = OKU_U32(p); p += 4;
    if (sihirli != TRBC_SIHIRLI) {
        fprintf(stderr, "Hata: Geçersiz dosya formatı (sihirli: 0x%08X)\n", sihirli);
        return -1;
    }
    uint16_t versiyon = OKU_U16(p); p += 2;
    if (versiyon != TRBC_VERSIYON) {
        fprintf(stderr, "Hata: Desteklenmeyen versiyon: %d\n", versiyon);
        return -1;
    }
    /* uint16_t bayraklar = OKU_U16(p); */
    p += 2; /* bayraklar */

    /* Sabit havuzu */
    if (p + 4 > veri + boyut) return -1;
    uint32_t sabit_sayisi = OKU_U32(p); p += 4;

    sm->sabitler = malloc(sabit_sayisi * sizeof(SmDeger));
    sm->sabit_sayisi = (int)sabit_sayisi;

    for (uint32_t i = 0; i < sabit_sayisi; i++) {
        if (p + 1 > veri + boyut) return -1;
        uint8_t tip = *p++;

        switch (tip) {
        case SABIT_TIP_TAM: {
            if (p + 8 > veri + boyut) return -1;
            int64_t deger;
            memcpy(&deger, p, 8);
            p += 8;
            sm->sabitler[i].tur = DEGER_TAM;
            sm->sabitler[i].deger.tam = deger;
            break;
        }
        case SABIT_TIP_ONDALIK: {
            if (p + 8 > veri + boyut) return -1;
            double deger;
            memcpy(&deger, p, 8);
            p += 8;
            sm->sabitler[i].tur = DEGER_ONDALIK;
            sm->sabitler[i].deger.ondalik = deger;
            break;
        }
        case SABIT_TIP_METIN: {
            if (p + 4 > veri + boyut) return -1;
            uint32_t uzunluk = OKU_U32(p); p += 4;
            if (p + uzunluk > veri + boyut) return -1;
            char *kopya = malloc(uzunluk + 1);
            memcpy(kopya, p, uzunluk);
            kopya[uzunluk] = '\0';
            p += uzunluk;
            sm->sabitler[i].tur = DEGER_METIN;
            sm->sabitler[i].deger.metin.ptr = kopya;
            sm->sabitler[i].deger.metin.uzunluk = (int64_t)uzunluk;
            break;
        }
        default:
            fprintf(stderr, "Hata: Bilinmeyen sabit tipi: %d\n", tip);
            return -1;
        }
    }

    /* Fonksiyon tablosu */
    if (p + 4 > veri + boyut) return -1;
    uint32_t fonk_sayisi = OKU_U32(p); p += 4;

    sm->fonksiyonlar = malloc(fonk_sayisi * sizeof(SmFonksiyon));
    sm->fonksiyon_sayisi = (int)fonk_sayisi;

    for (uint32_t i = 0; i < fonk_sayisi; i++) {
        if (p + 2 > veri + boyut) return -1;
        uint16_t isim_uz = OKU_U16(p); p += 2;

        if (isim_uz > 0) {
            if (p + isim_uz > veri + boyut) return -1;
            char *isim = malloc(isim_uz + 1);
            memcpy(isim, p, isim_uz);
            isim[isim_uz] = '\0';
            p += isim_uz;
            sm->fonksiyonlar[i].isim = isim;
        } else {
            sm->fonksiyonlar[i].isim = NULL;
        }

        if (p + 1 + 2 + 4 + 4 > veri + boyut) return -1;
        sm->fonksiyonlar[i].param_sayisi = *p++;
        sm->fonksiyonlar[i].yerel_sayisi = OKU_U16(p); p += 2;
        sm->fonksiyonlar[i].kod_baslangic = (int)OKU_U32(p); p += 4;
        sm->fonksiyonlar[i].kod_uzunluk = (int)OKU_U32(p); p += 4;
    }

    /* Bytecode */
    if (p + 4 > veri + boyut) return -1;
    uint32_t kod_uz = OKU_U32(p); p += 4;

    if (p + kod_uz > veri + boyut) return -1;
    sm->kod = malloc(kod_uz);
    memcpy(sm->kod, p, kod_uz);
    sm->kod_uzunluk = (int)kod_uz;

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 *  YARDIMCI FONKSİYONLAR
 * ═══════════════════════════════════════════════════════════════════ */

/* Değeri yazdır (satır sonu olmadan) */
static void deger_yazdir(SmDeger *d) {
    switch (d->tur) {
    case DEGER_TAM:
        printf("%" PRId64, d->deger.tam);
        break;
    case DEGER_ONDALIK: {
        double val = d->deger.ondalik;
        /* Tam sayı gibi görünüyorsa .0 ekle değil, %g formatı kullan */
        if (val == (double)(int64_t)val && val >= -1e15 && val <= 1e15) {
            printf("%.1f", val);
        } else {
            printf("%g", val);
        }
        break;
    }
    case DEGER_METIN:
        if (d->deger.metin.ptr)
            printf("%.*s", (int)d->deger.metin.uzunluk, d->deger.metin.ptr);
        break;
    case DEGER_MANTIK:
        printf("%s", d->deger.mantik ? "doğru" : "yanlış");
        break;
    case DEGER_BOS:
        printf("boş");
        break;
    }
}

/* Metin birleştirme: iki metin değerini birleştir */
static SmDeger metin_birlestir(SmDeger *a, SmDeger *b) {
    int64_t uz_a = a->deger.metin.uzunluk;
    int64_t uz_b = b->deger.metin.uzunluk;
    int64_t toplam = uz_a + uz_b;
    char *yeni = malloc(toplam + 1);
    if (a->deger.metin.ptr)
        memcpy(yeni, a->deger.metin.ptr, uz_a);
    if (b->deger.metin.ptr)
        memcpy(yeni + uz_a, b->deger.metin.ptr, uz_b);
    yeni[toplam] = '\0';
    SmDeger sonuc;
    sonuc.tur = DEGER_METIN;
    sonuc.deger.metin.ptr = yeni;
    sonuc.deger.metin.uzunluk = toplam;
    return sonuc;
}

/* Değer doğruluk testi (truthy/falsy) */
static int deger_dogru_mu(SmDeger *d) {
    switch (d->tur) {
    case DEGER_TAM:    return d->deger.tam != 0;
    case DEGER_ONDALIK: return d->deger.ondalik != 0.0;
    case DEGER_METIN:  return d->deger.metin.uzunluk > 0;
    case DEGER_MANTIK: return d->deger.mantik;
    case DEGER_BOS:    return 0;
    }
    return 0;
}

/* Tam sayıyı metin'e dönüştür */
static SmDeger tam_metine(int64_t deger) {
    char buf[64];
    int uzunluk = snprintf(buf, sizeof(buf), "%" PRId64, deger);
    char *kopya = malloc(uzunluk + 1);
    memcpy(kopya, buf, uzunluk + 1);
    SmDeger sonuc;
    sonuc.tur = DEGER_METIN;
    sonuc.deger.metin.ptr = kopya;
    sonuc.deger.metin.uzunluk = uzunluk;
    return sonuc;
}

/* ═══════════════════════════════════════════════════════════════════
 *  ANA ÇALIŞTIRMA DÖNGÜSÜ
 * ═══════════════════════════════════════════════════════════════════ */

static int trsm_calistir(TrSM *sm) {
    sm->pc = sm->kod;
    sm->sp = 0;
    sm->cerceve_sayisi = 0;

    /* Yığın taşma/alttan taşma makroları */
    #define YIGIN_AT(d) do { \
        if (sm->sp >= YIGIN_MAKS) { \
            fprintf(stderr, "Hata: Yığın taşması\n"); return -1; \
        } \
        sm->yigin[sm->sp++] = (d); \
    } while(0)

    #define YIGIN_AL() (sm->sp > 0 ? sm->yigin[--sm->sp] : \
        (SmDeger){.tur = DEGER_BOS})

    while (1) {
        if (sm->pc < sm->kod || sm->pc >= sm->kod + sm->kod_uzunluk) {
            fprintf(stderr, "Hata: Program sayacı sınır dışı\n");
            return -1;
        }

        uint8_t komut = *sm->pc++;

        switch (komut) {

        /* ─── Sabitler ─── */

        case SM_SABIT_TAM: {
            uint16_t idx = OKU_U16(sm->pc); sm->pc += 2;
            if (idx >= (uint16_t)sm->sabit_sayisi) {
                fprintf(stderr, "Hata: Sabit indeksi sınır dışı: %d\n", idx);
                return -1;
            }
            YIGIN_AT(sm->sabitler[idx]);
            break;
        }

        case SM_SABIT_ONDALIK: {
            uint16_t idx = OKU_U16(sm->pc); sm->pc += 2;
            if (idx >= (uint16_t)sm->sabit_sayisi) {
                fprintf(stderr, "Hata: Sabit indeksi sınır dışı: %d\n", idx);
                return -1;
            }
            YIGIN_AT(sm->sabitler[idx]);
            break;
        }

        case SM_SABIT_METIN: {
            uint16_t idx = OKU_U16(sm->pc); sm->pc += 2;
            if (idx >= (uint16_t)sm->sabit_sayisi) {
                fprintf(stderr, "Hata: Sabit indeksi sınır dışı: %d\n", idx);
                return -1;
            }
            YIGIN_AT(sm->sabitler[idx]);
            break;
        }

        case SM_DOGRU: {
            SmDeger d = {.tur = DEGER_MANTIK, .deger.mantik = 1};
            YIGIN_AT(d);
            break;
        }

        case SM_YANLIS: {
            SmDeger d = {.tur = DEGER_MANTIK, .deger.mantik = 0};
            YIGIN_AT(d);
            break;
        }

        case SM_BOS: {
            SmDeger d = {.tur = DEGER_BOS};
            YIGIN_AT(d);
            break;
        }

        /* ─── Yığın işlemleri ─── */

        case SM_CIKAR:
            if (sm->sp > 0) sm->sp--;
            break;

        case SM_KOPYALA: {
            if (sm->sp < 1) {
                fprintf(stderr, "Hata: Yığın boş (KOPYALA)\n");
                return -1;
            }
            SmDeger d = sm->yigin[sm->sp - 1];
            YIGIN_AT(d);
            break;
        }

        /* ─── Tam sayı aritmetik ─── */

        case SM_TOPLA: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_TAM, .deger.tam = a.deger.tam + b.deger.tam};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_CIKAR_SAYI: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_TAM, .deger.tam = a.deger.tam - b.deger.tam};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_CARP: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_TAM, .deger.tam = a.deger.tam * b.deger.tam};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_BOL: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            if (b.deger.tam == 0) {
                fprintf(stderr, "Hata: Sıfıra bölme\n");
                return -1;
            }
            SmDeger sonuc = {.tur = DEGER_TAM, .deger.tam = a.deger.tam / b.deger.tam};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_MOD: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            if (b.deger.tam == 0) {
                fprintf(stderr, "Hata: Sıfıra bölme (mod)\n");
                return -1;
            }
            SmDeger sonuc = {.tur = DEGER_TAM, .deger.tam = a.deger.tam % b.deger.tam};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_EKSI: {
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_TAM, .deger.tam = -a.deger.tam};
            YIGIN_AT(sonuc);
            break;
        }

        /* ─── Ondalık aritmetik ─── */

        case SM_TOPLA_OND: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_ONDALIK,
                .deger.ondalik = a.deger.ondalik + b.deger.ondalik};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_CIKAR_OND: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_ONDALIK,
                .deger.ondalik = a.deger.ondalik - b.deger.ondalik};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_CARP_OND: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_ONDALIK,
                .deger.ondalik = a.deger.ondalik * b.deger.ondalik};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_BOL_OND: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            if (b.deger.ondalik == 0.0) {
                fprintf(stderr, "Hata: Sıfıra bölme (ondalık)\n");
                return -1;
            }
            SmDeger sonuc = {.tur = DEGER_ONDALIK,
                .deger.ondalik = a.deger.ondalik / b.deger.ondalik};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_EKSI_OND: {
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_ONDALIK,
                .deger.ondalik = -a.deger.ondalik};
            YIGIN_AT(sonuc);
            break;
        }

        /* ─── Tam sayı karşılaştırma ─── */

        case SM_ESIT: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.tam == b.deger.tam)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_ESIT_DEGIL: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.tam != b.deger.tam)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_KUCUK: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.tam < b.deger.tam)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_BUYUK: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.tam > b.deger.tam)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_KUCUK_ESIT: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.tam <= b.deger.tam)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_BUYUK_ESIT: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.tam >= b.deger.tam)};
            YIGIN_AT(sonuc);
            break;
        }

        /* ─── Ondalık karşılaştırma ─── */

        case SM_ESIT_OND: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.ondalik == b.deger.ondalik)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_KUCUK_OND: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.ondalik < b.deger.ondalik)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_BUYUK_OND: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.ondalik > b.deger.ondalik)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_KUCUK_ESIT_OND: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.ondalik <= b.deger.ondalik)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_BUYUK_ESIT_OND: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (a.deger.ondalik >= b.deger.ondalik)};
            YIGIN_AT(sonuc);
            break;
        }

        /* ─── Metin karşılaştırma ─── */

        case SM_ESIT_METIN: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            int esit = 0;
            if (a.deger.metin.uzunluk == b.deger.metin.uzunluk) {
                if (a.deger.metin.uzunluk == 0)
                    esit = 1;
                else
                    esit = (memcmp(a.deger.metin.ptr, b.deger.metin.ptr,
                                   a.deger.metin.uzunluk) == 0);
            }
            SmDeger sonuc = {.tur = DEGER_MANTIK, .deger.mantik = esit};
            YIGIN_AT(sonuc);
            break;
        }

        /* ─── Mantık ─── */

        case SM_DEGIL: {
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = !deger_dogru_mu(&a)};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_VE: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (deger_dogru_mu(&a) && deger_dogru_mu(&b))};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_VEYA: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_MANTIK,
                .deger.mantik = (deger_dogru_mu(&a) || deger_dogru_mu(&b))};
            YIGIN_AT(sonuc);
            break;
        }

        /* ─── Değişkenler ─── */

        case SM_YUKLE_YEREL: {
            uint16_t idx = OKU_U16(sm->pc); sm->pc += 2;
            if (sm->cerceve_sayisi > 0) {
                CagriCercevesi *cerceve = &sm->cerceveler[sm->cerceve_sayisi - 1];
                if (idx < (uint16_t)cerceve->yerel_sayisi) {
                    YIGIN_AT(cerceve->yereller[idx]);
                } else {
                    SmDeger bos = {.tur = DEGER_BOS};
                    YIGIN_AT(bos);
                }
            } else {
                /* Global kapsamda yerel yok — genelden oku */
                if (idx < GENEL_MAKS) {
                    YIGIN_AT(sm->geneller[idx]);
                } else {
                    SmDeger bos = {.tur = DEGER_BOS};
                    YIGIN_AT(bos);
                }
            }
            break;
        }

        case SM_KAYDET_YEREL: {
            uint16_t idx = OKU_U16(sm->pc); sm->pc += 2;
            SmDeger deger = YIGIN_AL();
            if (sm->cerceve_sayisi > 0) {
                CagriCercevesi *cerceve = &sm->cerceveler[sm->cerceve_sayisi - 1];
                /* Yerel diziyi gerekirse genişlet */
                if (idx >= (uint16_t)cerceve->yerel_sayisi) {
                    int yeni_boyut = idx + 1;
                    cerceve->yereller = realloc(cerceve->yereller,
                        yeni_boyut * sizeof(SmDeger));
                    /* Yeni alanları sıfırla */
                    for (int j = cerceve->yerel_sayisi; j < yeni_boyut; j++) {
                        cerceve->yereller[j] = (SmDeger){.tur = DEGER_BOS};
                    }
                    cerceve->yerel_sayisi = yeni_boyut;
                }
                cerceve->yereller[idx] = deger;
            } else {
                /* Global kapsamda: genellere yaz */
                if (idx < GENEL_MAKS) {
                    sm->geneller[idx] = deger;
                }
            }
            break;
        }

        case SM_YUKLE_GENEL: {
            uint16_t idx = OKU_U16(sm->pc); sm->pc += 2;
            if (idx < GENEL_MAKS) {
                YIGIN_AT(sm->geneller[idx]);
            } else {
                SmDeger bos = {.tur = DEGER_BOS};
                YIGIN_AT(bos);
            }
            break;
        }

        case SM_KAYDET_GENEL: {
            uint16_t idx = OKU_U16(sm->pc); sm->pc += 2;
            SmDeger deger = YIGIN_AL();
            if (idx < GENEL_MAKS) {
                sm->geneller[idx] = deger;
            }
            break;
        }

        /* ─── Kontrol akışı ─── */

        case SM_ATLA: {
            int16_t ofset = OKU_I16(sm->pc); sm->pc += 2;
            sm->pc += ofset;
            break;
        }

        case SM_ATLA_YANLIS: {
            int16_t ofset = OKU_I16(sm->pc); sm->pc += 2;
            SmDeger d = YIGIN_AL();
            if (!deger_dogru_mu(&d)) {
                sm->pc += ofset;
            }
            break;
        }

        case SM_ATLA_DOGRU: {
            int16_t ofset = OKU_I16(sm->pc); sm->pc += 2;
            SmDeger d = YIGIN_AL();
            if (deger_dogru_mu(&d)) {
                sm->pc += ofset;
            }
            break;
        }

        /* ─── Fonksiyonlar ─── */

        case SM_CAGRI: {
            uint16_t fn_idx = OKU_U16(sm->pc); sm->pc += 2;
            uint8_t arg_sayisi = *sm->pc++;

            if (fn_idx >= (uint16_t)sm->fonksiyon_sayisi) {
                fprintf(stderr, "Hata: Fonksiyon indeksi sınır dışı: %d\n", fn_idx);
                return -1;
            }
            if (sm->cerceve_sayisi >= CERCEVE_MAKS) {
                fprintf(stderr, "Hata: Çağrı yığını taşması\n");
                return -1;
            }

            SmFonksiyon *fn = &sm->fonksiyonlar[fn_idx];
            CagriCercevesi *cerceve = &sm->cerceveler[sm->cerceve_sayisi++];
            cerceve->fonksiyon = fn;
            cerceve->dondur_adresi = sm->pc;
            cerceve->yigin_tabani = sm->sp - arg_sayisi;

            /* Yerel değişkenler için bellek ayır */
            int yerel_boyut = fn->yerel_sayisi > arg_sayisi ?
                              fn->yerel_sayisi : arg_sayisi;
            if (yerel_boyut < 1) yerel_boyut = 1;
            cerceve->yereller = malloc(yerel_boyut * sizeof(SmDeger));
            cerceve->yerel_sayisi = yerel_boyut;

            /* Yerel değişkenleri sıfırla */
            for (int i = 0; i < yerel_boyut; i++) {
                cerceve->yereller[i] = (SmDeger){.tur = DEGER_BOS};
            }

            /* Argümanları yerel değişkenlere kopyala */
            for (int i = 0; i < arg_sayisi; i++) {
                int yigin_pos = cerceve->yigin_tabani + i;
                if (yigin_pos >= 0 && yigin_pos < YIGIN_MAKS) {
                    cerceve->yereller[i] = sm->yigin[yigin_pos];
                }
            }

            /* Yığın işaretçisini argümanlardan önceye geri al */
            sm->sp = cerceve->yigin_tabani;

            /* PC'yi fonksiyon gövdesine ayarla */
            sm->pc = sm->kod + fn->kod_baslangic;
            break;
        }

        case SM_DONDUR: {
            if (sm->cerceve_sayisi < 1) {
                fprintf(stderr, "Hata: Çağrı yığını boş (DONDUR)\n");
                return -1;
            }
            CagriCercevesi *cerceve = &sm->cerceveler[--sm->cerceve_sayisi];
            sm->pc = cerceve->dondur_adresi;
            sm->sp = cerceve->yigin_tabani;
            free(cerceve->yereller);
            break;
        }

        case SM_DONDUR_DEGER: {
            if (sm->cerceve_sayisi < 1) {
                fprintf(stderr, "Hata: Çağrı yığını boş (DONDUR_DEGER)\n");
                return -1;
            }
            SmDeger dondur_degeri = YIGIN_AL();
            CagriCercevesi *cerceve = &sm->cerceveler[--sm->cerceve_sayisi];
            sm->pc = cerceve->dondur_adresi;
            sm->sp = cerceve->yigin_tabani;
            free(cerceve->yereller);
            YIGIN_AT(dondur_degeri);
            break;
        }

        /* ─── Yerleşik fonksiyonlar ─── */

        case SM_YAZDIR: {
            SmDeger d = YIGIN_AL();
            deger_yazdir(&d);
            break;
        }

        case SM_YAZDIR_SATIR: {
            SmDeger d = YIGIN_AL();
            deger_yazdir(&d);
            printf("\n");
            break;
        }

        /* ─── Metin ─── */

        case SM_METIN_BIRLESTIR: {
            SmDeger b = YIGIN_AL();
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = metin_birlestir(&a, &b);
            YIGIN_AT(sonuc);
            break;
        }

        /* ─── Tip dönüşümleri ─── */

        case SM_TAM_ONDALIK: {
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_ONDALIK,
                .deger.ondalik = (double)a.deger.tam};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_ONDALIK_TAM: {
            SmDeger a = YIGIN_AL();
            SmDeger sonuc = {.tur = DEGER_TAM,
                .deger.tam = (int64_t)a.deger.ondalik};
            YIGIN_AT(sonuc);
            break;
        }

        case SM_TAM_METIN: {
            SmDeger a = YIGIN_AL();
            SmDeger sonuc;
            if (a.tur == DEGER_TAM) {
                sonuc = tam_metine(a.deger.tam);
            } else if (a.tur == DEGER_ONDALIK) {
                char buf[64];
                int uzunluk = snprintf(buf, sizeof(buf), "%g", a.deger.ondalik);
                char *kopya = malloc(uzunluk + 1);
                memcpy(kopya, buf, uzunluk + 1);
                sonuc.tur = DEGER_METIN;
                sonuc.deger.metin.ptr = kopya;
                sonuc.deger.metin.uzunluk = uzunluk;
            } else if (a.tur == DEGER_MANTIK) {
                const char *m = a.deger.mantik ? "doğru" : "yanlış";
                int uz = (int)strlen(m);
                char *kopya = malloc(uz + 1);
                memcpy(kopya, m, uz + 1);
                sonuc.tur = DEGER_METIN;
                sonuc.deger.metin.ptr = kopya;
                sonuc.deger.metin.uzunluk = uz;
            } else {
                sonuc = tam_metine(0);
            }
            YIGIN_AT(sonuc);
            break;
        }

        case SM_METIN_TAM: {
            SmDeger a = YIGIN_AL();
            int64_t deger = 0;
            if (a.tur == DEGER_METIN && a.deger.metin.ptr) {
                deger = strtoll(a.deger.metin.ptr, NULL, 10);
            }
            SmDeger sonuc = {.tur = DEGER_TAM, .deger.tam = deger};
            YIGIN_AT(sonuc);
            break;
        }

        /* ─── Program sonu ─── */

        case SM_DUR:
            return 0;

        default:
            fprintf(stderr, "Hata: Bilinmeyen komut: %d (PC: %ld)\n",
                    komut, (long)(sm->pc - sm->kod - 1));
            return -1;
        }
    }

    #undef YIGIN_AT
    #undef YIGIN_AL
}

/* ═══════════════════════════════════════════════════════════════════
 *  TEMİZLİK
 * ═══════════════════════════════════════════════════════════════════ */

static void trsm_temizle(TrSM *sm) {
    /* Sabit havuzu */
    if (sm->sabitler) {
        for (int i = 0; i < sm->sabit_sayisi; i++) {
            if (sm->sabitler[i].tur == DEGER_METIN && sm->sabitler[i].deger.metin.ptr)
                free(sm->sabitler[i].deger.metin.ptr);
        }
        free(sm->sabitler);
    }

    /* Fonksiyon tablosu */
    if (sm->fonksiyonlar) {
        for (int i = 0; i < sm->fonksiyon_sayisi; i++) {
            if (sm->fonksiyonlar[i].isim)
                free(sm->fonksiyonlar[i].isim);
        }
        free(sm->fonksiyonlar);
    }

    /* Bytecode */
    if (sm->kod) free(sm->kod);

    /* Kalan çağrı çerçevelerini temizle */
    for (int i = 0; i < sm->cerceve_sayisi; i++) {
        if (sm->cerceveler[i].yereller)
            free(sm->cerceveler[i].yereller);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  ANA GİRİŞ NOKTASI
 * ═══════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Tonyukuk Sanal Makinesi (TrSM) v%d\n", TRBC_VERSIYON);
        fprintf(stderr, "Kullanım: trsm <dosya.trbc>\n");
        return 1;
    }

    const char *dosya_yolu = argv[1];

    /* Dosyayı oku */
    int boyut;
    uint8_t *veri = dosya_oku(dosya_yolu, &boyut);
    if (!veri) return 1;

    /* VM'i başlat */
    TrSM sm;
    memset(&sm, 0, sizeof(sm));

    if (trbc_yukle(&sm, veri, boyut) != 0) {
        free(veri);
        fprintf(stderr, "Hata: Bytecode dosyası yüklenemedi\n");
        return 1;
    }
    free(veri); /* ham veri artık gerekli değil */

    /* Çalıştır */
    int sonuc = trsm_calistir(&sm);

    /* Temizle */
    trsm_temizle(&sm);

    return sonuc;
}
