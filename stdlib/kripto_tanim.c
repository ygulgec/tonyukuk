/* Kripto modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon kripto_fonksiyonlar[] = {
    /* md5(m: metin) -> metin */
    {"md5", NULL, "_tr_md5", {TİP_METİN}, 1, TİP_METİN},

    /* sha256(m: metin) -> metin */
    {"sha256", NULL, "_tr_sha256", {TİP_METİN}, 1, TİP_METİN},

    /* base64_kodla(m: metin) -> metin */
    {"base64_kodla", NULL, "_tr_base64_kodla", {TİP_METİN}, 1, TİP_METİN},

    /* base64_çöz(m: metin) -> metin */
    {"base64_\xc3\xa7\xc3\xb6z", "base64_coz", "_tr_base64_coz", {TİP_METİN}, 1, TİP_METİN},
};

const ModülTanım kripto_modul = {
    "kripto", NULL,
    kripto_fonksiyonlar,
    sizeof(kripto_fonksiyonlar) / sizeof(kripto_fonksiyonlar[0])
};
