/*
 * arduino_uno.h — Arduino UNO (ATmega328P) Kart Tanımı
 *
 * Tonyukuk Türkçe programlama dili - Gömülü sistemler desteği
 */

#ifndef KART_ARDUINO_UNO_H
#define KART_ARDUINO_UNO_H

/* ============================================================
 * Kart Bilgileri
 * ============================================================ */

#define KART_ADI            "Arduino UNO"
#define KART_MCU            "atmega328p"
#define KART_SAAT_HZ        16000000UL
#define KART_FLASH_BOYUT    32768
#define KART_RAM_BOYUT      2048
#define KART_EEPROM_BOYUT   1024

/* Gömülü havuz boyutu (RAM kısıtlı) */
#define GOMULU_HAVUZ_BOYUT  256

/* ============================================================
 * Pin Mapping - Arduino -> AVR
 * ============================================================ */

/* Dijital Pinler */
#define PIN_D0      0       /* PD0 - RX */
#define PIN_D1      1       /* PD1 - TX */
#define PIN_D2      2       /* PD2 */
#define PIN_D3      3       /* PD3 - PWM */
#define PIN_D4      4       /* PD4 */
#define PIN_D5      5       /* PD5 - PWM */
#define PIN_D6      6       /* PD6 - PWM */
#define PIN_D7      7       /* PD7 */
#define PIN_D8      8       /* PB0 */
#define PIN_D9      9       /* PB1 - PWM */
#define PIN_D10     10      /* PB2 - PWM, SS */
#define PIN_D11     11      /* PB3 - PWM, MOSI */
#define PIN_D12     12      /* PB4 - MISO */
#define PIN_D13     13      /* PB5 - SCK, LED */

/* Analog Pinler */
#define PIN_A0      14      /* PC0 - ADC0 */
#define PIN_A1      15      /* PC1 - ADC1 */
#define PIN_A2      16      /* PC2 - ADC2 */
#define PIN_A3      17      /* PC3 - ADC3 */
#define PIN_A4      18      /* PC4 - ADC4, SDA */
#define PIN_A5      19      /* PC5 - ADC5, SCL */

/* ============================================================
 * Türkçe Pin Takma Adları
 * ============================================================ */

#define DAHILI_LED      PIN_D13

/* I2C Pinleri */
#define I2C_SDA         PIN_A4
#define I2C_SCL         PIN_A5

/* SPI Pinleri */
#define SPI_MOSI        PIN_D11
#define SPI_MISO        PIN_D12
#define SPI_SCK         PIN_D13
#define SPI_SS          PIN_D10

/* UART Pinleri */
#define UART_RX         PIN_D0
#define UART_TX         PIN_D1

/* ============================================================
 * PWM Kanalları
 * ============================================================ */

#define PWM_KANAL_SAYISI    6
/* PWM destekleyen pinler: 3, 5, 6, 9, 10, 11 */

/* ============================================================
 * ADC Ayarları
 * ============================================================ */

#define ADC_KANAL_SAYISI    6
#define ADC_COZUNURLUK      10      /* 10-bit ADC: 0-1023 */
#define ADC_REFERANS_MV     5000    /* 5V referans */

/* ============================================================
 * Timer Ayarları
 * ============================================================ */

#define TIMER0_PRESCALER    64
#define TIMER1_PRESCALER    64
#define TIMER2_PRESCALER    64

/* ============================================================
 * AVR Register Adresleri (I/O Map)
 * ============================================================ */

/* GPIO Port Adresleri */
#define PORTB_ADDR      0x25
#define DDRB_ADDR       0x24
#define PINB_ADDR       0x23

#define PORTC_ADDR      0x28
#define DDRC_ADDR       0x27
#define PINC_ADDR       0x26

#define PORTD_ADDR      0x2B
#define DDRD_ADDR       0x2A
#define PIND_ADDR       0x29

/* ============================================================
 * Pin -> Port/Bit Mapping Tabloları
 * ============================================================ */

/* Port seçimi: 0=PORTD, 1=PORTB, 2=PORTC */
static const uint8_t pin_port_tablo[] = {
    0, 0, 0, 0, 0, 0, 0, 0,     /* D0-D7:  PORTD */
    1, 1, 1, 1, 1, 1,           /* D8-D13: PORTB */
    2, 2, 2, 2, 2, 2            /* A0-A5:  PORTC */
};

/* Bit numarası (0-7) */
static const uint8_t pin_bit_tablo[] = {
    0, 1, 2, 3, 4, 5, 6, 7,     /* D0-D7 */
    0, 1, 2, 3, 4, 5,           /* D8-D13 */
    0, 1, 2, 3, 4, 5            /* A0-A5 */
};

/* PWM timer kanalı (-1 = PWM yok) */
static const int8_t pin_pwm_timer[] = {
    -1, -1, -1, 2, -1, 0, 0, -1,  /* D0-D7: D3=T2, D5=T0, D6=T0 */
     1,  1,  1,  2, -1, -1,       /* D8-D13: D9=T1, D10=T1, D11=T2 */
    -1, -1, -1, -1, -1, -1        /* A0-A5: PWM yok */
};

#endif /* KART_ARDUINO_UNO_H */
