/* Küme modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon kume_fonksiyonlar[] = {
    /* küme_yeni() -> tam */
    {"k\xc3\xbcme_yeni", "kume_yeni", "_tr_kume_yeni", {0}, 0, TİP_TAM},

    /* küme_ekle(kume: tam, deger: tam) -> boşluk */
    {"k\xc3\xbcme_ekle", "kume_ekle", "_tr_kume_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_BOŞLUK},

    /* küme_sil(kume: tam, deger: tam) -> boşluk */
    {"k\xc3\xbcme_sil", "kume_sil", "_tr_kume_sil", {TİP_TAM, TİP_TAM}, 2, TİP_BOŞLUK},

    /* küme_var_mı(kume: tam, deger: tam) -> mantık */
    {"k\xc3\xbcme_var_m\xc4\xb1", "kume_var_mi", "_tr_kume_var_mi", {TİP_TAM, TİP_TAM}, 2, TİP_MANTIK},

    /* küme_uzunluk(kume: tam) -> tam */
    {"k\xc3\xbcme_uzunluk", "kume_uzunluk", "_tr_kume_uzunluk", {TİP_TAM}, 1, TİP_TAM},

    /* küme_birleşim(a: tam, b: tam) -> tam */
    {"k\xc3\xbcme_birle\xc5\x9fim", "kume_birlesim", "_tr_kume_birlesim", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* küme_kesişim(a: tam, b: tam) -> tam */
    {"k\xc3\xbcme_kesi\xc5\x9fim", "kume_kesisim", "_tr_kume_kesisim", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* küme_fark(a: tam, b: tam) -> tam */
    {"k\xc3\xbcme_fark", "kume_fark", "_tr_kume_fark", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* küme_yazdır(kume: tam) -> boşluk */
    {"k\xc3\xbcme_yazd\xc4\xb1r", "kume_yazdir", "_tr_kume_yazdir", {TİP_TAM}, 1, TİP_BOŞLUK},
};

const ModülTanım kume_modul = {
    "k\xc3\xbcme", "kume",
    kume_fonksiyonlar,
    sizeof(kume_fonksiyonlar) / sizeof(kume_fonksiyonlar[0])
};
