/* Sistem modülü — derleme zamanı tanımları
 * Runtime implementasyonu: src/calismazamani.c */
#include "../src/modul.h"

static const ModülFonksiyon sistem_fonksiyonlar[] = {
    /* satıroku() -> metin */
    {"sat\xc4\xb1roku", "satiroku", "_tr_satiroku", {0}, 0, TİP_METİN},

    /* tam_metin(sayi: tam) -> metin */
    {"tam_metin", NULL, "_tr_tam_metin", {TİP_TAM}, 1, TİP_METİN},

    /* ondalık_metin(sayi: ondalık) -> metin */
    {"ondal\xc4\xb1k_metin", "ondalik_metin", "_tr_ondalik_metin", {TİP_ONDALIK}, 1, TİP_METİN},

    /* metin_tam(m: metin) -> tam */
    {"metin_tam", NULL, "_tr_metin_tam", {TİP_METİN}, 1, TİP_TAM},

    /* metin_ondalık(m: metin) -> ondalık */
    {"metin_ondal\xc4\xb1k", "metin_ondalik", "_tr_metin_ondalik", {TİP_METİN}, 1, TİP_ONDALIK},
};

const ModülTanım sistem_modul = {
    "sistem", NULL,
    sistem_fonksiyonlar,
    sizeof(sistem_fonksiyonlar) / sizeof(sistem_fonksiyonlar[0])
};
