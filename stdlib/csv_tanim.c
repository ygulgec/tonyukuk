/* CSV modülü — derleme zamanı tanımları */
#include "../src/modul.h"

static const ModülFonksiyon csv_fonksiyonlar[] = {
    /* csv_oku(dosya: metin) -> dizi: CSV dosyasını oku, 2D dizi döndür */
    {"csv_oku", NULL, "_tr_csv_oku", {TİP_METİN}, 1, TİP_DİZİ},

    /* csv_ayır(satır: metin) -> dizi: tek CSV satırını alanlarına ayır */
    {"csv_ay\xc4\xb1r", "csv_ayir", "_tr_csv_ayir", {TİP_METİN}, 1, TİP_DİZİ},

    /* csv_ayır_özel(satır: metin, ayırıcı: tam) -> dizi */
    {"csv_ay\xc4\xb1r_\xc3\xb6zel", "csv_ayir_ozel", "_tr_csv_ayir_ozel",
     {TİP_METİN, TİP_TAM}, 2, TİP_DİZİ},

    /* csv_satır_oluştur(alanlar: dizi) -> metin: diziden CSV satırı oluştur */
    {"csv_sat\xc4\xb1r_olu\xc5\x9ftur", "csv_satir_olustur", "_tr_csv_satir_olustur",
     {TİP_DİZİ}, 1, TİP_METİN},

    /* csv_yaz(dosya: metin, veri: dizi) -> tam: 2D diziyi CSV dosyasına yaz */
    {"csv_yaz", NULL, "_tr_csv_yaz", {TİP_METİN, TİP_DİZİ}, 2, TİP_TAM},

    /* csv_oku_özel(dosya: metin, ayırıcı: tam) -> dizi: özel ayırıcıyla oku */
    {"csv_oku_\xc3\xb6zel", "csv_oku_ozel", "_tr_csv_oku_ozel",
     {TİP_METİN, TİP_TAM}, 2, TİP_DİZİ},

    /* csv_satır_al(veri: dizi, satır: tam) -> dizi: belirli satırı al */
    {"csv_sat\xc4\xb1r_al", "csv_satir_al", "_tr_csv_satir_al",
     {TİP_DİZİ, TİP_TAM}, 2, TİP_DİZİ},

    /* csv_alan_al(veri: dizi, satır: tam, alan: tam) -> metin */
    {"csv_alan_al", NULL, "_tr_csv_alan_al",
     {TİP_DİZİ, TİP_TAM, TİP_TAM}, 3, TİP_METİN},

    /* csv_satır_sayısı(veri: dizi) -> tam */
    {"csv_sat\xc4\xb1r_say\xc4\xb1s\xc4\xb1", "csv_satir_sayisi", "_tr_csv_satir_sayisi",
     {TİP_DİZİ}, 1, TİP_TAM},

    /* csv_alan_sayısı(veri: dizi, satır: tam) -> tam */
    {"csv_alan_say\xc4\xb1s\xc4\xb1", "csv_alan_sayisi", "_tr_csv_alan_sayisi",
     {TİP_DİZİ, TİP_TAM}, 2, TİP_TAM},
};

const ModülTanım csv_modul = {
    "csv", NULL,
    csv_fonksiyonlar,
    sizeof(csv_fonksiyonlar) / sizeof(csv_fonksiyonlar[0])
};
