#include "hata.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

int hata_sayisi = 0;
const char *hata_dosya_adi = "<bilinmiyor>";
const char *hata_kaynak = NULL;

/* ANSI renk kodları */
#define RENK_KIRMIZI  "\033[1;31m"
#define RENK_SARI     "\033[1;33m"
#define RENK_MAVI     "\033[1;34m"
#define RENK_SIFIRLA  "\033[0m"
#define RENK_KALIN    "\033[1m"

static int renkli_mi(void) {
    return isatty(STDERR_FILENO);
}

static const char *renk(const char *kod) {
    return renkli_mi() ? kod : "";
}

static const char *hata_sablonlari[] = {
    [HATA_BEKLENMEYEN_KARAKTER] = "beklenmeyen karakter: '%s'",
    [HATA_BEKLENEN_SÖZCÜK]      = "'%s' bekleniyordu, '%s' bulundu",
    [HATA_TANIMSIZ_DEĞİŞKEN]    = "tan\xc4\xb1ms\xc4\xb1z de\xc4\x9fi\xc5\x9fken: '%s'",
    [HATA_TANIMSIZ_İŞLEV]       = "tan\xc4\xb1ms\xc4\xb1z i\xc5\x9flev: '%s'",
    [HATA_TİP_UYUMSUZLUĞU]      = "'%s' ve '%s' tipleri uyumsuz",
    [HATA_PARAMETRE_SAYISI]     = "'%s' i\xc5\x9flevi %d parametre bekliyor, %d verildi",
    [HATA_DOSYA_AÇILAMADI]      = "dosya a\xc3\xa7\xc4\xb1lamad\xc4\xb1: '%s'",
    [HATA_SÖZDİZİMİ]            = "s\xc3\xb6zdizimi hatas\xc4\xb1: %s",
    [HATA_KAPANMAMIŞ_METİN]     = "kapanmam\xc4\xb1\xc5\x9f metin de\xc4\x9f" "eri",
    [HATA_BEKLENEN_İFADE]       = "ifade bekleniyordu",
    [HATA_BEKLENEN_TİP]         = "tip belirteci bekleniyordu",
    [HATA_TEKRAR_TANIM]         = "'%s' zaten tan\xc4\xb1mlanm\xc4\xb1\xc5\x9f",
    [HATA_DÖNGÜ_DIŞI_KIR]       = "'k\xc4\xb1r' yaln\xc4\xb1zca d\xc3\xb6ng\xc3\xbc i\xc3\xa7inde kullan\xc4\xb1labilir",
    [HATA_İŞLEV_DIŞI_DÖNDÜR]    = "'d\xc3\xb6nd\xc3\xbcr' yaln\xc4\xb1zca i\xc5\x9flev i\xc3\xa7inde kullan\xc4\xb1labilir",
    [UYARI_BAŞLANGIÇSIZ_DEĞİŞKEN] = "'%s' de\xc4\x9fi\xc5\x9fkeni ba\xc5\x9flat\xc4\xb1lmadan kullan\xc4\xb1l\xc4\xb1yor",
    [HATA_KAPANMAMIŞ_YORUM]       = "kapanmam\xc4\xb1\xc5\x9f blok yorumu",
    [HATA_SABİT_ATAMA]            = "'%s' sabit de\xc4\x9fi\xc5\x9fkene atama yap\xc4\xb1lamaz",
    [HATA_ARAYÜZ_UYGULAMA]  = "s\xc4\xb1n\xc4\xb1" "f '%s' aray\xc3\xbcz '%s' metodunu uygulam\xc4\xb1yor",
    [HATA_SAYIM_TANIMSIZ]   = "'%s' tan\xc4\xb1ms\xc4\xb1z say\xc4\xb1m de\xc4\x9f" "eri",
    [HATA_SINIR_AŞIMI]      = "%s",
};

/* Kaynak koddan ilgili satırı bul ve göster (renkli, sütun işaretçisi ile) */
static void satir_goster(int satir, int sutun) {
    if (!hata_kaynak) return;

    const char *p = hata_kaynak;
    int mevcut_satir = 1;

    /* İlgili satıra git */
    while (*p && mevcut_satir < satir) {
        if (*p == '\n') mevcut_satir++;
        p++;
    }

    if (mevcut_satir != satir) return;

    /* Satırın sonunu bul */
    const char *satir_sonu = p;
    while (*satir_sonu && *satir_sonu != '\n') satir_sonu++;

    int satir_uzunluk = (int)(satir_sonu - p);
    if (satir_uzunluk > 120) satir_uzunluk = 120;

    fprintf(stderr, "  %s%d%s | %.*s\n",
            renk(RENK_MAVI), satir, renk(RENK_SIFIRLA),
            satir_uzunluk, p);

    /* Sütun işaretçisi */
    if (sutun > 0) {
        /* Satır numarasının genişliğini hesapla */
        int satir_genislik = 0;
        int tmp = satir;
        while (tmp > 0) { satir_genislik++; tmp /= 10; }

        fprintf(stderr, "  ");
        for (int i = 0; i < satir_genislik; i++) fprintf(stderr, " ");
        fprintf(stderr, " | ");
        for (int i = 1; i < sutun; i++) fprintf(stderr, " ");
        fprintf(stderr, "%s^^^%s\n", renk(RENK_KIRMIZI), renk(RENK_SIFIRLA));
    }
}

void hata_bildir(HataKodu kod, int satir, int sutun, ...) {
    hata_sayisi++;

    fprintf(stderr, "%s%shata:%s %s%s:%d:%d:%s ",
            renk(RENK_KALIN), renk(RENK_KIRMIZI), renk(RENK_SIFIRLA),
            renk(RENK_KALIN), hata_dosya_adi, satir, sutun, renk(RENK_SIFIRLA));

    va_list args;
    va_start(args, sutun);
    vfprintf(stderr, hata_sablonlari[kod], args);
    va_end(args);

    fprintf(stderr, "\n");
    satir_goster(satir, sutun);
}

void uyarı_bildir(HataKodu kod, int satir, int sutun, ...) {
    /* Uyarı - hata sayacını artırmaz */
    fprintf(stderr, "%s%suyar\xc4\xb1:%s %s%s:%d:%d:%s ",
            renk(RENK_KALIN), renk(RENK_SARI), renk(RENK_SIFIRLA),
            renk(RENK_KALIN), hata_dosya_adi, satir, sutun, renk(RENK_SIFIRLA));

    va_list args;
    va_start(args, sutun);
    vfprintf(stderr, hata_sablonlari[kod], args);
    va_end(args);

    fprintf(stderr, "\n");
    satir_goster(satir, sutun);
}

void hata_genel(const char *mesaj, ...) {
    hata_sayisi++;
    fprintf(stderr, "%s%shata:%s ", renk(RENK_KALIN), renk(RENK_KIRMIZI), renk(RENK_SIFIRLA));
    va_list args;
    va_start(args, mesaj);
    vfprintf(stderr, mesaj, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* "Bunu mu demek istediniz?" önerisi göster */
void hata_oneri_goster(const char *oneri) {
    if (!oneri) return;
    fprintf(stderr, "  %sipucu:%s bunu mu demek istediniz: '%s%s%s'?\n",
            renk(RENK_MAVI), renk(RENK_SIFIRLA),
            renk(RENK_KALIN), oneri, renk(RENK_SIFIRLA));
}

/* Levenshtein mesafe hesaplama (byte bazlı, eşik ile erken çıkış) */
int levenshtein_mesafe(const char *s1, const char *s2, int esik) {
    int len1 = (int)strlen(s1);
    int len2 = (int)strlen(s2);
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;
    if (len1 - len2 > esik || len2 - len1 > esik) return esik + 1;

    /* Basit DP — küçük diziler için yeterli */
    int satir[256];
    if (len2 >= 256) return esik + 1;

    for (int j = 0; j <= len2; j++) satir[j] = j;

    for (int i = 1; i <= len1; i++) {
        int onceki = satir[0];
        satir[0] = i;
        int min_satir = satir[0];
        for (int j = 1; j <= len2; j++) {
            int maliyet = (s1[i-1] == s2[j-1]) ? 0 : 1;
            int sil = satir[j] + 1;
            int ekle = satir[j-1] + 1;
            int degistir = onceki + maliyet;
            onceki = satir[j];
            int min = sil < ekle ? sil : ekle;
            satir[j] = min < degistir ? min : degistir;
            if (satir[j] < min_satir) min_satir = satir[j];
        }
        if (min_satir > esik) return esik + 1;
    }
    return satir[len2];
}
