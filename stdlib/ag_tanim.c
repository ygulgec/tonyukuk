/* Ağ/HTTP modülü — derleme zamanı tanımları
 * Tonyukuk Programlama Dili
 * Kapsamlı HTTP client/server desteği
 */
#include "../src/modul.h"

static const ModülFonksiyon ag_fonksiyonlar[] = {

    /* ========== TEMEL HTTP İSTEKLERİ ========== */

    /* http_al(url: metin) -> metin */
    {"http_al", NULL, "_tr_http_al", {TİP_METİN}, 1, TİP_METİN},

    /* http_gönder(url: metin, veri: metin) -> metin (POST) */
    {"http_g\xc3\xb6nder", "http_gonder", "_tr_http_gonder", {TİP_METİN, TİP_METİN}, 2, TİP_METİN},

    /* http_json_gönder(url: metin, json: metin) -> metin (POST JSON) */
    {"http_json_g\xc3\xb6nder", "http_json_gonder", "_tr_http_json_gonder", {TİP_METİN, TİP_METİN}, 2, TİP_METİN},

    /* http_put(url: metin, veri: metin) -> metin */
    {"http_put", NULL, "_tr_http_put", {TİP_METİN, TİP_METİN}, 2, TİP_METİN},

    /* http_sil(url: metin) -> metin (DELETE) */
    {"http_sil", NULL, "_tr_http_sil", {TİP_METİN}, 1, TİP_METİN},

    /* http_head(url: metin) -> metin (HEAD - başlıklar) */
    {"http_head", NULL, "_tr_http_head", {TİP_METİN}, 1, TİP_METİN},

    /* ========== ÖZEL BAŞLIKLI İSTEKLER ========== */

    /* http_al_başlıklı(url: metin, başlıklar: metin) -> metin */
    {"http_al_ba\xc5\x9fl\xc4\xb1kl\xc4\xb1", "http_al_baslikli", "_tr_http_al_baslikli", {TİP_METİN, TİP_METİN}, 2, TİP_METİN},

    /* http_gönder_başlıklı(url: metin, veri: metin, başlıklar: metin) -> metin */
    {"http_g\xc3\xb6nder_ba\xc5\x9fl\xc4\xb1kl\xc4\xb1", "http_gonder_baslikli", "_tr_http_gonder_baslikli", {TİP_METİN, TİP_METİN, TİP_METİN}, 3, TİP_METİN},

    /* ========== DURUM BİLGİLERİ ========== */

    /* http_durum_kodu() -> tam: Son HTTP durum kodu */
    {"http_durum_kodu", NULL, "_tr_http_durum_kodu", {0}, 0, TİP_TAM},

    /* http_hata_mesajı() -> metin: Son hata mesajı */
    {"http_hata_mesaj\xc4\xb1", "http_hata_mesaji", "_tr_http_hata_mesaji", {0}, 0, TİP_METİN},

    /* ========== URL YARDIMCI FONKSİYONLAR ========== */

    /* url_kodla(metin) -> metin: URL encode */
    {"url_kodla", NULL, "_tr_url_kodla", {TİP_METİN}, 1, TİP_METİN},

    /* url_çöz(metin) -> metin: URL decode */
    {"url_\xc3\xa7\xc3\xb6z", "url_coz", "_tr_url_coz", {TİP_METİN}, 1, TİP_METİN},

    /* ========== HTTP SUNUCU ========== */

    /* http_sunucu_başlat(port: tam) -> tam */
    {"http_sunucu_ba\xc5\x9flat", "http_sunucu_baslat", "_tr_http_sunucu_baslat", {TİP_TAM}, 1, TİP_TAM},

    /* http_sunucu_durdur() -> boşluk */
    {"http_sunucu_durdur", NULL, "_tr_http_sunucu_durdur", {0}, 0, TİP_BOŞLUK},

    /* http_istek_bekle() -> tam (client soket) */
    {"http_istek_bekle", NULL, "_tr_http_istek_bekle", {0}, 0, TİP_TAM},

    /* http_istek_oku(soket: tam) -> metin */
    {"http_istek_oku", NULL, "_tr_http_istek_oku", {TİP_TAM}, 1, TİP_METİN},

    /* http_yanıt_gönder(soket: tam, durum: tam, içerik: metin, tip: metin) -> tam */
    {"http_yan\xc4\xb1t_g\xc3\xb6nder", "http_yanit_gonder", "_tr_http_yanit_gonder", {TİP_TAM, TİP_TAM, TİP_METİN, TİP_METİN}, 4, TİP_TAM},

    /* http_bağlantı_kapat(soket: tam) -> boşluk */
    {"http_ba\xc4\x9flant\xc4\xb1_kapat", "http_baglanti_kapat", "_tr_http_baglanti_kapat", {TİP_TAM}, 1, TİP_BOŞLUK},
};

const ModülTanım ag_modul = {
    "a\xc4\x9f", "ag",
    ag_fonksiyonlar,
    sizeof(ag_fonksiyonlar) / sizeof(ag_fonksiyonlar[0])
};
