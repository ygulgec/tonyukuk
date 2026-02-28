/*
 * pico_rp2040.h — Raspberry Pi Pico (RP2040) Kart Tanımı
 *
 * Tonyukuk Türkçe programlama dili - Gömülü sistemler desteği
 */

#ifndef KART_PICO_RP2040_H
#define KART_PICO_RP2040_H

/* ============================================================
 * Kart Bilgileri
 * ============================================================ */

#define KART_ADI            "Raspberry Pi Pico"
#define KART_MCU            "rp2040"
#define KART_SAAT_HZ        133000000UL
#define KART_FLASH_BOYUT    (2 * 1024 * 1024)
#define KART_RAM_BOYUT      (264 * 1024)

/* Gömülü havuz boyutu */
#define GOMULU_HAVUZ_BOYUT  4096

/* Çekirdek sayısı */
#define CEKIRDEK_SAYISI     2

/* ============================================================
 * Pin Tanımları
 * ============================================================ */

/* GPIO Pinleri (0-29) */
#define PIN_GPIO0       0
#define PIN_GPIO1       1
#define PIN_GPIO2       2
#define PIN_GPIO3       3
#define PIN_GPIO4       4
#define PIN_GPIO5       5
#define PIN_GPIO6       6
#define PIN_GPIO7       7
#define PIN_GPIO8       8
#define PIN_GPIO9       9
#define PIN_GPIO10      10
#define PIN_GPIO11      11
#define PIN_GPIO12      12
#define PIN_GPIO13      13
#define PIN_GPIO14      14
#define PIN_GPIO15      15
#define PIN_GPIO16      16
#define PIN_GPIO17      17
#define PIN_GPIO18      18
#define PIN_GPIO19      19
#define PIN_GPIO20      20
#define PIN_GPIO21      21
#define PIN_GPIO22      22
#define PIN_GPIO23      23      /* SMPS mode kontrol */
#define PIN_GPIO24      24      /* VBUS sense */
#define PIN_GPIO25      25      /* Dahili LED */
#define PIN_GPIO26      26      /* ADC0 */
#define PIN_GPIO27      27      /* ADC1 */
#define PIN_GPIO28      28      /* ADC2 */
#define PIN_GPIO29      29      /* ADC3 (VSYS/3) */

/* ============================================================
 * Türkçe Pin Takma Adları
 * ============================================================ */

#define DAHILI_LED      25      /* GPIO25 - Dahili LED */

/* I2C0 Varsayılan Pinleri */
#define I2C0_SDA        4
#define I2C0_SCL        5

/* I2C1 Varsayılan Pinleri */
#define I2C1_SDA        6
#define I2C1_SCL        7

/* SPI0 Varsayılan Pinleri */
#define SPI0_RX         16      /* MISO */
#define SPI0_TX         19      /* MOSI */
#define SPI0_SCK        18
#define SPI0_CSN        17

/* SPI1 Varsayılan Pinleri */
#define SPI1_RX         12      /* MISO */
#define SPI1_TX         15      /* MOSI */
#define SPI1_SCK        14
#define SPI1_CSN        13

/* UART0 Varsayılan Pinleri */
#define UART0_TX        0
#define UART0_RX        1

/* UART1 Varsayılan Pinleri */
#define UART1_TX        4
#define UART1_RX        5

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

#define ADC_KANAL_SAYISI    4       /* ADC0-3 (GPIO26-29) */
#define ADC_COZUNURLUK      12      /* 12-bit ADC: 0-4095 */
#define ADC_REFERANS_MV     3300    /* 3.3V referans */

/* ADC Pin Eşleşmeleri */
#define ADC_PIN_ADC0    26
#define ADC_PIN_ADC1    27
#define ADC_PIN_ADC2    28
#define ADC_PIN_ADC3    29          /* VSYS/3 */

/* ============================================================
 * PWM Ayarları
 * ============================================================ */

#define PWM_KANAL_SAYISI    16      /* 8 slice x 2 kanal */
#define PWM_COZUNURLUK      16      /* 16-bit: 0-65535 */
#define PWM_VARSAYILAN_WRAP 255     /* 8-bit uyumluluk için */

/* ============================================================
 * PIO (Programmable I/O) Ayarları
 * ============================================================ */

#define PIO_SAYISI          2       /* PIO0, PIO1 */
#define PIO_SM_SAYISI       4       /* Her PIO'da 4 state machine */

/* ============================================================
 * USB Ayarları
 * ============================================================ */

#define USB_DESTEKLENIYOR   1

#endif /* KART_PICO_RP2040_H */
