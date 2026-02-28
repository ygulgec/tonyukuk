/* Web görünüm modülü — derleme zamanı tanımları
 * Tonyukuk Programlama Dili
 * WebKit2GTK web görünümü
 * Tüm fonksiyonlar wg_ ön ekiyle başlar (isim çakışmasını önlemek için)
 */
#include "../src/modul.h"

static const ModülFonksiyon webgorunum_fonksiyonlar[] = {
    /* ========== OLUŞTURMA ========== */

    /* wg_olustur() -> tam — WebKitWebView oluştur */
    {"wg_olu\xc5\x9ftur", "wg_olustur", "_tr_webgorunum_olustur", {0}, 0, TİP_TAM},

    /* ========== GEZİNTİ ========== */

    /* wg_yukle(wv_id: tam, url: metin) -> tam */
    {"wg_y\xc3\xbckle", "wg_yukle", "_tr_webgorunum_yukle", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* wg_geri(wv_id: tam) -> tam */
    {"wg_geri", NULL, "_tr_webgorunum_geri", {TİP_TAM}, 1, TİP_TAM},

    /* wg_ileri(wv_id: tam) -> tam */
    {"wg_ileri", NULL, "_tr_webgorunum_ileri", {TİP_TAM}, 1, TİP_TAM},

    /* wg_yenile(wv_id: tam) -> tam */
    {"wg_yenile", NULL, "_tr_webgorunum_yenile", {TİP_TAM}, 1, TİP_TAM},

    /* wg_durdur(wv_id: tam) -> tam */
    {"wg_durdur", NULL, "_tr_webgorunum_durdur", {TİP_TAM}, 1, TİP_TAM},

    /* ========== BİLGİ SORGULAMA ========== */

    /* wg_baslik_al(wv_id: tam) -> metin */
    {"wg_ba\xc5\x9fl\xc4\xb1k_al", "wg_baslik_al", "_tr_webgorunum_baslik_al", {TİP_TAM}, 1, TİP_METİN},

    /* wg_adres_al(wv_id: tam) -> metin */
    {"wg_adres_al", NULL, "_tr_webgorunum_adres_al", {TİP_TAM}, 1, TİP_METİN},

    /* wg_geri_gidebilir_mi(wv_id: tam) -> tam */
    {"wg_geri_gidebilir_mi", NULL, "_tr_webgorunum_geri_gidebilir_mi", {TİP_TAM}, 1, TİP_TAM},

    /* wg_ileri_gidebilir_mi(wv_id: tam) -> tam */
    {"wg_ileri_gidebilir_mi", NULL, "_tr_webgorunum_ileri_gidebilir_mi", {TİP_TAM}, 1, TİP_TAM},

    /* wg_yukleniyor_mu(wv_id: tam) -> tam */
    {"wg_y\xc3\xbckleniyor_mu", "wg_yukleniyor_mu", "_tr_webgorunum_yukleniyor_mu", {TİP_TAM}, 1, TİP_TAM},

    /* wg_yuklenme_yuzdesi(wv_id: tam) -> tam — 0-100 */
    {"wg_y\xc3\xbcklenme_y\xc3\xbczdesi", "wg_yuklenme_yuzdesi", "_tr_webgorunum_yuklenme_yuzdesi", {TİP_TAM}, 1, TİP_TAM},

    /* ========== OLAY YOKLAMA ========== */

    /* wg_baslik_degisti_mi(wv_id: tam) -> tam */
    {"wg_ba\xc5\x9fl\xc4\xb1k_de\xc4\x9fi\xc5\x9fti_mi", "wg_baslik_degisti_mi", "_tr_webgorunum_baslik_degisti_mi", {TİP_TAM}, 1, TİP_TAM},

    /* wg_adres_degisti_mi(wv_id: tam) -> tam */
    {"wg_adres_de\xc4\x9fi\xc5\x9fti_mi", "wg_adres_degisti_mi", "_tr_webgorunum_adres_degisti_mi", {TİP_TAM}, 1, TİP_TAM},

    /* wg_yuklenme_bitti_mi(wv_id: tam) -> tam */
    {"wg_y\xc3\xbcklenme_bitti_mi", "wg_yuklenme_bitti_mi", "_tr_webgorunum_yuklenme_bitti_mi", {TİP_TAM}, 1, TİP_TAM},

    /* ========== YAKINLAŞTIRMA ========== */

    /* wg_yakinlastir(wv_id: tam) -> tam */
    {"wg_yak\xc4\xb1nla\xc5\x9ft\xc4\xb1r", "wg_yakinlastir", "_tr_webgorunum_yakinlastir", {TİP_TAM}, 1, TİP_TAM},

    /* wg_uzaklastir(wv_id: tam) -> tam */
    {"wg_uzakla\xc5\x9ft\xc4\xb1r", "wg_uzaklastir", "_tr_webgorunum_uzaklastir", {TİP_TAM}, 1, TİP_TAM},

    /* wg_yakinlik_sifirla(wv_id: tam) -> tam */
    {"wg_yak\xc4\xb1nl\xc4\xb1k_s\xc4\xb1f\xc4\xb1rla", "wg_yakinlik_sifirla", "_tr_webgorunum_yakinlik_sifirla", {TİP_TAM}, 1, TİP_TAM},

    /* ========== İLERİ DÜZEY ========== */

    /* wg_js_calistir(wv_id: tam, kod: metin) -> tam */
    {"wg_js_\xc3\xa7al\xc4\xb1\xc5\x9ft\xc4\xb1r", "wg_js_calistir", "_tr_webgorunum_js_calistir", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* wg_html_yukle(wv_id: tam, html: metin) -> tam — HTML string yükle */
    {"wg_html_y\xc3\xbckle", "wg_html_yukle", "_tr_webgorunum_html_yukle", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* wg_widget_id(wv_id: tam) -> tam — Arayüz widget kimliği */
    {"wg_widget_id", NULL, "_tr_webgorunum_widget_id", {TİP_TAM}, 1, TİP_TAM},
};

const ModülTanım webgorunum_modul = {
    "web", NULL,
    webgorunum_fonksiyonlar,
    sizeof(webgorunum_fonksiyonlar) / sizeof(webgorunum_fonksiyonlar[0])
};
