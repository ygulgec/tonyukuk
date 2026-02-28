/*
 * esp32_devkit.h — ESP32 DevKit V1 Kart Tanımı
 *
 * Tonyukuk Türkçe programlama dili - Gömülü sistemler desteği
 */

#ifndef KART_ESP32_DEVKIT_H
#define KART_ESP32_DEVKIT_H

/* ============================================================
 * Kart Bilgileri
 * ============================================================ */

#define KART_ADI            "ESP32 DevKit V1"
#define KART_MCU            "esp32"
#define KART_SAAT_HZ        240000000UL
#define KART_FLASH_BOYUT    (4 * 1024 * 1024)
#define KART_RAM_BOYUT      (520 * 1024)
#define KART_PSRAM_BOYUT    0

/* Gömülü havuz boyutu */
#define GOMULU_HAVUZ_BOYUT  4096

/* ============================================================
 * Pin Tanımları
 * ============================================================ */

/* GPIO Pinleri */
#define PIN_GPIO0       0       /* Boot mode select, strapping pin */
#define PIN_GPIO1       1       /* TX0 (UART) */
#define PIN_GPIO2       2       /* Dahili LED (bazı kartlarda) */
#define PIN_GPIO3       3       /* RX0 (UART) */
#define PIN_GPIO4       4       /* GPIO */
#define PIN_GPIO5       5       /* GPIO, strapping pin */
#define PIN_GPIO12      12      /* GPIO, strapping pin */
#define PIN_GPIO13      13      /* GPIO */
#define PIN_GPIO14      14      /* GPIO */
#define PIN_GPIO15      15      /* GPIO, strapping pin */
#define PIN_GPIO16      16      /* GPIO */
#define PIN_GPIO17      17      /* GPIO */
#define PIN_GPIO18      18      /* GPIO, SPI CLK */
#define PIN_GPIO19      19      /* GPIO, SPI MISO */
#define PIN_GPIO21      21      /* GPIO, I2C SDA */
#define PIN_GPIO22      22      /* GPIO, I2C SCL */
#define PIN_GPIO23      23      /* GPIO, SPI MOSI */
#define PIN_GPIO25      25      /* GPIO, DAC1 */
#define PIN_GPIO26      26      /* GPIO, DAC2 */
#define PIN_GPIO27      27      /* GPIO */
#define PIN_GPIO32      32      /* GPIO, ADC1_CH4 */
#define PIN_GPIO33      33      /* GPIO, ADC1_CH5 */
#define PIN_GPIO34      34      /* Input only, ADC1_CH6 */
#define PIN_GPIO35      35      /* Input only, ADC1_CH7 */
#define PIN_GPIO36      36      /* Input only, VP, ADC1_CH0 */
#define PIN_GPIO39      39      /* Input only, VN, ADC1_CH3 */

/* ============================================================
 * Türkçe Pin Takma Adları
 * ============================================================ */

#define DAHILI_LED      2       /* GPIO2 - Dahili LED */

/* I2C Pinleri */
#define I2C_SDA         21
#define I2C_SCL         22

/* SPI Pinleri */
#define SPI_MOSI        23
#define SPI_MISO        19
#define SPI_SCK         18
#define SPI_SS          5

/* UART Pinleri */
#define UART_TX         1
#define UART_RX         3

/* ============================================================
 * Sabitler (Türkçe)
 * ============================================================ */

#define GIRIS           0
#define CIKIS           1
#define GIRIS_PULLUP    2
#define GIRIS_PULLDOWN  3

#define DUSUK           0
#define YUKSEK          1

/* ============================================================
 * ADC Ayarları
 * ============================================================ */

#define ADC_KANAL_SAYISI    18
#define ADC_COZUNURLUK      12      /* 12-bit ADC: 0-4095 */
#define ADC_REFERANS_MV     3300    /* 3.3V referans */

/* ADC1 Kanalları (GPIO ile eşleşme) */
#define ADC1_CH0        36      /* VP */
#define ADC1_CH3        39      /* VN */
#define ADC1_CH4        32
#define ADC1_CH5        33
#define ADC1_CH6        34
#define ADC1_CH7        35

/* ============================================================
 * PWM (LEDC) Ayarları
 * ============================================================ */

#define PWM_KANAL_SAYISI    16
#define PWM_COZUNURLUK      8       /* 8-bit: 0-255 */
#define PWM_FREKANS         5000    /* 5 kHz */

/* ============================================================
 * WiFi Ayarları
 * ============================================================ */

#define WIFI_MAKS_SSID_UZUNLUK      32
#define WIFI_MAKS_SIFRE_UZUNLUK     64

/* ============================================================
 * Bluetooth Ayarları
 * ============================================================ */

#define BT_DESTEKLENIYOR    1
#define BLE_DESTEKLENIYOR   1

#endif /* KART_ESP32_DEVKIT_H */
