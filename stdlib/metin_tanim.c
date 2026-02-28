/* Metin modülü — derleme zamanı tanımları (C runtime fonksiyonları)
 * NOT: harf_buyut, harf_kucult, kes, bul inline asm kullanır, burada değildir.
 * NOT: içerir özel sonuç dönüşümü gerektirir, burada değildir. */
#include "../src/modul.h"

static const ModülFonksiyon metin_fonksiyonlar[] = {
    /* kırp(m: metin) -> metin */
    {"k\xc4\xb1rp", "kirp", "_tr_kirp", {TİP_METİN}, 1, TİP_METİN},

    /* tersle(m: metin) -> metin */
    {"tersle", NULL, "_tr_tersle", {TİP_METİN}, 1, TİP_METİN},

    /* tekrarla(m: metin, kez: tam) -> metin */
    {"tekrarla", NULL, "_tr_tekrarla", {TİP_METİN, TİP_TAM}, 2, TİP_METİN},

    /* başlar_mi(m: metin, önek: metin) -> mantık */
    {"ba\xc5\x9flar_mi", "baslar_mi", "_tr_baslar_mi", {TİP_METİN, TİP_METİN}, 2, TİP_MANTIK},

    /* biter_mi(m: metin, sonek: metin) -> mantık */
    {"biter_mi", NULL, "_tr_biter_mi", {TİP_METİN, TİP_METİN}, 2, TİP_MANTIK},

    /* değiştir(m: metin, eski: metin, yeni: metin) -> metin */
    {"de\xc4\x9fi\xc5\x9ftir", "degistir", "_tr_degistir", {TİP_METİN, TİP_METİN, TİP_METİN}, 3, TİP_METİN},

    /* büyük_harf(m: metin) -> metin */
    {"b\xc3\xbcy\xc3\xbck_harf", "buyuk_harf", "_tr_buyuk_harf", {TİP_METİN}, 1, TİP_METİN},

    /* küçük_harf(m: metin) -> metin */
    {"k\xc3\xbc\xc3\xa7\xc3\xbck_harf", "kucuk_harf", "_tr_kucuk_harf", {TİP_METİN}, 1, TİP_METİN},

    /* böl(m: metin, ayırıcı: metin) -> dizi */
    {"b\xc3\xb6l", "bol", "_tr_bol", {TİP_METİN, TİP_METİN}, 2, TİP_DİZİ},

    /* birleştir_metin(d: dizi, ayırıcı: metin) -> metin */
    {"birle\xc5\x9ftir_metin", "birlestir_metin", "_tr_birlestir_metin", {TİP_DİZİ, TİP_METİN}, 2, TİP_METİN},

    /* dosya_satırlar(dosya: metin) -> dizi */
    {"dosya_sat\xc4\xb1rlar", "dosya_satirlar", "_tr_dosya_satirlar", {TİP_METİN}, 1, TİP_DİZİ},

    /* dosya_ekle(dosya: metin, içerik: metin) -> tam */
    {"dosya_ekle", NULL, "_tr_dosya_ekle", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* sözlük_yeni() -> tam */
    {"s\xc3\xb6zl\xc3\xbck_yeni", "sozluk_yeni", "_tr_sozluk_yeni", {0}, 0, TİP_TAM},

    /* sözlük_ekle(s: tam, anahtar: metin, değer: tam) -> tam */
    {"s\xc3\xb6zl\xc3\xbck_ekle", "sozluk_ekle", "_tr_sozluk_ekle", {TİP_TAM, TİP_METİN, TİP_TAM}, 3, TİP_TAM},

    /* sözlük_oku(s: tam, anahtar: metin) -> tam */
    {"s\xc3\xb6zl\xc3\xbck_oku", "sozluk_oku", "_tr_sozluk_oku", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* sözlük_uzunluk(s: tam) -> tam */
    {"s\xc3\xb6zl\xc3\xbck_uzunluk", "sozluk_uzunluk", "_tr_sozluk_uzunluk", {TİP_TAM}, 1, TİP_TAM},

    /* sözlük_var_mı(s: tam, anahtar: metin) -> mantık */
    {"s\xc3\xb6zl\xc3\xbck_var_m\xc4\xb1", "sozluk_var_mi", "_tr_sozluk_var_mi", {TİP_TAM, TİP_METİN}, 2, TİP_MANTIK},

    /* sözlük_sil(s: tam, anahtar: metin) -> tam */
    {"s\xc3\xb6zl\xc3\xbck_sil", "sozluk_sil", "_tr_sozluk_sil", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},
};

const ModülTanım metin_modul = {
    "metin", NULL,
    metin_fonksiyonlar,
    sizeof(metin_fonksiyonlar) / sizeof(metin_fonksiyonlar[0])
};
