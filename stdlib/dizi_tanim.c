/* Dizi modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon dizi_fonksiyonlar[] = {
    /* sırala(d: dizi) -> dizi */
    {"s\xc4\xb1rala", "sirala", "_tr_sirala", {TİP_DİZİ}, 1, TİP_DİZİ},

    /* ekle(d: dizi, eleman: tam) -> dizi */
    {"ekle", NULL, "_tr_ekle", {TİP_DİZİ, TİP_TAM}, 2, TİP_DİZİ},

    /* çıkar(d: dizi) -> dizi */
    {"\xc3\xa7\xc4\xb1kar", "cikar", "_tr_cikar", {TİP_DİZİ}, 1, TİP_DİZİ},

    /* birleştir(d1: dizi, d2: dizi) -> dizi */
    {"birle\xc5\x9ftir", "birlestir", "_tr_birlestir_dizi", {TİP_DİZİ, TİP_DİZİ}, 2, TİP_DİZİ},
};

const ModülTanım dizi_modul = {
    "dizi", NULL,
    dizi_fonksiyonlar,
    sizeof(dizi_fonksiyonlar) / sizeof(dizi_fonksiyonlar[0])
};
