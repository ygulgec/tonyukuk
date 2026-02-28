/* Arayüz modülü — derleme zamanı tanımları
 * Tonyukuk Programlama Dili
 * GTK3 widget'ları (düğme, etiket, giriş, kutu, sekmeler vb.)
 */
#include "../src/modul.h"

static const ModülFonksiyon arayuz_fonksiyonlar[] = {
    /* ========== KONTEYNERLER ========== */

    /* arayuz.kutu_yatay() -> tam — Yatay GtkBox */
    {"kutu_yatay", NULL, "_tr_arayuz_kutu_yatay", {0}, 0, TİP_TAM},

    /* arayuz.kutu_dikey() -> tam — Dikey GtkBox */
    {"kutu_dikey", NULL, "_tr_arayuz_kutu_dikey", {0}, 0, TİP_TAM},

    /* arayuz.kaydirma() -> tam — GtkScrolledWindow */
    {"kayd\xc4\xb1rma", "kaydirma", "_tr_arayuz_kaydirma", {0}, 0, TİP_TAM},

    /* ========== TEMEL WİDGET'LAR ========== */

    /* arayuz.dugme(etiket: metin) -> tam — GtkButton */
    {"d\xc3\xbc\xc4\x9fme", "dugme", "_tr_arayuz_dugme", {TİP_METİN}, 1, TİP_TAM},

    /* arayuz.etiket(metin: metin) -> tam — GtkLabel */
    {"etiket", NULL, "_tr_arayuz_etiket", {TİP_METİN}, 1, TİP_TAM},

    /* arayuz.giris() -> tam — GtkEntry (metin girişi) */
    {"giri\xc5\x9f", "giris", "_tr_arayuz_giris", {0}, 0, TİP_TAM},

    /* arayuz.ayirici() -> tam — GtkSeparator */
    {"ay\xc4\xb1r\xc4\xb1c\xc4\xb1", "ayirici", "_tr_arayuz_ayirici", {0}, 0, TİP_TAM},

    /* arayuz.resim_dosyadan(yol: metin) -> tam — GtkImage */
    {"resim_dosyadan", NULL, "_tr_arayuz_resim_dosyadan", {TİP_METİN}, 1, TİP_TAM},

    /* ========== GİRİŞ ALANI İŞLEMLERİ ========== */

    /* arayuz.giris_metni_al(giris_id: tam) -> metin */
    {"giri\xc5\x9f_metni_al", "giris_metni_al", "_tr_arayuz_giris_metni_al", {TİP_TAM}, 1, TİP_METİN},

    /* arayuz.giris_metni_ayarla(giris_id: tam, metin: metin) -> tam */
    {"giri\xc5\x9f_metni_ayarla", "giris_metni_ayarla", "_tr_arayuz_giris_metni_ayarla", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* arayuz.giris_ipucu_ayarla(giris_id: tam, ipucu: metin) -> tam */
    {"giri\xc5\x9f_ipucu_ayarla", "giris_ipucu_ayarla", "_tr_arayuz_giris_ipucu_ayarla", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* ========== ETİKET İŞLEMLERİ ========== */

    /* arayuz.etiket_metni_ayarla(etiket_id: tam, metin: metin) -> tam */
    {"etiket_metni_ayarla", NULL, "_tr_arayuz_etiket_metni_ayarla", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* arayuz.etiket_metni_al(etiket_id: tam) -> metin */
    {"etiket_metni_al", NULL, "_tr_arayuz_etiket_metni_al", {TİP_TAM}, 1, TİP_METİN},

    /* ========== SEKME YÖNETİMİ (GtkNotebook) ========== */

    /* arayuz.sekmeler() -> tam — GtkNotebook oluştur */
    {"sekmeler", NULL, "_tr_arayuz_sekmeler", {0}, 0, TİP_TAM},

    /* arayuz.sekme_ekle(notebook_id: tam, icerik_id: tam, baslik: metin) -> tam */
    {"sekme_ekle", NULL, "_tr_arayuz_sekme_ekle", {TİP_TAM, TİP_TAM, TİP_METİN}, 3, TİP_TAM},

    /* arayuz.sekme_kapatmali_ekle(notebook_id: tam, icerik_id: tam, baslik: metin) -> tam */
    {"sekme_kapatmal\xc4\xb1_ekle", "sekme_kapatmali_ekle", "_tr_arayuz_sekme_kapatmali_ekle",
     {TİP_TAM, TİP_TAM, TİP_METİN}, 3, TİP_TAM},

    /* arayuz.sekme_kaldir(notebook_id: tam, indeks: tam) -> tam */
    {"sekme_kald\xc4\xb1r", "sekme_kaldir", "_tr_arayuz_sekme_kaldir", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* arayuz.sekme_secili(notebook_id: tam) -> tam — Seçili sekme indeksi */
    {"sekme_se\xc3\xa7ili", "sekme_secili", "_tr_arayuz_sekme_secili", {TİP_TAM}, 1, TİP_TAM},

    /* arayuz.sekme_sec(notebook_id: tam, indeks: tam) -> tam */
    {"sekme_se\xc3\xa7", "sekme_sec", "_tr_arayuz_sekme_sec", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* arayuz.sekme_baslik_ayarla(notebook_id: tam, indeks: tam, baslik: metin) -> tam */
    {"sekme_ba\xc5\x9fl\xc4\xb1k_ayarla", "sekme_baslik_ayarla", "_tr_arayuz_sekme_baslik_ayarla",
     {TİP_TAM, TİP_TAM, TİP_METİN}, 3, TİP_TAM},

    /* arayuz.sekme_sayisi(notebook_id: tam) -> tam */
    {"sekme_say\xc4\xb1s\xc4\xb1", "sekme_sayisi", "_tr_arayuz_sekme_sayisi", {TİP_TAM}, 1, TİP_TAM},

    /* arayuz.sekme_kapandi_mi(notebook_id: tam) -> tam — Kapatılan sekme indeksi veya -1 */
    {"sekme_kapand\xc4\xb1_m\xc4\xb1", "sekme_kapandi_mi", "_tr_arayuz_sekme_kapandi_mi", {TİP_TAM}, 1, TİP_TAM},

    /* ========== YERLEŞIM ========== */

    /* arayuz.ekle(konteyner_id: tam, widget_id: tam) -> tam */
    {"ekle", NULL, "_tr_arayuz_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* arayuz.pakle(kutu_id: tam, widget_id: tam, genisle: tam, doldur: tam, bosluk: tam) -> tam */
    {"pakle", NULL, "_tr_arayuz_pakle", {TİP_TAM, TİP_TAM, TİP_TAM, TİP_TAM, TİP_TAM}, 5, TİP_TAM},

    /* arayuz.goster_tumu(widget_id: tam) -> tam — Widget ve çocuklarını göster */
    {"g\xc3\xb6ster_t\xc3\xbcm\xc3\xbc", "goster_tumu", "_tr_arayuz_goster_tumu", {TİP_TAM}, 1, TİP_TAM},

    /* ========== OLAY YOKLAMA (Polling) ========== */

    /* arayuz.dugme_basildi_mi(dugme_id: tam) -> tam — 1=basıldı, 0=hayır */
    {"d\xc3\xbc\xc4\x9fme_bas\xc4\xb1ld\xc4\xb1_m\xc4\xb1", "dugme_basildi_mi", "_tr_arayuz_dugme_basildi_mi", {TİP_TAM}, 1, TİP_TAM},

    /* arayuz.giris_enter_mi(giris_id: tam) -> tam — Enter basıldı mı? */
    {"giri\xc5\x9f_enter_mi", "giris_enter_mi", "_tr_arayuz_giris_enter_mi", {TİP_TAM}, 1, TİP_TAM},

    /* arayuz.sekme_degisti_mi(notebook_id: tam) -> tam — Sekme değişti mi? */
    {"sekme_de\xc4\x9fi\xc5\x9fti_mi", "sekme_degisti_mi", "_tr_arayuz_sekme_degisti_mi", {TİP_TAM}, 1, TİP_TAM},

    /* ========== CSS TEMA ========== */

    /* arayuz.css_yukle(css: metin) -> tam — CSS string'i uygula */
    {"css_y\xc3\xbckle", "css_yukle", "_tr_arayuz_css_yukle", {TİP_METİN}, 1, TİP_TAM},

    /* ========== WİDGET ÖZELLİKLERİ ========== */

    /* arayuz.css_sinif_ekle(widget_id: tam, sinif: metin) -> tam */
    {"css_s\xc4\xb1n\xc4\xb1f_ekle", "css_sinif_ekle", "_tr_arayuz_css_sinif_ekle", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* arayuz.css_sinif_kaldir(widget_id: tam, sinif: metin) -> tam */
    {"css_s\xc4\xb1n\xc4\xb1f_kald\xc4\xb1r", "css_sinif_kaldir", "_tr_arayuz_css_sinif_kaldir", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* arayuz.widget_genislik_ayarla(widget_id: tam, genislik: tam) -> tam */
    {"widget_geni\xc5\x9flik_ayarla", "widget_genislik_ayarla", "_tr_arayuz_widget_genislik_ayarla", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* arayuz.widget_yukseklik_ayarla(widget_id: tam, yukseklik: tam) -> tam */
    {"widget_y\xc3\xbckseklik_ayarla", "widget_yukseklik_ayarla", "_tr_arayuz_widget_yukseklik_ayarla", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* ========== İLERLEME ÇUBUĞU ========== */

    /* arayuz.ilerleme_cubugu() -> tam — GtkProgressBar */
    {"ilerleme_\xc3\xa7ubu\xc4\x9fu", "ilerleme_cubugu", "_tr_arayuz_ilerleme_cubugu", {0}, 0, TİP_TAM},

    /* arayuz.ilerleme_ayarla(id: tam, deger: tam) -> tam — 0-100 */
    {"ilerleme_ayarla", NULL, "_tr_arayuz_ilerleme_ayarla", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* ========== İPUCU ========== */

    /* arayuz.ipucu_ayarla(widget_id: tam, metin: metin) -> tam */
    {"ipucu_ayarla", NULL, "_tr_arayuz_ipucu_ayarla", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},

    /* ========== GÖRÜNÜRLÜK ========== */

    /* arayuz.widget_gizle(widget_id: tam) -> tam */
    {"widget_gizle", NULL, "_tr_arayuz_widget_gizle", {TİP_TAM}, 1, TİP_TAM},

    /* arayuz.widget_goster(widget_id: tam) -> tam */
    {"widget_g\xc3\xb6ster", "widget_goster", "_tr_arayuz_widget_goster", {TİP_TAM}, 1, TİP_TAM},

    /* ========== DÜĞME İŞLEMLERİ ========== */

    /* arayuz.dugme_etiket_ayarla(dugme_id: tam, metin: metin) -> tam */
    {"d\xc3\xbc\xc4\x9fme_etiket_ayarla", "dugme_etiket_ayarla", "_tr_arayuz_dugme_etiket_ayarla", {TİP_TAM, TİP_METİN}, 2, TİP_TAM},
};

const ModülTanım arayuz_modul = {
    "grafik", NULL,
    arayuz_fonksiyonlar,
    sizeof(arayuz_fonksiyonlar) / sizeof(arayuz_fonksiyonlar[0])
};
