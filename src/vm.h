/*
 * Tonyukuk Sanal Makinesi (TrSM) — Bytecode tanımları
 * Yığın tabanlı sanal makine komut seti, değer temsili ve dosya formatı
 */
#ifndef VM_H
#define VM_H

#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════
 *  BYTECODE KOMUT SETİ
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    /* Sabitler (operand: u16 sabit havuzu indeksi) */
    SM_SABIT_TAM,           /* sabitler[idx] → yığın (tam) */
    SM_SABIT_ONDALIK,       /* sabitler[idx] → yığın (ondalık) */
    SM_SABIT_METIN,         /* sabitler[idx] → yığın (metin) */
    SM_DOGRU,               /* doğru → yığın */
    SM_YANLIS,              /* yanlış → yığın */
    SM_BOS,                 /* boş → yığın */

    /* Yığın işlemleri */
    SM_CIKAR,               /* yığından çıkar (pop) */
    SM_KOPYALA,             /* yığın tepesini kopyala (dup) */

    /* Tam sayı aritmetik */
    SM_TOPLA,               /* b,a → a+b */
    SM_CIKAR_SAYI,          /* b,a → a-b */
    SM_CARP,                /* b,a → a*b */
    SM_BOL,                 /* b,a → a/b */
    SM_MOD,                 /* b,a → a%b */
    SM_EKSI,                /* a → -a */

    /* Ondalık aritmetik */
    SM_TOPLA_OND,
    SM_CIKAR_OND,
    SM_CARP_OND,
    SM_BOL_OND,
    SM_EKSI_OND,

    /* Tam sayı karşılaştırma */
    SM_ESIT,                /* b,a → a==b */
    SM_ESIT_DEGIL,          /* b,a → a!=b */
    SM_KUCUK,               /* b,a → a<b */
    SM_BUYUK,               /* b,a → a>b */
    SM_KUCUK_ESIT,          /* b,a → a<=b */
    SM_BUYUK_ESIT,          /* b,a → a>=b */

    /* Ondalık karşılaştırma */
    SM_ESIT_OND,
    SM_KUCUK_OND,
    SM_BUYUK_OND,
    SM_KUCUK_ESIT_OND,
    SM_BUYUK_ESIT_OND,

    /* Metin karşılaştırma */
    SM_ESIT_METIN,

    /* Mantık */
    SM_DEGIL,               /* a → !a */
    SM_VE,                  /* b,a → a&&b */
    SM_VEYA,                /* b,a → a||b */

    /* Değişkenler (operand: u16 indeks) */
    SM_YUKLE_YEREL,         /* yereller[idx] → yığın */
    SM_KAYDET_YEREL,        /* yığın → yereller[idx] */
    SM_YUKLE_GENEL,         /* geneller[idx] → yığın */
    SM_KAYDET_GENEL,        /* yığın → geneller[idx] */

    /* Kontrol akışı (operand: i16 göreceli ofset) */
    SM_ATLA,                /* koşulsuz atlama */
    SM_ATLA_YANLIS,         /* yığın tepesi falsy ise atla */
    SM_ATLA_DOGRU,          /* yığın tepesi truthy ise atla */

    /* Fonksiyonlar */
    SM_CAGRI,               /* operand: u16 fonksiyon_idx, ardından u8 arg_sayısı */
    SM_DONDUR,              /* fonksiyondan dön (void) */
    SM_DONDUR_DEGER,        /* değer ile dön */

    /* Yerleşik fonksiyonlar */
    SM_YAZDIR,              /* yığın tepesini yazdır */
    SM_YAZDIR_SATIR,        /* yığın tepesini yazdır (satır sonu ile) - varsayılan */

    /* Metin */
    SM_METIN_BIRLESTIR,     /* b,a → a+b (metin birleştir) */

    /* Tip dönüşümleri */
    SM_TAM_ONDALIK,         /* tam → ondalık */
    SM_ONDALIK_TAM,         /* ondalık → tam */
    SM_TAM_METIN,           /* tam → metin */
    SM_METIN_TAM,           /* metin → tam */

    /* Program sonu */
    SM_DUR,                 /* programı bitir */
} SmKomut;

/* ═══════════════════════════════════════════════════════════════════
 *  DEĞER TEMSİLİ
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    DEGER_BOS,
    DEGER_TAM,
    DEGER_ONDALIK,
    DEGER_METIN,
    DEGER_MANTIK,
} DegerTuru;

typedef struct {
    DegerTuru tur;
    union {
        int64_t   tam;
        double    ondalik;
        struct {
            char    *ptr;
            int64_t  uzunluk;
        } metin;
        int       mantik;
    } deger;
} SmDeger;

/* ═══════════════════════════════════════════════════════════════════
 *  FONKSİYON TABLOSU
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    char   *isim;
    int     param_sayisi;
    int     yerel_sayisi;       /* parametreler dahil toplam yerel */
    int     kod_baslangic;      /* ana bytecode içindeki ofset */
    int     kod_uzunluk;
} SmFonksiyon;

/* ═══════════════════════════════════════════════════════════════════
 *  BYTECODE DOSYA FORMATI (.trbc)
 * ═══════════════════════════════════════════════════════════════════ */

#define TRBC_SIHIRLI     0x43425254   /* "TRBC" (little-endian) */
#define TRBC_VERSIYON    1

/* Sabit havuzu tip etiketleri */
#define SABIT_TIP_TAM     1
#define SABIT_TIP_ONDALIK 2
#define SABIT_TIP_METIN   3

/* ═══════════════════════════════════════════════════════════════════
 *  ÇAĞRI ÇERÇEVESİ
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    SmFonksiyon *fonksiyon;
    uint8_t     *dondur_adresi;     /* geri dönüş PC */
    int          yigin_tabani;      /* bu çerçevenin yığın tabanı */
    SmDeger     *yereller;          /* yerel değişken dizisi */
    int          yerel_sayisi;
} CagriCercevesi;

/* ═══════════════════════════════════════════════════════════════════
 *  SANAL MAKİNE
 * ═══════════════════════════════════════════════════════════════════ */

#define YIGIN_MAKS    4096
#define CERCEVE_MAKS  256
#define GENEL_MAKS    1024

typedef struct {
    /* Bytecode */
    uint8_t *kod;
    int      kod_uzunluk;
    uint8_t *pc;

    /* Değer yığını */
    SmDeger  yigin[YIGIN_MAKS];
    int      sp;

    /* Çağrı yığını */
    CagriCercevesi cerceveler[CERCEVE_MAKS];
    int      cerceve_sayisi;

    /* Global değişkenler */
    SmDeger  geneller[GENEL_MAKS];

    /* Sabit havuzu */
    SmDeger *sabitler;
    int      sabit_sayisi;

    /* Fonksiyon tablosu */
    SmFonksiyon *fonksiyonlar;
    int      fonksiyon_sayisi;
} TrSM;

/* ═══════════════════════════════════════════════════════════════════
 *  YARDIMCI MAKROLAR
 * ═══════════════════════════════════════════════════════════════════ */

/* Bytecode'dan 2 byte oku (little-endian) */
#define OKU_U16(p) ((uint16_t)((p)[0] | ((p)[1] << 8)))
#define OKU_I16(p) ((int16_t)OKU_U16(p))

/* Bytecode'dan 4 byte oku (little-endian) */
#define OKU_U32(p) ((uint32_t)((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24)))
#define OKU_I32(p) ((int32_t)OKU_U32(p))

/* Bytecode'a 2 byte yaz (little-endian) */
#define YAZ_U16(p, v) do { (p)[0] = (v) & 0xFF; (p)[1] = ((v) >> 8) & 0xFF; } while(0)

/* Bytecode'a 4 byte yaz (little-endian) */
#define YAZ_U32(p, v) do { (p)[0] = (v) & 0xFF; (p)[1] = ((v) >> 8) & 0xFF; \
                           (p)[2] = ((v) >> 16) & 0xFF; (p)[3] = ((v) >> 24) & 0xFF; } while(0)

#endif /* VM_H */
