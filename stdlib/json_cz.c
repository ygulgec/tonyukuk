/* JSON modülü — çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * Gelişmiş JSON desteği: tip güvenli erişim, dizi, iç içe nesneler, builder API
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "runtime.h"

/* JSON değer tipleri */
#define JSON_TIP_NULL    0
#define JSON_TIP_MANTIK  1
#define JSON_TIP_TAM     2
#define JSON_TIP_ONDALIK 3
#define JSON_TIP_METIN   4
#define JSON_TIP_NESNE   5
#define JSON_TIP_DIZI    6

/* JSON değer yapısı - tip bilgisi ile birlikte saklama */
typedef struct {
    long long tip;      /* JSON_TIP_* */
    long long deger;    /* tam sayı, pointer, veya ondalık'ın bit değeri */
} JsonDeger;

/* JSON Nesne yapısı
 * [0] = JSON_TIP_NESNE marker
 * [1] = kapasite
 * [2] = eleman sayısı
 * [3+i*4] = anahtar_ptr
 * [3+i*4+1] = anahtar_len
 * [3+i*4+2] = deger_tip
 * [3+i*4+3] = deger_val
 */
#define JSON_NESNE_META 3
#define JSON_NESNE_GIRDI 4

/* JSON Dizi yapısı
 * [0] = JSON_TIP_DIZI marker
 * [1] = kapasite
 * [2] = eleman sayısı
 * [3+i*2] = deger_tip
 * [3+i*2+1] = deger_val
 */
#define JSON_DIZI_META 3
#define JSON_DIZI_ELEMAN 2

/* ========== YARDIMCI FONKSİYONLAR ========== */

/* Boşlukları atla */
static const char *json_bosluk_atla(const char *p, const char *son) {
    while (p < son && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

/* JSON metin parçala (escape destekli) */
static const char *json_metin_parcala(const char *p, const char *son,
                                       char **out_ptr, long long *out_len) {
    if (p >= son || *p != '"') { *out_ptr = NULL; *out_len = 0; return p; }
    p++; /* " atla */

    char buf[65536];
    int bi = 0;
    while (p < son && *p != '"') {
        if (*p == '\\' && p + 1 < son) {
            p++;
            switch (*p) {
                case '"': buf[bi++] = '"'; break;
                case '\\': buf[bi++] = '\\'; break;
                case 'n': buf[bi++] = '\n'; break;
                case 't': buf[bi++] = '\t'; break;
                case 'r': buf[bi++] = '\r'; break;
                case '/': buf[bi++] = '/'; break;
                case 'b': buf[bi++] = '\b'; break;
                case 'f': buf[bi++] = '\f'; break;
                case 'u': {
                    /* Unicode escape: \uXXXX */
                    if (p + 4 < son) {
                        unsigned int code = 0;
                        for (int i = 1; i <= 4; i++) {
                            char c = p[i];
                            code <<= 4;
                            if (c >= '0' && c <= '9') code |= (c - '0');
                            else if (c >= 'a' && c <= 'f') code |= (c - 'a' + 10);
                            else if (c >= 'A' && c <= 'F') code |= (c - 'A' + 10);
                        }
                        /* UTF-8 encode */
                        if (code < 0x80) {
                            buf[bi++] = (char)code;
                        } else if (code < 0x800) {
                            buf[bi++] = (char)(0xC0 | (code >> 6));
                            buf[bi++] = (char)(0x80 | (code & 0x3F));
                        } else {
                            buf[bi++] = (char)(0xE0 | (code >> 12));
                            buf[bi++] = (char)(0x80 | ((code >> 6) & 0x3F));
                            buf[bi++] = (char)(0x80 | (code & 0x3F));
                        }
                        p += 4;
                    }
                    break;
                }
                default: buf[bi++] = *p; break;
            }
            p++;
        } else {
            buf[bi++] = *p++;
        }
        if (bi >= (int)sizeof(buf) - 4) break;
    }
    if (p < son && *p == '"') p++; /* kapanış " */

    *out_ptr = (char *)malloc(bi + 1);
    if (*out_ptr) {
        memcpy(*out_ptr, buf, bi);
        (*out_ptr)[bi] = '\0';
    }
    *out_len = bi;
    return p;
}

/* JSON sayı parçala (tam ve ondalık) */
static const char *json_sayi_parcala(const char *p, const char *son,
                                      long long *tip, long long *deger) {
    char buf[64];
    int bi = 0;
    int ondalik_mi = 0;

    if (p < son && *p == '-') buf[bi++] = *p++;
    while (p < son && *p >= '0' && *p <= '9' && bi < 63) buf[bi++] = *p++;

    if (p < son && *p == '.') {
        ondalik_mi = 1;
        buf[bi++] = *p++;
        while (p < son && *p >= '0' && *p <= '9' && bi < 63) buf[bi++] = *p++;
    }

    if (p < son && (*p == 'e' || *p == 'E')) {
        ondalik_mi = 1;
        buf[bi++] = *p++;
        if (p < son && (*p == '+' || *p == '-')) buf[bi++] = *p++;
        while (p < son && *p >= '0' && *p <= '9' && bi < 63) buf[bi++] = *p++;
    }

    buf[bi] = '\0';

    if (ondalik_mi) {
        double d = atof(buf);
        *tip = JSON_TIP_ONDALIK;
        memcpy(deger, &d, sizeof(double));
    } else {
        *tip = JSON_TIP_TAM;
        *deger = atoll(buf);
    }
    return p;
}

/* İleri bildirim */
static const char *json_deger_parcala(const char *p, const char *son,
                                       long long *tip, long long *deger);

/* JSON nesne oluştur */
static long long json_nesne_olustur_ic(void) {
    /* Meta + 256 girdi * 4 alan */
    long long boyut = (JSON_NESNE_META + 256 * JSON_NESNE_GIRDI) * sizeof(long long);
    long long *blok = (long long *)_tr_nesne_olustur(NESNE_TIP_SOZLUK, boyut);
    if (!blok) return 0;
    blok[0] = JSON_TIP_NESNE;
    blok[1] = 256;  /* kapasite */
    blok[2] = 0;    /* eleman sayısı */
    return (long long)blok;
}

/* JSON dizi oluştur */
static long long json_dizi_olustur_ic(void) {
    /* Meta + 512 eleman * 2 alan */
    long long boyut = (JSON_DIZI_META + 512 * JSON_DIZI_ELEMAN) * sizeof(long long);
    long long *blok = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!blok) return 0;
    blok[0] = JSON_TIP_DIZI;
    blok[1] = 512;  /* kapasite */
    blok[2] = 0;    /* eleman sayısı */
    return (long long)blok;
}

/* JSON nesneye değer ekle */
static void json_nesne_ekle_ic(long long nesne_ptr,
                                const char *anahtar_ptr, long long anahtar_len,
                                long long tip, long long deger) {
    long long *blok = (long long *)nesne_ptr;
    long long say = blok[2];
    if (say >= blok[1]) return;  /* dolu */

    /* Anahtar kopyala */
    char *kopyala = (char *)malloc(anahtar_len + 1);
    if (kopyala) {
        memcpy(kopyala, anahtar_ptr, anahtar_len);
        kopyala[anahtar_len] = '\0';
    }

    long long idx = JSON_NESNE_META + say * JSON_NESNE_GIRDI;
    blok[idx] = (long long)kopyala;
    blok[idx + 1] = anahtar_len;
    blok[idx + 2] = tip;
    blok[idx + 3] = deger;
    blok[2] = say + 1;
}

/* JSON diziye eleman ekle */
static void json_dizi_ekle_ic(long long dizi_ptr, long long tip, long long deger) {
    long long *blok = (long long *)dizi_ptr;
    long long say = blok[2];
    if (say >= blok[1]) return;  /* dolu */

    long long idx = JSON_DIZI_META + say * JSON_DIZI_ELEMAN;
    blok[idx] = tip;
    blok[idx + 1] = deger;
    blok[2] = say + 1;
}

/* JSON nesne parçala */
static const char *json_nesne_parcala(const char *p, const char *son, long long *nesne_ptr) {
    *nesne_ptr = json_nesne_olustur_ic();
    if (*nesne_ptr == 0) return p;

    p = json_bosluk_atla(p, son);
    if (p >= son || *p != '{') return p;
    p++; /* { atla */

    while (1) {
        p = json_bosluk_atla(p, son);
        if (p >= son || *p == '}') { if (p < son) p++; break; }

        /* Anahtar */
        char *anahtar_ptr = NULL;
        long long anahtar_len = 0;
        p = json_metin_parcala(p, son, &anahtar_ptr, &anahtar_len);

        p = json_bosluk_atla(p, son);
        if (p < son && *p == ':') p++;
        p = json_bosluk_atla(p, son);

        /* Değer */
        long long tip = 0, deger = 0;
        p = json_deger_parcala(p, son, &tip, &deger);

        /* Nesneye ekle */
        if (anahtar_ptr) {
            json_nesne_ekle_ic(*nesne_ptr, anahtar_ptr, anahtar_len, tip, deger);
            free(anahtar_ptr);
        }

        p = json_bosluk_atla(p, son);
        if (p < son && *p == ',') p++;
    }
    return p;
}

/* JSON dizi parçala */
static const char *json_dizi_parcala(const char *p, const char *son, long long *dizi_ptr) {
    *dizi_ptr = json_dizi_olustur_ic();
    if (*dizi_ptr == 0) return p;

    p = json_bosluk_atla(p, son);
    if (p >= son || *p != '[') return p;
    p++; /* [ atla */

    while (1) {
        p = json_bosluk_atla(p, son);
        if (p >= son || *p == ']') { if (p < son) p++; break; }

        /* Değer */
        long long tip = 0, deger = 0;
        p = json_deger_parcala(p, son, &tip, &deger);

        /* Diziye ekle */
        json_dizi_ekle_ic(*dizi_ptr, tip, deger);

        p = json_bosluk_atla(p, son);
        if (p < son && *p == ',') p++;
    }
    return p;
}

/* JSON değer parçala (tüm tipler) */
static const char *json_deger_parcala(const char *p, const char *son,
                                       long long *tip, long long *deger) {
    p = json_bosluk_atla(p, son);
    if (p >= son) { *tip = JSON_TIP_NULL; *deger = 0; return p; }

    if (*p == '"') {
        /* Metin */
        char *str_ptr = NULL;
        long long str_len = 0;
        p = json_metin_parcala(p, son, &str_ptr, &str_len);
        *tip = JSON_TIP_METIN;
        /* Metin değeri: yüksek 32 bit = len, düşük 32 bit = ptr (aslında tam ptr kullanıyoruz) */
        /* Metin pointer'ı ve uzunluğu ayrı saklamak için özel yapı */
        long long *metin_blok = (long long *)malloc(2 * sizeof(long long));
        if (metin_blok) {
            metin_blok[0] = (long long)str_ptr;
            metin_blok[1] = str_len;
        }
        *deger = (long long)metin_blok;
        return p;
    } else if (*p == '{') {
        /* Nesne */
        long long nesne = 0;
        p = json_nesne_parcala(p, son, &nesne);
        *tip = JSON_TIP_NESNE;
        *deger = nesne;
        return p;
    } else if (*p == '[') {
        /* Dizi */
        long long dizi = 0;
        p = json_dizi_parcala(p, son, &dizi);
        *tip = JSON_TIP_DIZI;
        *deger = dizi;
        return p;
    } else if (*p == 't') {
        /* true */
        *tip = JSON_TIP_MANTIK;
        *deger = 1;
        if (p + 4 <= son) p += 4;
        return p;
    } else if (*p == 'f') {
        /* false */
        *tip = JSON_TIP_MANTIK;
        *deger = 0;
        if (p + 5 <= son) p += 5;
        return p;
    } else if (*p == 'n') {
        /* null */
        *tip = JSON_TIP_NULL;
        *deger = 0;
        if (p + 4 <= son) p += 4;
        return p;
    } else {
        /* Sayı */
        p = json_sayi_parcala(p, son, tip, deger);
        return p;
    }
}

/* ========== ANA API FONKSİYONLARI ========== */

/* json_cozumle(metin) -> tam (JSON nesne pointer) */
long long _tr_json_cozumle(const char *ptr, long long len) {
    long long tip = 0, deger = 0;
    const char *son = ptr + len;
    const char *p = json_bosluk_atla(ptr, son);

    if (p < son) {
        if (*p == '{') {
            json_nesne_parcala(p, son, &deger);
            return deger;
        } else if (*p == '[') {
            json_dizi_parcala(p, son, &deger);
            return deger;
        }
    }
    return 0;
}

/* Nesne içinde anahtar ara, tip ve değer döndür */
static int json_anahtar_bul(long long nesne_ptr, const char *anahtar_ptr,
                            long long anahtar_len, long long *out_tip, long long *out_deger) {
    if (nesne_ptr == 0) return 0;
    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return 0;

    long long say = blok[2];
    for (long long i = 0; i < say; i++) {
        long long idx = JSON_NESNE_META + i * JSON_NESNE_GIRDI;
        char *k = (char *)blok[idx];
        long long kl = blok[idx + 1];
        if (kl == anahtar_len && memcmp(k, anahtar_ptr, anahtar_len) == 0) {
            *out_tip = blok[idx + 2];
            *out_deger = blok[idx + 3];
            return 1;
        }
    }
    return 0;
}

/* json_tip(nesne, anahtar) -> tam (JSON_TIP_*) */
long long _tr_json_tip(long long nesne_ptr, const char *anahtar_ptr, long long anahtar_len) {
    long long tip = 0, deger = 0;
    if (json_anahtar_bul(nesne_ptr, anahtar_ptr, anahtar_len, &tip, &deger)) {
        return tip;
    }
    return -1; /* bulunamadı */
}

/* json_var_mi(nesne, anahtar) -> mantık */
long long _tr_json_var_mi(long long nesne_ptr, const char *anahtar_ptr, long long anahtar_len) {
    long long tip = 0, deger = 0;
    return json_anahtar_bul(nesne_ptr, anahtar_ptr, anahtar_len, &tip, &deger);
}

/* json_metin_al(nesne, anahtar) -> metin */
TrMetin _tr_json_metin_al(long long nesne_ptr, const char *anahtar_ptr, long long anahtar_len) {
    TrMetin m = {NULL, 0};
    long long tip = 0, deger = 0;

    if (!json_anahtar_bul(nesne_ptr, anahtar_ptr, anahtar_len, &tip, &deger)) {
        return m;
    }

    if (tip == JSON_TIP_METIN && deger != 0) {
        long long *metin_blok = (long long *)deger;
        char *str_ptr = (char *)metin_blok[0];
        long long str_len = metin_blok[1];
        m.ptr = (char *)malloc(str_len + 1);
        if (m.ptr) {
            memcpy(m.ptr, str_ptr, str_len);
            m.ptr[str_len] = '\0';
        }
        m.len = str_len;
    }
    return m;
}

/* json_tam_al(nesne, anahtar) -> tam */
long long _tr_json_tam_al(long long nesne_ptr, const char *anahtar_ptr, long long anahtar_len) {
    long long tip = 0, deger = 0;

    if (!json_anahtar_bul(nesne_ptr, anahtar_ptr, anahtar_len, &tip, &deger)) {
        return 0;
    }

    if (tip == JSON_TIP_TAM) {
        return deger;
    } else if (tip == JSON_TIP_ONDALIK) {
        double d;
        memcpy(&d, &deger, sizeof(double));
        return (long long)d;
    } else if (tip == JSON_TIP_MANTIK) {
        return deger;
    }
    return 0;
}

/* json_ondalik_al(nesne, anahtar) -> ondalık */
double _tr_json_ondalik_al(long long nesne_ptr, const char *anahtar_ptr, long long anahtar_len) {
    long long tip = 0, deger = 0;

    if (!json_anahtar_bul(nesne_ptr, anahtar_ptr, anahtar_len, &tip, &deger)) {
        return 0.0;
    }

    if (tip == JSON_TIP_ONDALIK) {
        double d;
        memcpy(&d, &deger, sizeof(double));
        return d;
    } else if (tip == JSON_TIP_TAM) {
        return (double)deger;
    }
    return 0.0;
}

/* json_mantik_al(nesne, anahtar) -> mantık */
long long _tr_json_mantik_al(long long nesne_ptr, const char *anahtar_ptr, long long anahtar_len) {
    long long tip = 0, deger = 0;

    if (!json_anahtar_bul(nesne_ptr, anahtar_ptr, anahtar_len, &tip, &deger)) {
        return 0;
    }

    if (tip == JSON_TIP_MANTIK) {
        return deger;
    } else if (tip == JSON_TIP_TAM) {
        return deger != 0 ? 1 : 0;
    }
    return 0;
}

/* json_nesne_al(nesne, anahtar) -> tam (iç nesne pointer) */
long long _tr_json_nesne_al(long long nesne_ptr, const char *anahtar_ptr, long long anahtar_len) {
    long long tip = 0, deger = 0;

    if (!json_anahtar_bul(nesne_ptr, anahtar_ptr, anahtar_len, &tip, &deger)) {
        return 0;
    }

    if (tip == JSON_TIP_NESNE) {
        return deger;
    }
    return 0;
}

/* json_dizi_al(nesne, anahtar) -> tam (dizi pointer) */
long long _tr_json_dizi_al(long long nesne_ptr, const char *anahtar_ptr, long long anahtar_len) {
    long long tip = 0, deger = 0;

    if (!json_anahtar_bul(nesne_ptr, anahtar_ptr, anahtar_len, &tip, &deger)) {
        return 0;
    }

    if (tip == JSON_TIP_DIZI) {
        return deger;
    }
    return 0;
}

/* ========== DİZİ ERİŞİM FONKSİYONLARI ========== */

/* json_dizi_uzunluk(dizi) -> tam */
long long _tr_json_dizi_uzunluk(long long dizi_ptr) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;
    return blok[2];
}

/* json_dizi_tip(dizi, indeks) -> tam */
long long _tr_json_dizi_tip(long long dizi_ptr, long long indeks) {
    if (dizi_ptr == 0) return -1;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return -1;
    if (indeks < 0 || indeks >= blok[2]) return -1;

    long long idx = JSON_DIZI_META + indeks * JSON_DIZI_ELEMAN;
    return blok[idx];
}

/* json_dizi_metin(dizi, indeks) -> metin */
TrMetin _tr_json_dizi_metin(long long dizi_ptr, long long indeks) {
    TrMetin m = {NULL, 0};
    if (dizi_ptr == 0) return m;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return m;
    if (indeks < 0 || indeks >= blok[2]) return m;

    long long idx = JSON_DIZI_META + indeks * JSON_DIZI_ELEMAN;
    long long tip = blok[idx];
    long long deger = blok[idx + 1];

    if (tip == JSON_TIP_METIN && deger != 0) {
        long long *metin_blok = (long long *)deger;
        char *str_ptr = (char *)metin_blok[0];
        long long str_len = metin_blok[1];
        m.ptr = (char *)malloc(str_len + 1);
        if (m.ptr) {
            memcpy(m.ptr, str_ptr, str_len);
            m.ptr[str_len] = '\0';
        }
        m.len = str_len;
    }
    return m;
}

/* json_dizi_tam(dizi, indeks) -> tam */
long long _tr_json_dizi_tam(long long dizi_ptr, long long indeks) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;
    if (indeks < 0 || indeks >= blok[2]) return 0;

    long long idx = JSON_DIZI_META + indeks * JSON_DIZI_ELEMAN;
    long long tip = blok[idx];
    long long deger = blok[idx + 1];

    if (tip == JSON_TIP_TAM) return deger;
    if (tip == JSON_TIP_ONDALIK) {
        double d;
        memcpy(&d, &deger, sizeof(double));
        return (long long)d;
    }
    return 0;
}

/* json_dizi_ondalik(dizi, indeks) -> ondalık */
double _tr_json_dizi_ondalik(long long dizi_ptr, long long indeks) {
    if (dizi_ptr == 0) return 0.0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0.0;
    if (indeks < 0 || indeks >= blok[2]) return 0.0;

    long long idx = JSON_DIZI_META + indeks * JSON_DIZI_ELEMAN;
    long long tip = blok[idx];
    long long deger = blok[idx + 1];

    if (tip == JSON_TIP_ONDALIK) {
        double d;
        memcpy(&d, &deger, sizeof(double));
        return d;
    }
    if (tip == JSON_TIP_TAM) return (double)deger;
    return 0.0;
}

/* json_dizi_mantik(dizi, indeks) -> mantık */
long long _tr_json_dizi_mantik(long long dizi_ptr, long long indeks) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;
    if (indeks < 0 || indeks >= blok[2]) return 0;

    long long idx = JSON_DIZI_META + indeks * JSON_DIZI_ELEMAN;
    long long tip = blok[idx];
    long long deger = blok[idx + 1];

    if (tip == JSON_TIP_MANTIK) return deger;
    return 0;
}

/* json_dizi_nesne(dizi, indeks) -> tam (nesne pointer) */
long long _tr_json_dizi_nesne(long long dizi_ptr, long long indeks) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;
    if (indeks < 0 || indeks >= blok[2]) return 0;

    long long idx = JSON_DIZI_META + indeks * JSON_DIZI_ELEMAN;
    long long tip = blok[idx];
    long long deger = blok[idx + 1];

    if (tip == JSON_TIP_NESNE) return deger;
    return 0;
}

/* json_dizi_dizi(dizi, indeks) -> tam (iç dizi pointer) */
long long _tr_json_dizi_dizi(long long dizi_ptr, long long indeks) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;
    if (indeks < 0 || indeks >= blok[2]) return 0;

    long long idx = JSON_DIZI_META + indeks * JSON_DIZI_ELEMAN;
    long long tip = blok[idx];
    long long deger = blok[idx + 1];

    if (tip == JSON_TIP_DIZI) return deger;
    return 0;
}

/* ========== BUILDER API ========== */

/* json_nesne_yeni() -> tam */
long long _tr_json_nesne_yeni(void) {
    return json_nesne_olustur_ic();
}

/* json_dizi_yeni() -> tam */
long long _tr_json_dizi_yeni(void) {
    return json_dizi_olustur_ic();
}

/* json_nesne_metin_ekle(nesne, anahtar, deger) -> tam */
long long _tr_json_nesne_metin_ekle(long long nesne_ptr,
                                     const char *anahtar_ptr, long long anahtar_len,
                                     const char *deger_ptr, long long deger_len) {
    if (nesne_ptr == 0) return 0;
    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return 0;

    /* Metin bloğu oluştur */
    long long *metin_blok = (long long *)malloc(2 * sizeof(long long));
    if (!metin_blok) return 0;

    char *str_copy = (char *)malloc(deger_len + 1);
    if (!str_copy) {
        free(metin_blok);
        return 0;
    }
    memcpy(str_copy, deger_ptr, deger_len);
    str_copy[deger_len] = '\0';
    metin_blok[0] = (long long)str_copy;
    metin_blok[1] = deger_len;

    json_nesne_ekle_ic(nesne_ptr, anahtar_ptr, anahtar_len, JSON_TIP_METIN, (long long)metin_blok);
    return nesne_ptr;
}

/* json_nesne_tam_ekle(nesne, anahtar, deger) -> tam */
long long _tr_json_nesne_tam_ekle(long long nesne_ptr,
                                   const char *anahtar_ptr, long long anahtar_len,
                                   long long deger) {
    if (nesne_ptr == 0) return 0;
    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return 0;

    json_nesne_ekle_ic(nesne_ptr, anahtar_ptr, anahtar_len, JSON_TIP_TAM, deger);
    return nesne_ptr;
}

/* json_nesne_ondalik_ekle(nesne, anahtar, deger) -> tam */
long long _tr_json_nesne_ondalik_ekle(long long nesne_ptr,
                                       const char *anahtar_ptr, long long anahtar_len,
                                       double deger) {
    if (nesne_ptr == 0) return 0;
    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return 0;

    long long deger_bits;
    memcpy(&deger_bits, &deger, sizeof(double));
    json_nesne_ekle_ic(nesne_ptr, anahtar_ptr, anahtar_len, JSON_TIP_ONDALIK, deger_bits);
    return nesne_ptr;
}

/* json_nesne_mantik_ekle(nesne, anahtar, deger) -> tam */
long long _tr_json_nesne_mantik_ekle(long long nesne_ptr,
                                      const char *anahtar_ptr, long long anahtar_len,
                                      long long deger) {
    if (nesne_ptr == 0) return 0;
    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return 0;

    json_nesne_ekle_ic(nesne_ptr, anahtar_ptr, anahtar_len, JSON_TIP_MANTIK, deger ? 1 : 0);
    return nesne_ptr;
}

/* json_nesne_null_ekle(nesne, anahtar) -> tam */
long long _tr_json_nesne_null_ekle(long long nesne_ptr,
                                    const char *anahtar_ptr, long long anahtar_len) {
    if (nesne_ptr == 0) return 0;
    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return 0;

    json_nesne_ekle_ic(nesne_ptr, anahtar_ptr, anahtar_len, JSON_TIP_NULL, 0);
    return nesne_ptr;
}

/* json_nesne_nesne_ekle(nesne, anahtar, ic_nesne) -> tam */
long long _tr_json_nesne_nesne_ekle(long long nesne_ptr,
                                     const char *anahtar_ptr, long long anahtar_len,
                                     long long ic_nesne) {
    if (nesne_ptr == 0) return 0;
    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return 0;

    json_nesne_ekle_ic(nesne_ptr, anahtar_ptr, anahtar_len, JSON_TIP_NESNE, ic_nesne);
    return nesne_ptr;
}

/* json_nesne_dizi_ekle(nesne, anahtar, dizi) -> tam */
long long _tr_json_nesne_dizi_ekle(long long nesne_ptr,
                                    const char *anahtar_ptr, long long anahtar_len,
                                    long long dizi) {
    if (nesne_ptr == 0) return 0;
    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return 0;

    json_nesne_ekle_ic(nesne_ptr, anahtar_ptr, anahtar_len, JSON_TIP_DIZI, dizi);
    return nesne_ptr;
}

/* Dizi builder fonksiyonları */

/* json_dizi_metin_ekle(dizi, deger) -> tam */
long long _tr_json_dizi_metin_ekle(long long dizi_ptr,
                                    const char *deger_ptr, long long deger_len) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;

    long long *metin_blok = (long long *)malloc(2 * sizeof(long long));
    if (!metin_blok) return 0;

    char *str_copy = (char *)malloc(deger_len + 1);
    if (!str_copy) {
        free(metin_blok);
        return 0;
    }
    memcpy(str_copy, deger_ptr, deger_len);
    str_copy[deger_len] = '\0';
    metin_blok[0] = (long long)str_copy;
    metin_blok[1] = deger_len;

    json_dizi_ekle_ic(dizi_ptr, JSON_TIP_METIN, (long long)metin_blok);
    return dizi_ptr;
}

/* json_dizi_tam_ekle(dizi, deger) -> tam */
long long _tr_json_dizi_tam_ekle(long long dizi_ptr, long long deger) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;

    json_dizi_ekle_ic(dizi_ptr, JSON_TIP_TAM, deger);
    return dizi_ptr;
}

/* json_dizi_ondalik_ekle(dizi, deger) -> tam */
long long _tr_json_dizi_ondalik_ekle(long long dizi_ptr, double deger) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;

    long long deger_bits;
    memcpy(&deger_bits, &deger, sizeof(double));
    json_dizi_ekle_ic(dizi_ptr, JSON_TIP_ONDALIK, deger_bits);
    return dizi_ptr;
}

/* json_dizi_mantik_ekle(dizi, deger) -> tam */
long long _tr_json_dizi_mantik_ekle(long long dizi_ptr, long long deger) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;

    json_dizi_ekle_ic(dizi_ptr, JSON_TIP_MANTIK, deger ? 1 : 0);
    return dizi_ptr;
}

/* json_dizi_null_ekle(dizi) -> tam */
long long _tr_json_dizi_null_ekle(long long dizi_ptr) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;

    json_dizi_ekle_ic(dizi_ptr, JSON_TIP_NULL, 0);
    return dizi_ptr;
}

/* json_dizi_nesne_ekle(dizi, nesne) -> tam */
long long _tr_json_dizi_nesne_ekle(long long dizi_ptr, long long nesne) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;

    json_dizi_ekle_ic(dizi_ptr, JSON_TIP_NESNE, nesne);
    return dizi_ptr;
}

/* json_dizi_dizi_ekle(dizi, ic_dizi) -> tam */
long long _tr_json_dizi_dizi_ekle(long long dizi_ptr, long long ic_dizi) {
    if (dizi_ptr == 0) return 0;
    long long *blok = (long long *)dizi_ptr;
    if (blok[0] != JSON_TIP_DIZI) return 0;

    json_dizi_ekle_ic(dizi_ptr, JSON_TIP_DIZI, ic_dizi);
    return dizi_ptr;
}

/* ========== SERİLEŞTİRME ========== */

/* İleri bildirim */
static void json_deger_yaz(char *buf, int *pos, int kap, long long tip, long long deger);

/* Metin escape ile yaz */
static void json_metin_yaz(char *buf, int *pos, int kap, const char *ptr, long long len) {
    if (*pos < kap) buf[(*pos)++] = '"';
    if (!ptr) {
        if (*pos < kap) buf[(*pos)++] = '"';
        return;
    }
    for (long long i = 0; i < len && *pos < kap - 6; i++) {
        unsigned char c = (unsigned char)ptr[i];
        if (c == '"') { buf[(*pos)++] = '\\'; buf[(*pos)++] = '"'; }
        else if (c == '\\') { buf[(*pos)++] = '\\'; buf[(*pos)++] = '\\'; }
        else if (c == '\n') { buf[(*pos)++] = '\\'; buf[(*pos)++] = 'n'; }
        else if (c == '\r') { buf[(*pos)++] = '\\'; buf[(*pos)++] = 'r'; }
        else if (c == '\t') { buf[(*pos)++] = '\\'; buf[(*pos)++] = 't'; }
        else if (c < 0x20) {
            /* Kontrol karakterleri: \uXXXX */
            int n = snprintf(buf + *pos, kap - *pos, "\\u%04x", c);
            *pos += n;
        }
        else { buf[(*pos)++] = c; }
    }
    if (*pos < kap) buf[(*pos)++] = '"';
}

/* Nesneyi JSON'a yaz */
static void json_nesne_yaz(char *buf, int *pos, int kap, long long nesne_ptr) {
    if (nesne_ptr == 0) {
        if (*pos + 2 <= kap) { buf[(*pos)++] = '{'; buf[(*pos)++] = '}'; }
        return;
    }

    long long *blok = (long long *)nesne_ptr;
    long long say = blok[2];

    buf[(*pos)++] = '{';
    for (long long i = 0; i < say && *pos < kap - 10; i++) {
        if (i > 0 && *pos < kap) buf[(*pos)++] = ',';

        long long idx = JSON_NESNE_META + i * JSON_NESNE_GIRDI;
        char *k = (char *)blok[idx];
        long long kl = blok[idx + 1];
        long long tip = blok[idx + 2];
        long long deger = blok[idx + 3];

        json_metin_yaz(buf, pos, kap, k, kl);
        if (*pos < kap) buf[(*pos)++] = ':';
        json_deger_yaz(buf, pos, kap, tip, deger);
    }
    if (*pos < kap) buf[(*pos)++] = '}';
}

/* Diziyi JSON'a yaz */
static void json_dizi_yaz(char *buf, int *pos, int kap, long long dizi_ptr) {
    if (dizi_ptr == 0) {
        if (*pos + 2 <= kap) { buf[(*pos)++] = '['; buf[(*pos)++] = ']'; }
        return;
    }

    long long *blok = (long long *)dizi_ptr;
    long long say = blok[2];

    buf[(*pos)++] = '[';
    for (long long i = 0; i < say && *pos < kap - 10; i++) {
        if (i > 0 && *pos < kap) buf[(*pos)++] = ',';

        long long idx = JSON_DIZI_META + i * JSON_DIZI_ELEMAN;
        long long tip = blok[idx];
        long long deger = blok[idx + 1];

        json_deger_yaz(buf, pos, kap, tip, deger);
    }
    if (*pos < kap) buf[(*pos)++] = ']';
}

/* Değeri JSON'a yaz */
static void json_deger_yaz(char *buf, int *pos, int kap, long long tip, long long deger) {
    switch (tip) {
        case JSON_TIP_NULL:
            if (*pos + 4 <= kap) {
                memcpy(buf + *pos, "null", 4);
                *pos += 4;
            }
            break;
        case JSON_TIP_MANTIK:
            if (deger) {
                if (*pos + 4 <= kap) { memcpy(buf + *pos, "true", 4); *pos += 4; }
            } else {
                if (*pos + 5 <= kap) { memcpy(buf + *pos, "false", 5); *pos += 5; }
            }
            break;
        case JSON_TIP_TAM: {
            char num[32];
            int n = snprintf(num, sizeof(num), "%lld", deger);
            if (*pos + n <= kap) { memcpy(buf + *pos, num, n); *pos += n; }
            break;
        }
        case JSON_TIP_ONDALIK: {
            double d;
            memcpy(&d, &deger, sizeof(double));
            char num[64];
            int n = snprintf(num, sizeof(num), "%.17g", d);
            if (*pos + n <= kap) { memcpy(buf + *pos, num, n); *pos += n; }
            break;
        }
        case JSON_TIP_METIN:
            if (deger != 0) {
                long long *metin_blok = (long long *)deger;
                char *str_ptr = (char *)metin_blok[0];
                long long str_len = metin_blok[1];
                json_metin_yaz(buf, pos, kap, str_ptr, str_len);
            } else {
                if (*pos + 2 <= kap) { buf[(*pos)++] = '"'; buf[(*pos)++] = '"'; }
            }
            break;
        case JSON_TIP_NESNE:
            json_nesne_yaz(buf, pos, kap, deger);
            break;
        case JSON_TIP_DIZI:
            json_dizi_yaz(buf, pos, kap, deger);
            break;
    }
}

/* json_olustur(tam nesne_veya_dizi_ptr) -> metin (JSON string) */
TrMetin _tr_json_olustur(long long ptr) {
    TrMetin m = {NULL, 0};

    if (ptr == 0) {
        m.ptr = (char *)malloc(3);
        if (m.ptr) { m.ptr[0] = '{'; m.ptr[1] = '}'; m.ptr[2] = '\0'; }
        m.len = 2;
        return m;
    }

    /* Stack-based buffer for small outputs, heap for large */
    char stack_buf[8192];
    char *buf = stack_buf;
    int kap = 8192;

    int pos = 0;

    long long *blok = (long long *)ptr;
    if (blok[0] == JSON_TIP_NESNE) {
        json_nesne_yaz(buf, &pos, kap, ptr);
    } else if (blok[0] == JSON_TIP_DIZI) {
        json_dizi_yaz(buf, &pos, kap, ptr);
    } else {
        /* Eski sözlük formatı için geriye uyumluluk */
        json_nesne_yaz(buf, &pos, kap, ptr);
    }

    /* Null terminate for safety */
    if (pos < kap) buf[pos] = '\0';

    m.ptr = (char *)malloc(pos + 1);
    if (m.ptr) {
        memcpy(m.ptr, buf, pos);
        m.ptr[pos] = '\0';
    }
    m.len = pos;
    return m;
}

/* json_guzel_olustur(ptr, girinti) -> metin (formatlı JSON) */
/* Bu fonksiyon ileride eklenebilir - şimdilik basit versiyon */
TrMetin _tr_json_guzel_olustur(long long ptr, long long girinti) {
    (void)girinti;
    return _tr_json_olustur(ptr);
}

/* ========== NESNE BİLGİ FONKSİYONLARI ========== */

/* json_nesne_uzunluk(nesne) -> tam */
long long _tr_json_nesne_uzunluk(long long nesne_ptr) {
    if (nesne_ptr == 0) return 0;
    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return 0;
    return blok[2];
}

/* json_nesne_anahtarlar(nesne) -> dizi<metin> */
TrDizi _tr_json_nesne_anahtarlar(long long nesne_ptr) {
    TrDizi d = {NULL, 0};
    if (nesne_ptr == 0) return d;

    long long *blok = (long long *)nesne_ptr;
    if (blok[0] != JSON_TIP_NESNE) return d;

    long long say = blok[2];
    if (say == 0) return d;

    long long boyut = say * 2 * sizeof(long long);
    long long *sonuç = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!sonuç) return d;

    for (long long i = 0; i < say; i++) {
        long long idx = JSON_NESNE_META + i * JSON_NESNE_GIRDI;
        sonuç[i * 2] = blok[idx];         /* anahtar ptr */
        sonuç[i * 2 + 1] = blok[idx + 1]; /* anahtar len */
    }
    d.ptr = sonuç;
    d.count = say;
    return d;
}
