#ifndef HATA_H
#define HATA_H

typedef enum {
    HATA_BEKLENMEYEN_KARAKTER,
    HATA_BEKLENEN_SÖZCÜK,
    HATA_TANIMSIZ_DEĞİŞKEN,
    HATA_TANIMSIZ_İŞLEV,
    HATA_TİP_UYUMSUZLUĞU,
    HATA_PARAMETRE_SAYISI,
    HATA_DOSYA_AÇILAMADI,
    HATA_SÖZDİZİMİ,
    HATA_KAPANMAMIŞ_METİN,
    HATA_BEKLENEN_İFADE,
    HATA_BEKLENEN_TİP,
    HATA_TEKRAR_TANIM,
    HATA_DÖNGÜ_DIŞI_KIR,
    HATA_İŞLEV_DIŞI_DÖNDÜR,
    UYARI_BAŞLANGIÇSIZ_DEĞİŞKEN,
    HATA_KAPANMAMIŞ_YORUM,
    HATA_SABİT_ATAMA,
    HATA_ARAYÜZ_UYGULAMA,   /* arayüz metodu uygulanmamış */
    HATA_SAYIM_TANIMSIZ,    /* tanımsız sayım değeri */
    HATA_SINIR_AŞIMI,       /* sabit limit aşıldı */
} HataKodu;

/* Hata sayacı */
extern int hata_sayisi;

/* Kaynak dosya bilgisi (hata mesajlarında kullanılır) */
extern const char *hata_dosya_adi;
extern const char *hata_kaynak;

/* Hata bildir (Türkçe mesajlar) */
void hata_bildir(HataKodu kod, int satir, int sutun, ...);

/* Genel hata mesajı */
void hata_genel(const char *mesaj, ...);

/* Uyarı bildir (hata sayacını artırmaz) */
void uyarı_bildir(HataKodu kod, int satir, int sutun, ...);

/* "Bunu mu demek istediniz?" önerisi */
void hata_oneri_goster(const char *oneri);

/* Levenshtein mesafe hesaplama (UTF-8 uyumlu, eşik ile erken çıkış) */
int levenshtein_mesafe(const char *s1, const char *s2, int esik);

#endif
