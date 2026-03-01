/*
 * Tonyukuk Sanal Makinesi — Bytecode Emitter
 * AST → .trbc bytecode dosyası
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "vm.h"
#include "agac.h"
#include "tablo.h"
#include "uretici.h"

/* ═══════════════════════════════════════════════════════════════════
 *  BYTECODE ÜRETİCİ YAPISI
 * ═══════════════════════════════════════════════════════════════════ */

#define VM_KOD_KAP    65536
#define VM_SABIT_KAP  4096
#define VM_FONK_KAP   256
#define VM_YEREL_KAP  256

/* Yerel değişken girişi */
typedef struct {
    const char *isim;
    int   indeks;
    int   derinlik;  /* kapsam derinliği */
} VmYerel;

/* Global değişken girişi */
typedef struct {
    const char *isim;
    int   indeks;
} VmGenel;

typedef struct {
    /* Bytecode tamponu */
    uint8_t  *kod;
    int       kod_uzunluk;
    int       kod_kapasite;

    /* Sabit havuzu */
    SmDeger  *sabitler;
    int       sabit_sayisi;
    int       sabit_kapasite;

    /* Fonksiyon tablosu */
    SmFonksiyon *fonksiyonlar;
    int       fonksiyon_sayisi;
    int       fonksiyon_kapasite;

    /* Yerel değişkenler (mevcut fonksiyon/global kapsam) */
    VmYerel   yereller[VM_YEREL_KAP];
    int       yerel_sayisi;
    int       yerel_derinlik;

    /* Global değişkenler */
    VmGenel   geneller[VM_YEREL_KAP];
    int       genel_sayisi;

    /* Fonksiyon derleme durumu */
    int       fonksiyon_icinde;

    /* Döngü break/continue hedefleri */
    int       dongu_kir_yamalari[64];
    int       dongu_kir_sayisi;
    int       dongu_devam_hedefi;

    /* Çıktı dosya adı */
    const char *cikti_dosya;

    Arena    *arena;
} VmUretici;

/* ═══════════════════════════════════════════════════════════════════
 *  YARDIMCI FONKSİYONLAR
 * ═══════════════════════════════════════════════════════════════════ */

static void vm_kod_genislet(VmUretici *v) {
    v->kod_kapasite *= 2;
    v->kod = realloc(v->kod, v->kod_kapasite);
}

static void vm_bayt_yaz(VmUretici *v, uint8_t b) {
    if (v->kod_uzunluk >= v->kod_kapasite) vm_kod_genislet(v);
    v->kod[v->kod_uzunluk++] = b;
}

static void vm_u16_yaz(VmUretici *v, uint16_t val) {
    if (v->kod_uzunluk + 2 > v->kod_kapasite) vm_kod_genislet(v);
    YAZ_U16(v->kod + v->kod_uzunluk, val);
    v->kod_uzunluk += 2;
}

static void vm_i16_yaz(VmUretici *v, int16_t val) {
    vm_u16_yaz(v, (uint16_t)val);
}

static void vm_komut(VmUretici *v, uint8_t op) {
    vm_bayt_yaz(v, op);
}

static void vm_komut_u16(VmUretici *v, uint8_t op, uint16_t arg) {
    vm_bayt_yaz(v, op);
    vm_u16_yaz(v, arg);
}

/* Sabit havuzuna tam sayı ekle */
static int vm_sabit_tam(VmUretici *v, int64_t deger) {
    /* Mevcut sabitleri kontrol et */
    for (int i = 0; i < v->sabit_sayisi; i++) {
        if (v->sabitler[i].tur == DEGER_TAM && v->sabitler[i].deger.tam == deger)
            return i;
    }
    if (v->sabit_sayisi >= v->sabit_kapasite) {
        v->sabit_kapasite *= 2;
        v->sabitler = realloc(v->sabitler, v->sabit_kapasite * sizeof(SmDeger));
    }
    int idx = v->sabit_sayisi++;
    v->sabitler[idx].tur = DEGER_TAM;
    v->sabitler[idx].deger.tam = deger;
    return idx;
}

/* Sabit havuzuna ondalık sayı ekle */
static int vm_sabit_ondalik(VmUretici *v, double deger) {
    for (int i = 0; i < v->sabit_sayisi; i++) {
        if (v->sabitler[i].tur == DEGER_ONDALIK && v->sabitler[i].deger.ondalik == deger)
            return i;
    }
    if (v->sabit_sayisi >= v->sabit_kapasite) {
        v->sabit_kapasite *= 2;
        v->sabitler = realloc(v->sabitler, v->sabit_kapasite * sizeof(SmDeger));
    }
    int idx = v->sabit_sayisi++;
    v->sabitler[idx].tur = DEGER_ONDALIK;
    v->sabitler[idx].deger.ondalik = deger;
    return idx;
}

/* Sabit havuzuna metin ekle */
static int vm_sabit_metin(VmUretici *v, const char *ptr, int64_t uzunluk) {
    for (int i = 0; i < v->sabit_sayisi; i++) {
        if (v->sabitler[i].tur == DEGER_METIN &&
            v->sabitler[i].deger.metin.uzunluk == uzunluk &&
            memcmp(v->sabitler[i].deger.metin.ptr, ptr, uzunluk) == 0)
            return i;
    }
    if (v->sabit_sayisi >= v->sabit_kapasite) {
        v->sabit_kapasite *= 2;
        v->sabitler = realloc(v->sabitler, v->sabit_kapasite * sizeof(SmDeger));
    }
    int idx = v->sabit_sayisi++;
    v->sabitler[idx].tur = DEGER_METIN;
    char *kopya = malloc(uzunluk + 1);
    memcpy(kopya, ptr, uzunluk);
    kopya[uzunluk] = '\0';
    v->sabitler[idx].deger.metin.ptr = kopya;
    v->sabitler[idx].deger.metin.uzunluk = uzunluk;
    return idx;
}

/* Yerel değişken ara */
static int vm_yerel_bul(VmUretici *v, const char *isim) {
    for (int i = v->yerel_sayisi - 1; i >= 0; i--) {
        if (strcmp(v->yereller[i].isim, isim) == 0)
            return v->yereller[i].indeks;
    }
    return -1;
}

/* Yerel değişken ekle */
static int vm_yerel_ekle(VmUretici *v, const char *isim) {
    int idx = v->yerel_sayisi;
    if (idx >= VM_YEREL_KAP) return -1;
    v->yereller[idx].isim = isim;
    v->yereller[idx].indeks = idx;
    v->yereller[idx].derinlik = v->yerel_derinlik;
    v->yerel_sayisi++;
    return idx;
}

/* Global değişken ara */
static int vm_genel_bul(VmUretici *v, const char *isim) {
    for (int i = 0; i < v->genel_sayisi; i++) {
        if (strcmp(v->geneller[i].isim, isim) == 0)
            return v->geneller[i].indeks;
    }
    return -1;
}

/* Global değişken ekle */
static int vm_genel_ekle(VmUretici *v, const char *isim) {
    int idx = v->genel_sayisi;
    v->geneller[idx].isim = isim;
    v->geneller[idx].indeks = idx;
    v->genel_sayisi++;
    return idx;
}

/* Fonksiyon ara */
static int vm_fonksiyon_bul(VmUretici *v, const char *isim) {
    for (int i = 0; i < v->fonksiyon_sayisi; i++) {
        if (v->fonksiyonlar[i].isim && strcmp(v->fonksiyonlar[i].isim, isim) == 0)
            return i;
    }
    return -1;
}

/* Atlama hedefini düzelt (patch) */
static void vm_atla_yamala(VmUretici *v, int ofset, int16_t hedef) {
    YAZ_U16(v->kod + ofset, (uint16_t)hedef);
}

/* ═══════════════════════════════════════════════════════════════════
 *  İLERİ BİLDİRİMLER
 * ═══════════════════════════════════════════════════════════════════ */

static void vm_ifade_uret(VmUretici *v, Düğüm *d);
static void vm_bildirim_uret(VmUretici *v, Düğüm *d);
static void vm_blok_uret(VmUretici *v, Düğüm *blok);

/* ═══════════════════════════════════════════════════════════════════
 *  ESCAPE SEQUENCE İŞLEME
 * ═══════════════════════════════════════════════════════════════════ */

/* Escape sequence'ları işle (\n, \t, \\, \") */
static int vm_escape_isle(const char *kaynak, int kaynak_uzunluk, char *hedef) {
    int j = 0;
    for (int i = 0; i < kaynak_uzunluk; i++) {
        if (kaynak[i] == '\\' && i + 1 < kaynak_uzunluk) {
            switch (kaynak[i + 1]) {
                case 'n':  hedef[j++] = '\n'; i++; break;
                case 't':  hedef[j++] = '\t'; i++; break;
                case '\\': hedef[j++] = '\\'; i++; break;
                case '"':  hedef[j++] = '"';  i++; break;
                default:   hedef[j++] = kaynak[i]; break;
            }
        } else {
            hedef[j++] = kaynak[i];
        }
    }
    return j;
}

/* ═══════════════════════════════════════════════════════════════════
 *  İFADE ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════ */

static void vm_ifade_uret(VmUretici *v, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
    case DÜĞÜM_TAM_SAYI: {
        int idx = vm_sabit_tam(v, d->veri.tam_deger);
        vm_komut_u16(v, SM_SABIT_TAM, (uint16_t)idx);
        break;
    }

    case DÜĞÜM_ONDALIK_SAYI: {
        int idx = vm_sabit_ondalik(v, d->veri.ondalık_değer);
        vm_komut_u16(v, SM_SABIT_ONDALIK, (uint16_t)idx);
        break;
    }

    case DÜĞÜM_METİN_DEĞERİ: {
        const char *metin = d->veri.metin_değer;
        int uzunluk = metin ? (int)strlen(metin) : 0;

        /* Escape sequence işleme */
        char *islenmis = malloc(uzunluk + 1);
        int gercek_uzunluk = vm_escape_isle(metin, uzunluk, islenmis);

        /* Metin interpolasyonu kontrolü (${...}) */
        int interpolasyon_var = 0;
        for (int i = 0; i < gercek_uzunluk - 1; i++) {
            if (islenmis[i] == '$' && islenmis[i + 1] == '{') {
                interpolasyon_var = 1;
                break;
            }
        }

        if (interpolasyon_var) {
            /* Metin interpolasyonu: parçalara ayır ve birleştir */
            int parca_sayisi = 0;
            int bas = 0;

            for (int i = 0; i < gercek_uzunluk; i++) {
                if (islenmis[i] == '$' && i + 1 < gercek_uzunluk && islenmis[i + 1] == '{') {
                    /* Önceki düz metin parçası */
                    if (i > bas) {
                        int idx = vm_sabit_metin(v, islenmis + bas, i - bas);
                        vm_komut_u16(v, SM_SABIT_METIN, (uint16_t)idx);
                        if (parca_sayisi > 0) vm_komut(v, SM_METIN_BIRLESTIR);
                        parca_sayisi++;
                    }
                    /* ${...} içindeki değişken ismini bul */
                    int degisken_bas = i + 2;
                    int degisken_son = degisken_bas;
                    while (degisken_son < gercek_uzunluk && islenmis[degisken_son] != '}')
                        degisken_son++;

                    char degisken_isim[256];
                    int isim_uzunluk = degisken_son - degisken_bas;
                    if (isim_uzunluk > 255) isim_uzunluk = 255;
                    memcpy(degisken_isim, islenmis + degisken_bas, isim_uzunluk);
                    degisken_isim[isim_uzunluk] = '\0';

                    /* Değişkeni yükle */
                    int yerel = vm_yerel_bul(v, degisken_isim);
                    if (yerel >= 0) {
                        vm_komut_u16(v, SM_YUKLE_YEREL, (uint16_t)yerel);
                    } else {
                        int genel = vm_genel_bul(v, degisken_isim);
                        if (genel >= 0)
                            vm_komut_u16(v, SM_YUKLE_GENEL, (uint16_t)genel);
                        else {
                            /* Bulunamadı: boş metin */
                            int idx2 = vm_sabit_metin(v, "", 0);
                            vm_komut_u16(v, SM_SABIT_METIN, (uint16_t)idx2);
                        }
                    }
                    /* Metin'e dönüştür */
                    vm_komut(v, SM_TAM_METIN);
                    if (parca_sayisi > 0) vm_komut(v, SM_METIN_BIRLESTIR);
                    parca_sayisi++;

                    i = degisken_son; /* } atla */
                    bas = degisken_son + 1;
                }
            }
            /* Son düz metin parçası */
            if (bas < gercek_uzunluk) {
                int idx = vm_sabit_metin(v, islenmis + bas, gercek_uzunluk - bas);
                vm_komut_u16(v, SM_SABIT_METIN, (uint16_t)idx);
                if (parca_sayisi > 0) vm_komut(v, SM_METIN_BIRLESTIR);
            } else if (parca_sayisi == 0) {
                int idx = vm_sabit_metin(v, "", 0);
                vm_komut_u16(v, SM_SABIT_METIN, (uint16_t)idx);
            }
        } else {
            int idx = vm_sabit_metin(v, islenmis, gercek_uzunluk);
            vm_komut_u16(v, SM_SABIT_METIN, (uint16_t)idx);
        }
        free(islenmis);
        break;
    }

    case DÜĞÜM_MANTIK_DEĞERİ:
        vm_komut(v, d->veri.mantık_değer ? SM_DOGRU : SM_YANLIS);
        break;

    case DÜĞÜM_BOŞ_DEĞER:
        vm_komut(v, SM_BOS);
        break;

    case DÜĞÜM_TANIMLAYICI: {
        const char *isim = d->veri.tanimlayici.isim;
        if (!isim) break;

        int yerel = vm_yerel_bul(v, isim);
        if (yerel >= 0) {
            vm_komut_u16(v, SM_YUKLE_YEREL, (uint16_t)yerel);
        } else {
            int genel = vm_genel_bul(v, isim);
            if (genel >= 0) {
                vm_komut_u16(v, SM_YUKLE_GENEL, (uint16_t)genel);
            } else {
                /* Bulunamadı — 0 yükle */
                int idx = vm_sabit_tam(v, 0);
                vm_komut_u16(v, SM_SABIT_TAM, (uint16_t)idx);
            }
        }
        break;
    }

    case DÜĞÜM_İKİLİ_İŞLEM: {
        if (d->çocuk_sayısı < 2) break;

        /* Kısa devre mantık: VE / VEYA */
        SözcükTürü op = d->veri.islem.islem;
        if (op == TOK_VE) {
            vm_ifade_uret(v, d->çocuklar[0]);
            vm_komut(v, SM_KOPYALA);
            vm_komut(v, SM_ATLA_YANLIS);
            int yamala = v->kod_uzunluk;
            vm_i16_yaz(v, 0); /* sonra düzelt */
            vm_komut(v, SM_CIKAR); /* ilk değeri at */
            vm_ifade_uret(v, d->çocuklar[1]);
            int16_t ofset = (int16_t)(v->kod_uzunluk - yamala - 2);
            vm_atla_yamala(v, yamala, ofset);
            break;
        }
        if (op == TOK_VEYA) {
            vm_ifade_uret(v, d->çocuklar[0]);
            vm_komut(v, SM_KOPYALA);
            vm_komut(v, SM_ATLA_DOGRU);
            int yamala = v->kod_uzunluk;
            vm_i16_yaz(v, 0);
            vm_komut(v, SM_CIKAR);
            vm_ifade_uret(v, d->çocuklar[1]);
            int16_t ofset = (int16_t)(v->kod_uzunluk - yamala - 2);
            vm_atla_yamala(v, yamala, ofset);
            break;
        }

        /* Normal ikili işlem */
        vm_ifade_uret(v, d->çocuklar[0]);
        vm_ifade_uret(v, d->çocuklar[1]);

        int ondalik = (d->çocuklar[0]->sonuç_tipi == TİP_ONDALIK ||
                       d->çocuklar[1]->sonuç_tipi == TİP_ONDALIK);
        int metin = (d->çocuklar[0]->sonuç_tipi == TİP_METİN ||
                     d->çocuklar[1]->sonuç_tipi == TİP_METİN);

        switch (op) {
        case TOK_ARTI:
            if (metin) vm_komut(v, SM_METIN_BIRLESTIR);
            else if (ondalik) vm_komut(v, SM_TOPLA_OND);
            else vm_komut(v, SM_TOPLA);
            break;
        case TOK_EKSI:
            if (ondalik) vm_komut(v, SM_CIKAR_OND);
            else vm_komut(v, SM_CIKAR_SAYI);
            break;
        case TOK_ÇARPIM:
            if (ondalik) vm_komut(v, SM_CARP_OND);
            else vm_komut(v, SM_CARP);
            break;
        case TOK_BÖLME:
            if (ondalik) vm_komut(v, SM_BOL_OND);
            else vm_komut(v, SM_BOL);
            break;
        case TOK_YÜZDE:
            vm_komut(v, SM_MOD);
            break;
        case TOK_EŞİT_EŞİT:
            if (metin) vm_komut(v, SM_ESIT_METIN);
            else if (ondalik) vm_komut(v, SM_ESIT_OND);
            else vm_komut(v, SM_ESIT);
            break;
        case TOK_EŞİT_DEĞİL:
            if (metin) { vm_komut(v, SM_ESIT_METIN); vm_komut(v, SM_DEGIL); }
            else vm_komut(v, SM_ESIT_DEGIL);
            break;
        case TOK_KÜÇÜK:
            if (ondalik) vm_komut(v, SM_KUCUK_OND);
            else vm_komut(v, SM_KUCUK);
            break;
        case TOK_BÜYÜK:
            if (ondalik) vm_komut(v, SM_BUYUK_OND);
            else vm_komut(v, SM_BUYUK);
            break;
        case TOK_KÜÇÜK_EŞİT:
            if (ondalik) vm_komut(v, SM_KUCUK_ESIT_OND);
            else vm_komut(v, SM_KUCUK_ESIT);
            break;
        case TOK_BÜYÜK_EŞİT:
            if (ondalik) vm_komut(v, SM_BUYUK_ESIT_OND);
            else vm_komut(v, SM_BUYUK_ESIT);
            break;
        default:
            break;
        }
        break;
    }

    case DÜĞÜM_TEKLİ_İŞLEM: {
        if (d->çocuk_sayısı < 1) break;
        vm_ifade_uret(v, d->çocuklar[0]);
        SözcükTürü op = d->veri.islem.islem;
        if (op == TOK_EKSI) {
            int ondalik = (d->çocuklar[0]->sonuç_tipi == TİP_ONDALIK);
            vm_komut(v, ondalik ? SM_EKSI_OND : SM_EKSI);
        } else if (op == TOK_DEĞİL) {
            vm_komut(v, SM_DEGIL);
        }
        break;
    }

    case DÜĞÜM_ÇAĞRI: {
        /* Fonksiyon adı node'un veri alanında, çocuklar sadece argümanlar */
        const char *fn_isim = d->veri.tanimlayici.isim;
        if (!fn_isim) {
            /* Bilinmeyen çağrı: 0 döndür */
            int idx = vm_sabit_tam(v, 0);
            vm_komut_u16(v, SM_SABIT_TAM, (uint16_t)idx);
            break;
        }

        /* yazdır() yerleşik fonksiyon */
        if (strcmp(fn_isim, "yazd\xc4\xb1r") == 0 ||
            strcmp(fn_isim, "yazdır") == 0 ||
            strcmp(fn_isim, "yazdir") == 0) {
            if (d->çocuk_sayısı > 0) {
                vm_ifade_uret(v, d->çocuklar[0]);
                vm_komut(v, SM_YAZDIR_SATIR);
            }
            break;
        }

        /* metin_tam() dönüşüm */
        if (strcmp(fn_isim, "metin_tam") == 0) {
            if (d->çocuk_sayısı > 0) vm_ifade_uret(v, d->çocuklar[0]);
            vm_komut(v, SM_METIN_TAM);
            break;
        }
        if (strcmp(fn_isim, "tam_metin") == 0) {
            if (d->çocuk_sayısı > 0) vm_ifade_uret(v, d->çocuklar[0]);
            vm_komut(v, SM_TAM_METIN);
            break;
        }

        /* Kullanıcı fonksiyonu çağrısı */
        int fn_idx = vm_fonksiyon_bul(v, fn_isim);
        if (fn_idx >= 0) {
            /* Argümanları yığına at — tüm çocuklar argüman */
            int arg_sayisi = d->çocuk_sayısı;
            for (int i = 0; i < d->çocuk_sayısı; i++) {
                vm_ifade_uret(v, d->çocuklar[i]);
            }
            vm_komut_u16(v, SM_CAGRI, (uint16_t)fn_idx);
            vm_bayt_yaz(v, (uint8_t)arg_sayisi);
            break;
        }

        /* Bilinmeyen fonksiyon: 0 döndür */
        int idx = vm_sabit_tam(v, 0);
        vm_komut_u16(v, SM_SABIT_TAM, (uint16_t)idx);
        break;
    }

    default:
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  BİLDİRİM ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════ */

static void vm_bildirim_uret(VmUretici *v, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
    case DÜĞÜM_DEĞİŞKEN: {
        const char *isim = d->veri.değişken.isim;
        if (!isim) break;

        /* İlk değer varsa üret */
        if (d->çocuk_sayısı > 0) {
            vm_ifade_uret(v, d->çocuklar[0]);
        } else {
            /* Varsayılan: 0 */
            int idx = vm_sabit_tam(v, 0);
            vm_komut_u16(v, SM_SABIT_TAM, (uint16_t)idx);
        }

        if (v->fonksiyon_icinde) {
            int yerel = vm_yerel_ekle(v, isim);
            if (yerel >= 0)
                vm_komut_u16(v, SM_KAYDET_YEREL, (uint16_t)yerel);
        } else {
            int genel = vm_genel_ekle(v, isim);
            vm_komut_u16(v, SM_KAYDET_GENEL, (uint16_t)genel);
        }
        break;
    }

    case DÜĞÜM_ATAMA: {
        /* Hedef isim node'un veri alanında, çocuklar[0] = değer ifadesi */
        const char *isim = d->veri.tanimlayici.isim;
        if (!isim || d->çocuk_sayısı < 1) break;

        vm_ifade_uret(v, d->çocuklar[0]);

        int yerel = vm_yerel_bul(v, isim);
        if (yerel >= 0) {
            vm_komut_u16(v, SM_KAYDET_YEREL, (uint16_t)yerel);
        } else {
            int genel = vm_genel_bul(v, isim);
            if (genel >= 0) {
                vm_komut_u16(v, SM_KAYDET_GENEL, (uint16_t)genel);
            }
        }
        break;
    }

    case DÜĞÜM_İFADE_BİLDİRİMİ: {
        if (d->çocuk_sayısı > 0) {
            Düğüm *ifade = d->çocuklar[0];
            /* Atama bildirimi olarak ele al */
            if (ifade->tur == DÜĞÜM_ATAMA) {
                vm_bildirim_uret(v, ifade);
                break;
            }
            vm_ifade_uret(v, ifade);
            /* İfade sonucunu at (statement context) */
            /* Ama yazdır gibi yerleşikler zaten yığından çıkarıyor */
            if (ifade->tur == DÜĞÜM_ÇAĞRI) {
                /* Fonksiyon adı node'un veri alanında */
                const char *fn = ifade->veri.tanimlayici.isim;
                if (fn &&
                    strcmp(fn, "yazd\xc4\xb1r") != 0 &&
                    strcmp(fn, "yazdır") != 0 &&
                    strcmp(fn, "yazdir") != 0) {
                    /* yazdır dışındaki çağrılar yığında değer bırakabilir */
                    vm_komut(v, SM_CIKAR);
                }
            }
        }
        break;
    }

    case DÜĞÜM_EĞER: {
        /* çocuklar[0]=koşul, çocuklar[1]=doğru blok, çocuklar[2]=yanlış blok (opsiyonel) */
        if (d->çocuk_sayısı < 2) break;

        vm_ifade_uret(v, d->çocuklar[0]);
        vm_komut(v, SM_ATLA_YANLIS);
        int yanlis_yamala = v->kod_uzunluk;
        vm_i16_yaz(v, 0);

        /* Doğru bloğu */
        vm_blok_uret(v, d->çocuklar[1]);

        if (d->çocuk_sayısı > 2) {
            /* yoksa bloğu var */
            vm_komut(v, SM_ATLA);
            int son_yamala = v->kod_uzunluk;
            vm_i16_yaz(v, 0);

            int16_t yanlis_ofset = (int16_t)(v->kod_uzunluk - yanlis_yamala - 2);
            vm_atla_yamala(v, yanlis_yamala, yanlis_ofset);

            vm_blok_uret(v, d->çocuklar[2]);

            int16_t son_ofset = (int16_t)(v->kod_uzunluk - son_yamala - 2);
            vm_atla_yamala(v, son_yamala, son_ofset);
        } else {
            int16_t yanlis_ofset = (int16_t)(v->kod_uzunluk - yanlis_yamala - 2);
            vm_atla_yamala(v, yanlis_yamala, yanlis_ofset);
        }
        break;
    }

    case DÜĞÜM_İKEN: {
        /* çocuklar[0]=koşul, çocuklar[1]=gövde */
        if (d->çocuk_sayısı < 2) break;

        /* Döngü break/continue kaydet */
        int onceki_kir_sayisi = v->dongu_kir_sayisi;
        int onceki_devam = v->dongu_devam_hedefi;
        v->dongu_kir_sayisi = 0;

        int dongu_bas = v->kod_uzunluk;
        v->dongu_devam_hedefi = dongu_bas;

        vm_ifade_uret(v, d->çocuklar[0]);
        vm_komut(v, SM_ATLA_YANLIS);
        int cikis_yamala = v->kod_uzunluk;
        vm_i16_yaz(v, 0);

        vm_blok_uret(v, d->çocuklar[1]);

        /* Başa dön */
        vm_komut(v, SM_ATLA);
        int16_t geri_ofset = (int16_t)(dongu_bas - v->kod_uzunluk - 2);
        vm_i16_yaz(v, geri_ofset);

        /* Çıkış hedefini düzelt */
        int16_t cikis_ofset = (int16_t)(v->kod_uzunluk - cikis_yamala - 2);
        vm_atla_yamala(v, cikis_yamala, cikis_ofset);

        /* Break yamalarını düzelt */
        for (int i = 0; i < v->dongu_kir_sayisi; i++) {
            int16_t kir_ofset = (int16_t)(v->kod_uzunluk - v->dongu_kir_yamalari[i] - 2);
            vm_atla_yamala(v, v->dongu_kir_yamalari[i], kir_ofset);
        }

        v->dongu_kir_sayisi = onceki_kir_sayisi;
        v->dongu_devam_hedefi = onceki_devam;
        break;
    }

    case DÜĞÜM_KIR: {
        vm_komut(v, SM_ATLA);
        if (v->dongu_kir_sayisi < 64) {
            v->dongu_kir_yamalari[v->dongu_kir_sayisi++] = v->kod_uzunluk;
        }
        vm_i16_yaz(v, 0); /* sonra düzelt */
        break;
    }

    case DÜĞÜM_DEVAM: {
        vm_komut(v, SM_ATLA);
        int16_t ofset = (int16_t)(v->dongu_devam_hedefi - v->kod_uzunluk - 2);
        vm_i16_yaz(v, ofset);
        break;
    }

    case DÜĞÜM_DÖNDÜR: {
        if (d->çocuk_sayısı > 0) {
            vm_ifade_uret(v, d->çocuklar[0]);
            vm_komut(v, SM_DONDUR_DEGER);
        } else {
            vm_komut(v, SM_DONDUR);
        }
        break;
    }

    case DÜĞÜM_İŞLEV: {
        /* Fonksiyon tanımı — bytecode'u ayrı derle */
        const char *isim = d->veri.islev.isim;
        if (!isim) break;

        /* İlk geçişte zaten kaydedildi, mevcut girişi bul */
        int fn_idx = vm_fonksiyon_bul(v, isim);
        if (fn_idx < 0) {
            /* İlk geçişte kayıt olmamış: yeni ekle */
            if (v->fonksiyon_sayisi >= v->fonksiyon_kapasite) {
                v->fonksiyon_kapasite *= 2;
                v->fonksiyonlar = realloc(v->fonksiyonlar,
                    v->fonksiyon_kapasite * sizeof(SmFonksiyon));
            }
            fn_idx = v->fonksiyon_sayisi++;
            v->fonksiyonlar[fn_idx].isim = (char *)isim;
        }

        /* Parametreler ilk çocukta DÜĞÜM_BLOK içinde, gövde son çocukta */
        int param_sayisi = 0;
        Düğüm *params_blok = NULL;
        if (d->çocuk_sayısı >= 2 && d->çocuklar[0]->tur == DÜĞÜM_BLOK) {
            params_blok = d->çocuklar[0];
            param_sayisi = params_blok->çocuk_sayısı;
        }
        v->fonksiyonlar[fn_idx].param_sayisi = param_sayisi;

        /* Ana bytecode akışına atlama: fonksiyon gövdesi inline derlenecek */
        vm_komut(v, SM_ATLA);
        int atla_yamala = v->kod_uzunluk;
        vm_i16_yaz(v, 0);

        /* Fonksiyon gövdesi */
        v->fonksiyonlar[fn_idx].kod_baslangic = v->kod_uzunluk;

        /* Yerel durum kaydet */
        int onceki_yerel_sayisi = v->yerel_sayisi;
        int onceki_fonksiyon_icinde = v->fonksiyon_icinde;
        v->yerel_sayisi = 0;
        v->fonksiyon_icinde = 1;

        /* Parametreleri yerel olarak kaydet */
        for (int i = 0; i < param_sayisi; i++) {
            vm_yerel_ekle(v, params_blok->çocuklar[i]->veri.değişken.isim);
        }

        /* Gövde bloğu — son çocuk */
        Düğüm *govde = d->çocuklar[d->çocuk_sayısı - 1];
        if (govde && govde->tur == DÜĞÜM_BLOK) {
            vm_blok_uret(v, govde);
        }

        /* Örtük dönüş (fonksiyon sonunda) */
        vm_komut(v, SM_DONDUR);

        v->fonksiyonlar[fn_idx].kod_uzunluk =
            v->kod_uzunluk - v->fonksiyonlar[fn_idx].kod_baslangic;
        v->fonksiyonlar[fn_idx].yerel_sayisi = v->yerel_sayisi;

        /* Yerel durum geri yükle */
        v->yerel_sayisi = onceki_yerel_sayisi;
        v->fonksiyon_icinde = onceki_fonksiyon_icinde;

        /* Atlama hedefini düzelt */
        int16_t atla_ofset = (int16_t)(v->kod_uzunluk - atla_yamala - 2);
        vm_atla_yamala(v, atla_yamala, atla_ofset);
        break;
    }

    case DÜĞÜM_BLOK:
        vm_blok_uret(v, d);
        break;

    default:
        /* Bilinmeyen düğüm: ifade olarak dene */
        if (d->çocuk_sayısı > 0) {
            vm_ifade_uret(v, d);
        }
        break;
    }
}

/* Blok üretimi */
static void vm_blok_uret(VmUretici *v, Düğüm *blok) {
    if (!blok) return;
    for (int i = 0; i < blok->çocuk_sayısı; i++) {
        vm_bildirim_uret(v, blok->çocuklar[i]);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  BYTECODE DOSYA YAZMA
 * ═══════════════════════════════════════════════════════════════════ */

static int vm_dosya_yaz(VmUretici *v, const char *dosya_adi) {
    FILE *f = fopen(dosya_adi, "wb");
    if (!f) {
        fprintf(stderr, "Hata: '%s' dosyası açılamadı\n", dosya_adi);
        return -1;
    }

    /* Başlık */
    uint32_t sihirli = TRBC_SIHIRLI;
    fwrite(&sihirli, 4, 1, f);
    uint16_t versiyon = TRBC_VERSIYON;
    fwrite(&versiyon, 2, 1, f);
    uint16_t bayraklar = 0;
    fwrite(&bayraklar, 2, 1, f);

    /* Sabit havuzu */
    uint32_t sabit_sayisi = (uint32_t)v->sabit_sayisi;
    fwrite(&sabit_sayisi, 4, 1, f);
    for (int i = 0; i < v->sabit_sayisi; i++) {
        SmDeger *s = &v->sabitler[i];
        switch (s->tur) {
        case DEGER_TAM: {
            uint8_t tip = SABIT_TIP_TAM;
            fwrite(&tip, 1, 1, f);
            fwrite(&s->deger.tam, 8, 1, f);
            break;
        }
        case DEGER_ONDALIK: {
            uint8_t tip = SABIT_TIP_ONDALIK;
            fwrite(&tip, 1, 1, f);
            fwrite(&s->deger.ondalik, 8, 1, f);
            break;
        }
        case DEGER_METIN: {
            uint8_t tip = SABIT_TIP_METIN;
            fwrite(&tip, 1, 1, f);
            uint32_t uzunluk = (uint32_t)s->deger.metin.uzunluk;
            fwrite(&uzunluk, 4, 1, f);
            fwrite(s->deger.metin.ptr, 1, uzunluk, f);
            break;
        }
        default:
            break;
        }
    }

    /* Fonksiyon tablosu */
    uint32_t fonk_sayisi = (uint32_t)v->fonksiyon_sayisi;
    fwrite(&fonk_sayisi, 4, 1, f);
    for (int i = 0; i < v->fonksiyon_sayisi; i++) {
        SmFonksiyon *fn = &v->fonksiyonlar[i];
        /* İsim uzunluğu + isim */
        uint16_t isim_uz = fn->isim ? (uint16_t)strlen(fn->isim) : 0;
        fwrite(&isim_uz, 2, 1, f);
        if (isim_uz > 0) fwrite(fn->isim, 1, isim_uz, f);
        /* Parametreler ve yerel sayısı */
        uint8_t param = (uint8_t)fn->param_sayisi;
        fwrite(&param, 1, 1, f);
        uint16_t yerel = (uint16_t)fn->yerel_sayisi;
        fwrite(&yerel, 2, 1, f);
        /* Bytecode konum ve uzunluk */
        uint32_t bas = (uint32_t)fn->kod_baslangic;
        fwrite(&bas, 4, 1, f);
        uint32_t uz = (uint32_t)fn->kod_uzunluk;
        fwrite(&uz, 4, 1, f);
    }

    /* Bytecode */
    uint32_t kod_uz = (uint32_t)v->kod_uzunluk;
    fwrite(&kod_uz, 4, 1, f);
    fwrite(v->kod, 1, v->kod_uzunluk, f);

    fclose(f);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 *  ANA GİRİŞ NOKTASI
 * ═══════════════════════════════════════════════════════════════════ */

void kod_uret_vm(Üretici *u, Düğüm *program, Arena *arena) {
    (void)u;

    VmUretici v;
    memset(&v, 0, sizeof(v));
    v.arena = arena;

    /* Tamponları ayır */
    v.kod_kapasite = VM_KOD_KAP;
    v.kod = malloc(v.kod_kapasite);
    v.sabit_kapasite = VM_SABIT_KAP;
    v.sabitler = malloc(v.sabit_kapasite * sizeof(SmDeger));
    v.fonksiyon_kapasite = VM_FONK_KAP;
    v.fonksiyonlar = malloc(v.fonksiyon_kapasite * sizeof(SmFonksiyon));

    /* İlk geçiş: fonksiyon tanımlarını topla (ileriye referans için) */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur == DÜĞÜM_İŞLEV && d->veri.islev.isim) {
            /* Fonksiyonu tablosuna önceden kaydet */
            if (v.fonksiyon_sayisi < v.fonksiyon_kapasite) {
                int idx = v.fonksiyon_sayisi++;
                v.fonksiyonlar[idx].isim = (char *)d->veri.islev.isim;
                v.fonksiyonlar[idx].param_sayisi = 0;
                v.fonksiyonlar[idx].yerel_sayisi = 0;
                v.fonksiyonlar[idx].kod_baslangic = 0;
                v.fonksiyonlar[idx].kod_uzunluk = 0;
            }
        }
    }

    /* Ana geçiş: bytecode üret */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        vm_bildirim_uret(&v, program->çocuklar[i]);
    }

    /* Program sonu */
    vm_komut(&v, SM_DUR);

    /* Çıktı dosya adı — Üretici'nin cikti alanından al */
    const char *cikti = u->cikti.veri;
    if (!cikti || strlen(cikti) == 0) cikti = "a.trbc";

    vm_dosya_yaz(&v, cikti);

    /* Temizlik */
    free(v.kod);
    for (int i = 0; i < v.sabit_sayisi; i++) {
        if (v.sabitler[i].tur == DEGER_METIN && v.sabitler[i].deger.metin.ptr)
            free(v.sabitler[i].deger.metin.ptr);
    }
    free(v.sabitler);
    free(v.fonksiyonlar);
}
