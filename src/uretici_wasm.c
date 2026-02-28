/*
 * uretici_wasm.c — WebAssembly Text Format (.wat) kod üretici
 *
 * Tonyukuk Turkce programlama dili derleyicisi icin WASM backend.
 * Mevcut Üretici yapisini kullanir; WAT (WebAssembly Text Format) uretir.
 *
 * WASM MVP:
 *   - i64 tam sayi aritmetik
 *   - Yerel degiskenler (local.get / local.set)
 *   - Fonksiyon tanimlari
 *   - yazdir icin import (env.print_i64 / env.print_str)
 *   - if/else kontrol akisi
 *   - Donguler (block/loop/br_if)
 *   - String literaller (data section)
 */

#include "uretici_wasm.h"
#include "uretici.h"
#include "hata.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* ---- Yardimci: WAT satiri yaz ---- */

static void yaz(Üretici *u, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->cikti, buf);
}

/* Fonksiyonlari ayri tamponlara yaz */
static void fn_yaz(Üretici *u, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->yardimcilar, buf);
}

static void veri_yaz(Üretici *u, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->veri_bolumu, buf);
}

static int yeni_etiket(Üretici *u) {
    return u->etiket_sayac++;
}

/* ---- Ileri bildirimler ---- */
static void ifade_üret(Üretici *u, Düğüm *d);
static void bildirim_uret(Üretici *u, Düğüm *d, int girinti);
static void blok_uret(Üretici *u, Düğüm *blok, int girinti);
static void islev_uret(Üretici *u, Düğüm *d);

/* Girinti yardimcisi */
static const char *indent(int n) {
    static char buf[64];
    if (n > 30) n = 30;
    for (int i = 0; i < n * 2; i++) buf[i] = ' ';
    buf[n * 2] = '\0';
    return buf;
}

/* ---- WASM yerel değişken takibi ---- */

/* Her fonksiyon icin yerel değişken tablosu */
#define WASM_MAKS_YEREL 256

typedef struct {
    char *isim;
    int   indeks;
} WasmYerel;

static WasmYerel wasm_yereller[WASM_MAKS_YEREL];
static int wasm_yerel_sayisi = 0;
static int wasm_yerel_sayac = 0;

static void wasm_yerel_sifirla(void) {
    wasm_yerel_sayisi = 0;
    wasm_yerel_sayac = 0;
}

static int wasm_yerel_ekle(const char *isim) {
    int idx = wasm_yerel_sayac++;
    if (wasm_yerel_sayisi < WASM_MAKS_YEREL) {
        wasm_yereller[wasm_yerel_sayisi].isim = (char *)isim;
        wasm_yereller[wasm_yerel_sayisi].indeks = idx;
        wasm_yerel_sayisi++;
    }
    return idx;
}

static int wasm_yerel_bul(const char *isim) {
    for (int i = 0; i < wasm_yerel_sayisi; i++) {
        if (strcmp(wasm_yereller[i].isim, isim) == 0)
            return wasm_yereller[i].indeks;
    }
    return -1;
}

/* ---- Metin literal veri bolumu ---- */

static int wasm_metin_offset = 0;  /* Data section'daki mevcut offset */

typedef struct {
    int offset;
    int uzunluk;
} WasmMetinBilgi;

static WasmMetinBilgi wasm_metinler[256];
static int wasm_metin_sayisi_g = 0;

static WasmMetinBilgi wasm_metin_literal_ekle(Üretici *u, const char *metin, int uzunluk) {
    WasmMetinBilgi bilgi;
    bilgi.offset = wasm_metin_offset;

    /* Escape islemesi */
    int gercek_uzunluk = 0;
    char gecici[4096];
    for (int i = 0; i < uzunluk && gercek_uzunluk < (int)sizeof(gecici) - 1; i++) {
        unsigned char c = (unsigned char)metin[i];
        if (c == '\\' && i + 1 < uzunluk) {
            char sonraki = metin[i + 1];
            switch (sonraki) {
                case 'n': gecici[gercek_uzunluk++] = '\n'; i++; continue;
                case 't': gecici[gercek_uzunluk++] = '\t'; i++; continue;
                case 'r': gecici[gercek_uzunluk++] = '\r'; i++; continue;
                case '\\': gecici[gercek_uzunluk++] = '\\'; i++; continue;
                case '"': gecici[gercek_uzunluk++] = '"'; i++; continue;
                case '0': gecici[gercek_uzunluk++] = '\0'; i++; continue;
            }
        }
        gecici[gercek_uzunluk++] = (char)c;
    }

    bilgi.uzunluk = gercek_uzunluk;

    /* WAT data segmenti icin hex string olustur */
    Metin hex_buf;
    metin_baslat(&hex_buf);
    for (int i = 0; i < gercek_uzunluk; i++) {
        char h[4];
        snprintf(h, sizeof(h), "\\%02x", (unsigned char)gecici[i]);
        metin_ekle(&hex_buf, h);
    }

    veri_yaz(u, "  (data (i32.const %d) \"%s\")", wasm_metin_offset, hex_buf.veri);
    metin_serbest(&hex_buf);

    wasm_metin_offset += gercek_uzunluk;

    if (wasm_metin_sayisi_g < 256) {
        wasm_metinler[wasm_metin_sayisi_g++] = bilgi;
    }

    return bilgi;
}

/* ---- Ifade uretimi (sonuç WASM stack'te) ---- */

static void ifade_üret(Üretici *u, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
    case DÜĞÜM_TAM_SAYI:
        yaz(u, "    i64.const %lld", (long long)d->veri.tam_deger);
        break;

    case DÜĞÜM_METİN_DEĞERİ: {
        /* Metin literali data section'a ekle */
        WasmMetinBilgi bilgi = wasm_metin_literal_ekle(u, d->veri.metin_değer,
                                                        (int)strlen(d->veri.metin_değer));
        /* Degisken atamasinda kullaniliyorsa i64 olarak kodla:
           (offset << 32) | length */
        /* Yazdir cagrisi icinde kullaniliyorsa i32 cift olarak birak */
        /* Kontrol: ust dugum yazdir cagrisi mi? Bunu bilemeyiz.
           Cozum: her zaman i32 cift koy, yazdir bunu bekler.
           Degisken atamasinda ise ozel islem gerekir. */
        yaz(u, "    i32.const %d  ;; metin offset", bilgi.offset);
        yaz(u, "    i32.const %d  ;; metin uzunluk", bilgi.uzunluk);
        break;
    }

    case DÜĞÜM_MANTIK_DEĞERİ:
        yaz(u, "    i64.const %d", d->veri.mantık_değer);
        break;

    case DÜĞÜM_BOŞ_DEĞER:
        yaz(u, "    i64.const 0");
        break;

    case DÜĞÜM_TANIMLAYICI: {
        int idx = wasm_yerel_bul(d->veri.tanimlayici.isim);
        if (idx >= 0) {
            yaz(u, "    local.get %d  ;; %s", idx, d->veri.tanimlayici.isim);
        } else {
            /* Tanimlanamadi — varsayilan 0 */
            yaz(u, "    i64.const 0  ;; tanimsiz: %s", d->veri.tanimlayici.isim);
        }
        break;
    }

    case DÜĞÜM_İKİLİ_İŞLEM:
        /* Her iki tarafi hesapla (sol, sag WASM stack'te) */
        ifade_üret(u, d->çocuklar[0]);
        ifade_üret(u, d->çocuklar[1]);

        switch (d->veri.islem.islem) {
        case TOK_ARTI:
            yaz(u, "    i64.add");
            break;
        case TOK_EKSI:
            yaz(u, "    i64.sub");
            break;
        case TOK_ÇARPIM:
            yaz(u, "    i64.mul");
            break;
        case TOK_BÖLME:
            yaz(u, "    i64.div_s");
            break;
        case TOK_YÜZDE:
            yaz(u, "    i64.rem_s");
            break;
        case TOK_EŞİT_EŞİT:
            yaz(u, "    i64.eq");
            /* i64.eq returns i32, extend to i64 */
            yaz(u, "    i64.extend_i32_u");
            break;
        case TOK_EŞİT_DEĞİL:
            yaz(u, "    i64.ne");
            yaz(u, "    i64.extend_i32_u");
            break;
        case TOK_KÜÇÜK:
            yaz(u, "    i64.lt_s");
            yaz(u, "    i64.extend_i32_u");
            break;
        case TOK_BÜYÜK:
            yaz(u, "    i64.gt_s");
            yaz(u, "    i64.extend_i32_u");
            break;
        case TOK_KÜÇÜK_EŞİT:
            yaz(u, "    i64.le_s");
            yaz(u, "    i64.extend_i32_u");
            break;
        case TOK_BÜYÜK_EŞİT:
            yaz(u, "    i64.ge_s");
            yaz(u, "    i64.extend_i32_u");
            break;
        case TOK_VE:
            /* a && b: stack'te [sol_i64, sag_i64] var */
            /* Strateji: sol != 0 AND sag != 0 */
            /* Sag'i bool yap */
            yaz(u, "    i64.const 0");
            yaz(u, "    i64.ne");       /* sag_bool (i32) */
            yaz(u, "    i64.extend_i32_u");  /* sag_bool (i64) */
            /* Bitwise AND: sol & sag_bool */
            /* sol 0 ise sonuç 0, sol != 0 ve sag_bool 1 ise sonuç != 0 */
            yaz(u, "    i64.and");
            /* Sonucu bool'a cevir */
            yaz(u, "    i64.const 0");
            yaz(u, "    i64.ne");
            yaz(u, "    i64.extend_i32_u");
            break;
        case TOK_VEYA:
            yaz(u, "    i64.or");
            yaz(u, "    i64.const 0");
            yaz(u, "    i64.ne");
            yaz(u, "    i64.extend_i32_u");
            break;
        default:
            break;
        }
        break;

    case DÜĞÜM_TEKLİ_İŞLEM:
        ifade_üret(u, d->çocuklar[0]);
        if (d->veri.islem.islem == TOK_EKSI) {
            /* -deger: deger zaten stack'te, -1 ile carp */
            yaz(u, "    i64.const -1");
            yaz(u, "    i64.mul");
        } else if (d->veri.islem.islem == TOK_DEĞİL) {
            yaz(u, "    i64.eqz");
            yaz(u, "    i64.extend_i32_u");
        }
        break;

    case DÜĞÜM_ÇAĞRI:
        /* Fonksiyon cagrisi */
        if (!d->veri.tanimlayici.isim) break;

        if (strcmp(d->veri.tanimlayici.isim, "yazd\xc4\xb1r") == 0) {
            /* yazdir */
            if (d->çocuk_sayısı > 0) {
                Düğüm *arg = d->çocuklar[0];
                if (arg->tur == DÜĞÜM_METİN_DEĞERİ) {
                    /* Metin literal yazdir: print_str(offset, len) */
                    ifade_üret(u, arg);  /* stack'e offset ve len koyar (i32) */
                    yaz(u, "    call $print_str");
                } else if (arg->sonuç_tipi == TİP_METİN) {
                    /* Metin degiskeni: değişken degerini al,
                       i64 icinde (offset << 32 | length) kodlanmis */
                    ifade_üret(u, arg);
                    /* offset = (val >> 32) as i32 */
                    yaz(u, "    i64.const 32");
                    yaz(u, "    i64.shr_u");
                    yaz(u, "    i32.wrap_i64");
                    /* length icin tekrar al */
                    ifade_üret(u, arg);
                    yaz(u, "    i32.wrap_i64");  /* alt 32 bit = length */
                    yaz(u, "    call $print_str");
                } else {
                    /* Tam sayi yazdir */
                    ifade_üret(u, arg);
                    yaz(u, "    call $print_i64");
                }
            }
        } else {
            /* Genel fonksiyon cagrisi */
            for (int i = 0; i < d->çocuk_sayısı; i++) {
                ifade_üret(u, d->çocuklar[i]);
            }
            yaz(u, "    call $%s", d->veri.tanimlayici.isim);
        }
        break;

    default:
        yaz(u, "    i64.const 0  ;; desteklenmiyor: %d", d->tur);
        break;
    }
}

/* ---- Bildirim uretimi ---- */

static void bildirim_uret(Üretici *u, Düğüm *d, int girinti) {
    if (!d) return;
    (void)girinti;

    switch (d->tur) {
    case DÜĞÜM_BLOK:
        blok_uret(u, d, girinti);
        break;

    case DÜĞÜM_DEĞİŞKEN: {
        /* Yerel değişken: local.set */
        int idx = wasm_yerel_ekle(d->veri.değişken.isim);
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
            yaz(u, "    local.set %d  ;; %s", idx, d->veri.değişken.isim);
        }
        break;
    }

    case DÜĞÜM_ATAMA: {
        int idx = wasm_yerel_bul(d->veri.tanimlayici.isim);
        if (idx >= 0 && d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
            yaz(u, "    local.set %d  ;; %s", idx, d->veri.tanimlayici.isim);
        }
        break;
    }

    case DÜĞÜM_İFADE_BİLDİRİMİ:
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
            /* Eger ifade sonuç uretiyorsa stack'ten at
               (yazdir zaten drop etmez, ama diger fonksiyonlar olabilir) */
            /* Basitlestirme: yazdir haricinde drop */
            if (d->çocuklar[0]->tur == DÜĞÜM_ÇAĞRI &&
                d->çocuklar[0]->veri.tanimlayici.isim &&
                strcmp(d->çocuklar[0]->veri.tanimlayici.isim, "yazd\xc4\xb1r") == 0) {
                /* yazdir sonuç dondurmuyor, drop'a gerek yok */
            } else if (d->çocuklar[0]->tur == DÜĞÜM_ÇAĞRI) {
                /* Fonksiyon sonuç dondurebilir, drop gerekebilir */
                /* MVP icin atla */
            }
        }
        break;

    case DÜĞÜM_EĞER: {
        /* if / else */
        ifade_üret(u, d->çocuklar[0]);
        /* i64 -> i32 donusumu (WASM if i32 bekler) */
        yaz(u, "    i64.const 0");
        yaz(u, "    i64.ne");
        yaz(u, "    if");

        if (d->çocuk_sayısı > 1) blok_uret(u, d->çocuklar[1], girinti + 1);

        /* yoksa blogu */
        if (d->çocuk_sayısı > 2) {
            yaz(u, "    else");
            /* Son cocuk blok ise yoksa blogu */
            if (d->çocuklar[d->çocuk_sayısı - 1]->tur == DÜĞÜM_BLOK) {
                blok_uret(u, d->çocuklar[d->çocuk_sayısı - 1], girinti + 1);
            }
        }

        yaz(u, "    end");
        break;
    }

    case DÜĞÜM_İKEN: {
        /* while dongusu: block { loop { ... br_if ... br } } */
        yaz(u, "    block $blk_%d", yeni_etiket(u));
        int loop_lbl = yeni_etiket(u);
        yaz(u, "    loop $loop_%d", loop_lbl);

        /* Kosul: yanlis ise block'tan cik */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    i64.eqz");
        yaz(u, "    br_if 1  ;; kosul yanlis, block'tan cik");

        /* Govde */
        if (d->çocuk_sayısı > 1) blok_uret(u, d->çocuklar[1], girinti + 1);

        /* Basa don */
        yaz(u, "    br 0  ;; loop basina don");
        yaz(u, "    end  ;; loop sonu");
        yaz(u, "    end  ;; block sonu");
        break;
    }

    case DÜĞÜM_DÖNGÜ: {
        /* for dongusu: değişken tanimla, block/loop ile isle */
        int sayac_idx = wasm_yerel_ekle(d->veri.dongu.isim);

        /* Baslangic degeri */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    local.set %d  ;; %s", sayac_idx, d->veri.dongu.isim);

        /* Bitis degerini gecici degiskene kaydet */
        int bitis_idx = wasm_yerel_ekle("__dongu_bitis");
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    local.set %d  ;; bitis", bitis_idx);

        int blk_lbl = yeni_etiket(u);
        int loop_lbl = yeni_etiket(u);
        yaz(u, "    block $blk_%d", blk_lbl);
        yaz(u, "    loop $loop_%d", loop_lbl);

        /* Kosul: sayac > bitis ise cik */
        yaz(u, "    local.get %d", sayac_idx);
        yaz(u, "    local.get %d", bitis_idx);
        yaz(u, "    i64.gt_s");
        yaz(u, "    br_if 1  ;; sayac > bitis, cik");

        /* Govde */
        if (d->çocuk_sayısı > 2) blok_uret(u, d->çocuklar[2], girinti + 1);

        /* Sayaci artir */
        yaz(u, "    local.get %d", sayac_idx);
        yaz(u, "    i64.const 1");
        yaz(u, "    i64.add");
        yaz(u, "    local.set %d", sayac_idx);

        yaz(u, "    br 0  ;; loop basina don");
        yaz(u, "    end  ;; loop sonu");
        yaz(u, "    end  ;; block sonu");
        break;
    }

    case DÜĞÜM_DÖNDÜR:
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
        }
        yaz(u, "    return");
        break;

    case DÜĞÜM_KULLAN:
    case DÜĞÜM_SINIF:
        break;

    default:
        break;
    }
}

static void blok_uret(Üretici *u, Düğüm *blok, int girinti) {
    if (!blok) return;
    for (int i = 0; i < blok->çocuk_sayısı; i++) {
        bildirim_uret(u, blok->çocuklar[i], girinti);
    }
}

/* ---- Islev uretimi ---- */

static void islev_uret(Üretici *u, Düğüm *d) {
    char *isim = d->veri.islev.isim;
    TipTürü donus = d->veri.islev.dönüş_tipi ?
        tip_adı_çevir(d->veri.islev.dönüş_tipi) : TİP_BOŞLUK;

    wasm_yerel_sifirla();

    /* Fonksiyon imzasi */
    Metin imza;
    metin_baslat(&imza);

    /* Parametreler */
    if (d->çocuk_sayısı > 0) {
        Düğüm *params = d->çocuklar[0];
        for (int i = 0; i < params->çocuk_sayısı; i++) {
            wasm_yerel_ekle(params->çocuklar[i]->veri.değişken.isim);
            char p[64];
            snprintf(p, sizeof(p), " (param i64)");
            metin_ekle(&imza, p);
        }
    }

    /* Donus tipi */
    if (donus != TİP_BOŞLUK) {
        metin_ekle(&imza, " (result i64)");
    }

    fn_yaz(u, "  (func $%s (export \"%s\")%s", isim, isim, imza.veri);
    metin_serbest(&imza);

    /* Govdedeki yerel degiskenleri hesapla (basit tahmin: 16 yerel) */
    /* Onceden tanimlayamayiz — govdeyi analiz edip sonra yazmeliyiz.
       Basitlestirme: yeterli sayida yerel tanimla. */
    fn_yaz(u, "    ;; yerel degiskenler (MVP: on tanimli havuz)");
    int yerel_baslangic = wasm_yerel_sayac;
    /* 16 adet yerel tanimla (gerekmedikleri takdirde bos kalir) */
    for (int i = 0; i < 16; i++) {
        fn_yaz(u, "    (local i64)");
        wasm_yerel_sayac++;
    }

    /* Govde */
    /* Gecici olarak fn_yaz yerine yaz kullan (ana buffer'a) —
       sonra fonksiyonlar arasina ekle */
    Metin onceki = u->cikti;
    metin_baslat(&u->cikti);

    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1], 2);
    }

    /* Varsayilan donus */
    if (donus != TİP_BOŞLUK) {
        yaz(u, "    i64.const 0");
    }

    /* Govdeyi fn tamponuna ekle */
    if (u->cikti.uzunluk > 0) {
        metin_ekle(&u->yardimcilar, u->cikti.veri);
    }
    metin_serbest(&u->cikti);
    u->cikti = onceki;

    fn_yaz(u, "  )");
    (void)yerel_baslangic;
}

/* ================ Ana giris noktasi ================ */

void kod_uret_wasm(Üretici *u, Düğüm *program, Arena *arena) {
    u->arena = arena;
    u->etiket_sayac = 0;
    u->metin_sayac = 0;
    u->dongu_baslangic_etiket = 0;
    u->dongu_bitis_etiket = 0;
    u->mevcut_islev_donus_tipi = TİP_BOŞLUK;
    u->mevcut_sinif = NULL;
    u->yazdir_tam_uretildi = 0;
    u->yazdir_metin_uretildi = 0;
    u->yazdir_ondalik_uretildi = 0;
    u->ondalik_sayac = 0;
    metin_baslat(&u->cikti);
    metin_baslat(&u->veri_bolumu);
    metin_baslat(&u->bss_bolumu);
    metin_baslat(&u->yardimcilar);
    u->kapsam = kapsam_oluştur(arena, NULL);

    /* Global WASM metin state sifirla */
    wasm_metin_offset = 0;
    wasm_metin_sayisi_g = 0;

    /* Modul baslangi */
    yaz(u, "(module");
    yaz(u, "  ;; Tonyukuk Derleyici tarafindan uretildi — WASM hedefi");
    yaz(u, "");

    /* Importlar */
    yaz(u, "  ;; Import: yazdirma fonksiyonlari");
    yaz(u, "  (import \"env\" \"print_i64\" (func $print_i64 (param i64)))");
    yaz(u, "  (import \"env\" \"print_str\" (func $print_str (param i32) (param i32)))");
    yaz(u, "");

    /* Bellek */
    yaz(u, "  ;; Bellek (1 sayfa = 64KB)");
    yaz(u, "  (memory (export \"memory\") 1)");
    yaz(u, "");

    /* Fonksiyon sembollerini on-kaydet */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur == DÜĞÜM_İŞLEV) {
            TipTürü donus = d->veri.islev.dönüş_tipi ?
                tip_adı_çevir(d->veri.islev.dönüş_tipi) : TİP_BOŞLUK;
            Sembol *fn_sem = sembol_ekle(u->arena, u->kapsam, d->veri.islev.isim, donus);
            if (fn_sem) {
                fn_sem->dönüş_tipi = donus;
                if (d->çocuk_sayısı > 0) {
                    Düğüm *params = d->çocuklar[0];
                    fn_sem->param_sayisi = params->çocuk_sayısı;
                    for (int j = 0; j < params->çocuk_sayısı && j < 32; j++) {
                        fn_sem->param_tipleri[j] = tip_adı_çevir(params->çocuklar[j]->veri.değişken.tip);
                    }
                }
            }
        }
    }

    /* Fonksiyonlari uret (yardimcilar tamponuna) */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur == DÜĞÜM_İŞLEV) {
            Kapsam *onceki_kapsam = u->kapsam;
            islev_uret(u, d);
            u->kapsam = onceki_kapsam;
        }
    }

    /* main fonksiyonu: ust-duzey bildirimler */
    wasm_yerel_sifirla();
    yaz(u, "  (func $main (export \"main\")");

    /* Yerel değişken havuzu */
    for (int i = 0; i < 32; i++) {
        yaz(u, "    (local i64)");
        wasm_yerel_sayac++;
    }

    /* Ust-duzey bildirimleri isle */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur != DÜĞÜM_İŞLEV && d->tur != DÜĞÜM_SINIF) {
            bildirim_uret(u, d, 2);
        }
    }

    yaz(u, "  )  ;; main sonu");
    yaz(u, "");

    /* Fonksiyonlari ekle */
    if (u->yardimcilar.uzunluk > 0) {
        metin_ekle(&u->cikti, u->yardimcilar.veri);
    }

    /* Data section */
    if (u->veri_bolumu.uzunluk > 0) {
        yaz(u, "  ;; Veri bolumu (string literaller)");
        metin_ekle(&u->cikti, u->veri_bolumu.veri);
    }

    /* Modul sonu */
    yaz(u, ")  ;; modul sonu");
}
