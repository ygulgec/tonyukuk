/* Soket modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon soket_fonksiyonlar[] = {
    /* soket_oluştur() -> tam */
    {"soket_olu\xc5\x9ftur", "soket_olustur", "_tr_soket_olustur", {0}, 0, TİP_TAM},

    /* soket_bağlan(fd: tam, adres: metin, port: tam) -> mantık */
    {"soket_ba\xc4\x9flan", "soket_baglan", "_tr_soket_baglan", {TİP_TAM, TİP_METİN, TİP_TAM}, 3, TİP_MANTIK},

    /* soket_dinle(port: tam) -> tam */
    {"soket_dinle", NULL, "_tr_soket_dinle", {TİP_TAM}, 1, TİP_TAM},

    /* soket_kabul(fd: tam) -> tam */
    {"soket_kabul", NULL, "_tr_soket_kabul", {TİP_TAM}, 1, TİP_TAM},

    /* soket_gönder(fd: tam, veri: metin) -> tam */
    {"soket_g\xc3\xb6nder", "soket_gonder", "_tr_soket_gonder", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* soket_al(fd: tam) -> metin */
    {"soket_al", NULL, "_tr_soket_al", {TİP_TAM}, 1, TİP_METİN},

    /* soket_kapat(fd: tam) -> boşluk */
    {"soket_kapat", NULL, "_tr_soket_kapat", {TİP_TAM}, 1, TİP_BOŞLUK},
};

const ModülTanım soket_modul = {
    "soket", NULL,
    soket_fonksiyonlar,
    sizeof(soket_fonksiyonlar) / sizeof(soket_fonksiyonlar[0])
};
