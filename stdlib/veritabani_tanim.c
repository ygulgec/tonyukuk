/* Veritabanı modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon veritabani_fonksiyonlar[] = {
    /* vt_aç(yol: metin) -> tam */
    {"vt_a\xc3\xa7", "vt_ac", "_tr_vt_ac", {TİP_METİN}, 1, TİP_TAM},

    /* vt_çalıştır(db: tam, sql: metin) -> tam (DİKKAT: SQL injection riski!) */
    {"vt_\xc3\xa7" "al\xc4\xb1\xc5\x9f" "t\xc4\xb1r", "vt_calistir", "_tr_vt_calistir", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* vt_sorgula(db: tam, sql: metin) -> metin (DİKKAT: SQL injection riski!) */
    {"vt_sorgula", NULL, "_tr_vt_sorgula", {TİP_TAM, TİP_METİN}, 2, TİP_METİN},

    /* vt_kapat(db: tam) -> boşluk */
    {"vt_kapat", NULL, "_tr_vt_kapat", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* === GÜVENLİ PREPARED STATEMENT API === */

    /* vt_escape(metin) -> metin - SQL injection koruması için */
    {"vt_escape", NULL, "_tr_vt_escape", {TİP_METİN}, 1, TİP_METİN},

    /* vt_hazırla(db: tam, sql: metin) -> tam (statement handle) */
    {"vt_haz\xc4\xb1rla", "vt_hazirla", "_tr_vt_hazirla", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* vt_bağla_metin(stmt: tam, index: tam, değer: metin) -> tam */
    {"vt_ba\xc4\x9fla_metin", "vt_bagla_metin", "_tr_vt_bagla_metin", {TİP_TAM, TİP_TAM, TİP_METİN}, 3, TİP_TAM},

    /* vt_bağla_tam(stmt: tam, index: tam, değer: tam) -> tam */
    {"vt_ba\xc4\x9fla_tam", "vt_bagla_tam", "_tr_vt_bagla_tam", {TİP_TAM, TİP_TAM, TİP_TAM}, 3, TİP_TAM},

    /* vt_bağla_ondalık(stmt: tam, index: tam, değer: ondalık) -> tam */
    {"vt_ba\xc4\x9fla_ondal\xc4\xb1k", "vt_bagla_ondalik", "_tr_vt_bagla_ondalik", {TİP_TAM, TİP_TAM, TİP_ONDALIK}, 3, TİP_TAM},

    /* vt_adımla(stmt: tam) -> tam (0=bitti, 1=veri var, -1=hata) */
    {"vt_ad\xc4\xb1mla", "vt_adimla", "_tr_vt_adimla", {TİP_TAM}, 1, TİP_TAM},

    /* vt_sütun_metin(stmt: tam, col: tam) -> metin */
    {"vt_s\xc3\xbctun_metin", "vt_sutun_metin", "_tr_vt_sutun_metin", {TİP_TAM, TİP_TAM}, 2, TİP_METİN},

    /* vt_sütun_tam(stmt: tam, col: tam) -> tam */
    {"vt_s\xc3\xbctun_tam", "vt_sutun_tam", "_tr_vt_sutun_tam", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* vt_sütun_ondalık(stmt: tam, col: tam) -> ondalık */
    {"vt_s\xc3\xbctun_ondal\xc4\xb1k", "vt_sutun_ondalik", "_tr_vt_sutun_ondalik", {TİP_TAM, TİP_TAM}, 2, TİP_ONDALIK},

    /* vt_sıfırla(stmt: tam) -> tam */
    {"vt_s\xc4\xb1f\xc4\xb1rla", "vt_sifirla", "_tr_vt_sifirla", {TİP_TAM}, 1, TİP_TAM},

    /* vt_bitir(stmt: tam) -> boşluk */
    {"vt_bitir", NULL, "_tr_vt_bitir", {TİP_TAM}, 1, TİP_BOŞLUK},
};

const ModülTanım veritabani_modul = {
    "veritaban\xc4\xb1", "veritabani",
    veritabani_fonksiyonlar,
    sizeof(veritabani_fonksiyonlar) / sizeof(veritabani_fonksiyonlar[0])
};
