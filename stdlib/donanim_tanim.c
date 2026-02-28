/*
 * donanim_tanim.c — Donanım Soyutlama Modülü (Derleme Zamanı Tanımları)
 *
 * Tonyukuk Türkçe programlama dili - Arduino/ESP32/Pico desteği
 *
 * Bu modül GPIO, PWM, ADC, I2C, SPI, UART ve zamanlama fonksiyonlarını
 * Türkçe API ile sunar.
 */

#include "../src/modul.h"

static const ModülFonksiyon donanim_fonksiyonlar[] = {
    /* ========================================
     * GPIO Fonksiyonları
     * ======================================== */

    /* pin_modu(pin: tam, mod: tam) -> boşluk
     * Pin modunu ayarla: GIRIS=0, CIKIS=1, GIRIS_PULLUP=2 */
    {"pin_modu", NULL, "_tr_pin_modu", {TİP_TAM, TİP_TAM}, 2, TİP_BOŞLUK},

    /* dijital_yaz(pin: tam, deger: tam) -> boşluk
     * Pin'e dijital değer yaz: DUSUK=0, YUKSEK=1 */
    {"dijital_yaz", NULL, "_tr_dijital_yaz", {TİP_TAM, TİP_TAM}, 2, TİP_BOŞLUK},

    /* dijital_oku(pin: tam) -> tam
     * Pin'den dijital değer oku (0 veya 1) */
    {"dijital_oku", NULL, "_tr_dijital_oku", {TİP_TAM}, 1, TİP_TAM},

    /* ========================================
     * PWM Fonksiyonları
     * ======================================== */

    /* pwm_baslat(pin: tam) -> boşluk
     * PWM kanalını başlat */
    {"pwm_baslat", NULL, "_tr_pwm_baslat", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* pwm_yaz(pin: tam, deger: tam) -> boşluk
     * PWM duty cycle yaz (0-255) */
    {"pwm_yaz", NULL, "_tr_pwm_yaz", {TİP_TAM, TİP_TAM}, 2, TİP_BOŞLUK},

    /* ========================================
     * ADC (Analog-Dijital Dönüştürücü)
     * ======================================== */

    /* analog_oku(pin: tam) -> tam
     * Analog değer oku (0-1023 arası, 10-bit) */
    {"analog_oku", NULL, "_tr_analog_oku", {TİP_TAM}, 1, TİP_TAM},

    /* ========================================
     * I2C Fonksiyonları
     * ======================================== */

    /* i2c_baslat(hiz: tam) -> boşluk
     * I2C başlat (hiz: 100=100kHz, 400=400kHz) */
    {"i2c_baslat", NULL, "_tr_i2c_baslat", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* i2c_yaz(adres: tam, veri: tam) -> tam
     * I2C cihazına veri yaz, başarı için 0 döndürür */
    {"i2c_yaz", NULL, "_tr_i2c_yaz", {TİP_TAM, TİP_TAM}, 2, TİP_TAM},

    /* i2c_oku(adres: tam) -> tam
     * I2C cihazından veri oku */
    {"i2c_oku", NULL, "_tr_i2c_oku", {TİP_TAM}, 1, TİP_TAM},

    /* ========================================
     * SPI Fonksiyonları
     * ======================================== */

    /* spi_baslat(hiz: tam) -> boşluk
     * SPI başlat (hiz: kHz cinsinden) */
    {"spi_baslat", NULL, "_tr_spi_baslat", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* spi_aktar(veri: tam) -> tam
     * SPI üzerinden veri gönder ve al (full-duplex) */
    {"spi_aktar", NULL, "_tr_spi_aktar", {TİP_TAM}, 1, TİP_TAM},

    /* ========================================
     * UART (Seri Port) Fonksiyonları
     * ======================================== */

    /* seri_baslat(baud: tam) -> boşluk
     * Seri portu başlat (örn: 9600, 115200) */
    {"seri_baslat", NULL, "_tr_seri_baslat", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* seri_yaz(metin: metin) -> boşluk
     * Seri porta metin yaz */
    {"seri_yaz", NULL, "_tr_seri_yaz_metin", {TİP_METİN}, 1, TİP_BOŞLUK},

    /* seri_oku() -> tam
     * Seri porttan bir byte oku (bloke eder) */
    {"seri_oku", NULL, "_tr_seri_oku", {0}, 0, TİP_TAM},

    /* seri_hazir_mi() -> tam
     * Okunacak veri var mı kontrol et */
    {"seri_hazir_mi", "seri_hazir_mi", "_tr_seri_hazir_mi", {0}, 0, TİP_TAM},

    /* ========================================
     * Zamanlama Fonksiyonları
     * ======================================== */

    /* bekle_ms(ms: tam) -> boşluk
     * Belirtilen milisaniye kadar bekle */
    {"bekle_ms", NULL, "_tr_bekle_ms", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* bekle_us(us: tam) -> boşluk
     * Belirtilen mikrosaniye kadar bekle */
    {"bekle_us", NULL, "_tr_bekle_us", {TİP_TAM}, 1, TİP_BOŞLUK},

    /* milis() -> tam
     * Program başlangıcından bu yana geçen milisaniye */
    {"milis", NULL, "_tr_milis", {0}, 0, TİP_TAM},

    /* mikros() -> tam
     * Program başlangıcından bu yana geçen mikrosaniye */
    {"mikros", NULL, "_tr_mikros", {0}, 0, TİP_TAM},

    /* ========================================
     * Kesme (Interrupt) Fonksiyonları
     * ======================================== */

    /* kesme_ac() -> boşluk
     * Global kesmeleri etkinleştir */
    {"kesme_ac", "kesme_ac", "_tr_kesme_ac", {0}, 0, TİP_BOŞLUK},

    /* kesme_kapat() -> boşluk
     * Global kesmeleri devre dışı bırak */
    {"kesme_kapat", NULL, "_tr_kesme_kapat", {0}, 0, TİP_BOŞLUK},
};

const ModülTanım donanim_modul = {
    "donan\xc4\xb1m",   /* donanım (UTF-8) */
    "donanim",          /* ASCII alternatif */
    donanim_fonksiyonlar,
    sizeof(donanim_fonksiyonlar) / sizeof(donanim_fonksiyonlar[0])
};
