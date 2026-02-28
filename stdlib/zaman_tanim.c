/* Zaman modülü — derleme zamanı tanımları
 * Tonyukuk Programlama Dili
 * Kapsamlı tarih/saat işlemleri
 */
#include "../src/modul.h"

static const ModülFonksiyon zaman_fonksiyonlar[] = {

    /* ========== TEMEL ZAMAN ========== */

    /* şimdi() -> tam: Unix timestamp saniye */
    {"\xc5\x9fimdi", "simdi", "_tr_simdi", {0}, 0, TİP_TAM},

    /* şimdi_ms() -> tam: Unix timestamp milisaniye */
    {"\xc5\x9fimdi_ms", "simdi_ms", "_tr_simdi_ms", {0}, 0, TİP_TAM},

    /* şimdi_us() -> tam: Unix timestamp mikrosaniye */
    {"\xc5\x9fimdi_us", "simdi_us", "_tr_simdi_us", {0}, 0, TİP_TAM},

    /* şimdi_ns() -> tam: Unix timestamp nanosaniye */
    {"\xc5\x9fimdi_ns", "simdi_ns", "_tr_simdi_ns", {0}, 0, TİP_TAM},

    /* ========== MEVCUT ZAMAN BİLEŞENLERİ ========== */

    /* saat() -> tam */
    {"saat", NULL, "_tr_saat", {0}, 0, TİP_TAM},

    /* dakika() -> tam */
    {"dakika", NULL, "_tr_dakika", {0}, 0, TİP_TAM},

    /* saniye() -> tam */
    {"saniye", NULL, "_tr_saniye", {0}, 0, TİP_TAM},

    /* gün() -> tam */
    {"g\xc3\xbcn", "gun", "_tr_gun", {0}, 0, TİP_TAM},

    /* ay() -> tam */
    {"ay", NULL, "_tr_ay", {0}, 0, TİP_TAM},

    /* yıl() -> tam */
    {"y\xc4\xb1l", "yil", "_tr_yil", {0}, 0, TİP_TAM},

    /* hafta_gunu() -> tam: 1=Pazartesi, 7=Pazar */
    {"hafta_g\xc3\xbcn\xc3\xbc", "hafta_gunu", "_tr_hafta_gunu", {0}, 0, TİP_TAM},

    /* yilin_gunu() -> tam: 1-366 */
    {"y\xc4\xb1l\xc4\xb1n_g\xc3\xbcn\xc3\xbc", "yilin_gunu", "_tr_yilin_gunu", {0}, 0, TİP_TAM},

    /* hafta_numarasi() -> tam: ISO hafta 1-53 */
    {"hafta_numaras\xc4\xb1", "hafta_numarasi", "_tr_hafta_numarasi", {0}, 0, TİP_TAM},

    /* ========== TIMESTAMP BİLEŞENLERİ ========== */

    /* zaman_saat(ts: tam) -> tam */
    {"zaman_saat", NULL, "_tr_zaman_saat", {TİP_TAM}, 1, TİP_TAM},

    /* zaman_dakika(ts: tam) -> tam */
    {"zaman_dakika", NULL, "_tr_zaman_dakika", {TİP_TAM}, 1, TİP_TAM},

    /* zaman_saniye(ts: tam) -> tam */
    {"zaman_saniye", NULL, "_tr_zaman_saniye", {TİP_TAM}, 1, TİP_TAM},

    /* zaman_gün(ts: tam) -> tam */
    {"zaman_g\xc3\xbcn", "zaman_gun", "_tr_zaman_gun", {TİP_TAM}, 1, TİP_TAM},

    /* zaman_ay(ts: tam) -> tam */
    {"zaman_ay", NULL, "_tr_zaman_ay", {TİP_TAM}, 1, TİP_TAM},

    /* zaman_yıl(ts: tam) -> tam */
    {"zaman_y\xc4\xb1l", "zaman_yil", "_tr_zaman_yil", {TİP_TAM}, 1, TİP_TAM},

    /* zaman_hafta_gunu(ts: tam) -> tam */
    {"zaman_hafta_g\xc3\xbcn\xc3\xbc", "zaman_hafta_gunu", "_tr_zaman_hafta_gunu", {TİP_TAM}, 1, TİP_TAM},

    /* ========== TARİH OLUŞTURMA ========== */

    /* tarih_olustur(yıl, ay, gün) -> tam */
    {"tarih_olu\xc5\x9ftur", "tarih_olustur", "_tr_tarih_olustur", {TİP_TAM, TİP_TAM, TİP_TAM}, 3, TİP_TAM},

    /* zaman_olustur(yıl, ay, gün, saat, dakika, saniye) -> tam */
    {"zaman_olu\xc5\x9ftur", "zaman_olustur", "_tr_zaman_olustur", {TİP_TAM, TİP_TAM, TİP_TAM, TİP_TAM, TİP_TAM, TİP_TAM}, 6, TİP_TAM},

    /* ========== TARİH ARİTMETİĞİ ========== */

    /* gun_ekle(ts, gün_sayısı) -> tam */
    {"g\xc3\xbcn_ekle", "gun_ekle", "_tr_gun_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* saat_ekle(ts, saat_sayısı) -> tam */
    {"saat_ekle", NULL, "_tr_saat_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* dakika_ekle(ts, dakika_sayısı) -> tam */
    {"dakika_ekle", NULL, "_tr_dakika_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* ay_ekle(ts, ay_sayısı) -> tam */
    {"ay_ekle", NULL, "_tr_ay_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* yil_ekle(ts, yıl_sayısı) -> tam */
    {"y\xc4\xb1l_ekle", "yil_ekle", "_tr_yil_ekle", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* gun_farki(ts1, ts2) -> tam */
    {"g\xc3\xbcn_fark\xc4\xb1", "gun_farki", "_tr_gun_farki", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* saniye_farki(ts1, ts2) -> tam */
    {"saniye_fark\xc4\xb1", "saniye_farki", "_tr_saniye_farki", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* ========== BİÇİMLENDİRME ========== */

    /* tarih_metin() -> metin: Mevcut zamanı formatlı döndür */
    {"tarih_metin", NULL, "_tr_tarih_metin", {0}, 0, TİP_METİN},

    /* zaman_bicimle(ts, format) -> metin */
    {"zaman_bi\xc3\xa7imle", "zaman_bicimle", "_tr_zaman_bicimle", {TİP_TAM, TİP_METİN}, 2, TİP_METİN},

    /* tarih_cozumle(metin, format) -> tam */
    {"tarih_\xc3\xa7\xc3\xb6z\xc3\xbcmle", "tarih_cozumle", "_tr_tarih_cozumle", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* ========== ISO 8601 ========== */

    /* iso_tarih(ts) -> metin: "2024-01-15" */
    {"iso_tarih", NULL, "_tr_iso_tarih", {TİP_TAM}, 1, TİP_METİN},

    /* iso_zaman(ts) -> metin: "2024-01-15T10:30:45" */
    {"iso_zaman", NULL, "_tr_iso_zaman", {TİP_TAM}, 1, TİP_METİN},

    /* ========== UTC ========== */

    /* utc_simdi() -> tam */
    {"utc_\xc5\x9fimdi", "utc_simdi", "_tr_utc_simdi", {0}, 0, TİP_TAM},

    /* utc_saat(ts) -> tam */
    {"utc_saat", NULL, "_tr_utc_saat", {TİP_TAM}, 1, TİP_TAM},

    /* utc_zaman_bicimle(ts, format) -> metin */
    {"utc_zaman_bi\xc3\xa7imle", "utc_zaman_bicimle", "_tr_utc_zaman_bicimle", {TİP_TAM, TİP_METİN}, 2, TİP_METİN},

    /* ========== KONTROLLER ========== */

    /* artik_yil_mi(yıl) -> mantık */
    {"art\xc4\xb1k_y\xc4\xb1l_m\xc4\xb1", "artik_yil_mi", "_tr_artik_yil_mi", {TİP_TAM}, 1, TİP_MANTIK},

    /* aydaki_gun_sayisi(yıl, ay) -> tam */
    {"aydaki_g\xc3\xbcn_say\xc4\xb1s\xc4\xb1", "aydaki_gun_sayisi", "_tr_aydaki_gun_sayisi", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* ========== TÜRKÇE İSİMLER ========== */

    /* hafta_gunu_adi(gün: 1-7) -> metin */
    {"hafta_g\xc3\xbcn\xc3\xbc_ad\xc4\xb1", "hafta_gunu_adi", "_tr_hafta_gunu_adi", {TİP_TAM}, 1, TİP_METİN},

    /* ay_adi(ay: 1-12) -> metin */
    {"ay_ad\xc4\xb1", "ay_adi", "_tr_ay_adi", {TİP_TAM}, 1, TİP_METİN},

    /* zaman_hafta_gunu_adi(ts) -> metin */
    {"zaman_hafta_g\xc3\xbcn\xc3\xbc_ad\xc4\xb1", "zaman_hafta_gunu_adi", "_tr_zaman_hafta_gunu_adi", {TİP_TAM}, 1, TİP_METİN},

    /* zaman_ay_adi(ts) -> metin */
    {"zaman_ay_ad\xc4\xb1", "zaman_ay_adi", "_tr_zaman_ay_adi", {TİP_TAM}, 1, TİP_METİN},

    /* ========== PERFORMANS ÖLÇÜMÜ ========== */

    /* kronometre_baslat() -> tam (nanosaniye başlangıç) */
    {"kronometre_ba\xc5\x9flat", "kronometre_baslat", "_tr_kronometre_baslat", {0}, 0, TİP_TAM},

    /* kronometre_gecen(başlangıç) -> tam (nanosaniye) */
    {"kronometre_ge\xc3\xa7en", "kronometre_gecen", "_tr_kronometre_gecen", {TİP_TAM}, 1, TİP_TAM},

    /* kronometre_gecen_ms(başlangıç) -> tam (milisaniye) */
    {"kronometre_ge\xc3\xa7en_ms", "kronometre_gecen_ms", "_tr_kronometre_gecen_ms", {TİP_TAM}, 1, TİP_TAM},

    /* kronometre_gecen_us(başlangıç) -> tam (mikrosaniye) */
    {"kronometre_ge\xc3\xa7en_us", "kronometre_gecen_us", "_tr_kronometre_gecen_us", {TİP_TAM}, 1, TİP_TAM},

    /* ========== UYUMA ========== */

    /* bekle_saniye(saniye) -> boşluk */
    {"bekle_saniye", NULL, "_tr_bekle_saniye", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* bekle_ms(milisaniye) -> boşluk */
    {"bekle_ms", NULL, "_tr_bekle_ms", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* bekle_us(mikrosaniye) -> boşluk */
    {"bekle_us", NULL, "_tr_bekle_us", {TİP_TAM}, 1, TİP_BOŞLUK},
};

const ModülTanım zaman_modul = {
    "zaman", NULL,
    zaman_fonksiyonlar,
    sizeof(zaman_fonksiyonlar) / sizeof(zaman_fonksiyonlar[0])
};
