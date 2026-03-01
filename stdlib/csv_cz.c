/* CSV modülü — çalışma zamanı implementasyonu
 * RFC 4180 uyumlu CSV parser
 * - Tırnaklı alanlar: "alan, virgüllü"
 * - Kaçış tırnakları: "alan ""tırnak"" içinde"
 * - CRLF/LF desteği
 * - UTF-8 geçirgen
 * - Özel ayırıcı desteği (';' Türk Excel için)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { char *ptr; long long len; } TrMetin;
typedef struct { long long *ptr; long long count; } TrDizi;

/* dosya_oku runtime fonksiyonu */
extern TrMetin _tr_dosya_oku(const char *yol_ptr, long long yol_uzunluk);

/* Yardımcı: metin → C string */
static char *metin_cstr(const char *ptr, long long len) {
    char *s = (char *)malloc(len + 1);
    if (!s) return NULL;
    memcpy(s, ptr, len);
    s[len] = '\0';
    return s;
}

/* ========== CSV DURUM MAKİNESİ ========== */

typedef enum {
    CSV_ALAN_BAS,
    CSV_ALAN_NORMAL,
    CSV_ALAN_TIRNAK,
    CSV_ALAN_TIRNAK_KACIS
} CsvDurum;

/* Tek bir CSV satırını alanlarına ayır.
 * Sonuç: metin dizisi (her alan ptr+len çifti olarak saklanır) */
static TrDizi csv_satir_parse(const char *satir, long long satir_len, char ayirici) {
    TrDizi d = {NULL, 0};

    int kapasite = 16;
    long long *sonuc = (long long *)malloc(kapasite * 2 * sizeof(long long));
    if (!sonuc) return d;

    int alan_kap = 256;
    char *alan_buf = (char *)malloc(alan_kap);
    if (!alan_buf) { free(sonuc); return d; }
    int alan_len = 0;
    int alan_sayisi = 0;

    CsvDurum durum = CSV_ALAN_BAS;

    for (long long i = 0; i <= satir_len; i++) {
        char c = (i < satir_len) ? satir[i] : '\0';

        /* \r atla */
        if (c == '\r') continue;

        switch (durum) {
        case CSV_ALAN_BAS:
            if (c == '"') {
                durum = CSV_ALAN_TIRNAK;
                alan_len = 0;
            } else if (c == ayirici || c == '\0') {
                goto alan_kaydet;
            } else {
                durum = CSV_ALAN_NORMAL;
                alan_buf[alan_len++] = c;
            }
            break;

        case CSV_ALAN_NORMAL:
            if (c == ayirici || c == '\0') {
                goto alan_kaydet;
            } else {
                if (alan_len >= alan_kap - 1) {
                    alan_kap *= 2;
                    alan_buf = (char *)realloc(alan_buf, alan_kap);
                }
                alan_buf[alan_len++] = c;
            }
            break;

        case CSV_ALAN_TIRNAK:
            if (c == '"') {
                durum = CSV_ALAN_TIRNAK_KACIS;
            } else if (c == '\0') {
                goto alan_kaydet;
            } else {
                if (alan_len >= alan_kap - 1) {
                    alan_kap *= 2;
                    alan_buf = (char *)realloc(alan_buf, alan_kap);
                }
                alan_buf[alan_len++] = c;
            }
            break;

        case CSV_ALAN_TIRNAK_KACIS:
            if (c == '"') {
                /* "" → tek tırnak */
                if (alan_len >= alan_kap - 1) {
                    alan_kap *= 2;
                    alan_buf = (char *)realloc(alan_buf, alan_kap);
                }
                alan_buf[alan_len++] = '"';
                durum = CSV_ALAN_TIRNAK;
            } else if (c == ayirici || c == '\0') {
                goto alan_kaydet;
            } else {
                durum = CSV_ALAN_NORMAL;
            }
            break;
        }
        continue;

    alan_kaydet:
        if (alan_sayisi >= kapasite) {
            kapasite *= 2;
            sonuc = (long long *)realloc(sonuc, kapasite * 2 * sizeof(long long));
        }
        char *alan_kopya = NULL;
        if (alan_len > 0) {
            alan_kopya = (char *)malloc(alan_len);
            if (alan_kopya) memcpy(alan_kopya, alan_buf, alan_len);
        }
        sonuc[alan_sayisi * 2] = (long long)(intptr_t)alan_kopya;
        sonuc[alan_sayisi * 2 + 1] = alan_len;
        alan_sayisi++;
        alan_len = 0;
        durum = CSV_ALAN_BAS;
    }

    free(alan_buf);
    d.ptr = sonuc;
    d.count = alan_sayisi;
    return d;
}

/* ========== İÇ YARDIMCI: dosya içeriğini satırlara ayırıp parse et ========== */

static TrDizi csv_dosya_parse(const char *icerik, long long icerik_len, char ayirici) {
    TrDizi d = {NULL, 0};
    if (!icerik || icerik_len <= 0) return d;

    int satir_kap = 64;
    long long *satirlar = (long long *)malloc(satir_kap * sizeof(long long));
    if (!satirlar) return d;
    int satir_sayisi = 0;

    /* Tırnak-farkındalıklı satır bölme */
    long long bas = 0;
    int tirnak_icinde = 0;

    for (long long i = 0; i <= icerik_len; i++) {
        char c = (i < icerik_len) ? icerik[i] : '\n';

        if (c == '"') tirnak_icinde = !tirnak_icinde;

        if (!tirnak_icinde && (c == '\n' || i == icerik_len)) {
            long long satir_len = i - bas;
            /* Sondaki \r kaldır */
            if (satir_len > 0 && icerik[bas + satir_len - 1] == '\r')
                satir_len--;

            /* Dosya sonundaki boş satırı atla */
            if (i == icerik_len && satir_len == 0) break;

            /* Bu satırı parse et */
            TrDizi satir = csv_satir_parse(icerik + bas, satir_len, ayirici);

            if (satir_sayisi >= satir_kap) {
                satir_kap *= 2;
                satirlar = (long long *)realloc(satirlar, satir_kap * sizeof(long long));
            }

            /* Satır bloğu: {ptr_to_fields, field_count, metin_dizisi_mi} */
            long long *satir_blok = (long long *)malloc(3 * sizeof(long long));
            if (satir_blok) {
                satir_blok[0] = (long long)(intptr_t)satir.ptr;
                satir_blok[1] = satir.count;
                satir_blok[2] = 1; /* metin dizisi işareti */
            }
            satirlar[satir_sayisi] = (long long)(intptr_t)satir_blok;
            satir_sayisi++;

            bas = i + 1;
        }
    }

    d.ptr = satirlar;
    d.count = satir_sayisi;
    return d;
}

/* ========== GENEL FONKSİYONLAR ========== */

/* csv_ayır(satır: metin) -> dizi<metin> */
TrDizi _tr_csv_ayir(const char *ptr, long long len) {
    return csv_satir_parse(ptr, len, ',');
}

/* csv_ayır_özel(satır: metin, ayırıcı: tam) -> dizi<metin> */
TrDizi _tr_csv_ayir_ozel(const char *ptr, long long len, long long ayirici) {
    return csv_satir_parse(ptr, len, (char)ayirici);
}

/* csv_oku(dosya: metin) -> dizi (2D) */
TrDizi _tr_csv_oku(const char *dosya_ptr, long long dosya_len) {
    TrMetin icerik = _tr_dosya_oku(dosya_ptr, dosya_len);
    if (!icerik.ptr) { TrDizi d = {NULL, 0}; return d; }
    TrDizi sonuc = csv_dosya_parse(icerik.ptr, icerik.len, ',');
    free(icerik.ptr);
    return sonuc;
}

/* csv_oku_özel(dosya: metin, ayırıcı: tam) -> dizi */
TrDizi _tr_csv_oku_ozel(const char *dosya_ptr, long long dosya_len, long long ayirici) {
    TrMetin icerik = _tr_dosya_oku(dosya_ptr, dosya_len);
    if (!icerik.ptr) { TrDizi d = {NULL, 0}; return d; }
    TrDizi sonuc = csv_dosya_parse(icerik.ptr, icerik.len, (char)ayirici);
    free(icerik.ptr);
    return sonuc;
}

/* csv_satır_al(veri: dizi, satır_indeks: tam) -> dizi<metin> */
TrDizi _tr_csv_satir_al(long long *veri_ptr, long long veri_count, long long satir_idx) {
    TrDizi d = {NULL, 0};
    if (!veri_ptr || satir_idx < 0 || satir_idx >= veri_count) return d;

    long long *satir_blok = (long long *)veri_ptr[satir_idx];
    if (!satir_blok) return d;

    d.ptr = (long long *)satir_blok[0];
    d.count = satir_blok[1];
    return d;
}

/* csv_alan_al(veri: dizi, satır: tam, alan: tam) -> metin */
TrMetin _tr_csv_alan_al(long long *veri_ptr, long long veri_count,
                         long long satir_idx, long long alan_idx) {
    TrMetin m = {NULL, 0};
    if (!veri_ptr || satir_idx < 0 || satir_idx >= veri_count) return m;

    long long *satir_blok = (long long *)veri_ptr[satir_idx];
    if (!satir_blok) return m;

    long long *alanlar = (long long *)satir_blok[0];
    long long alan_sayisi = satir_blok[1];

    if (!alanlar || alan_idx < 0 || alan_idx >= alan_sayisi) return m;

    m.ptr = (char *)(intptr_t)alanlar[alan_idx * 2];
    m.len = alanlar[alan_idx * 2 + 1];
    return m;
}

/* csv_satır_sayısı(veri: dizi) -> tam */
long long _tr_csv_satir_sayisi(long long *veri_ptr, long long veri_count) {
    (void)veri_ptr;
    return veri_count;
}

/* csv_alan_sayısı(veri: dizi, satır: tam) -> tam */
long long _tr_csv_alan_sayisi(long long *veri_ptr, long long veri_count, long long satir_idx) {
    if (!veri_ptr || satir_idx < 0 || satir_idx >= veri_count) return 0;

    long long *satir_blok = (long long *)veri_ptr[satir_idx];
    if (!satir_blok) return 0;

    return satir_blok[1];
}

/* csv_satır_oluştur(alanlar: dizi<metin>) -> metin */
TrMetin _tr_csv_satir_olustur(long long *dizi_ptr, long long dizi_count) {
    TrMetin m = {NULL, 0};
    if (!dizi_ptr || dizi_count <= 0) return m;

    /* Üst sınır hesapla */
    long long toplam = 0;
    for (long long i = 0; i < dizi_count; i++) {
        long long alan_len = dizi_ptr[i * 2 + 1];
        toplam += alan_len * 2 + 3;
    }
    toplam += 2;

    char *buf = (char *)malloc(toplam);
    if (!buf) return m;
    long long pos = 0;

    for (long long i = 0; i < dizi_count; i++) {
        if (i > 0) buf[pos++] = ',';

        char *alan_ptr = (char *)(intptr_t)dizi_ptr[i * 2];
        long long alan_len = dizi_ptr[i * 2 + 1];

        /* Tırnak gerekli mi kontrol et */
        int tirnak_gerek = 0;
        for (long long j = 0; j < alan_len; j++) {
            if (alan_ptr[j] == ',' || alan_ptr[j] == '"' ||
                alan_ptr[j] == '\n' || alan_ptr[j] == '\r') {
                tirnak_gerek = 1;
                break;
            }
        }

        if (tirnak_gerek) {
            buf[pos++] = '"';
            for (long long j = 0; j < alan_len; j++) {
                if (alan_ptr[j] == '"') buf[pos++] = '"';
                buf[pos++] = alan_ptr[j];
            }
            buf[pos++] = '"';
        } else {
            if (alan_ptr && alan_len > 0) {
                memcpy(buf + pos, alan_ptr, alan_len);
                pos += alan_len;
            }
        }
    }
    buf[pos++] = '\n';

    m.ptr = buf;
    m.len = pos;
    return m;
}

/* csv_yaz(dosya: metin, veri: dizi<dizi<metin>>) -> tam */
long long _tr_csv_yaz(const char *dosya_ptr, long long dosya_len,
                      long long *veri_ptr, long long veri_count) {
    char *dosya_adi = metin_cstr(dosya_ptr, dosya_len);
    if (!dosya_adi) return -1;

    FILE *f = fopen(dosya_adi, "w");
    free(dosya_adi);
    if (!f) return -1;

    for (long long i = 0; i < veri_count; i++) {
        long long *satir_blok = (long long *)veri_ptr[i];
        if (!satir_blok) continue;

        long long *satir_ptr = (long long *)satir_blok[0];
        long long satir_count = satir_blok[1];

        TrMetin satir = _tr_csv_satir_olustur(satir_ptr, satir_count);
        if (satir.ptr) {
            fwrite(satir.ptr, 1, satir.len, f);
            free(satir.ptr);
        }
    }

    fclose(f);
    return 0;
}
