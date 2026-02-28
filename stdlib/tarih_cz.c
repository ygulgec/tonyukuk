/* Tarih modülü — çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * Basit tarih/saat erişim fonksiyonları
 */
#define _POSIX_C_SOURCE 200809L

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { char *ptr; long long len; } TrMetin;

/* Türkçe gün isimleri (0=Pazar, 1=Pazartesi, ..., 6=Cumartesi) */
static const char *tarih_gun_isimleri[] = {
    "Pazar", "Pazartesi", "Sal\xc4\xb1", "\xc3\x87" "ar\xc5\x9f" "amba",
    "Per\xc5\x9f" "embe", "Cuma", "Cumartesi"
};

/* Türkçe ay isimleri (0=Ocak, ..., 11=Aralık) */
static const char *tarih_ay_isimleri[] = {
    "Ocak", "\xc5\x9e" "ubat", "Mart", "Nisan", "May\xc4\xb1s", "Haziran",
    "Temmuz", "A\xc4\x9f" "ustos", "Eyl\xc3\xbc" "l", "Ekim", "Kas\xc4\xb1m", "Aral\xc4\xb1k"
};

/* tarih_simdi() -> metin: "2026-02-07 13:45:30" formatında */
TrMetin _tr_tarih_simdi(void) {
    TrMetin m = {NULL, 0};
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    char buf[64];
    int len = (int)strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", l);
    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, buf, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* tarih_gun() -> tam: gün numarası (1-31) */
long long _tr_tarih_gun(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_mday;
}

/* tarih_ay() -> tam: ay numarası (1-12) */
long long _tr_tarih_ay(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_mon + 1;
}

/* tarih_yil() -> tam: yıl */
long long _tr_tarih_yil(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_year + 1900;
}

/* tarih_saat() -> tam: saat (0-23) */
long long _tr_tarih_saat(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_hour;
}

/* tarih_dakika() -> tam: dakika (0-59) */
long long _tr_tarih_dakika(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_min;
}

/* tarih_saniye() -> tam: saniye (0-59) */
long long _tr_tarih_saniye(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_sec;
}

/* tarih_damga() -> tam: Unix timestamp (epoch saniye) */
long long _tr_tarih_damga(void) {
    return (long long)time(NULL);
}

/* tarih_gun_adi() -> metin: Türkçe gün adı */
TrMetin _tr_tarih_gun_adi(void) {
    TrMetin m = {NULL, 0};
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    const char *isim = tarih_gun_isimleri[l->tm_wday];
    int len = (int)strlen(isim);
    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, isim, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* tarih_ay_adi() -> metin: Türkçe ay adı */
TrMetin _tr_tarih_ay_adi(void) {
    TrMetin m = {NULL, 0};
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    const char *isim = tarih_ay_isimleri[l->tm_mon];
    int len = (int)strlen(isim);
    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, isim, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}
