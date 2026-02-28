/* Düzeni (regex) modülü — derleme zamanı tanımları
 * Tonyukuk Programlama Dili
 * Kapsamlı düzenli ifade desteği
 */
#include "../src/modul.h"

static const ModülFonksiyon duzeni_fonksiyonlar[] = {

    /* ========== TEMEL EŞLEŞME ========== */

    /* eşleşir_mi(metin, desen) -> mantık */
    {"e\xc5\x9fle\xc5\x9fir_mi", "eslesir_mi", "_tr_eslesir_mi", {TİP_METİN, TİP_METİN}, 2, TİP_MANTIK},

    /* eşleşir_mi_bk(metin, desen, büyük_küçük_duyarsız) -> mantık */
    {"e\xc5\x9fle\xc5\x9fir_mi_bk", "eslesir_mi_bk", "_tr_eslesir_mi_bk", {TİP_METİN, TİP_METİN, TİP_MANTIK}, 3, TİP_MANTIK},

    /* eşleşme_sayısı(metin, desen) -> tam */
    {"e\xc5\x9fle\xc5\x9fme_say\xc4\xb1s\xc4\xb1", "eslesme_sayisi", "_tr_eslesme_sayisi", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* ========== EŞLEŞME BULMA ========== */

    /* eşleşme_bul(metin, desen) -> metin (ilk eşleşme) */
    {"e\xc5\x9fle\xc5\x9fme_bul", "eslesme_bul", "_tr_eslesme_bul", {TİP_METİN, TİP_METİN}, 2, TİP_METİN},

    /* tüm_eşleşmeler(metin, desen) -> dizi<metin> */
    {"t\xc3\xbcm_e\xc5\x9fle\xc5\x9fmeler", "tum_eslesmeler", "_tr_tum_eslesmeler", {TİP_METİN, TİP_METİN}, 2, TİP_DİZİ},

    /* ========== EŞLEŞME KONUMU ========== */

    /* eşleşme_konumu(metin, desen) -> tam (-1 bulunamadı) */
    {"e\xc5\x9fle\xc5\x9fme_konumu", "eslesme_konumu", "_tr_eslesme_konumu", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* eşleşme_sonu(metin, desen) -> tam (-1 bulunamadı) */
    {"e\xc5\x9fle\xc5\x9fme_sonu", "eslesme_sonu", "_tr_eslesme_sonu", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* ========== DEĞİŞTİRME ========== */

    /* değiştir(metin, desen, yeni) -> metin (ilk eşleşmeyi değiştir) */
    {"de\xc4\x9fi\xc5\x9ftir", "degistir", "_tr_duzeni_degistir", {TİP_METİN, TİP_METİN, TİP_METİN}, 3, TİP_METİN},

    /* değiştir_hepsi(metin, desen, yeni) -> metin (tüm eşleşmeleri değiştir) */
    {"de\xc4\x9fi\xc5\x9ftir_hepsi", "degistir_hepsi", "_tr_degistir_hepsi", {TİP_METİN, TİP_METİN, TİP_METİN}, 3, TİP_METİN},

    /* ========== AYIRMA ========== */

    /* böl(metin, desen) -> dizi<metin> */
    {"b\xc3\xb6l", "bol", "_tr_duzeni_bol", {TİP_METİN, TİP_METİN}, 2, TİP_DİZİ},

    /* ========== YAKALAMA GRUPLARI ========== */

    /* gruplar(metin, desen) -> dizi<metin> (yakalama grupları) */
    {"gruplar", NULL, "_tr_gruplar", {TİP_METİN, TİP_METİN}, 2, TİP_DİZİ},

    /* grup_al(metin, desen, grup_no) -> metin */
    {"grup_al", NULL, "_tr_grup_al", {TİP_METİN, TİP_METİN, TİP_TAM}, 3, TİP_METİN},

    /* ========== DESEN DOĞRULAMA ========== */

    /* geçerli_desen_mi(desen) -> mantık */
    {"ge\xc3\xa7erli_desen_mi", "gecerli_desen_mi", "_tr_gecerli_desen_mi", {TİP_METİN}, 1, TİP_MANTIK},

    /* ========== ÖZEL EŞLEŞMELER ========== */

    /* başlangıçta_eşleşir_mi(metin, desen) -> mantık */
    {"ba\xc5\x9flang\xc4\xb1\xc3\xa7ta_e\xc5\x9fle\xc5\x9fir_mi", "baslangicta_eslesir_mi", "_tr_baslangicta_eslesir_mi", {TİP_METİN, TİP_METİN}, 2, TİP_MANTIK},

    /* sonunda_eşleşir_mi(metin, desen) -> mantık */
    {"sonunda_e\xc5\x9fle\xc5\x9fir_mi", "sonunda_eslesir_mi", "_tr_sonunda_eslesir_mi", {TİP_METİN, TİP_METİN}, 2, TİP_MANTIK},

    /* tam_eşleşir_mi(metin, desen) -> mantık (tüm metin eşleşmeli) */
    {"tam_e\xc5\x9fle\xc5\x9fir_mi", "tam_eslesir_mi", "_tr_tam_eslesir_mi", {TİP_METİN, TİP_METİN}, 2, TİP_MANTIK},

    /* ========== ÖZEL KARAKTERLER ========== */

    /* kaçış_karakterleri(metin) -> metin */
    {"ka\xc3\xa7\xc4\xb1\xc5\x9f_karakterleri", "kacis_karakterleri", "_tr_kacis_karakterleri", {TİP_METİN}, 1, TİP_METİN},
};

const ModülTanım duzeni_modul = {
    "d\xc3\xbczeni", "duzeni",
    duzeni_fonksiyonlar,
    sizeof(duzeni_fonksiyonlar) / sizeof(duzeni_fonksiyonlar[0])
};
