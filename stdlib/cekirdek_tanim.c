/* Çekirdek (core) fonksiyonlar — her zaman erişilebilir, 'kullan' gerektirmez.
 * Bu dosya sadece kod üretici (üretici.c) tarafından modul_fonksiyon_bul()
 * ile runtime isimlerini bulmak için kullanılır.
 * Semantik kayıt hâlâ anlam.c:donusum_fonksiyonlari_kaydet() içindedir. */
#include "../src/modul.h"

static const ModülFonksiyon cekirdek_fonksiyonlar[] = {
    /* numarala(d: dizi) -> dizi */
    {"numarala", NULL, "_tr_numarala", {TİP_DİZİ}, 1, TİP_DİZİ},

    /* eşleştir(d1: dizi, d2: dizi) -> dizi */
    {"e\xc5\x9fle\xc5\x9ftir", "eslestir", "_tr_eslestir", {TİP_DİZİ, TİP_DİZİ}, 2, TİP_DİZİ},

    /* ters(d: dizi) -> dizi */
    {"ters", NULL, "_tr_ters_dizi", {TİP_DİZİ}, 1, TİP_DİZİ},

    /* dilimle(d: dizi, baş: tam, bitiş: tam, adım: tam) -> dizi */
    {"dilimle", NULL, "_tr_dilimle", {TİP_DİZİ, TİP_TAM, TİP_TAM, TİP_TAM}, 4, TİP_DİZİ},
};

const ModülTanım cekirdek_modul = {
    "çekirdek", "cekirdek",
    cekirdek_fonksiyonlar,
    sizeof(cekirdek_fonksiyonlar) / sizeof(cekirdek_fonksiyonlar[0])
};
