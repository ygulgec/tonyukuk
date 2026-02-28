/* Sözlük modülü — derleme zamanı tanımları
 * Runtime implementasyonu: stdlib/sozluk_cz.c */
#include "../src/modul.h"

static const ModülFonksiyon sozluk_fonksiyonlar[] = {
    /* yeni() -> tam (sözlük pointer) */
    {"yeni", NULL, "_tr_sozluk_yeni", {0}, 0, TİP_TAM},

    /* ekle(sozluk: tam, anahtar: metin, deger: tam) -> tam */
    {"ekle", NULL, "_tr_sozluk_ekle", {TİP_TAM, TİP_METİN, TİP_TAM}, 3, TİP_TAM},

    /* oku(sozluk: tam, anahtar: metin) -> tam */
    {"oku", NULL, "_tr_sozluk_oku", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* uzunluk(sozluk: tam) -> tam */
    {"uzunluk", NULL, "_tr_sozluk_uzunluk", {TİP_TAM}, 1, TİP_TAM},

    /* var_mı(sozluk: tam, anahtar: metin) -> tam */
    {"var_m\xc4\xb1", "var_mi", "_tr_sozluk_var_mi", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* anahtarlar(sozluk: tam) -> dizi */
    {"anahtarlar", NULL, "_tr_sozluk_anahtarlar", {TİP_TAM}, 1, TİP_DİZİ},
};

const ModülTanım sozluk_modul = {
    "s\xc3\xb6zl\xc3\xbck", "sozluk",
    sozluk_fonksiyonlar,
    sizeof(sozluk_fonksiyonlar) / sizeof(sozluk_fonksiyonlar[0])
};
