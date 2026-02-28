/* JSON modülü — derleme zamanı tanımları
 * Tonyukuk Programlama Dili
 * Gelişmiş JSON desteği: tip güvenli erişim, dizi, builder API
 */
#include "../src/modul.h"

static const ModülFonksiyon json_fonksiyonlar[] = {

    /* ========== TEMEL ÇÖZÜMLEME ========== */

    /* json_çözümle(m: metin) -> tam
     * JSON metnini parçalar, nesne/dizi pointer döndürür */
    {"json_\xc3\xa7\xc3\xb6z\xc3\xbcmle", "json_cozumle", "_tr_json_cozumle", {TİP_METİN}, 1, TİP_TAM},

    /* json_oluştur(nesne: tam) -> metin
     * JSON nesnesini/dizisini metin olarak serialize eder */
    {"json_olu\xc5\x9ftur", "json_olustur", "_tr_json_olustur", {TİP_TAM}, 1, TİP_METİN},

    /* json_güzel_oluştur(nesne: tam, girinti: tam) -> metin
     * Formatlı JSON çıktısı (ileride geliştirilecek) */
    {"json_g\xc3\xbczel_olu\xc5\x9ftur", "json_guzel_olustur", "_tr_json_guzel_olustur", {TİP_TAM, TİP_TAM}, 2, TİP_METİN},

    /* ========== TİP KONTROL ========== */

    /* json_tip(nesne: tam, anahtar: metin) -> tam
     * Anahtarın değer tipini döndürür (0=null, 1=mantık, 2=tam, 3=ondalık, 4=metin, 5=nesne, 6=dizi) */
    {"json_tip", NULL, "_tr_json_tip", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* json_var_mı(nesne: tam, anahtar: metin) -> mantık
     * Anahtar nesne içinde var mı kontrol eder */
    {"json_var_m\xc4\xb1", "json_var_mi", "_tr_json_var_mi", {TİP_TAM, TİP_METİN}, 2, TİP_MANTIK},

    /* ========== TİP GÜVENLİ OKUMA ========== */

    /* json_metin_al(nesne: tam, anahtar: metin) -> metin
     * Anahtar değerini metin olarak döndürür */
    {"json_metin_al", NULL, "_tr_json_metin_al", {TİP_TAM, TİP_METİN}, 2, TİP_METİN},

    /* json_tam_al(nesne: tam, anahtar: metin) -> tam
     * Anahtar değerini tam sayı olarak döndürür */
    {"json_tam_al", NULL, "_tr_json_tam_al", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* json_ondalık_al(nesne: tam, anahtar: metin) -> ondalık
     * Anahtar değerini ondalık sayı olarak döndürür */
    {"json_ondal\xc4\xb1k_al", "json_ondalik_al", "_tr_json_ondalik_al", {TİP_TAM, TİP_METİN}, 2, TİP_ONDALIK},

    /* json_mantık_al(nesne: tam, anahtar: metin) -> mantık
     * Anahtar değerini mantıksal olarak döndürür */
    {"json_mant\xc4\xb1k_al", "json_mantik_al", "_tr_json_mantik_al", {TİP_TAM, TİP_METİN}, 2, TİP_MANTIK},

    /* json_nesne_al(nesne: tam, anahtar: metin) -> tam
     * İç içe nesne pointer döndürür */
    {"json_nesne_al", NULL, "_tr_json_nesne_al", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* json_dizi_al(nesne: tam, anahtar: metin) -> tam
     * Dizi pointer döndürür */
    {"json_dizi_al", NULL, "_tr_json_dizi_al", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* ========== DİZİ ERİŞİM ========== */

    /* json_dizi_uzunluk(dizi: tam) -> tam
     * Dizi eleman sayısını döndürür */
    {"json_dizi_uzunluk", NULL, "_tr_json_dizi_uzunluk", {TİP_TAM}, 1, TİP_TAM},

    /* json_dizi_tip(dizi: tam, indeks: tam) -> tam
     * Dizi elemanının tipini döndürür */
    {"json_dizi_tip", NULL, "_tr_json_dizi_tip", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* json_dizi_metin(dizi: tam, indeks: tam) -> metin
     * Dizi elemanını metin olarak döndürür */
    {"json_dizi_metin", NULL, "_tr_json_dizi_metin", {TİP_TAM, TİP_TAM}, 2, TİP_METİN},

    /* json_dizi_tam(dizi: tam, indeks: tam) -> tam
     * Dizi elemanını tam sayı olarak döndürür */
    {"json_dizi_tam", NULL, "_tr_json_dizi_tam", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* json_dizi_ondalık(dizi: tam, indeks: tam) -> ondalık
     * Dizi elemanını ondalık olarak döndürür */
    {"json_dizi_ondal\xc4\xb1k", "json_dizi_ondalik", "_tr_json_dizi_ondalik", {TİP_TAM, TİP_TAM}, 2, TİP_ONDALIK},

    /* json_dizi_mantık(dizi: tam, indeks: tam) -> mantık
     * Dizi elemanını mantıksal olarak döndürür */
    {"json_dizi_mant\xc4\xb1k", "json_dizi_mantik", "_tr_json_dizi_mantik", {TİP_TAM, TİP_TAM}, 2, TİP_MANTIK},

    /* json_dizi_nesne(dizi: tam, indeks: tam) -> tam
     * Dizi içindeki nesne pointer döndürür */
    {"json_dizi_nesne", NULL, "_tr_json_dizi_nesne", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* json_dizi_dizi(dizi: tam, indeks: tam) -> tam
     * Dizi içindeki iç dizi pointer döndürür */
    {"json_dizi_dizi", NULL, "_tr_json_dizi_dizi", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* ========== BUILDER API - NESNE ========== */

    /* json_nesne_yeni() -> tam
     * Yeni boş JSON nesnesi oluşturur */
    {"json_nesne_yeni", NULL, "_tr_json_nesne_yeni", {0}, 0, TİP_TAM},

    /* json_nesne_uzunluk(nesne: tam) -> tam
     * Nesne anahtar sayısını döndürür */
    {"json_nesne_uzunluk", NULL, "_tr_json_nesne_uzunluk", {TİP_TAM}, 1, TİP_TAM},

    /* json_nesne_anahtarlar(nesne: tam) -> dizi<metin>
     * Nesne anahtarlarını dizi olarak döndürür */
    {"json_nesne_anahtarlar", NULL, "_tr_json_nesne_anahtarlar", {TİP_TAM}, 1, TİP_DİZİ},

    /* json_nesne_metin_ekle(nesne: tam, anahtar: metin, değer: metin) -> tam
     * Nesneye metin değer ekler */
    {"json_nesne_metin_ekle", NULL, "_tr_json_nesne_metin_ekle", {TİP_TAM, TİP_METİN, TİP_METİN}, 3, TİP_TAM},

    /* json_nesne_tam_ekle(nesne: tam, anahtar: metin, değer: tam) -> tam
     * Nesneye tam sayı değer ekler */
    {"json_nesne_tam_ekle", NULL, "_tr_json_nesne_tam_ekle", {TİP_TAM, TİP_METİN, TİP_TAM}, 3, TİP_TAM},

    /* json_nesne_ondalık_ekle(nesne: tam, anahtar: metin, değer: ondalık) -> tam
     * Nesneye ondalık değer ekler */
    {"json_nesne_ondal\xc4\xb1k_ekle", "json_nesne_ondalik_ekle", "_tr_json_nesne_ondalik_ekle", {TİP_TAM, TİP_METİN, TİP_ONDALIK}, 3, TİP_TAM},

    /* json_nesne_mantık_ekle(nesne: tam, anahtar: metin, değer: mantık) -> tam
     * Nesneye mantıksal değer ekler */
    {"json_nesne_mant\xc4\xb1k_ekle", "json_nesne_mantik_ekle", "_tr_json_nesne_mantik_ekle", {TİP_TAM, TİP_METİN, TİP_MANTIK}, 3, TİP_TAM},

    /* json_nesne_null_ekle(nesne: tam, anahtar: metin) -> tam
     * Nesneye null değer ekler */
    {"json_nesne_null_ekle", NULL, "_tr_json_nesne_null_ekle", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* json_nesne_nesne_ekle(nesne: tam, anahtar: metin, iç_nesne: tam) -> tam
     * Nesneye iç nesne ekler */
    {"json_nesne_nesne_ekle", NULL, "_tr_json_nesne_nesne_ekle", {TİP_TAM, TİP_METİN, TİP_TAM}, 3, TİP_TAM},

    /* json_nesne_dizi_ekle(nesne: tam, anahtar: metin, dizi: tam) -> tam
     * Nesneye dizi ekler */
    {"json_nesne_dizi_ekle", NULL, "_tr_json_nesne_dizi_ekle", {TİP_TAM, TİP_METİN, TİP_TAM}, 3, TİP_TAM},

    /* ========== BUILDER API - DİZİ ========== */

    /* json_dizi_yeni() -> tam
     * Yeni boş JSON dizisi oluşturur */
    {"json_dizi_yeni", NULL, "_tr_json_dizi_yeni", {0}, 0, TİP_TAM},

    /* json_dizi_metin_ekle(dizi: tam, değer: metin) -> tam
     * Diziye metin değer ekler */
    {"json_dizi_metin_ekle", NULL, "_tr_json_dizi_metin_ekle", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* json_dizi_tam_ekle(dizi: tam, değer: tam) -> tam
     * Diziye tam sayı değer ekler */
    {"json_dizi_tam_ekle", NULL, "_tr_json_dizi_tam_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* json_dizi_ondalık_ekle(dizi: tam, değer: ondalık) -> tam
     * Diziye ondalık değer ekler */
    {"json_dizi_ondal\xc4\xb1k_ekle", "json_dizi_ondalik_ekle", "_tr_json_dizi_ondalik_ekle", {TİP_TAM, TİP_ONDALIK}, 2, TİP_TAM},

    /* json_dizi_mantık_ekle(dizi: tam, değer: mantık) -> tam
     * Diziye mantıksal değer ekler */
    {"json_dizi_mant\xc4\xb1k_ekle", "json_dizi_mantik_ekle", "_tr_json_dizi_mantik_ekle", {TİP_TAM, TİP_MANTIK}, 2, TİP_TAM},

    /* json_dizi_null_ekle(dizi: tam) -> tam
     * Diziye null değer ekler */
    {"json_dizi_null_ekle", NULL, "_tr_json_dizi_null_ekle", {TİP_TAM}, 1, TİP_TAM},

    /* json_dizi_nesne_ekle(dizi: tam, nesne: tam) -> tam
     * Diziye nesne ekler */
    {"json_dizi_nesne_ekle", NULL, "_tr_json_dizi_nesne_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* json_dizi_dizi_ekle(dizi: tam, iç_dizi: tam) -> tam
     * Diziye iç dizi ekler */
    {"json_dizi_dizi_ekle", NULL, "_tr_json_dizi_dizi_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},
};

const ModülTanım json_modul = {
    "json", NULL,
    json_fonksiyonlar,
    sizeof(json_fonksiyonlar) / sizeof(json_fonksiyonlar[0])
};
