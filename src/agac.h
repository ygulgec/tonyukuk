#ifndef AĞAÇ_H
#define AĞAÇ_H

#include <stdint.h>
#include "bellek.h"
#include "sozcuk.h"

typedef enum {
    /* Üst düzey */
    DÜĞÜM_PROGRAM,

    /* Tanımlar */
    DÜĞÜM_DEĞİŞKEN,
    DÜĞÜM_İŞLEV,
    DÜĞÜM_SINIF,
    DÜĞÜM_KULLAN,

    /* İfadeler */
    DÜĞÜM_EĞER,
    DÜĞÜM_DÖNGÜ,
    DÜĞÜM_İKEN,
    DÜĞÜM_DÖNDÜR,
    DÜĞÜM_KIR,
    DÜĞÜM_DEVAM,
    DÜĞÜM_İFADE_BİLDİRİMİ,
    DÜĞÜM_BLOK,

    /* Değerler */
    DÜĞÜM_TAM_SAYI,
    DÜĞÜM_ONDALIK_SAYI,
    DÜĞÜM_METİN_DEĞERİ,
    DÜĞÜM_MANTIK_DEĞERİ,
    DÜĞÜM_TANIMLAYICI,
    DÜĞÜM_İKİLİ_İŞLEM,
    DÜĞÜM_TEKLİ_İŞLEM,
    DÜĞÜM_ÇAĞRI,
    DÜĞÜM_ERİŞİM,
    DÜĞÜM_DİZİ_DEĞERİ,
    DÜĞÜM_DİZİ_ERİŞİM,
    DÜĞÜM_BORU,
    DÜĞÜM_ATAMA,
    DÜĞÜM_DİZİ_ATAMA,    /* a[i] = değer */
    DÜĞÜM_ERİŞİM_ATAMA,  /* nesne.alan = değer */
    DÜĞÜM_DENE_YAKALA,   /* dene...yakala...son */
    DÜĞÜM_FIRLAT,        /* fırlat ifade */
    DÜĞÜM_LAMBDA,        /* anonim fonksiyon */
    DÜĞÜM_BOŞ_DEĞER,     /* bos (null) */
    DÜĞÜM_EŞLE,          /* esle/durum/varsayilan */
    DÜĞÜM_HER_İÇİN,      /* her...icin dongusu */
    DÜĞÜM_SAYIM,         /* sayım tanımı (enum) */
    DÜĞÜM_ARAYÜZ,        /* arayüz tanımı (interface) */
    DÜĞÜM_SÖZLÜK_DEĞERİ, /* sözlük literal {"a": 1, "b": 2} */
    DÜĞÜM_SÖZLÜK_ERİŞİM, /* sözlük erişim s["anahtar"] — reuse DIZI_ERISIM */
    DÜĞÜM_ARALIK,        /* aralık: alt..üst */
    DÜĞÜM_BEKLE,         /* bekle ifade */
    DÜĞÜM_DEMET,         /* çoklu dönüş / demet */
    DÜĞÜM_DİLİM,         /* dizi/metin dilim: a[1:3] */
    DÜĞÜM_ÜÇLÜ,          /* üçlü ifade: koşul ? değer1 : değer2 */
    DÜĞÜM_ÜRET,          /* üreteç üret ifadesi */
    DÜĞÜM_TİP_TANIMI,    /* tip takma adı: tip Sayı = tam */
    DÜĞÜM_TEST,          /* test blok: test "isim" ise ... son */
    DÜĞÜM_KÜME_DEĞERİ,   /* küme literal: küme{1, 2, 3} */
    DÜĞÜM_LİSTE_ÜRETİMİ, /* liste üretimi: [ifade her x için dizi eğer koşul] */
    DÜĞÜM_İLE_İSE,       /* bağlam yöneticisi: ile ifade olarak d ise ... son */
    DÜĞÜM_PAKET_AÇ,      /* yapı bozma: [a, b, c] = dizi */
    DÜĞÜM_WALRUS,        /* atama ifadesi: (n := ifade) */
    DÜĞÜM_SÖZLÜK_ÜRETİMİ, /* sözlük üretimi: {k:v her x için dizi eğer koşul} */

    /* Sonuç/Seçenek tip sistemi */
    DÜĞÜM_SONUÇ_OLUŞTUR, /* Tamam(değer) veya Hata(değer) */
    DÜĞÜM_SEÇENEK_OLUŞTUR, /* Bir(değer) veya Hiç */
    DÜĞÜM_SORU_OP,       /* ? operatörü (hata yayılımı) */
    DÜĞÜM_TİP_PARAMETRELİ, /* Sonuç<T, H> veya Seçenek<T> tip kullanımı */

    /* OOP iyileştirmeleri */
    DÜĞÜM_ÖZELLİK,       /* özellik tanımı (getter/setter) */
    DÜĞÜM_STATİK_ERİŞİM, /* SinifAdi.statik_uye erişimi */
} DüğümTürü;

typedef struct Düğüm {
    DüğümTürü tur;
    int satir;
    int sutun;
    int sonuç_tipi;    /* Semantik analiz tarafından doldurulur (TipTürü) */

    /* Çocuk düğümler */
    struct Düğüm **çocuklar;
    int çocuk_sayısı;
    int çocuk_kapasite;

    /* Veri alanları */
    union {
        int64_t tam_deger;
        double  ondalık_değer;
        int     mantık_değer;
        char   *metin_değer;

        struct {
            char *isim;
            char *tip;
            char *cagri_tip_parametre;  /* Generic çağrı tip parametresi: f<tam>(...) */
        } tanimlayici;

        struct {
            SözcükTürü islem;
        } islem;

        struct {
            char *isim;
            char *dönüş_tipi;
            char *dekorator;      /* @dekorator ismi, NULL ise yok */
            char *tip_parametre;  /* <T> generic tip parametresi, NULL ise yok */
            int   eszamansiz;     /* eşzamansız işlev mi? */
            int   soyut;         /* soyut işlev mi? */
            int   variadic;      /* son parametre variadic mi? (...param) */
            int   erisim;        /* erişim düzeyi: 0=genel, 1=özel, 2=korumalı */
            int   statik;        /* statik metot mu? */
            /* Kapanış (closure) bilgisi */
            char **yakalanan_isimler;  /* yakalanan değişken isimleri */
            int   *yakalanan_indeksler; /* üst kapsamdaki yerel indeksleri */
            int    yakalanan_sayisi;
        } islev;

        struct {
            char *isim;
            char *tip;
            int   genel;     /* global değişken mi? */
            int   sabit;     /* sabit (const) mi? */
            int   erisim;    /* erişim düzeyi: 0=genel, 1=özel, 2=korumalı */
            int   statik;    /* statik alan mı? */
        } değişken;

        struct {
            char *isim;
            char *ebeveyn;   /* kalıtım: ebeveyn sınıf adı, NULL ise yok */
            int   soyut;     /* soyut sınıf mı? */
            char *tip_parametre;  /* <T> generic tip parametresi, NULL ise yok */
            char *arayuzler[8];   /* uygulanan arayüz isimleri */
            int   arayuz_sayisi;  /* uygulanan arayüz sayısı */
        } sinif;

        struct {
            char *modul;
        } kullan;

        struct {
            char *isim;       /* döngü değişkeni */
        } dongu;

        struct {
            char *isim;       /* sayım adı */
        } sayim;

        struct {
            char *isim;       /* test bloğu adı */
        } test;

        /* Sonuç/Seçenek yapıcıları */
        struct {
            int  varyant;     /* 0=Tamam/Bir, 1=Hata/Hiç */
            char *hata_tipi;  /* Hata varyantı için hata tipi (örn: "Bulunamadı") */
        } sonuç_seçenek;

        /* Parametreli tip (Sonuç<T, H>, Seçenek<T>) */
        struct {
            char *temel_tip;        /* "Sonuç" veya "Seçenek" */
            char **tip_parametreleri;  /* Tip parametreleri */
            int   parametre_sayisi;
        } tip_parametreli;

        /* OOP: erişim düzeyi ve statik */
        struct {
            int   erisim;     /* 0=genel, 1=özel, 2=korumalı */
            int   statik;     /* statik mi? */
            char *sınıf_adı;  /* hangi sınıfa ait */
        } oop;

        /* Özellik (property) */
        struct {
            char *isim;       /* özellik adı */
            int   getter_var; /* getter var mı? */
            int   setter_var; /* setter var mı? */
        } özellik;
    } veri;
} Düğüm;

/* Düğüm oluşturma */
Düğüm *düğüm_oluştur(Arena *a, DüğümTürü tur, int satir, int sutun);

/* Çocuk ekleme */
void düğüm_çocuk_ekle(Arena *a, Düğüm *ebeveyn, Düğüm *cocuk);

#endif
