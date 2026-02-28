/* Tarih modülü — derleme zamanı tanımları
 * Tonyukuk Programlama Dili
 * Basit tarih/saat erişim fonksiyonları
 */
#include "../src/modul.h"

static const ModülFonksiyon tarih_fonksiyonlar[] = {

    /* tarih_simdi() -> metin: "2026-02-07 13:45:30" formatında */
    {"tarih_\xc5\x9fimdi", "tarih_simdi", "_tr_tarih_simdi", {0}, 0, TİP_METİN},

    /* tarih_gun() -> tam: gün numarası (1-31) */
    {"tarih_g\xc3\xbcn", "tarih_gun", "_tr_tarih_gun", {0}, 0, TİP_TAM},

    /* tarih_ay() -> tam: ay numarası (1-12) */
    {"tarih_ay", NULL, "_tr_tarih_ay", {0}, 0, TİP_TAM},

    /* tarih_yil() -> tam: yıl (2026) */
    {"tarih_y\xc4\xb1l", "tarih_yil", "_tr_tarih_yil", {0}, 0, TİP_TAM},

    /* tarih_saat() -> tam: saat (0-23) */
    {"tarih_saat", NULL, "_tr_tarih_saat", {0}, 0, TİP_TAM},

    /* tarih_dakika() -> tam: dakika (0-59) */
    {"tarih_dakika", NULL, "_tr_tarih_dakika", {0}, 0, TİP_TAM},

    /* tarih_saniye() -> tam: saniye (0-59) */
    {"tarih_saniye", NULL, "_tr_tarih_saniye", {0}, 0, TİP_TAM},

    /* tarih_damga() -> tam: Unix timestamp (epoch saniye) */
    {"tarih_damga", NULL, "_tr_tarih_damga", {0}, 0, TİP_TAM},

    /* tarih_gun_adi() -> metin: Türkçe gün adı */
    {"tarih_g\xc3\xbcn_ad\xc4\xb1", "tarih_gun_adi", "_tr_tarih_gun_adi", {0}, 0, TİP_METİN},

    /* tarih_ay_adi() -> metin: Türkçe ay adı */
    {"tarih_ay_ad\xc4\xb1", "tarih_ay_adi", "_tr_tarih_ay_adi", {0}, 0, TİP_METİN},
};

const ModülTanım tarih_modul = {
    "tarih", NULL,
    tarih_fonksiyonlar,
    sizeof(tarih_fonksiyonlar) / sizeof(tarih_fonksiyonlar[0])
};
