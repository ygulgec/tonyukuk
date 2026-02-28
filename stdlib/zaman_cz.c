/* Zaman modülü — çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * Kapsamlı tarih/saat işlemleri
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

typedef struct { char *ptr; long long len; } TrMetin;

/* ========== TEMEL ZAMAN FONKSİYONLARI ========== */

/* şimdi() -> tam: Unix timestamp (saniye) */
long long _tr_simdi(void) {
    return (long long)time(NULL);
}

/* şimdi_ms() -> tam: Unix timestamp milisaniye */
long long _tr_simdi_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + (long long)tv.tv_usec / 1000LL;
}

/* şimdi_us() -> tam: Unix timestamp mikrosaniye */
long long _tr_simdi_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + (long long)tv.tv_usec;
}

/* şimdi_ns() -> tam: Unix timestamp nanosaniye */
long long _tr_simdi_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

/* ========== MEVCUT ZAMAN BİLEŞENLERİ ========== */

/* saat() -> tam */
long long _tr_saat(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_hour;
}

/* dakika() -> tam */
long long _tr_dakika(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_min;
}

/* saniye() -> tam */
long long _tr_saniye(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_sec;
}

/* gün() -> tam */
long long _tr_gun(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_mday;
}

/* ay() -> tam */
long long _tr_ay(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_mon + 1;
}

/* yıl() -> tam */
long long _tr_yil(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_year + 1900;
}

/* hafta_gunu() -> tam: 1=Pazartesi, 7=Pazar */
long long _tr_hafta_gunu(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_wday == 0 ? 7 : l->tm_wday;
}

/* yilin_gunu() -> tam: 1-366 */
long long _tr_yilin_gunu(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    return l->tm_yday + 1;
}

/* hafta_numarasi() -> tam: ISO hafta numarası (1-53) */
long long _tr_hafta_numarasi(void) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    char buf[4];
    strftime(buf, sizeof(buf), "%V", l);
    return atoll(buf);
}

/* ========== TIMESTAMP BİLEŞENLERİ ========== */

/* zaman_saat(timestamp) -> tam */
long long _tr_zaman_saat(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    return l ? l->tm_hour : 0;
}

/* zaman_dakika(timestamp) -> tam */
long long _tr_zaman_dakika(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    return l ? l->tm_min : 0;
}

/* zaman_saniye(timestamp) -> tam */
long long _tr_zaman_saniye(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    return l ? l->tm_sec : 0;
}

/* zaman_gun(timestamp) -> tam */
long long _tr_zaman_gun(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    return l ? l->tm_mday : 0;
}

/* zaman_ay(timestamp) -> tam */
long long _tr_zaman_ay(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    return l ? l->tm_mon + 1 : 0;
}

/* zaman_yil(timestamp) -> tam */
long long _tr_zaman_yil(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    return l ? l->tm_year + 1900 : 0;
}

/* zaman_hafta_gunu(timestamp) -> tam */
long long _tr_zaman_hafta_gunu(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    if (!l) return 0;
    return l->tm_wday == 0 ? 7 : l->tm_wday;
}

/* ========== TARİH OLUŞTURMA ========== */

/* tarih_olustur(yıl, ay, gün) -> tam (timestamp) */
long long _tr_tarih_olustur(long long yil, long long ay, long long gun) {
    struct tm t = {0};
    t.tm_year = (int)yil - 1900;
    t.tm_mon = (int)ay - 1;
    t.tm_mday = (int)gun;
    t.tm_isdst = -1;
    return (long long)mktime(&t);
}

/* zaman_olustur(yıl, ay, gün, saat, dakika, saniye) -> tam (timestamp) */
long long _tr_zaman_olustur(long long yil, long long ay, long long gun,
                            long long saat, long long dakika, long long saniye) {
    struct tm t = {0};
    t.tm_year = (int)yil - 1900;
    t.tm_mon = (int)ay - 1;
    t.tm_mday = (int)gun;
    t.tm_hour = (int)saat;
    t.tm_min = (int)dakika;
    t.tm_sec = (int)saniye;
    t.tm_isdst = -1;
    return (long long)mktime(&t);
}

/* ========== TARİH ARİTMETİĞİ ========== */

/* gun_ekle(timestamp, gün_sayısı) -> tam */
long long _tr_gun_ekle(long long ts, long long gun) {
    return ts + gun * 86400;
}

/* saat_ekle(timestamp, saat_sayısı) -> tam */
long long _tr_saat_ekle(long long ts, long long saat) {
    return ts + saat * 3600;
}

/* dakika_ekle(timestamp, dakika_sayısı) -> tam */
long long _tr_dakika_ekle(long long ts, long long dakika) {
    return ts + dakika * 60;
}

/* ay_ekle(timestamp, ay_sayısı) -> tam */
long long _tr_ay_ekle(long long ts, long long ay) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    if (!l) return ts;

    struct tm yeni = *l;
    yeni.tm_mon += (int)ay;
    /* Normalize */
    while (yeni.tm_mon >= 12) { yeni.tm_mon -= 12; yeni.tm_year++; }
    while (yeni.tm_mon < 0) { yeni.tm_mon += 12; yeni.tm_year--; }
    yeni.tm_isdst = -1;
    return (long long)mktime(&yeni);
}

/* yil_ekle(timestamp, yıl_sayısı) -> tam */
long long _tr_yil_ekle(long long ts, long long yil) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    if (!l) return ts;

    struct tm yeni = *l;
    yeni.tm_year += (int)yil;
    yeni.tm_isdst = -1;
    return (long long)mktime(&yeni);
}

/* gun_farki(ts1, ts2) -> tam: ts2 - ts1 gün cinsinden */
long long _tr_gun_farki(long long ts1, long long ts2) {
    return (ts2 - ts1) / 86400;
}

/* saniye_farki(ts1, ts2) -> tam */
long long _tr_saniye_farki(long long ts1, long long ts2) {
    return ts2 - ts1;
}

/* ========== BİÇİMLENDİRME ========== */

/* tarih_metin() -> metin: Mevcut zamanı formatlı döndür */
TrMetin _tr_tarih_metin(void) {
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

/* zaman_bicimle(timestamp, format) -> metin */
TrMetin _tr_zaman_bicimle(long long ts, const char *fmt_ptr, long long fmt_len) {
    TrMetin m = {NULL, 0};
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    if (!l) return m;

    /* Format stringi null-terminate et */
    char *fmt = (char *)malloc(fmt_len + 1);
    if (!fmt) return m;
    memcpy(fmt, fmt_ptr, fmt_len);
    fmt[fmt_len] = '\0';

    char buf[256];
    int len = (int)strftime(buf, sizeof(buf), fmt, l);
    free(fmt);

    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, buf, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* tarih_cozumle(metin, format) -> tam (timestamp) */
long long _tr_tarih_cozumle(const char *str_ptr, long long str_len,
                            const char *fmt_ptr, long long fmt_len) {
    /* String ve formatı null-terminate et */
    char *str = (char *)malloc(str_len + 1);
    char *fmt = (char *)malloc(fmt_len + 1);
    if (!str || !fmt) {
        if (str) free(str);
        if (fmt) free(fmt);
        return 0;
    }
    memcpy(str, str_ptr, str_len);
    str[str_len] = '\0';
    memcpy(fmt, fmt_ptr, fmt_len);
    fmt[fmt_len] = '\0';

    struct tm t = {0};
    t.tm_isdst = -1;
    char *ret = strptime(str, fmt, &t);

    free(str);
    free(fmt);

    if (!ret) return 0;
    return (long long)mktime(&t);
}

/* ========== ISO 8601 ========== */

/* iso_tarih(timestamp) -> metin: "2024-01-15" */
TrMetin _tr_iso_tarih(long long ts) {
    TrMetin m = {NULL, 0};
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    if (!l) return m;

    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                       l->tm_year + 1900, l->tm_mon + 1, l->tm_mday);

    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, buf, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* iso_zaman(timestamp) -> metin: "2024-01-15T10:30:45" */
TrMetin _tr_iso_zaman(long long ts) {
    TrMetin m = {NULL, 0};
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    if (!l) return m;

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                       l->tm_year + 1900, l->tm_mon + 1, l->tm_mday,
                       l->tm_hour, l->tm_min, l->tm_sec);

    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, buf, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* ========== UTC ZAMAN ========== */

/* utc_simdi() -> tam */
long long _tr_utc_simdi(void) {
    return (long long)time(NULL);
}

/* utc_saat(timestamp) -> tam */
long long _tr_utc_saat(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = gmtime(&t);
    return l ? l->tm_hour : 0;
}

/* utc_zaman_bicimle(timestamp, format) -> metin */
TrMetin _tr_utc_zaman_bicimle(long long ts, const char *fmt_ptr, long long fmt_len) {
    TrMetin m = {NULL, 0};
    time_t t = (time_t)ts;
    struct tm *l = gmtime(&t);
    if (!l) return m;

    char *fmt = (char *)malloc(fmt_len + 1);
    if (!fmt) return m;
    memcpy(fmt, fmt_ptr, fmt_len);
    fmt[fmt_len] = '\0';

    char buf[256];
    int len = (int)strftime(buf, sizeof(buf), fmt, l);
    free(fmt);

    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, buf, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* ========== KONTROLLER ========== */

/* artik_yil_mi(yıl) -> mantık */
long long _tr_artik_yil_mi(long long yil) {
    if (yil % 400 == 0) return 1;
    if (yil % 100 == 0) return 0;
    if (yil % 4 == 0) return 1;
    return 0;
}

/* aydaki_gun_sayisi(yıl, ay) -> tam */
long long _tr_aydaki_gun_sayisi(long long yil, long long ay) {
    static const int gunler[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (ay < 1 || ay > 12) return 0;
    if (ay == 2 && _tr_artik_yil_mi(yil)) return 29;
    return gunler[ay - 1];
}

/* ========== TÜRKÇE İSİMLER ========== */

static const char *gun_isimleri[] = {
    "Pazar", "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi"
};

static const char *ay_isimleri[] = {
    "Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran",
    "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık"
};

/* hafta_gunu_adi(gün: 1-7) -> metin */
TrMetin _tr_hafta_gunu_adi(long long gun) {
    TrMetin m = {NULL, 0};
    if (gun < 1 || gun > 7) return m;

    /* 1=Pazartesi ... 7=Pazar map to tm_wday (0=Pazar) */
    int idx = (gun == 7) ? 0 : (int)gun;
    const char *isim = gun_isimleri[idx];
    int len = (int)strlen(isim);

    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, isim, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* ay_adi(ay: 1-12) -> metin */
TrMetin _tr_ay_adi(long long ay) {
    TrMetin m = {NULL, 0};
    if (ay < 1 || ay > 12) return m;

    const char *isim = ay_isimleri[ay - 1];
    int len = (int)strlen(isim);

    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, isim, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* zaman_hafta_gunu_adi(timestamp) -> metin */
TrMetin _tr_zaman_hafta_gunu_adi(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    if (!l) {
        TrMetin m = {NULL, 0};
        return m;
    }
    return _tr_hafta_gunu_adi(l->tm_wday == 0 ? 7 : l->tm_wday);
}

/* zaman_ay_adi(timestamp) -> metin */
TrMetin _tr_zaman_ay_adi(long long ts) {
    time_t t = (time_t)ts;
    struct tm *l = localtime(&t);
    if (!l) {
        TrMetin m = {NULL, 0};
        return m;
    }
    return _tr_ay_adi(l->tm_mon + 1);
}

/* ========== PERFORMANS ÖLÇÜMÜ ========== */

/* kronometre_baslat() -> tam (nanosaniye başlangıç) */
long long _tr_kronometre_baslat(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

/* kronometre_gecen(başlangıç) -> tam (geçen nanosaniye) */
long long _tr_kronometre_gecen(long long başlangıç) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long long simdi = (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
    return simdi - başlangıç;
}

/* kronometre_gecen_ms(başlangıç) -> tam (geçen milisaniye) */
long long _tr_kronometre_gecen_ms(long long başlangıç) {
    return _tr_kronometre_gecen(başlangıç) / 1000000;
}

/* kronometre_gecen_us(başlangıç) -> tam (geçen mikrosaniye) */
long long _tr_kronometre_gecen_us(long long başlangıç) {
    return _tr_kronometre_gecen(başlangıç) / 1000;
}

/* ========== UYUMA ========== */

/* bekle_saniye(saniye) -> bosluk */
void _tr_bekle_saniye(long long saniye) {
    struct timespec ts;
    ts.tv_sec = (time_t)saniye;
    ts.tv_nsec = 0;
    nanosleep(&ts, NULL);
}

/* bekle_ms(milisaniye) -> bosluk */
void _tr_bekle_ms(long long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/* bekle_us(mikrosaniye) -> bosluk */
void _tr_bekle_us(long long us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}
