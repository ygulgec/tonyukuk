#ifndef SÖZCÜK_H
#define SÖZCÜK_H

#include <stdint.h>

typedef enum {
    /* Değer sabitleri */
    TOK_TAM_SAYI,
    TOK_ONDALIK_SAYI,
    TOK_METİN_DEĞERİ,
    TOK_DOĞRU,
    TOK_YANLIŞ,
    TOK_BOŞ,

    /* Tip anahtar kelimeleri */
    TOK_TAM,
    TOK_ONDALIK,
    TOK_METİN,
    TOK_MANTIK,
    TOK_DİZİ,

    /* Kontrol akışı */
    TOK_EĞER,
    TOK_YOKSA,
    TOK_İSE,
    TOK_SON,
    TOK_DÖNGÜ,
    TOK_İKEN,
    TOK_DÖNDÜR,
    TOK_KIR,
    TOK_DEVAM,
    TOK_EŞLE,
    TOK_DURUM,
    TOK_VARSAYILAN,
    TOK_HER,
    TOK_İÇİN,

    /* Tanımlar */
    TOK_İŞLEV,
    TOK_SINIF,
    TOK_KULLAN,
    TOK_BU,
    TOK_GENEL,
    TOK_SABIT,
    TOK_SAYIM,        /* sayım (enum) */
    TOK_ARAYÜZ,       /* arayüz (interface) */
    TOK_UYGULA,       /* uygula (implements) */

    /* Hata yakalama */
    TOK_DENE,
    TOK_YAKALA,
    TOK_FIRLAT,
    TOK_SONUNDA,
    TOK_WALRUS,       /* := (atama ifadesi) */

    /* Mantık operatörleri */
    TOK_VE,
    TOK_VEYA,
    TOK_DEĞİL,

    /* Yerleşik fonksiyonlar */
    TOK_YAZDIR,

    /* Semboller */
    TOK_EŞİTTİR,
    TOK_EŞİT_EŞİT,
    TOK_EŞİT_DEĞİL,
    TOK_KÜÇÜK,
    TOK_BÜYÜK,
    TOK_KÜÇÜK_EŞİT,
    TOK_BÜYÜK_EŞİT,
    TOK_ARTI,
    TOK_EKSI,
    TOK_ÇARPIM,
    TOK_BÖLME,
    TOK_YÜZDE,
    TOK_ARTI_EŞİT,
    TOK_EKSİ_EŞİT,
    TOK_ÇARPIM_EŞİT,
    TOK_BÖLME_EŞİT,
    TOK_YÜZDE_EŞİT,
    TOK_BORU,
    TOK_OK,
    TOK_NOKTA,
    TOK_VİRGÜL,
    TOK_İKİ_NOKTA,
    TOK_PAREN_AC,
    TOK_PAREN_KAPA,
    TOK_KÖŞELİ_AÇ,
    TOK_KÖŞELİ_KAPA,
    TOK_SÜSLÜ_AÇ,     /* { */
    TOK_SÜSLÜ_KAPA,   /* } */
    TOK_SORU,         /* ? */
    TOK_SORU_SORU,    /* ?? */
    TOK_AT,           /* @ (dekoratör) */
    TOK_ARALIK,       /* .. (aralık) */
    TOK_ÜÇ_NOKTA,     /* ... (variadic/rest) */

    /* Bağlam yöneticisi */
    TOK_ILE,          /* ile (with) */
    TOK_OLARAK,       /* olarak (as) */

    /* Küme */
    TOK_KÜME,         /* küme (set) */

    /* Async/Await */
    TOK_EŞZAMANSIZ,   /* eşzamansız */
    TOK_BEKLE,        /* bekle */

    /* Tip çıkarımı */
    TOK_DEĞİŞKEN,     /* değişken */

    /* Bit işlemleri */
    TOK_BİT_VE,       /* & */
    TOK_BİT_VEYA,     /* | */
    TOK_BİT_XOR,      /* ^ */
    TOK_SOL_KAYDIR,   /* << */
    TOK_SAĞ_KAYDIR,   /* >> */
    TOK_BİT_DEĞİL,    /* ~ */

    /* Üreteç */
    TOK_ÜRETEÇ,       /* üreteç */
    TOK_ÜRET,         /* üret */

    /* Tip takma adı */
    TOK_TİP_TANIMI,   /* tip (tanım bağlamında) */

    /* Soyut sınıf */
    TOK_SOYUT,        /* soyut */

    /* Test çerçevesi */
    TOK_TEST,         /* test */
    TOK_DOĞRULA,      /* doğrula */

    /* Sonuç/Seçenek tip sistemi */
    TOK_TAMAM,        /* Tamam (Result::Ok) */
    TOK_HATA_SONUÇ,   /* Hata (Result::Err) - HATA adı çakışmasın diye _SONUC eki */
    TOK_BİR,          /* Bir (Option::Some) */
    TOK_HİÇ,          /* Hiç (Option::None) */
    TOK_SONUÇ,        /* Sonuç tip ismi */
    TOK_SEÇENEK,      /* Seçenek tip ismi */

    /* OOP erişim belirleyicileri */
    TOK_ÖZEL,         /* özel (private) */
    TOK_KORUMALI,     /* korumalı (protected) */
    TOK_STATİK,       /* statik (static) */
    TOK_ÖZELLİK,      /* özellik (property) */
    TOK_AL,           /* al (getter) */
    TOK_AYARLA,       /* ayarla (setter) */
    TOK_YOK_ET,       /* yok_et (destructor) */

    /* Özel */
    TOK_TANIMLAYICI,
    TOK_YENİ_SATIR,
    TOK_DOSYA_SONU,
} SözcükTürü;

typedef struct {
    SözcükTürü  tur;
    const char *başlangıç;
    int         uzunluk;
    int         satir;
    int         sutun;
    union {
        int64_t tam_deger;
        double  ondalık_değer;
    } deger;
} Sözcük;

/* Sözcük çözümleyici durumu */
typedef struct {
    const char *kaynak;
    int         pos;
    int         satir;
    int         sutun;
    Sözcük     *sozcukler;
    int         sozcuk_sayisi;
    int         sozcuk_kapasite;
} SözcükÇözümleyici;

/* Kaynak kodu sözcüklere ayır */
void sözcük_çözümle(SözcükÇözümleyici *sc, const char *kaynak);

/* Sözcük türünün adını döndür (hata mesajları için) */
const char *sözcük_tür_adı(SözcükTürü tur);

/* Bellek serbest bırak */
void sözcük_serbest(SözcükÇözümleyici *sc);

#endif
