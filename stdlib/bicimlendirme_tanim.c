/* Biçimlendirme modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon bicimlendirme_fonksiyonlar[] = {
    /* sıfır_doldur(sayı: tam, genişlik: tam) -> metin: "00042" */
    {"s\xc4\xb1f\xc4\xb1r_doldur", "sifir_doldur", "_tr_sifir_doldur",
     {TİP_TAM, TİP_TAM}, 2, TİP_METİN},

    /* ondalık_biçimle(sayı: ondalık, basamak: tam) -> metin: "3.14" */
    {"ondal\xc4\xb1k_bi\xc3\xa7imle", "ondalik_bicimle", "_tr_ondalik_bicimle2",
     {TİP_ONDALIK, TİP_TAM}, 2, TİP_METİN},

    /* para_biçimle(sayı: ondalık) -> metin: "1.234.567,89" */
    {"para_bi\xc3\xa7imle", "para_bicimle", "_tr_para_bicimle",
     {TİP_ONDALIK}, 1, TİP_METİN},

    /* para_biçimle_sembol(sayı: ondalık, sembol: metin) -> metin: "1.234,56 TL" */
    {"para_bi\xc3\xa7imle_sembol", "para_bicimle_sembol", "_tr_para_bicimle_sembol",
     {TİP_ONDALIK, TİP_METİN}, 2, TİP_METİN},

    /* binlik_ayır(sayı: tam) -> metin: "1.234.567" */
    {"binlik_ay\xc4\xb1r", "binlik_ayir", "_tr_binlik_ayir",
     {TİP_TAM}, 1, TİP_METİN},

    /* sağa_hizala(metin: metin, genişlik: tam) -> metin */
    {"sa\xc4\x9fa_hizala", "saga_hizala", "_tr_saga_hizala",
     {TİP_METİN, TİP_TAM}, 2, TİP_METİN},

    /* sola_hizala(metin: metin, genişlik: tam) -> metin */
    {"sola_hizala", NULL, "_tr_sola_hizala",
     {TİP_METİN, TİP_TAM}, 2, TİP_METİN},

    /* ortala(metin: metin, genişlik: tam) -> metin */
    {"ortala", NULL, "_tr_ortala",
     {TİP_METİN, TİP_TAM}, 2, TİP_METİN},

    /* ondalık_binlik(sayı: ondalık, basamak: tam) -> metin: "1.234,56" */
    {"ondal\xc4\xb1k_binlik", "ondalik_binlik", "_tr_ondalik_binlik",
     {TİP_ONDALIK, TİP_TAM}, 2, TİP_METİN},
};

const ModülTanım bicimlendirme_modul = {
    "bi\xc3\xa7imlendirme", "bicimlendirme",
    bicimlendirme_fonksiyonlar,
    sizeof(bicimlendirme_fonksiyonlar) / sizeof(bicimlendirme_fonksiyonlar[0])
};
