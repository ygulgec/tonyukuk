/* Matematik modülü — derleme zamanı tanımları (C runtime fonksiyonları)
 * NOT: mutlak, kuvvet, karekök, min, maks, mod inline asm kullanır, burada değildir. */
#include "../src/modul.h"

static const ModülFonksiyon matematik_fonksiyonlar[] = {
    /* === Trigonometrik === */

    /* sin(x: ondalık) -> ondalık */
    {"sin", NULL, "_tr_sin", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* cos(x: ondalık) -> ondalık */
    {"cos", NULL, "_tr_cos", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* tan(x: ondalık) -> ondalık */
    {"tan", NULL, "_tr_tan", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* asin(x: ondalık) -> ondalık */
    {"asin", NULL, "_tr_asin", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* acos(x: ondalık) -> ondalık */
    {"acos", NULL, "_tr_acos", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* atan(x: ondalık) -> ondalık */
    {"atan", NULL, "_tr_atan", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* atan2(y: ondalık, x: ondalık) -> ondalık */
    {"atan2", NULL, "_tr_atan2", {TİP_ONDALIK, TİP_ONDALIK}, 2, TİP_ONDALIK},

    /* === Hiperbolik === */

    /* sinh(x: ondalık) -> ondalık */
    {"sinh", NULL, "_tr_sinh", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* cosh(x: ondalık) -> ondalık */
    {"cosh", NULL, "_tr_cosh", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* tanh(x: ondalık) -> ondalık */
    {"tanh", NULL, "_tr_tanh", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* === Logaritmik === */

    /* log(x: ondalık) -> ondalık */
    {"log", NULL, "_tr_log", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* log10(x: ondalık) -> ondalık */
    {"log10", NULL, "_tr_log10", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* log2(x: ondalık) -> ondalık */
    {"log2", NULL, "_tr_log2_", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* === Yuvarlama === */

    /* taban(x: ondalık) -> ondalık */
    {"taban", NULL, "_tr_taban", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* tavan(x: ondalık) -> ondalık */
    {"tavan", NULL, "_tr_tavan", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* yuvarla(x: ondalık) -> ondalık */
    {"yuvarla", NULL, "_tr_yuvarla", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* buda(x: ondalık) -> ondalık (trunc) */
    {"buda", NULL, "_tr_buda", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* === Üstel === */

    /* üstel(x: ondalık) -> ondalık (exp) */
    {"\xc3\xbcstel", "ustel", "_tr_ustel", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* küpkök(x: ondalık) -> ondalık (cbrt) */
    {"k\xc3\xbcpk\xc3\xb6k", "kupkok", "_tr_kupkok", {TİP_ONDALIK}, 1, TİP_ONDALIK},

    /* === İki parametreli ondalık === */

    /* üst(x: ondalık, y: ondalık) -> ondalık (pow) */
    {"\xc3\xbcst", "ust", "_tr_ust", {TİP_ONDALIK, TİP_ONDALIK}, 2, TİP_ONDALIK},

    /* hipot(x: ondalık, y: ondalık) -> ondalık */
    {"hipot", NULL, "_tr_hipot", {TİP_ONDALIK, TİP_ONDALIK}, 2, TİP_ONDALIK},

    /* fmod(x: ondalık, y: ondalık) -> ondalık */
    {"fmod", NULL, "_tr_fmod", {TİP_ONDALIK, TİP_ONDALIK}, 2, TİP_ONDALIK},

    /* === Tam sayı fonksiyonları === */

    /* faktöriyel(n: tam) -> tam */
    {"fakt\xc3\xb6riyel", "faktoriyel", "_tr_faktoriyel", {TİP_TAM}, 1, TİP_TAM},

    /* obeb(a: tam, b: tam) -> tam (GCD) */
    {"obeb", NULL, "_tr_obeb", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* okek(a: tam, b: tam) -> tam (LCM) */
    {"okek", NULL, "_tr_okek", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* rastgele(maks: tam) -> tam */
    {"rastgele", NULL, "_tr_rastgele", {TİP_TAM}, 1, TİP_TAM},

    /* işaret(x: tam) -> tam (signum) */
    {"i\xc5\x9f" "aret", "isaret", "_tr_isaret", {TİP_TAM}, 1, TİP_TAM},

    /* kombinasyon(n: tam, k: tam) -> tam */
    {"kombinasyon", NULL, "_tr_kombinasyon", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* permütasyon(n: tam, k: tam) -> tam */
    {"perm\xc3\xbctasyon", "permutasyon", "_tr_permutasyon", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* === Kontrol fonksiyonları === */

    /* sonsuz_mu(x: ondalık) -> mantık */
    {"sonsuz_mu", NULL, "_tr_sonsuz_mu", {TİP_ONDALIK}, 1, TİP_MANTIK},

    /* sayidegil_mi(x: ondalık) -> mantık */
    {"sayidegil_mi", NULL, "_tr_sayidegil_mi", {TİP_ONDALIK}, 1, TİP_MANTIK},

    /* === Sabitler === */

    /* pi() -> ondalık */
    {"pi", NULL, "_tr_pi", {0}, 0, TİP_ONDALIK},

    /* e() -> ondalık */
    {"e", NULL, "_tr_e", {0}, 0, TİP_ONDALIK},

    /* === Matris İşlemleri === */

    /* matris(satır: tam, sütun: tam) -> dizi */
    {"matris", NULL, "_tr_matris", {TİP_TAM, TİP_TAM}, 2, TİP_DİZİ},

    /* matris_birim(n: tam) -> dizi */
    {"matris_birim", NULL, "_tr_matris_birim", {TİP_TAM}, 1, TİP_DİZİ},

    /* matris_topla(a: dizi, b: dizi) -> dizi */
    {"matris_topla", NULL, "_tr_matris_topla", {TİP_DİZİ, TİP_DİZİ}, 2, TİP_DİZİ},

    /* matris_çıkar(a: dizi, b: dizi) -> dizi */
    {"matris_\xc3\xa7\xc4\xb1kar", "matris_cikar", "_tr_matris_cikar", {TİP_DİZİ, TİP_DİZİ}, 2, TİP_DİZİ},

    /* matris_çarp(a: dizi, b: dizi) -> dizi */
    {"matris_\xc3\xa7" "arp", "matris_carp", "_tr_matris_carp", {TİP_DİZİ, TİP_DİZİ}, 2, TİP_DİZİ},

    /* matris_skaler(a: dizi, k: tam) -> dizi */
    {"matris_skaler", NULL, "_tr_matris_skaler", {TİP_DİZİ, TİP_TAM}, 2, TİP_DİZİ},

    /* matris_transpoz(a: dizi) -> dizi */
    {"matris_transpoz", NULL, "_tr_matris_transpoz", {TİP_DİZİ}, 1, TİP_DİZİ},

    /* matris_determinant(a: dizi) -> tam */
    {"matris_determinant", NULL, "_tr_matris_determinant", {TİP_DİZİ}, 1, TİP_TAM},

    /* matris_ters(a: dizi) -> dizi */
    {"matris_ters", NULL, "_tr_matris_ters", {TİP_DİZİ}, 1, TİP_DİZİ},

    /* matris_oku(m: dizi, i: tam, j: tam) -> tam */
    {"matris_oku", NULL, "_tr_matris_oku", {TİP_DİZİ, TİP_TAM, TİP_TAM}, 3, TİP_TAM},

    /* matris_yaz(m: dizi, i: tam, j: tam, v: tam) -> dizi */
    {"matris_yaz", NULL, "_tr_matris_yaz", {TİP_DİZİ, TİP_TAM, TİP_TAM, TİP_TAM}, 4, TİP_DİZİ},

    /* matris_iz(a: dizi) -> tam */
    {"matris_iz", NULL, "_tr_matris_iz", {TİP_DİZİ}, 1, TİP_TAM},

    /* === İstatistik === */

    /* toplam(d: dizi) -> tam */
    {"toplam", NULL, "_tr_toplam", {TİP_DİZİ}, 1, TİP_TAM},

    /* ortalama(d: dizi) -> ondalık */
    {"ortalama", NULL, "_tr_ortalama", {TİP_DİZİ}, 1, TİP_ONDALIK},

    /* medyan(d: dizi) -> ondalık */
    {"medyan", NULL, "_tr_medyan", {TİP_DİZİ}, 1, TİP_ONDALIK},

    /* varyans(d: dizi) -> ondalık */
    {"varyans", NULL, "_tr_varyans", {TİP_DİZİ}, 1, TİP_ONDALIK},

    /* std_sapma(d: dizi) -> ondalık */
    {"std_sapma", NULL, "_tr_std_sapma", {TİP_DİZİ}, 1, TİP_ONDALIK},

    /* en_küçük(d: dizi) -> tam */
    {"en_k\xc3\xbc\xc3\xa7\xc3\xbck", "en_kucuk", "_tr_en_kucuk", {TİP_DİZİ}, 1, TİP_TAM},

    /* en_büyük(d: dizi) -> tam */
    {"en_b\xc3\xbcy\xc3\xbck", "en_buyuk", "_tr_en_buyuk", {TİP_DİZİ}, 1, TİP_TAM},

    /* korelasyon(x: dizi, y: dizi) -> ondalık */
    {"korelasyon", NULL, "_tr_korelasyon", {TİP_DİZİ, TİP_DİZİ}, 2, TİP_ONDALIK},

    /* === Sayısal Analiz === */

    /* polinom_hesapla(k: dizi, x: tam) -> tam */
    {"polinom_hesapla", NULL, "_tr_polinom_hesapla", {TİP_DİZİ, TİP_TAM}, 2, TİP_TAM},

    /* polinom_türev(k: dizi) -> dizi */
    {"polinom_t\xc3\xbcrev", "polinom_turev", "_tr_polinom_turev", {TİP_DİZİ}, 1, TİP_DİZİ},

    /* polinom_integral(k: dizi) -> dizi */
    {"polinom_integral", NULL, "_tr_polinom_integral", {TİP_DİZİ}, 1, TİP_DİZİ},

    /* sayısal_integral(x: dizi, y: dizi) -> tam */
    {"say\xc4\xb1sal_integral", "sayisal_integral", "_tr_sayisal_integral", {TİP_DİZİ, TİP_DİZİ}, 2, TİP_TAM},

    /* lineer_regresyon(x: dizi, y: dizi) -> dizi */
    {"lineer_regresyon", NULL, "_tr_lineer_regresyon", {TİP_DİZİ, TİP_DİZİ}, 2, TİP_DİZİ},
};

const ModülTanım matematik_modul = {
    "matematik", NULL,
    matematik_fonksiyonlar,
    sizeof(matematik_fonksiyonlar) / sizeof(matematik_fonksiyonlar[0])
};
