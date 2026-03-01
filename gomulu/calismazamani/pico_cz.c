/*
 * pico_cz.c — Raspberry Pi Pico (RP2040) Runtime Implementation
 *
 * Tonyukuk Türkçe programlama dili - Pico donanım kütüphanesi
 * Hedef: RP2040 (Pico SDK)
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "hardware/timer.h"
#include <stdint.h>
#include <string.h>

#include "gomulu.h"

/* Kart header'ı: IDE proje yapısında kartlar/ altında,
 * standart konumda ../kartlar/ altında olabilir */
#if __has_include("kartlar/pico_rp2040.h")
#include "kartlar/pico_rp2040.h"
#elif __has_include("../kartlar/pico_rp2040.h")
#include "../kartlar/pico_rp2040.h"
#else
/* Varsayılan sabitler */
#define GOMULU_HAVUZ_BOYUT  4096
#define I2C0_SDA  4
#define I2C0_SCL  5
#define SPI0_RX   16
#define SPI0_TX   19
#define SPI0_SCK  18
#define SPI0_CSN  17
#define UART0_TX  0
#define UART0_RX  1
#define PWM_VARSAYILAN_WRAP 255
#endif

/* ============================================================
 * Havuz Bellek Yönetimi
 * ============================================================ */

uint8_t _gomulu_havuz[GOMULU_HAVUZ_BOYUT];
uint16_t _gomulu_havuz_ptr = 0;

void *_tr_gomulu_ayir(uint16_t boyut) {
    if (_gomulu_havuz_ptr + boyut > GOMULU_HAVUZ_BOYUT) {
        return (void *)0;
    }
    void *ptr = &_gomulu_havuz[_gomulu_havuz_ptr];
    _gomulu_havuz_ptr += boyut;
    return ptr;
}

void _tr_gomulu_sifirla(void) {
    _gomulu_havuz_ptr = 0;
}

/* ============================================================
 * GPIO Fonksiyonları
 * ============================================================ */

void _tr_pin_modu(int16_t pin, int16_t mod) {
    if (pin < 0 || pin > 29) return;

    gpio_init(pin);

    if (mod == CIKIS) {
        gpio_set_dir(pin, GPIO_OUT);
    } else if (mod == GIRIS_PULLUP) {
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    } else if (mod == GIRIS_PULLDOWN) {
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_down(pin);
    } else {
        gpio_set_dir(pin, GPIO_IN);
        gpio_disable_pulls(pin);
    }
}

void _tr_dijital_yaz(int16_t pin, int16_t deger) {
    if (pin < 0 || pin > 29) return;
    gpio_put(pin, deger ? 1 : 0);
}

int16_t _tr_dijital_oku(int16_t pin) {
    if (pin < 0 || pin > 29) return 0;
    return (int16_t)gpio_get(pin);
}

/* ============================================================
 * PWM Fonksiyonları
 * ============================================================ */

void _tr_pwm_baslat(int16_t pin) {
    if (pin < 0 || pin > 29) return;

    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_wrap(slice, PWM_VARSAYILAN_WRAP);
    pwm_set_enabled(slice, true);
}

void _tr_pwm_yaz(int16_t pin, int16_t deger) {
    if (pin < 0 || pin > 29) return;

    uint16_t duty = (deger > 255) ? 255 : ((deger < 0) ? 0 : (uint16_t)deger);
    pwm_set_gpio_level(pin, duty);
}

/* ============================================================
 * ADC Fonksiyonları
 * ============================================================ */

static int adc_ilk_kez = 1;

int16_t _tr_analog_oku(int16_t pin) {
    /* Pico ADC pinleri: GPIO26=ADC0, GPIO27=ADC1, GPIO28=ADC2, GPIO29=ADC3 */
    int kanal;
    if (pin >= 26 && pin <= 29) {
        kanal = pin - 26;
    } else if (pin >= 0 && pin <= 3) {
        kanal = pin;  /* Doğrudan kanal numarası */
    } else {
        return 0;
    }

    if (adc_ilk_kez) {
        adc_init();
        adc_ilk_kez = 0;
    }

    /* Pin'i ADC girişi olarak ayarla */
    if (kanal <= 3) {
        adc_gpio_init(26 + kanal);
    }

    adc_select_input(kanal);
    return (int16_t)adc_read();  /* 12-bit: 0-4095 */
}

/* ============================================================
 * I2C Fonksiyonları
 * ============================================================ */

static int i2c_baslatildi = 0;

void _tr_i2c_baslat(int16_t hiz) {
    if (i2c_baslatildi) return;

    uint baudrate = (hiz >= 400) ? 400000 : 100000;
    i2c_init(i2c0, baudrate);

    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);

    i2c_baslatildi = 1;
}

int16_t _tr_i2c_yaz(int16_t adres, int16_t veri) {
    uint8_t data = (uint8_t)veri;
    int ret = i2c_write_blocking(i2c0, (uint8_t)adres, &data, 1, false);
    return (ret == 1) ? 0 : -1;
}

int16_t _tr_i2c_oku(int16_t adres) {
    uint8_t data = 0;
    int ret = i2c_read_blocking(i2c0, (uint8_t)adres, &data, 1, false);
    return (ret == 1) ? (int16_t)data : -1;
}

/* ============================================================
 * SPI Fonksiyonları
 * ============================================================ */

static int spi_baslatildi = 0;

void _tr_spi_baslat(int16_t hiz) {
    if (spi_baslatildi) return;

    uint baudrate = (uint)(hiz * 1000);
    spi_init(spi0, baudrate);

    gpio_set_function(SPI0_RX, GPIO_FUNC_SPI);    /* MISO */
    gpio_set_function(SPI0_TX, GPIO_FUNC_SPI);    /* MOSI */
    gpio_set_function(SPI0_SCK, GPIO_FUNC_SPI);   /* SCK */

    /* CS pin'i GPIO olarak kullan (yazılım kontrolü) */
    gpio_init(SPI0_CSN);
    gpio_set_dir(SPI0_CSN, GPIO_OUT);
    gpio_put(SPI0_CSN, 1);

    spi_baslatildi = 1;
}

int16_t _tr_spi_aktar(int16_t veri) {
    uint8_t tx = (uint8_t)veri;
    uint8_t rx = 0;

    gpio_put(SPI0_CSN, 0);
    spi_write_read_blocking(spi0, &tx, &rx, 1);
    gpio_put(SPI0_CSN, 1);

    return (int16_t)rx;
}

/* ============================================================
 * UART Fonksiyonları
 * ============================================================ */

static int uart_baslatildi = 0;

void _tr_seri_baslat(int16_t baud) {
    if (uart_baslatildi) return;

    uart_init(uart0, (uint)baud);
    gpio_set_function(UART0_TX, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX, GPIO_FUNC_UART);

    uart_baslatildi = 1;
}

void _tr_seri_yaz_byte(int16_t veri) {
    char c = (char)veri;
    uart_write_blocking(uart0, (const uint8_t *)&c, 1);
}

void _tr_seri_yaz_metin(const char *ptr, uint16_t len) {
    uart_write_blocking(uart0, (const uint8_t *)ptr, len);
}

int16_t _tr_seri_oku(void) {
    if (uart_is_readable(uart0)) {
        return (int16_t)uart_getc(uart0);
    }
    return -1;
}

int16_t _tr_seri_hazir_mi(void) {
    return uart_is_readable(uart0) ? 1 : 0;
}

/* ============================================================
 * Zamanlama Fonksiyonları
 * ============================================================ */

void _tr_bekle_ms(int16_t ms) {
    sleep_ms((uint32_t)ms);
}

void _tr_bekle_us(int16_t us) {
    sleep_us((uint64_t)us);
}

int32_t _tr_milis(void) {
    return (int32_t)(to_ms_since_boot(get_absolute_time()));
}

int32_t _tr_mikros(void) {
    return (int32_t)(to_us_since_boot(get_absolute_time()));
}

/* ============================================================
 * Kesme Yönetimi
 * ============================================================ */

void _tr_kesme_ac(void) {
    /* Pico'da otomatik yönetilir */
}

void _tr_kesme_kapat(void) {
    /* Pico'da otomatik yönetilir */
}

/* ============================================================
 * Sistem Başlatma
 * ============================================================ */

static void _gomulu_sistem_baslat(void) {
    stdio_init_all();
}

/* Ana fonksiyon - derleyici tarafından çağrılır */
extern int16_t ana(void);

int main(void) {
    _gomulu_sistem_baslat();
    ana();

    while (1) {
        tight_loop_contents();
    }
    return 0;
}
