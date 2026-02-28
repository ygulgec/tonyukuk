/* Argüman modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon arguman_fonksiyonlar[] = {
    /* argüman_sayısı() -> tam */
    {"arg\xc3\xbcman_say\xc4\xb1s\xc4\xb1", "arguman_sayisi", "_tr_arguman_sayisi", {0}, 0, TİP_TAM},

    /* argüman_al(indeks: tam) -> metin */
    {"arg\xc3\xbcman_al", "arguman_al", "_tr_arguman_al", {TİP_TAM}, 1, TİP_METİN},

    /* argüman_hepsi() -> dizi */
    {"arg\xc3\xbcman_hepsi", "arguman_hepsi", "_tr_arguman_hepsi", {0}, 0, TİP_DİZİ},
};

const ModülTanım arguman_modul = {
    "arg\xc3\xbcman", "arguman",
    arguman_fonksiyonlar,
    sizeof(arguman_fonksiyonlar) / sizeof(arguman_fonksiyonlar[0])
};
