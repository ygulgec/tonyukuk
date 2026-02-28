/*
 * gomulu.h — Gömülü sistemler için ortak runtime header
 *
 * Tonyukuk Türkçe programlama dili - Arduino/ESP32/Pico desteği
 */

#ifndef GOMULU_CALISMAZAMANI_H
#define GOMULU_CALISMAZAMANI_H

#include <stdint.h>

/* ============================================================
 * Tip Tanımları
 * ============================================================ */

/* Gömülü sistemler için minimal string tipi */
typedef struct {
    const char *ptr;
    uint16_t len;
} GomMetin;

/* Gömülü sistemler için minimal dizi tipi */
typedef struct {
    int16_t *ptr;
    uint16_t count;
} GomDizi;

/* ============================================================
 * Havuz Bellek Yönetimi (Pool Allocator)
 * ============================================================ */

/* Varsayılan havuz boyutu - kart tanımında override edilebilir */
#ifndef GOMULU_HAVUZ_BOYUT
#define GOMULU_HAVUZ_BOYUT 256
#endif

/* Havuz bellek alanı */
extern uint8_t _gomulu_havuz[GOMULU_HAVUZ_BOYUT];
extern uint16_t _gomulu_havuz_ptr;

/* Bellek ayırma (basit bump allocator) */
void *_tr_gomulu_ayir(uint16_t boyut);

/* Havuzu sıfırla */
void _tr_gomulu_sifirla(void);

/* ============================================================
 * GPIO Sabitleri (Türkçe)
 * ============================================================ */

#define GIRIS           0
#define CIKIS           1
#define GIRIS_PULLUP    2

#define DUSUK           0
#define YUKSEK          1

/* ============================================================
 * Donanım Fonksiyon Bildirimleri
 * ============================================================ */

/* GPIO */
void _tr_pin_modu(int16_t pin, int16_t mod);
void _tr_dijital_yaz(int16_t pin, int16_t deger);
int16_t _tr_dijital_oku(int16_t pin);

/* PWM */
void _tr_pwm_baslat(int16_t pin);
void _tr_pwm_yaz(int16_t pin, int16_t deger);

/* ADC */
int16_t _tr_analog_oku(int16_t pin);

/* I2C */
void _tr_i2c_baslat(int16_t hiz);
int16_t _tr_i2c_yaz(int16_t adres, int16_t veri);
int16_t _tr_i2c_oku(int16_t adres);

/* SPI */
void _tr_spi_baslat(int16_t hiz);
int16_t _tr_spi_aktar(int16_t veri);

/* UART */
void _tr_seri_baslat(int16_t baud);
void _tr_seri_yaz_byte(int16_t veri);
void _tr_seri_yaz_metin(const char *ptr, uint16_t len);
int16_t _tr_seri_oku(void);
int16_t _tr_seri_hazir_mi(void);

/* Zamanlama */
void _tr_bekle_ms(int16_t ms);
void _tr_bekle_us(int16_t us);
int32_t _tr_milis(void);
int32_t _tr_mikros(void);

/* ============================================================
 * Kesme (Interrupt) Yönetimi
 * ============================================================ */

void _tr_kesme_ac(void);
void _tr_kesme_kapat(void);

#endif /* GOMULU_CALISMAZAMANI_H */
