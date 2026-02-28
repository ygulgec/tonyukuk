/* Düzeni (Regex) modülü — çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * Kapsamlı düzenli ifade desteği
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include "runtime.h"

/* ========== YARDIMCI FONKSİYONLAR ========== */

/* Metin'i null-terminated string'e çevir */
static char *metin_nul(const char *ptr, long long len) {
    char *buf = (char *)malloc(len + 1);
    if (buf) {
        memcpy(buf, ptr, len);
        buf[len] = '\0';
    }
    return buf;
}

/* Boş metin döndür */
static TrMetin bos_metin(void) {
    TrMetin m = {NULL, 0};
    return m;
}

/* Boş dizi döndür */
static TrDizi bos_dizi(void) {
    TrDizi d = {NULL, 0};
    return d;
}

/* String'den TrMetin oluştur */
static TrMetin cstr_metin(const char *str) {
    TrMetin m = {NULL, 0};
    if (!str) return m;
    int len = (int)strlen(str);
    m.ptr = (char *)malloc(len + 1);
    if (m.ptr) {
        memcpy(m.ptr, str, len);
        m.ptr[len] = '\0';
    }
    m.len = len;
    return m;
}

/* ========== TEMEL EŞLEŞME ========== */

/* eslesir_mi(metin, desen) -> mantık */
long long _tr_eslesir_mi(const char *m_ptr, long long m_len,
                          const char *d_ptr, long long d_len) {
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return 0;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED | REG_NOSUB) != 0) {
        free(metin);
        free(desen);
        return 0;
    }

    int sonuç = regexec(&reg, metin, 0, NULL, 0);
    regfree(&reg);
    free(metin);
    free(desen);
    return (sonuç == 0) ? 1 : 0;
}

/* eslesir_mi_buyuk_kucuk(metin, desen, buyuk_kucuk_duyarsiz) -> mantık */
long long _tr_eslesir_mi_bk(const char *m_ptr, long long m_len,
                             const char *d_ptr, long long d_len,
                             long long duyarsiz) {
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return 0;
    }

    regex_t reg;
    int flags = REG_EXTENDED | REG_NOSUB;
    if (duyarsiz) flags |= REG_ICASE;

    if (regcomp(&reg, desen, flags) != 0) {
        free(metin);
        free(desen);
        return 0;
    }

    int sonuç = regexec(&reg, metin, 0, NULL, 0);
    regfree(&reg);
    free(metin);
    free(desen);
    return (sonuç == 0) ? 1 : 0;
}

/* eslesme_sayisi(metin, desen) -> tam */
long long _tr_eslesme_sayisi(const char *m_ptr, long long m_len,
                              const char *d_ptr, long long d_len) {
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return 0;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        return 0;
    }

    long long sayac = 0;
    const char *p = metin;
    regmatch_t eslesme;
    while (regexec(&reg, p, 1, &eslesme, 0) == 0) {
        sayac++;
        p += eslesme.rm_eo;
        if (eslesme.rm_eo == 0) break;
    }

    regfree(&reg);
    free(metin);
    free(desen);
    return sayac;
}

/* ========== EŞLEŞME BULMA ========== */

/* eslesme_bul(metin, desen) -> metin (ilk eşleşme) */
TrMetin _tr_eslesme_bul(const char *m_ptr, long long m_len,
                         const char *d_ptr, long long d_len) {
    TrMetin m = bos_metin();
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return m;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        return m;
    }

    regmatch_t eslesme;
    if (regexec(&reg, metin, 1, &eslesme, 0) == 0) {
        int uzunluk = eslesme.rm_eo - eslesme.rm_so;
        m.ptr = (char *)malloc(uzunluk + 1);
        if (m.ptr) {
            memcpy(m.ptr, metin + eslesme.rm_so, uzunluk);
            m.ptr[uzunluk] = '\0';
        }
        m.len = uzunluk;
    }

    regfree(&reg);
    free(metin);
    free(desen);
    return m;
}

/* tum_eslesmeler(metin, desen) -> dizi<metin> */
TrDizi _tr_tum_eslesmeler(const char *m_ptr, long long m_len,
                           const char *d_ptr, long long d_len) {
    TrDizi sonuç = bos_dizi();
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return sonuç;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        return sonuç;
    }

    /* Geçici depolama */
    long long kap = 256;
    long long *veri = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, kap * 2 * sizeof(long long));
    if (!veri) {
        regfree(&reg);
        free(metin);
        free(desen);
        return sonuç;
    }

    long long sayac = 0;
    const char *p = metin;
    regmatch_t eslesme;
    while (regexec(&reg, p, 1, &eslesme, 0) == 0 && sayac < kap) {
        int uzunluk = eslesme.rm_eo - eslesme.rm_so;
        char *parca = (char *)malloc(uzunluk + 1);
        if (parca) {
            memcpy(parca, p + eslesme.rm_so, uzunluk);
            parca[uzunluk] = '\0';
        }
        veri[sayac * 2] = (long long)parca;
        veri[sayac * 2 + 1] = uzunluk;
        sayac++;
        p += eslesme.rm_eo;
        if (eslesme.rm_eo == 0) break;
    }

    regfree(&reg);
    free(metin);
    free(desen);
    sonuç.ptr = veri;
    sonuç.count = sayac;
    return sonuç;
}

/* ========== EŞLEŞME KONUMU ========== */

/* eslesme_konumu(metin, desen) -> tam (-1 bulunamadı) */
long long _tr_eslesme_konumu(const char *m_ptr, long long m_len,
                              const char *d_ptr, long long d_len) {
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return -1;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        return -1;
    }

    regmatch_t eslesme;
    long long konum = -1;
    if (regexec(&reg, metin, 1, &eslesme, 0) == 0) {
        konum = eslesme.rm_so;
    }

    regfree(&reg);
    free(metin);
    free(desen);
    return konum;
}

/* eslesme_sonu(metin, desen) -> tam (-1 bulunamadı) */
long long _tr_eslesme_sonu(const char *m_ptr, long long m_len,
                            const char *d_ptr, long long d_len) {
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return -1;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        return -1;
    }

    regmatch_t eslesme;
    long long konum = -1;
    if (regexec(&reg, metin, 1, &eslesme, 0) == 0) {
        konum = eslesme.rm_eo;
    }

    regfree(&reg);
    free(metin);
    free(desen);
    return konum;
}

/* ========== DEĞİŞTİRME ========== */

/* degistir(metin, desen, yeni) -> metin (ilk eşleşmeyi değiştir) */
TrMetin _tr_duzeni_degistir(const char *m_ptr, long long m_len,
                      const char *d_ptr, long long d_len,
                      const char *y_ptr, long long y_len) {
    TrMetin sonuç = bos_metin();
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    char *yeni = metin_nul(y_ptr, y_len);
    if (!metin || !desen || !yeni) {
        free(metin);
        free(desen);
        free(yeni);
        return sonuç;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        free(yeni);
        return sonuç;
    }

    regmatch_t eslesme;
    if (regexec(&reg, metin, 1, &eslesme, 0) == 0) {
        /* Eşleşme bulundu */
        long long yeni_uzunluk = eslesme.rm_so + y_len + (m_len - eslesme.rm_eo);
        sonuç.ptr = (char *)malloc(yeni_uzunluk + 1);
        if (sonuç.ptr) {
            /* Eşleşmeden önceki kısım */
            memcpy(sonuç.ptr, metin, eslesme.rm_so);
            /* Yeni değer */
            memcpy(sonuç.ptr + eslesme.rm_so, yeni, y_len);
            /* Eşleşmeden sonraki kısım */
            memcpy(sonuç.ptr + eslesme.rm_so + y_len,
                   metin + eslesme.rm_eo, m_len - eslesme.rm_eo);
            sonuç.ptr[yeni_uzunluk] = '\0';
            sonuç.len = yeni_uzunluk;
        }
    } else {
        /* Eşleşme yok, orijinali döndür */
        sonuç.ptr = (char *)malloc(m_len + 1);
        if (sonuç.ptr) {
            memcpy(sonuç.ptr, metin, m_len);
            sonuç.ptr[m_len] = '\0';
            sonuç.len = m_len;
        }
    }

    regfree(&reg);
    free(metin);
    free(desen);
    free(yeni);
    return sonuç;
}

/* degistir_hepsi(metin, desen, yeni) -> metin (tüm eşleşmeleri değiştir) */
TrMetin _tr_degistir_hepsi(const char *m_ptr, long long m_len,
                            const char *d_ptr, long long d_len,
                            const char *y_ptr, long long y_len) {
    TrMetin sonuç = bos_metin();
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    char *yeni = metin_nul(y_ptr, y_len);
    if (!metin || !desen || !yeni) {
        free(metin);
        free(desen);
        free(yeni);
        return sonuç;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        free(yeni);
        return sonuç;
    }

    /* Sonuç buffer */
    long long buf_kap = m_len * 2 + 256;
    char *buf = (char *)malloc(buf_kap);
    if (!buf) {
        regfree(&reg);
        free(metin);
        free(desen);
        free(yeni);
        return sonuç;
    }

    long long buf_pos = 0;
    const char *p = metin;
    regmatch_t eslesme;

    while (regexec(&reg, p, 1, &eslesme, 0) == 0) {
        /* Eşleşmeden önceki kısım */
        if (buf_pos + eslesme.rm_so + y_len + 1 > buf_kap) {
            buf_kap *= 2;
            char *yeni_buf = (char *)realloc(buf, buf_kap);
            if (!yeni_buf) break;
            buf = yeni_buf;
        }

        memcpy(buf + buf_pos, p, eslesme.rm_so);
        buf_pos += eslesme.rm_so;

        /* Yeni değer */
        memcpy(buf + buf_pos, yeni, y_len);
        buf_pos += y_len;

        p += eslesme.rm_eo;
        if (eslesme.rm_eo == 0) break;
    }

    /* Kalan kısım */
    long long kalan = strlen(p);
    if (buf_pos + kalan + 1 > buf_kap) {
        char *yeni_buf = (char *)realloc(buf, buf_pos + kalan + 1);
        if (yeni_buf) buf = yeni_buf;
    }
    memcpy(buf + buf_pos, p, kalan);
    buf_pos += kalan;
    buf[buf_pos] = '\0';

    sonuç.ptr = (char *)malloc(buf_pos + 1);
    if (sonuç.ptr) {
        memcpy(sonuç.ptr, buf, buf_pos + 1);
        sonuç.len = buf_pos;
    }

    free(buf);
    regfree(&reg);
    free(metin);
    free(desen);
    free(yeni);
    return sonuç;
}

/* ========== AYIRMA ========== */

/* bol(metin, desen) -> dizi<metin> (regex ile böl) */
TrDizi _tr_duzeni_bol(const char *m_ptr, long long m_len,
               const char *d_ptr, long long d_len) {
    TrDizi sonuç = bos_dizi();
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return sonuç;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        return sonuç;
    }

    /* Geçici depolama */
    long long kap = 256;
    long long *veri = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, kap * 2 * sizeof(long long));
    if (!veri) {
        regfree(&reg);
        free(metin);
        free(desen);
        return sonuç;
    }

    long long sayac = 0;
    const char *p = metin;
    const char *son = metin + m_len;
    regmatch_t eslesme;

    while (regexec(&reg, p, 1, &eslesme, 0) == 0 && sayac < kap - 1) {
        /* Eşleşmeden önceki parça */
        int uzunluk = eslesme.rm_so;
        char *parca = (char *)malloc(uzunluk + 1);
        if (parca) {
            memcpy(parca, p, uzunluk);
            parca[uzunluk] = '\0';
        }
        veri[sayac * 2] = (long long)parca;
        veri[sayac * 2 + 1] = uzunluk;
        sayac++;

        p += eslesme.rm_eo;
        if (eslesme.rm_eo == 0) p++; /* Boş eşleşme durumunda ilerle */
        if (p >= son) break;
    }

    /* Son parça */
    if (sayac < kap && p <= son) {
        int uzunluk = (int)(son - p);
        char *parca = (char *)malloc(uzunluk + 1);
        if (parca) {
            memcpy(parca, p, uzunluk);
            parca[uzunluk] = '\0';
        }
        veri[sayac * 2] = (long long)parca;
        veri[sayac * 2 + 1] = uzunluk;
        sayac++;
    }

    regfree(&reg);
    free(metin);
    free(desen);
    sonuç.ptr = veri;
    sonuç.count = sayac;
    return sonuç;
}

/* ========== YAKALAMA GRUPLARI ========== */

/* gruplar(metin, desen) -> dizi<metin> (yakalama grupları) */
TrDizi _tr_gruplar(const char *m_ptr, long long m_len,
                   const char *d_ptr, long long d_len) {
    TrDizi sonuç = bos_dizi();
    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return sonuç;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        return sonuç;
    }

    /* Maksimum 16 grup */
    #define MAKS_GRUP 16
    regmatch_t eslesme[MAKS_GRUP];

    if (regexec(&reg, metin, MAKS_GRUP, eslesme, 0) != 0) {
        regfree(&reg);
        free(metin);
        free(desen);
        return sonuç;
    }

    /* Kaç grup eşleşti say */
    int grup_sayisi = 0;
    for (int i = 0; i < MAKS_GRUP && eslesme[i].rm_so != -1; i++) {
        grup_sayisi++;
    }

    long long *veri = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI,
                                                       grup_sayisi * 2 * sizeof(long long));
    if (!veri) {
        regfree(&reg);
        free(metin);
        free(desen);
        return sonuç;
    }

    for (int i = 0; i < grup_sayisi; i++) {
        int uzunluk = eslesme[i].rm_eo - eslesme[i].rm_so;
        char *parca = (char *)malloc(uzunluk + 1);
        if (parca) {
            memcpy(parca, metin + eslesme[i].rm_so, uzunluk);
            parca[uzunluk] = '\0';
        }
        veri[i * 2] = (long long)parca;
        veri[i * 2 + 1] = uzunluk;
    }

    regfree(&reg);
    free(metin);
    free(desen);
    sonuç.ptr = veri;
    sonuç.count = grup_sayisi;
    return sonuç;
}

/* grup_al(metin, desen, grup_no) -> metin */
TrMetin _tr_grup_al(const char *m_ptr, long long m_len,
                    const char *d_ptr, long long d_len,
                    long long grup_no) {
    TrMetin sonuç = bos_metin();
    if (grup_no < 0 || grup_no >= 16) return sonuç;

    char *metin = metin_nul(m_ptr, m_len);
    char *desen = metin_nul(d_ptr, d_len);
    if (!metin || !desen) {
        free(metin);
        free(desen);
        return sonuç;
    }

    regex_t reg;
    if (regcomp(&reg, desen, REG_EXTENDED) != 0) {
        free(metin);
        free(desen);
        return sonuç;
    }

    regmatch_t eslesme[16];
    if (regexec(&reg, metin, 16, eslesme, 0) == 0 && eslesme[grup_no].rm_so != -1) {
        int uzunluk = eslesme[grup_no].rm_eo - eslesme[grup_no].rm_so;
        sonuç.ptr = (char *)malloc(uzunluk + 1);
        if (sonuç.ptr) {
            memcpy(sonuç.ptr, metin + eslesme[grup_no].rm_so, uzunluk);
            sonuç.ptr[uzunluk] = '\0';
            sonuç.len = uzunluk;
        }
    }

    regfree(&reg);
    free(metin);
    free(desen);
    return sonuç;
}

/* ========== DESEN DOĞRULAMA ========== */

/* gecerli_desen_mi(desen) -> mantık */
long long _tr_gecerli_desen_mi(const char *d_ptr, long long d_len) {
    char *desen = metin_nul(d_ptr, d_len);
    if (!desen) return 0;

    regex_t reg;
    int sonuç = regcomp(&reg, desen, REG_EXTENDED | REG_NOSUB);
    if (sonuç == 0) {
        regfree(&reg);
    }
    free(desen);
    return (sonuç == 0) ? 1 : 0;
}

/* ========== ÖZEL EŞLEŞMELER ========== */

/* baslangicta_eslesir_mi(metin, desen) -> mantık */
long long _tr_baslangicta_eslesir_mi(const char *m_ptr, long long m_len,
                                       const char *d_ptr, long long d_len) {
    /* Desenin başına ^ ekle */
    char *desen_buf = (char *)malloc(d_len + 2);
    if (!desen_buf) return 0;
    desen_buf[0] = '^';
    memcpy(desen_buf + 1, d_ptr, d_len);
    desen_buf[d_len + 1] = '\0';

    long long sonuç = _tr_eslesir_mi(m_ptr, m_len, desen_buf, d_len + 1);
    free(desen_buf);
    return sonuç;
}

/* sonunda_eslesir_mi(metin, desen) -> mantık */
long long _tr_sonunda_eslesir_mi(const char *m_ptr, long long m_len,
                                   const char *d_ptr, long long d_len) {
    /* Desenin sonuna $ ekle */
    char *desen_buf = (char *)malloc(d_len + 2);
    if (!desen_buf) return 0;
    memcpy(desen_buf, d_ptr, d_len);
    desen_buf[d_len] = '$';
    desen_buf[d_len + 1] = '\0';

    long long sonuç = _tr_eslesir_mi(m_ptr, m_len, desen_buf, d_len + 1);
    free(desen_buf);
    return sonuç;
}

/* tam_eslesir_mi(metin, desen) -> mantık (tüm metin eşleşmeli) */
long long _tr_tam_eslesir_mi(const char *m_ptr, long long m_len,
                               const char *d_ptr, long long d_len) {
    /* Desenin başına ^ ve sonuna $ ekle */
    char *desen_buf = (char *)malloc(d_len + 3);
    if (!desen_buf) return 0;
    desen_buf[0] = '^';
    memcpy(desen_buf + 1, d_ptr, d_len);
    desen_buf[d_len + 1] = '$';
    desen_buf[d_len + 2] = '\0';

    long long sonuç = _tr_eslesir_mi(m_ptr, m_len, desen_buf, d_len + 2);
    free(desen_buf);
    return sonuç;
}

/* ========== ÖZEL KARAKTERLER ========== */

/* kacis_karakterleri(metin) -> metin: Özel regex karakterlerini escape et */
TrMetin _tr_kacis_karakterleri(const char *m_ptr, long long m_len) {
    TrMetin sonuç = bos_metin();

    /* En kötü durumda her karakter escape edilir */
    char *buf = (char *)malloc(m_len * 2 + 1);
    if (!buf) return sonuç;

    long long pos = 0;
    for (long long i = 0; i < m_len; i++) {
        char c = m_ptr[i];
        /* Özel regex karakterleri */
        if (c == '.' || c == '*' || c == '+' || c == '?' ||
            c == '[' || c == ']' || c == '(' || c == ')' ||
            c == '{' || c == '}' || c == '|' || c == '^' ||
            c == '$' || c == '\\') {
            buf[pos++] = '\\';
        }
        buf[pos++] = c;
    }
    buf[pos] = '\0';

    sonuç.ptr = (char *)malloc(pos + 1);
    if (sonuç.ptr) {
        memcpy(sonuç.ptr, buf, pos + 1);
        sonuç.len = pos;
    }
    free(buf);
    return sonuç;
}
