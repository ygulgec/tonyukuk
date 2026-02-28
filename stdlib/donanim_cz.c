/*
 * donanim_cz.c — Donanım Soyutlama Modülü (Runtime - Simülasyon)
 *
 * Tonyukuk Türkçe programlama dili - Desktop simülasyon implementasyonu
 *
 * Bu dosya x86_64 üzerinde test için simülasyon sağlar.
 * Gerçek donanım implementasyonları gomulu/calismazamani/ altındadır.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "runtime.h"

/* Simülasyon için pin durumları */
#define MAKS_PIN 64
static int pin_modlari[MAKS_PIN] = {0};
static int pin_degerleri[MAKS_PIN] = {0};
static int pwm_degerleri[MAKS_PIN] = {0};

/* Başlangıç zamanı */
static struct timespec baslangic_zamani = {0, 0};

static void zaman_baslat(void) {
    if (baslangic_zamani.tv_sec == 0 && baslangic_zamani.tv_nsec == 0) {
        clock_gettime(CLOCK_MONOTONIC, &baslangic_zamani);
    }
}

/* ========================================
 * GPIO Fonksiyonları
 * ======================================== */

void _tr_pin_modu(long long pin, long long mod) {
    if (pin < 0 || pin >= MAKS_PIN) return;
    pin_modlari[pin] = (int)mod;
    printf("[SIM] pin_modu(%lld, %lld) - %s\n", pin, mod,
           mod == 0 ? "GIRIS" : (mod == 1 ? "CIKIS" : "GIRIS_PULLUP"));
}

void _tr_dijital_yaz(long long pin, long long deger) {
    if (pin < 0 || pin >= MAKS_PIN) return;
    pin_degerleri[pin] = deger ? 1 : 0;
    printf("[SIM] dijital_yaz(%lld, %lld) - %s\n", pin, deger,
           deger ? "YUKSEK" : "DUSUK");
}

long long _tr_dijital_oku(long long pin) {
    if (pin < 0 || pin >= MAKS_PIN) return 0;
    printf("[SIM] dijital_oku(%lld) -> %d\n", pin, pin_degerleri[pin]);
    return pin_degerleri[pin];
}

/* ========================================
 * PWM Fonksiyonları
 * ======================================== */

void _tr_pwm_baslat(long long pin) {
    if (pin < 0 || pin >= MAKS_PIN) return;
    printf("[SIM] pwm_baslat(%lld)\n", pin);
}

void _tr_pwm_yaz(long long pin, long long deger) {
    if (pin < 0 || pin >= MAKS_PIN) return;
    int val = (deger > 255) ? 255 : ((deger < 0) ? 0 : (int)deger);
    pwm_degerleri[pin] = val;
    printf("[SIM] pwm_yaz(%lld, %d) - %%%d duty\n", pin, val, (val * 100) / 255);
}

/* ========================================
 * ADC Fonksiyonları
 * ======================================== */

long long _tr_analog_oku(long long pin) {
    /* Rastgele değer simülasyonu */
    int val = rand() % 1024;
    printf("[SIM] analog_oku(%lld) -> %d\n", pin, val);
    return val;
}

/* ========================================
 * I2C Fonksiyonları
 * ======================================== */

void _tr_i2c_baslat(long long hiz) {
    printf("[SIM] i2c_baslat(%lld kHz)\n", hiz);
}

long long _tr_i2c_yaz(long long adres, long long veri) {
    printf("[SIM] i2c_yaz(0x%02llX, 0x%02llX)\n", adres, veri);
    return 0;  /* Başarılı */
}

long long _tr_i2c_oku(long long adres) {
    int val = rand() % 256;
    printf("[SIM] i2c_oku(0x%02llX) -> 0x%02X\n", adres, val);
    return val;
}

/* ========================================
 * SPI Fonksiyonları
 * ======================================== */

void _tr_spi_baslat(long long hiz) {
    printf("[SIM] spi_baslat(%lld kHz)\n", hiz);
}

long long _tr_spi_aktar(long long veri) {
    int val = rand() % 256;
    printf("[SIM] spi_aktar(0x%02llX) -> 0x%02X\n", veri, val);
    return val;
}

/* ========================================
 * UART Fonksiyonları
 * ======================================== */

void _tr_seri_baslat(long long baud) {
    printf("[SIM] seri_baslat(%lld baud)\n", baud);
}

void _tr_seri_yaz_metin(const char *ptr, long long len) {
    printf("[SIM] seri_yaz: \"");
    for (long long i = 0; i < len; i++) {
        putchar(ptr[i]);
    }
    printf("\"\n");
}

long long _tr_seri_oku(void) {
    printf("[SIM] seri_oku() - bekleniyor...\n");
    return getchar();
}

long long _tr_seri_hazir_mi(void) {
    return 0;  /* Simülasyonda her zaman hazır değil */
}

/* ========================================
 * Zamanlama Fonksiyonları
 * ======================================== */

void _tr_bekle_ms(long long ms) {
    printf("[SIM] bekle_ms(%lld)\n", ms);
    usleep((unsigned int)(ms * 1000));
}

void _tr_bekle_us(long long us) {
    printf("[SIM] bekle_us(%lld)\n", us);
    usleep((unsigned int)us);
}

long long _tr_milis(void) {
    zaman_baslat();
    struct timespec simdi;
    clock_gettime(CLOCK_MONOTONIC, &simdi);
    long long ms = (simdi.tv_sec - baslangic_zamani.tv_sec) * 1000 +
                   (simdi.tv_nsec - baslangic_zamani.tv_nsec) / 1000000;
    return ms;
}

long long _tr_mikros(void) {
    zaman_baslat();
    struct timespec simdi;
    clock_gettime(CLOCK_MONOTONIC, &simdi);
    long long us = (simdi.tv_sec - baslangic_zamani.tv_sec) * 1000000 +
                   (simdi.tv_nsec - baslangic_zamani.tv_nsec) / 1000;
    return us;
}

/* ========================================
 * Kesme Fonksiyonları
 * ======================================== */

void _tr_kesme_ac(void) {
    printf("[SIM] kesme_ac() - kesmeler etkinleştirildi\n");
}

void _tr_kesme_kapat(void) {
    printf("[SIM] kesme_kapat() - kesmeler devre dışı\n");
}
