/* Pencere modülü — derleme zamanı tanımları
 * Tonyukuk Programlama Dili
 * GTK3 pencere yönetimi
 */
#include "../src/modul.h"

static const ModülFonksiyon pencere_fonksiyonlar[] = {
    /* ========== BAŞLATMA ========== */

    /* pencere.baslat() -> tam
     * GTK'yı başlatır. Program başında bir kez çağrılmalı. */
    {"ba\xc5\x9flat", "baslat", "_tr_pencere_baslat", {0}, 0, TİP_TAM},

    /* ========== PENCERE OLUŞTURMA ========== */

    /* pencere.olustur(baslik: metin, genislik: tam, yukseklik: tam) -> tam
     * Yeni pencere oluşturur, pencere kimliğini döndürür */
    {"olu\xc5\x9ftur", "olustur", "_tr_pencere_olustur", {TİP_METİN, TİP_TAM, TİP_TAM}, 3, TİP_TAM},

    /* ========== PENCERE ÖZELLİKLERİ ========== */

    /* pencere.baslik_ayarla(pencere_id: tam, baslik: metin) -> tam */
    {"ba\xc5\x9fl\xc4\xb1k_ayarla", "baslik_ayarla", "_tr_pencere_baslik_ayarla", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* pencere.boyut_ayarla(pencere_id: tam, genislik: tam, yukseklik: tam) -> tam */
    {"boyut_ayarla", NULL, "_tr_pencere_boyut_ayarla", {TİP_TAM, TİP_TAM, TİP_TAM}, 3, TİP_TAM},

    /* pencere.simge_ayarla(pencere_id: tam, dosya_yolu: metin) -> tam */
    {"simge_ayarla", NULL, "_tr_pencere_simge_ayarla", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* ========== GÖRÜNÜRLÜK ========== */

    /* pencere.goster(pencere_id: tam) -> tam */
    {"g\xc3\xb6ster", "goster", "_tr_pencere_goster", {TİP_TAM}, 1, TİP_TAM},

    /* pencere.gizle(pencere_id: tam) -> tam */
    {"gizle", NULL, "_tr_pencere_gizle", {TİP_TAM}, 1, TİP_TAM},

    /* pencere.kapat(pencere_id: tam) -> tam */
    {"kapat", NULL, "_tr_pencere_kapat", {TİP_TAM}, 1, TİP_TAM},

    /* ========== TAM EKRAN ========== */

    /* pencere.tam_ekran(pencere_id: tam) -> tam */
    {"tam_ekran", NULL, "_tr_pencere_tam_ekran", {TİP_TAM}, 1, TİP_TAM},

    /* pencere.tam_ekrandan_cik(pencere_id: tam) -> tam */
    {"tam_ekrandan_\xc3\xa7\xc4\xb1k", "tam_ekrandan_cik", "_tr_pencere_tam_ekrandan_cik", {TİP_TAM}, 1, TİP_TAM},

    /* ========== OLAY DÖNGÜSÜ ========== */

    /* pencere.olaylari_isle() -> tam
     * GTK olaylarını işler (non-blocking). Ana döngüde her adımda çağrılmalı.
     * Döndürür: 1 (devam et), 0 (tüm pencereler kapatıldı) */
    {"olaylar\xc4\xb1_i\xc5\x9fle", "olaylari_isle", "_tr_pencere_olaylari_isle", {0}, 0, TİP_TAM},

    /* pencere.calistir() -> tam
     * GTK ana döngüsünü çalıştırır (bloklayıcı).
     * Tüm pencereler kapatılana kadar döner. */
    {"\xc3\xa7al\xc4\xb1\xc5\x9ft\xc4\xb1r", "calistir", "_tr_pencere_calistir", {0}, 0, TİP_TAM},

    /* ========== DURUM SORGULAMA ========== */

    /* pencere.kapatildi_mi(pencere_id: tam) -> tam
     * Pencere kapatıldı mı? 1=evet, 0=hayır */
    {"kapat\xc4\xb1ld\xc4\xb1_m\xc4\xb1", "kapatildi_mi", "_tr_pencere_kapatildi_mi", {TİP_TAM}, 1, TİP_TAM},

    /* pencere.icerik_ayarla(pencere_id: tam, widget_id: tam) -> tam
     * Pencereye ana içerik widget'ını ekler */
    {"i\xc3\xa7erik_ayarla", "icerik_ayarla", "_tr_pencere_icerik_ayarla", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},
};

const ModülTanım pencere_modul = {
    "pencere", NULL,
    pencere_fonksiyonlar,
    sizeof(pencere_fonksiyonlar) / sizeof(pencere_fonksiyonlar[0])
};
