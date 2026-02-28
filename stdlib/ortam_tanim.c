/* Ortam modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon ortam_fonksiyonlar[] = {
    /* ortam_al(anahtar: metin) -> metin */
    {"ortam_al", NULL, "_tr_ortam_al", {TİP_METİN}, 1, TİP_METİN},

    /* ortam_koy(anahtar: metin, deger: metin) -> boşluk */
    {"ortam_koy", NULL, "_tr_ortam_koy", {TİP_METİN, TİP_METİN}, 2, TİP_BOŞLUK},

    /* ortam_sil(anahtar: metin) -> boşluk */
    {"ortam_sil", NULL, "_tr_ortam_sil", {TİP_METİN}, 1, TİP_BOŞLUK},

    /* ortam_hepsi() -> dizi */
    {"ortam_hepsi", NULL, "_tr_ortam_hepsi", {0}, 0, TİP_DİZİ},
};

const ModülTanım ortam_modul = {
    "ortam", NULL,
    ortam_fonksiyonlar,
    sizeof(ortam_fonksiyonlar) / sizeof(ortam_fonksiyonlar[0])
};
