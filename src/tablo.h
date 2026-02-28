#ifndef TABLO_H
#define TABLO_H

#include "bellek.h"

typedef enum {
    TİP_TAM,
    TİP_ONDALIK,
    TİP_METİN,
    TİP_MANTIK,
    TİP_DİZİ,
    TİP_SINIF,
    TİP_BOŞLUK,
    TİP_İŞLEV,
    TİP_SÖZLÜK,
    TİP_SAYIM,
    TİP_KÜME,
    TİP_SONUÇ,       /* Sonuç<T, H> tipi */
    TİP_SEÇENEK,     /* Seçenek<T> tipi */
    TİP_BİLİNMİYOR,
} TipTürü;

/* Sınıf alan bilgisi */
typedef struct {
    char   *isim;
    TipTürü tip;
    int     offset;   /* byte offset in object (ignored for static) */
    int     statik;   /* statik alan mı? */
    int     erisim;   /* 0=genel, 1=özel, 2=korumalı */
} SınıfAlanı;

/* Sınıf düzen bilgisi */
typedef struct {
    char       *isim;
    SınıfAlanı  alanlar[64];
    int         alan_sayisi;
    int         boyut;         /* total bytes */
    char       *metot_isimleri[64];  /* mangled method names */
    int         metot_sayisi;
    char       *ozellik_getters[64];   /* @özellik metot isimleri */
    int         ozellik_getter_sayisi;
    char       *ozellik_setters[64];   /* @ozellik_ata metot isimleri */
    int         ozellik_setter_sayisi;
} SinifBilgi;

typedef struct {
    char    *isim;
    TipTürü  tip;
    int      yerel_indeks;   /* stack offset */
    int      parametre_mi;
    int      global_mi;
    /* İşlev bilgisi */
    int      param_sayisi;
    TipTürü  param_tipleri[32];  /* max 32 parametre */
    TipTürü  dönüş_tipi;
    int      varsayilan_sayisi;  /* varsayılan değeri olan parametre sayısı */
    void    *varsayilan_dugumler[32]; /* varsayılan değer AST düğümleri (Düğüm*) */
    /* Sınıf bilgisi */
    char       *sınıf_adı;     /* class name for instance variables */
    SinifBilgi *sınıf_bilgi;   /* class layout (for class type symbols) */
    int         baslangic_var; /* başlangıç değeri atanmış mı? */
    int         sabit_mi;     /* sabit (const) değişken mi? */
    /* Sayım bilgisi */
    char   *sayim_degerler[64];  /* enum değer isimleri */
    int     sayim_deger_sayisi;  /* enum değer sayısı */
    /* Arayüz bilgisi */
    char   *arayuz_metotlar[32]; /* arayüz metot isimleri */
    int     arayuz_metot_sayisi;
    /* Modül runtime bilgisi */
    const char *runtime_isim;    /* "_tr_xxx" — NULL ise kullanıcı fonksiyonu */

    /* Sonuç/Seçenek tip bilgisi */
    TipTürü ic_tip;              /* İç tip (Sonuç<tam, _> için tam) */
    TipTürü hata_tip;            /* Hata tipi (Sonuç<_, DosyaHatası> için DosyaHatası) */
    char   *ic_tip_adi;          /* İç tip adı (generic için) */
    char   *hata_tip_adi;        /* Hata tip adı */

    /* Generic tip parametreleri */
    char   *tip_parametreleri[8]; /* Generic tip parametreleri: T, H, vs. */
    int     tip_parametre_sayisi;

    /* Monomorphization bilgisi */
    int     generic_mi;           /* Bu bir generic fonksiyon/sınıf mı? */
    char   *tip_parametre;        /* Generic tip parametresi adı (örn: "T") */
    void   *generic_dugum;        /* Orijinal generic AST düğümü (Düğüm*) */
    char   *somut_tip;            /* Özelleştirilmiş versiyon için somut tip adı */
} Sembol;

#define TABLO_BOYUT 1024

typedef struct Kapsam {
    Sembol         *tablo[TABLO_BOYUT];
    int             sembol_sayisi;
    struct Kapsam  *ust;
    int             yerel_sayac;   /* yerel değişken sayacı */
} Kapsam;

/* Kapsam oluştur/yok et */
Kapsam *kapsam_oluştur(Arena *a, Kapsam *ust);

/* Sembol ekle */
Sembol *sembol_ekle(Arena *a, Kapsam *k, const char *isim, TipTürü tip);

/* Sembol ara (üst kapsamlara da bakar) */
Sembol *sembol_ara(Kapsam *k, const char *isim);

/* Tip adını TipTürü'na çevir */
TipTürü tip_adı_çevir(const char *isim);

/* TipTürü'nu Türkçe ada çevir */
const char *tip_adı(TipTürü tip);

/* Sınıf bilgisini bul (kapsam zincirinde arar) */
SinifBilgi *sınıf_bul(Kapsam *k, const char *isim);

#endif
