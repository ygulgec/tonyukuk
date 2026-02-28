/* Tekrarlayıcı modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon tekrarlayici_fonksiyonlar[] = {
    /* zincir(d1: dizi, d2: dizi) -> dizi */
    {"zincir", NULL, "_tr_zincir", {TİP_DİZİ, TİP_DİZİ}, 2, TİP_DİZİ},

    /* tekrarla_dizi(d: dizi, kez: tam) -> dizi */
    {"tekrarla_dizi", NULL, "_tr_tekrarla_dizi", {TİP_DİZİ, TİP_TAM}, 2, TİP_DİZİ},

    /* parçala(d: dizi, boyut: tam) -> dizi */
    {"par\xc3\xa7" "ala", "parcala", "_tr_parcala", {TİP_DİZİ, TİP_TAM}, 2, TİP_DİZİ},

    /* permütasyonlar(d: dizi) -> dizi */
    {"perm\xc3\xbctasyonlar", "permutasyonlar", "_tr_permutasyonlar", {TİP_DİZİ}, 1, TİP_DİZİ},

    /* kombinasyonlar(d: dizi, r: tam) -> dizi */
    {"kombinasyonlar", NULL, "_tr_kombinasyonlar", {TİP_DİZİ, TİP_TAM}, 2, TİP_DİZİ},

    /* düz(d: dizi) -> dizi */
    {"d\xc3\xbcz", "duz", "_tr_duz", {TİP_DİZİ}, 1, TİP_DİZİ},
};

const ModülTanım tekrarlayici_modul = {
    "tekrarlay\xc4\xb1" "c\xc4\xb1", "tekrarlayici",
    tekrarlayici_fonksiyonlar,
    sizeof(tekrarlayici_fonksiyonlar) / sizeof(tekrarlayici_fonksiyonlar[0])
};
