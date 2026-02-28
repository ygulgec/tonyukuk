/*
 * uretici_avr.c — AVR (Arduino) kod üreteci
 *
 * Tonyukuk Türkçe programlama dili derleyicisi için AVR backend.
 * Arduino UNO (ATmega328P), Nano, Mega kartlarını destekler.
 *
 * AVR ABI özeti:
 *   - r0-r1   : geçici (r1 her zaman 0 olmalı - MUL için)
 *   - r2-r17  : callee-saved
 *   - r18-r25 : caller-saved, fonksiyon argümanları
 *   - r24-r25 : return değeri (16-bit)
 *   - r26-r27 : X pointer
 *   - r28-r29 : Y pointer (frame pointer)
 *   - r30-r31 : Z pointer
 *
 * 8-bit mimari, 16-bit pointer adresleme
 * Harvard mimarisi: ayrı program (flash) ve veri (SRAM) belleği
 */

#include "uretici_avr.h"
#include "uretici.h"
#include "hata.h"
#include "modul.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* ---- Yardımcı: assembly satırı yaz ---- */

static void yaz(Üretici *u, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->cikti, buf);
}

static void veri_yaz(Üretici *u, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->veri_bolumu, buf);
}

static void yardimci_yaz(Üretici *u, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->yardimcilar, buf);
}

static int yeni_etiket(Üretici *u) {
    return u->etiket_sayac++;
}

/* İleri bildirimler */
static void ifade_üret(Üretici *u, Düğüm *d);
static void bildirim_uret(Üretici *u, Düğüm *d);
static void blok_uret(Üretici *u, Düğüm *blok);

/* ---- String literal (PROGMEM) ---- */

static int metin_literal_ekle(Üretici *u, const char *metin, int uzunluk) {
    int idx = u->metin_sayac++;
    veri_yaz(u, ".LC%d:", idx);

    /* Byte dizisi olarak yaz */
    Metin buf;
    metin_baslat(&buf);
    metin_ekle(&buf, "    .byte ");

    int gercek_uzunluk = 0;
    for (int i = 0; i < uzunluk; i++) {
        if (gercek_uzunluk > 0) metin_ekle(&buf, ",");
        char num[8];
        unsigned char c = (unsigned char)metin[i];

        if (c == '\\' && i + 1 < uzunluk) {
            char sonraki = metin[i + 1];
            int escape_degeri = -1;
            switch (sonraki) {
                case 'n':  escape_degeri = 10;  break;
                case 't':  escape_degeri = 9;   break;
                case 'r':  escape_degeri = 13;  break;
                case '\\': escape_degeri = 92;  break;
                case '"':  escape_degeri = 34;  break;
                case '0':  escape_degeri = 0;   break;
            }
            if (escape_degeri >= 0) {
                snprintf(num, sizeof(num), "%d", escape_degeri);
                metin_ekle(&buf, num);
                gercek_uzunluk++;
                i++;
                continue;
            }
        }

        snprintf(num, sizeof(num), "%d", c);
        metin_ekle(&buf, num);
        gercek_uzunluk++;
    }

    /* Null terminator ekle */
    metin_ekle(&buf, ",0");

    veri_yaz(u, "%s", buf.veri);
    veri_yaz(u, ".LC%d_len = %d", idx, gercek_uzunluk);
    metin_serbest(&buf);
    return idx;
}

/* ---- 16-bit immediate yükle (r24:r25) ---- */

static void avr_ldi_16(Üretici *u, const char *reg_lo, const char *reg_hi, int16_t deger) {
    yaz(u, "    ldi     %s, lo8(%d)", reg_lo, deger);
    yaz(u, "    ldi     %s, hi8(%d)", reg_hi, deger);
}

/* ---- İfade üretimi (sonuç r24:r25'te) ---- */

static void tam_sayi_uret(Üretici *u, Düğüm *d) {
    int16_t val = (int16_t)d->veri.tam_deger;
    avr_ldi_16(u, "r24", "r25", val);
}

static void metin_degeri_uret(Üretici *u, Düğüm *d) {
    int idx = metin_literal_ekle(u, d->veri.metin_değer,
                                  (int)strlen(d->veri.metin_değer));
    /* Z pointer'a (r30:r31) adres yükle */
    yaz(u, "    ldi     r30, lo8(.LC%d)", idx);
    yaz(u, "    ldi     r31, hi8(.LC%d)", idx);
    /* r24:r25 = pointer (Z'den kopyala) */
    yaz(u, "    movw    r24, r30");
}

static void mantik_uret(Üretici *u, Düğüm *d) {
    yaz(u, "    ldi     r24, %d", d->veri.mantık_değer ? 1 : 0);
    yaz(u, "    clr     r25");
}

static void tanimlayici_uret(Üretici *u, Düğüm *d) {
    Sembol *s = sembol_ara(u->kapsam, d->veri.tanimlayici.isim);
    if (!s) return;

    if (s->global_mi) {
        /* Global değişken: lds ile yükle */
        yaz(u, "    lds     r24, _genel_%s", d->veri.tanimlayici.isim);
        yaz(u, "    lds     r25, _genel_%s+1", d->veri.tanimlayici.isim);
        return;
    }

    /* Yerel değişken: Y pointer'dan yükle */
    int offset = (s->yerel_indeks + 1) * 2;  /* 16-bit değişkenler */
    if (offset <= 63) {
        yaz(u, "    ldd     r24, Y+%d", offset);
        yaz(u, "    ldd     r25, Y+%d", offset + 1);
    } else {
        /* Büyük offset için geçici adres hesapla */
        yaz(u, "    movw    r26, r28");  /* X = Y */
        yaz(u, "    subi    r26, lo8(-%d)", offset);
        yaz(u, "    sbci    r27, hi8(-%d)", offset);
        yaz(u, "    ld      r24, X+");
        yaz(u, "    ld      r25, X");
    }
}

static void ikili_islem_uret(Üretici *u, Düğüm *d) {
    /* Sol tarafı hesapla -> r24:r25, stack'e at */
    ifade_üret(u, d->çocuklar[0]);
    yaz(u, "    push    r24");
    yaz(u, "    push    r25");

    /* Sağ tarafı hesapla -> r24:r25 */
    ifade_üret(u, d->çocuklar[1]);

    /* Stack'ten sol tarafı al -> r22:r23 */
    yaz(u, "    pop     r23");
    yaz(u, "    pop     r22");

    /* İşlem türüne göre üret */
    SözcükTürü op = d->veri.islem.islem;

    switch (op) {
        case TOK_ARTI:
            yaz(u, "    add     r22, r24");
            yaz(u, "    adc     r23, r25");
            yaz(u, "    movw    r24, r22");
            break;

        case TOK_EKSI:
            yaz(u, "    sub     r22, r24");
            yaz(u, "    sbc     r23, r25");
            yaz(u, "    movw    r24, r22");
            break;

        case TOK_ÇARPIM:
            /* 16x16 çarpma - basitleştirilmiş (sadece 16-bit sonuç) */
            yaz(u, "    ; 16-bit carpma");
            yaz(u, "    mul     r22, r24");
            yaz(u, "    movw    r24, r0");
            yaz(u, "    mul     r23, r24");
            yaz(u, "    add     r25, r0");
            yaz(u, "    mul     r22, r25");
            yaz(u, "    add     r25, r0");
            yaz(u, "    clr     r1");  /* r1 = 0 kuralı */
            break;

        case TOK_BÖLME:
            /* Bölme: runtime fonksiyon çağır */
            yaz(u, "    ; 16-bit bolme");
            yaz(u, "    rcall   _div16");
            break;

        case TOK_YÜZDE:
            /* Mod: runtime fonksiyon çağır */
            yaz(u, "    rcall   _mod16");
            break;

        /* Karşılaştırma operatörleri */
        case TOK_EŞİT_EŞİT:
            yaz(u, "    cp      r22, r24");
            yaz(u, "    cpc     r23, r25");
            yaz(u, "    ldi     r24, 1");
            yaz(u, "    breq    .+2");
            yaz(u, "    ldi     r24, 0");
            yaz(u, "    clr     r25");
            break;

        case TOK_EŞİT_DEĞİL:
            yaz(u, "    cp      r22, r24");
            yaz(u, "    cpc     r23, r25");
            yaz(u, "    ldi     r24, 1");
            yaz(u, "    brne    .+2");
            yaz(u, "    ldi     r24, 0");
            yaz(u, "    clr     r25");
            break;

        case TOK_KÜÇÜK:
            yaz(u, "    cp      r22, r24");
            yaz(u, "    cpc     r23, r25");
            yaz(u, "    ldi     r24, 1");
            yaz(u, "    brlt    .+2");
            yaz(u, "    ldi     r24, 0");
            yaz(u, "    clr     r25");
            break;

        case TOK_KÜÇÜK_EŞİT:
            yaz(u, "    cp      r24, r22");
            yaz(u, "    cpc     r25, r23");
            yaz(u, "    ldi     r24, 1");
            yaz(u, "    brge    .+2");
            yaz(u, "    ldi     r24, 0");
            yaz(u, "    clr     r25");
            break;

        case TOK_BÜYÜK:
            yaz(u, "    cp      r24, r22");
            yaz(u, "    cpc     r25, r23");
            yaz(u, "    ldi     r24, 1");
            yaz(u, "    brlt    .+2");
            yaz(u, "    ldi     r24, 0");
            yaz(u, "    clr     r25");
            break;

        case TOK_BÜYÜK_EŞİT:
            yaz(u, "    cp      r22, r24");
            yaz(u, "    cpc     r23, r25");
            yaz(u, "    ldi     r24, 1");
            yaz(u, "    brge    .+2");
            yaz(u, "    ldi     r24, 0");
            yaz(u, "    clr     r25");
            break;

        /* Mantıksal operatörler */
        case TOK_VE:
            yaz(u, "    and     r24, r22");
            yaz(u, "    and     r25, r23");
            break;

        case TOK_VEYA:
            yaz(u, "    or      r24, r22");
            yaz(u, "    or      r25, r23");
            break;

        default:
            break;
    }
}

static void tekli_islem_uret(Üretici *u, Düğüm *d) {
    ifade_üret(u, d->çocuklar[0]);

    SözcükTürü op = d->veri.islem.islem;

    switch (op) {
        case TOK_EKSI:
            /* Negatif: two's complement */
            yaz(u, "    com     r24");
            yaz(u, "    com     r25");
            yaz(u, "    adiw    r24, 1");
            break;

        case TOK_DEĞİL:
            /* Mantıksal değil */
            yaz(u, "    or      r24, r25");
            yaz(u, "    ldi     r25, 0");
            yaz(u, "    cpse    r24, r25");
            yaz(u, "    ldi     r24, 0");
            yaz(u, "    rjmp    .+2");
            yaz(u, "    ldi     r24, 1");
            yaz(u, "    clr     r25");
            break;

        default:
            break;
    }
}

static void cagri_uret(Üretici *u, Düğüm *d) {
    /* Fonksiyon adı DÜĞÜM_ÇAĞRI düğümünün kendisinde, argümanlar çocuklar'da */
    const char *isim = d->veri.tanimlayici.isim;
    int arg_sayisi = d->çocuk_sayısı;

    /* Modül fonksiyonuysa runtime_isim kullan (ör: pin_modu -> _tr_pin_modu) */
    const char *cagri_isim = isim;
    const ModülFonksiyon *mf = modul_fonksiyon_bul(isim);
    if (mf && mf->runtime_isim) {
        cagri_isim = mf->runtime_isim;
    }

    /* Argümanları r24:r25, r22:r23, r20:r21, r18:r19 şeklinde yükle */
    const char *arg_regs_lo[] = {"r24", "r22", "r20", "r18"};
    const char *arg_regs_hi[] = {"r25", "r23", "r21", "r19"};

    /* Argümanları ters sırada stack'e at */
    for (int i = arg_sayisi - 1; i >= 0; i--) {
        ifade_üret(u, d->çocuklar[i]);
        yaz(u, "    push    r24");
        yaz(u, "    push    r25");
    }

    /* Stack'ten argüman register'larına al */
    for (int i = 0; i < arg_sayisi && i < 4; i++) {
        yaz(u, "    pop     %s", arg_regs_hi[i]);
        yaz(u, "    pop     %s", arg_regs_lo[i]);
    }

    /* Fonksiyon çağır */
    yaz(u, "    rcall   %s", cagri_isim);
}

static void ifade_üret(Üretici *u, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
        case DÜĞÜM_TAM_SAYI:
            tam_sayi_uret(u, d);
            break;
        case DÜĞÜM_METİN_DEĞERİ:
            metin_degeri_uret(u, d);
            break;
        case DÜĞÜM_MANTIK_DEĞERİ:
            mantik_uret(u, d);
            break;
        case DÜĞÜM_TANIMLAYICI:
            tanimlayici_uret(u, d);
            break;
        case DÜĞÜM_ÇAĞRI:
            cagri_uret(u, d);
            break;
        case DÜĞÜM_İKİLİ_İŞLEM:
            ikili_islem_uret(u, d);
            break;
        case DÜĞÜM_TEKLİ_İŞLEM:
            tekli_islem_uret(u, d);
            break;
        default:
            break;
    }
}

/* ---- Bildirim üretimi ---- */

static void degisken_uret(Üretici *u, Düğüm *d) {
    const char *isim = d->veri.değişken.isim;

    /* Sembol ekle */
    Sembol *s = sembol_ekle(u->arena, u->kapsam, isim, TİP_TAM);
    s->yerel_indeks = u->kapsam->yerel_sayac++;

    /* Başlangıç değeri varsa hesapla ve sakla */
    if (d->çocuk_sayısı > 0) {
        ifade_üret(u, d->çocuklar[0]);
        int offset = (s->yerel_indeks + 1) * 2;
        if (offset <= 63) {
            yaz(u, "    std     Y+%d, r24", offset);
            yaz(u, "    std     Y+%d, r25", offset + 1);
        } else {
            yaz(u, "    movw    r26, r28");
            yaz(u, "    subi    r26, lo8(-%d)", offset);
            yaz(u, "    sbci    r27, hi8(-%d)", offset);
            yaz(u, "    st      X+, r24");
            yaz(u, "    st      X, r25");
        }
    }
}

static void atama_uret(Üretici *u, Düğüm *d) {
    /* Sağ tarafı hesapla */
    ifade_üret(u, d->çocuklar[1]);

    /* Sol tarafa ata */
    Düğüm *sol = d->çocuklar[0];
    if (sol->tur == DÜĞÜM_TANIMLAYICI) {
        Sembol *s = sembol_ara(u->kapsam, sol->veri.tanimlayici.isim);
        if (!s) return;

        if (s->global_mi) {
            yaz(u, "    sts     _genel_%s, r24", sol->veri.tanimlayici.isim);
            yaz(u, "    sts     _genel_%s+1, r25", sol->veri.tanimlayici.isim);
        } else {
            int offset = (s->yerel_indeks + 1) * 2;
            if (offset <= 63) {
                yaz(u, "    std     Y+%d, r24", offset);
                yaz(u, "    std     Y+%d, r25", offset + 1);
            } else {
                yaz(u, "    movw    r26, r28");
                yaz(u, "    subi    r26, lo8(-%d)", offset);
                yaz(u, "    sbci    r27, hi8(-%d)", offset);
                yaz(u, "    st      X+, r24");
                yaz(u, "    st      X, r25");
            }
        }
    }
}

static void eger_uret(Üretici *u, Düğüm *d) {
    int yoksa_etiket = yeni_etiket(u);
    int son_etiket = yeni_etiket(u);

    /* Koşul */
    ifade_üret(u, d->çocuklar[0]);
    yaz(u, "    or      r24, r25");
    yaz(u, "    breq    .L%d", yoksa_etiket);

    /* Doğru blok */
    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1]);
    }
    yaz(u, "    rjmp    .L%d", son_etiket);

    /* Yanlış blok (yoksa) */
    yaz(u, ".L%d:", yoksa_etiket);
    if (d->çocuk_sayısı > 2) {
        blok_uret(u, d->çocuklar[2]);
    }

    yaz(u, ".L%d:", son_etiket);
}

static void iken_uret(Üretici *u, Düğüm *d) {
    int başlangıç = yeni_etiket(u);
    int bitis = yeni_etiket(u);

    int eski_baslangic = u->dongu_baslangic_etiket;
    int eski_bitis = u->dongu_bitis_etiket;
    u->dongu_baslangic_etiket = başlangıç;
    u->dongu_bitis_etiket = bitis;

    yaz(u, ".L%d:", başlangıç);

    /* Koşul */
    ifade_üret(u, d->çocuklar[0]);
    yaz(u, "    or      r24, r25");
    yaz(u, "    breq    .L%d", bitis);

    /* Gövde */
    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1]);
    }

    yaz(u, "    rjmp    .L%d", başlangıç);
    yaz(u, ".L%d:", bitis);

    u->dongu_baslangic_etiket = eski_baslangic;
    u->dongu_bitis_etiket = eski_bitis;
}

static void dongu_uret(Üretici *u, Düğüm *d) {
    /* döngü i = 0, 10 ise ... son */
    int başlangıç = yeni_etiket(u);
    int bitis = yeni_etiket(u);

    int eski_baslangic = u->dongu_baslangic_etiket;
    int eski_bitis = u->dongu_bitis_etiket;
    u->dongu_baslangic_etiket = başlangıç;
    u->dongu_bitis_etiket = bitis;

    /* Sayaç değişkenini tanımla ve başlat */
    const char *sayac_isim = d->veri.dongu.isim;
    Sembol *s = sembol_ekle(u->arena, u->kapsam, sayac_isim, TİP_TAM);
    s->yerel_indeks = u->kapsam->yerel_sayac++;

    /* Başlangıç değeri */
    ifade_üret(u, d->çocuklar[0]);
    int offset = (s->yerel_indeks + 1) * 2;
    yaz(u, "    std     Y+%d, r24", offset);
    yaz(u, "    std     Y+%d, r25", offset + 1);

    /* Bitiş değerini hesapla ve sakla (geçici) */
    ifade_üret(u, d->çocuklar[1]);
    yaz(u, "    push    r24");
    yaz(u, "    push    r25");

    yaz(u, ".L%d:", başlangıç);

    /* Sayaç < bitiş kontrolü */
    yaz(u, "    ldd     r22, Y+%d", offset);
    yaz(u, "    ldd     r23, Y+%d", offset + 1);
    /* Bitiş değerini stack'ten peek */
    yaz(u, "    pop     r25");
    yaz(u, "    pop     r24");
    yaz(u, "    push    r24");
    yaz(u, "    push    r25");
    yaz(u, "    cp      r22, r24");
    yaz(u, "    cpc     r23, r25");
    yaz(u, "    brge    .L%d", bitis);

    /* Gövde */
    if (d->çocuk_sayısı > 2) {
        blok_uret(u, d->çocuklar[2]);
    }

    /* Sayacı artır */
    yaz(u, "    ldd     r24, Y+%d", offset);
    yaz(u, "    ldd     r25, Y+%d", offset + 1);
    yaz(u, "    adiw    r24, 1");
    yaz(u, "    std     Y+%d, r24", offset);
    yaz(u, "    std     Y+%d, r25", offset + 1);

    yaz(u, "    rjmp    .L%d", başlangıç);
    yaz(u, ".L%d:", bitis);

    /* Bitiş değerini stack'ten temizle */
    yaz(u, "    pop     r25");
    yaz(u, "    pop     r24");

    u->dongu_baslangic_etiket = eski_baslangic;
    u->dongu_bitis_etiket = eski_bitis;
}

static void dondur_uret(Üretici *u, Düğüm *d) {
    if (d->çocuk_sayısı > 0) {
        ifade_üret(u, d->çocuklar[0]);
    }
    /* Fonksiyon epilog'una atla */
    yaz(u, "    rjmp    .Lret_fn");
}

static void kir_uret(Üretici *u, Düğüm *d) {
    (void)d;
    if (u->dongu_bitis_etiket > 0) {
        yaz(u, "    rjmp    .L%d", u->dongu_bitis_etiket);
    }
}

static void devam_uret(Üretici *u, Düğüm *d) {
    (void)d;
    if (u->dongu_baslangic_etiket > 0) {
        yaz(u, "    rjmp    .L%d", u->dongu_baslangic_etiket);
    }
}

static void bildirim_uret(Üretici *u, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
        case DÜĞÜM_DEĞİŞKEN:
            degisken_uret(u, d);
            break;
        case DÜĞÜM_ATAMA:
            atama_uret(u, d);
            break;
        case DÜĞÜM_EĞER:
            eger_uret(u, d);
            break;
        case DÜĞÜM_İKEN:
            iken_uret(u, d);
            break;
        case DÜĞÜM_DÖNGÜ:
            dongu_uret(u, d);
            break;
        case DÜĞÜM_DÖNDÜR:
            dondur_uret(u, d);
            break;
        case DÜĞÜM_KIR:
            kir_uret(u, d);
            break;
        case DÜĞÜM_DEVAM:
            devam_uret(u, d);
            break;
        case DÜĞÜM_İFADE_BİLDİRİMİ:
            if (d->çocuk_sayısı > 0) {
                ifade_üret(u, d->çocuklar[0]);
            }
            break;
        case DÜĞÜM_ÇAĞRI:
            cagri_uret(u, d);
            break;
        default:
            /* Diğer ifadeler */
            ifade_üret(u, d);
            break;
    }
}

static void blok_uret(Üretici *u, Düğüm *blok) {
    if (!blok) return;

    for (int i = 0; i < blok->çocuk_sayısı; i++) {
        bildirim_uret(u, blok->çocuklar[i]);
    }
}

/* ---- Fonksiyon üretimi ---- */

static void islev_uret(Üretici *u, Düğüm *d) {
    const char *isim = d->veri.islev.isim;

    /* Yeni kapsam */
    Kapsam *eski_kapsam = u->kapsam;
    u->kapsam = kapsam_oluştur(u->arena, eski_kapsam);

    yaz(u, "");
    yaz(u, "; ======== islev: %s ========", isim);
    yaz(u, ".global %s", isim);
    yaz(u, "%s:", isim);

    /* Prolog */
    yaz(u, "    push    r28");
    yaz(u, "    push    r29");
    yaz(u, "    in      r28, 0x3d");  /* SPL -> Y low */
    yaz(u, "    in      r29, 0x3e");  /* SPH -> Y high */

    /* Stack alanı ayır (maksimum 32 yerel değişken = 64 byte) */
    int stack_boyut = 64;
    yaz(u, "    subi    r28, lo8(%d)", stack_boyut);
    yaz(u, "    sbci    r29, hi8(%d)", stack_boyut);
    yaz(u, "    out     0x3d, r28");
    yaz(u, "    out     0x3e, r29");

    /* Parametreleri yerel değişken olarak kaydet */
    Düğüm *params = d->çocuklar[0];
    const char *param_regs_lo[] = {"r24", "r22", "r20", "r18"};
    const char *param_regs_hi[] = {"r25", "r23", "r21", "r19"};

    for (int i = 0; i < params->çocuk_sayısı && i < 4; i++) {
        const char *param_isim = params->çocuklar[i]->veri.değişken.isim;
        Sembol *s = sembol_ekle(u->arena, u->kapsam, param_isim, TİP_TAM);
        s->yerel_indeks = u->kapsam->yerel_sayac++;
        s->parametre_mi = 1;

        int offset = (s->yerel_indeks + 1) * 2;
        yaz(u, "    std     Y+%d, %s", offset, param_regs_lo[i]);
        yaz(u, "    std     Y+%d, %s", offset + 1, param_regs_hi[i]);
    }

    /* Fonksiyon gövdesi */
    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1]);
    }

    /* Epilog */
    yaz(u, ".Lret_fn:");
    /* adiw sadece 0-63 arası değer alır, büyük değerler için subi/sbci kullan */
    if (stack_boyut <= 63) {
        yaz(u, "    adiw    r28, %d", stack_boyut);
    } else {
        yaz(u, "    subi    r28, lo8(-%d)", stack_boyut);
        yaz(u, "    sbci    r29, hi8(-%d)", stack_boyut);
    }
    yaz(u, "    out     0x3d, r28");
    yaz(u, "    out     0x3e, r29");
    yaz(u, "    pop     r29");
    yaz(u, "    pop     r28");
    yaz(u, "    ret");

    u->kapsam = eski_kapsam;
}

/* ---- Yardımcı fonksiyonlar (runtime) ---- */

static void runtime_uret(Üretici *u) {
    /* 16-bit bölme */
    yardimci_yaz(u, "");
    yardimci_yaz(u, "; 16-bit bolme: r22:r23 / r24:r25 -> r24:r25");
    yardimci_yaz(u, "_div16:");
    yardimci_yaz(u, "    push    r16");
    yardimci_yaz(u, "    push    r17");
    yardimci_yaz(u, "    clr     r16");
    yardimci_yaz(u, "    clr     r17");
    yardimci_yaz(u, "    ldi     r18, 16");
    yardimci_yaz(u, "_div16_loop:");
    yardimci_yaz(u, "    lsl     r22");
    yardimci_yaz(u, "    rol     r23");
    yardimci_yaz(u, "    rol     r16");
    yardimci_yaz(u, "    rol     r17");
    yardimci_yaz(u, "    cp      r16, r24");
    yardimci_yaz(u, "    cpc     r17, r25");
    yardimci_yaz(u, "    brcs    _div16_skip");
    yardimci_yaz(u, "    sub     r16, r24");
    yardimci_yaz(u, "    sbc     r17, r25");
    yardimci_yaz(u, "    inc     r22");
    yardimci_yaz(u, "_div16_skip:");
    yardimci_yaz(u, "    dec     r18");
    yardimci_yaz(u, "    brne    _div16_loop");
    yardimci_yaz(u, "    movw    r24, r22");
    yardimci_yaz(u, "    pop     r17");
    yardimci_yaz(u, "    pop     r16");
    yardimci_yaz(u, "    ret");

    /* 16-bit mod */
    yardimci_yaz(u, "");
    yardimci_yaz(u, "; 16-bit mod: r22:r23 %% r24:r25 -> r24:r25");
    yardimci_yaz(u, "_mod16:");
    yardimci_yaz(u, "    push    r16");
    yardimci_yaz(u, "    push    r17");
    yardimci_yaz(u, "    clr     r16");
    yardimci_yaz(u, "    clr     r17");
    yardimci_yaz(u, "    ldi     r18, 16");
    yardimci_yaz(u, "_mod16_loop:");
    yardimci_yaz(u, "    lsl     r22");
    yardimci_yaz(u, "    rol     r23");
    yardimci_yaz(u, "    rol     r16");
    yardimci_yaz(u, "    rol     r17");
    yardimci_yaz(u, "    cp      r16, r24");
    yardimci_yaz(u, "    cpc     r17, r25");
    yardimci_yaz(u, "    brcs    _mod16_skip");
    yardimci_yaz(u, "    sub     r16, r24");
    yardimci_yaz(u, "    sbc     r17, r25");
    yardimci_yaz(u, "_mod16_skip:");
    yardimci_yaz(u, "    dec     r18");
    yardimci_yaz(u, "    brne    _mod16_loop");
    yardimci_yaz(u, "    movw    r24, r16");
    yardimci_yaz(u, "    pop     r17");
    yardimci_yaz(u, "    pop     r16");
    yardimci_yaz(u, "    ret");
}

/* ---- Ana kod üretim fonksiyonu ---- */

void kod_uret_avr(Üretici *u, Düğüm *program, Arena *arena) {
    u->arena = arena;
    u->etiket_sayac = 0;
    u->metin_sayac = 0;
    u->dongu_baslangic_etiket = 0;
    u->dongu_bitis_etiket = 0;
    u->mevcut_islev_donus_tipi = TİP_BOŞLUK;
    u->mevcut_sinif = NULL;

    metin_baslat(&u->cikti);
    metin_baslat(&u->veri_bolumu);
    metin_baslat(&u->bss_bolumu);
    metin_baslat(&u->yardimcilar);

    u->kapsam = kapsam_oluştur(arena, NULL);

    /* AVR başlangıç - GNU as uyumlu */
    yaz(u, "; ========================================");
    yaz(u, "; Tonyukuk AVR Derleyici Ciktisi");
    yaz(u, "; Hedef: ATmega328P (Arduino UNO/Nano)");
    yaz(u, "; GNU as uyumlu assembly");
    yaz(u, "; ========================================");
    yaz(u, "");
    yaz(u, "#include <avr/io.h>");
    yaz(u, "");
    yaz(u, ".section .text");
    yaz(u, ".weak main");
    yaz(u, ".global _baslangic");
    yaz(u, "");

    /* Varsayılan başlangıç kodu (runtime varsa onun main'i kullanılır) */
    yaz(u, "_baslangic:");
    yaz(u, "main:");
    yaz(u, "    ; Stack pointer avr-gcc tarafindan ayarlandi");
    yaz(u, "    ; r1 = 0 (ABI kurali)");
    yaz(u, "    clr     r1");
    yaz(u, "    ; ana fonksiyonu cagir");
    yaz(u, "    rcall   ana");
    yaz(u, "_dur:");
    yaz(u, "    rjmp    _dur");
    yaz(u, "");

    /* Fonksiyonları üret */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur == DÜĞÜM_İŞLEV) {
            islev_uret(u, d);
        }
    }

    /* Runtime yardımcı fonksiyonları */
    runtime_uret(u);

    /* Veri bölümü */
    if (u->veri_bolumu.uzunluk > 0) {
        yaz(u, "");
        yaz(u, "; ======== Veri Bolumu ========");
        yaz(u, ".section .rodata");
        metin_satir_ekle(&u->cikti, u->veri_bolumu.veri);
    }

    /* BSS bölümü */
    if (u->bss_bolumu.uzunluk > 0) {
        yaz(u, "");
        yaz(u, "; ======== BSS Bolumu ========");
        yaz(u, ".section .bss");
        metin_satir_ekle(&u->cikti, u->bss_bolumu.veri);
    }

    /* Yardımcı fonksiyonlar */
    if (u->yardimcilar.uzunluk > 0) {
        metin_satir_ekle(&u->cikti, u->yardimcilar.veri);
    }
}
