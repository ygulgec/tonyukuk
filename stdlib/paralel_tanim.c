/* Paralel modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon paralel_fonksiyonlar[] = {
    /* iş_oluştur(fn: tam) -> tam */
    {"i\xc5\x9f_olu\xc5\x9ftur", "is_olustur", "_tr_is_olustur", {TİP_TAM}, 1, TİP_TAM},

    /* iş_bekle(handle: tam) -> tam */
    {"i\xc5\x9f_bekle", "is_bekle", "_tr_is_bekle", {TİP_TAM}, 1, TİP_TAM},

    /* kilit_oluştur() -> tam */
    {"kilit_olu\xc5\x9ftur", "kilit_olustur", "_tr_kilit_olustur", {0}, 0, TİP_TAM},

    /* kilitle(kilit: tam) -> boşluk */
    {"kilitle", NULL, "_tr_kilitle", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* kilit_bırak(kilit: tam) -> boşluk */
    {"kilit_b\xc4\xb1rak", "kilit_birak", "_tr_kilit_birak", {TİP_TAM}, 1, TİP_BOŞLUK},
};

const ModülTanım paralel_modul = {
    "paralel", NULL,
    paralel_fonksiyonlar,
    sizeof(paralel_fonksiyonlar) / sizeof(paralel_fonksiyonlar[0])
};
