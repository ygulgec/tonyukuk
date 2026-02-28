/* Metin modülü — çalışma zamanı implementasyonu */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "runtime.h"

/* Forward declarations for cross-references */
extern TrMetin _tr_tam_metin(long long sayi);
extern TrMetin _tr_ondalik_metin(double sayi);

/* ---- Ek String Islemleri ---- */

/* Metin -> ondalik */
double _tr_metin_ondalik(const char *ptr, long long len) {
    char buf[64];
    int n = (int)len;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    memcpy(buf, ptr, n);
    buf[n] = '\0';
    return atof(buf);
}

/* Metin kirp (bosluk karakterlerini bastan/sondan sil) */
TrMetin _tr_kirp(const char *ptr, long long len) {
    TrMetin m = {NULL, 0};
    int bas = 0, son = (int)len - 1;
    while (bas <= son && (ptr[bas] == ' ' || ptr[bas] == '\t' || ptr[bas] == '\n' || ptr[bas] == '\r'))
        bas++;
    while (son >= bas && (ptr[son] == ' ' || ptr[son] == '\t' || ptr[son] == '\n' || ptr[son] == '\r'))
        son--;
    int yeni_len = son - bas + 1;
    if (yeni_len <= 0) { m.len = 0; m.ptr = NULL; return m; }
    m.ptr = (char *)malloc(yeni_len);
    if (m.ptr) memcpy(m.ptr, ptr + bas, yeni_len);
    m.len = yeni_len;
    return m;
}

/* Metin tersle (UTF-8 karakter tabanli) */
TrMetin _tr_utf8_tersle(const char *ptr, long long byte_len);  /* forward decl */
TrMetin _tr_tersle(const char *ptr, long long len) {
    return _tr_utf8_tersle(ptr, len);
}

/* Metin tekrarla */
TrMetin _tr_tekrarla(const char *ptr, long long len, long long kez) {
    TrMetin m = {NULL, 0};
    if (kez <= 0) return m;
    long long toplam = len * kez;
    m.ptr = (char *)malloc(toplam);
    if (m.ptr) {
        for (long long i = 0; i < kez; i++)
            memcpy(m.ptr + i * len, ptr, len);
    }
    m.len = toplam;
    return m;
}

/* Metin baslar_mi (startsWith) */
long long _tr_baslar_mi(const char *ptr, long long len,
                         const char *onek_ptr, long long onek_len) {
    if (onek_len > len) return 0;
    return memcmp(ptr, onek_ptr, onek_len) == 0 ? 1 : 0;
}

/* Metin biter_mi (endsWith) */
long long _tr_biter_mi(const char *ptr, long long len,
                        const char *sonek_ptr, long long sonek_len) {
    if (sonek_len > len) return 0;
    return memcmp(ptr + len - sonek_len, sonek_ptr, sonek_len) == 0 ? 1 : 0;
}

/* Metin degistir (replace first) */
TrMetin _tr_degistir(const char *ptr, long long len,
                      const char *eski_ptr, long long eski_len,
                      const char *yeni_ptr, long long yeni_len) {
    TrMetin m = {NULL, 0};
    /* Eski metni bul */
    for (long long i = 0; i <= len - eski_len; i++) {
        if (memcmp(ptr + i, eski_ptr, eski_len) == 0) {
            long long toplam = len - eski_len + yeni_len;
            m.ptr = (char *)malloc(toplam);
            if (m.ptr) {
                memcpy(m.ptr, ptr, i);
                memcpy(m.ptr + i, yeni_ptr, yeni_len);
                memcpy(m.ptr + i + yeni_len, ptr + i + eski_len, len - i - eski_len);
            }
            m.len = toplam;
            return m;
        }
    }
    /* Bulunamazsa kopyala */
    m.ptr = (char *)malloc(len);
    if (m.ptr) memcpy(m.ptr, ptr, len);
    m.len = len;
    return m;
}
/* === Tip Donusum === */

/* tam_don(metin) -> tam  -- metin_tam ile ayni, farkli isim */
long long _tr_tam_don(const char *ptr, long long len) {
    char buf[64];
    int n = (int)len;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    memcpy(buf, ptr, n);
    buf[n] = '\0';
    return atoll(buf);
}

/* ondalik_don(metin) -> ondalik */
double _tr_ondalik_don(const char *ptr, long long len) {
    char buf[64];
    int n = (int)len;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    memcpy(buf, ptr, n);
    buf[n] = '\0';
    return atof(buf);
}

/* metin_don_tam(tam) -> metin  -- tam sayiyi metne cevir */
TrMetin _tr_metin_don_tam(long long sayi) {
    return _tr_tam_metin(sayi);
}

/* metin_don_ondalik(ondalik) -> metin */
TrMetin _tr_metin_don_ondalik(double sayi) {
    return _tr_ondalik_metin(sayi);
}

/* metin_don_mantik(mantik) -> metin */
TrMetin _tr_metin_don_mantik(long long deger) {
    TrMetin m;
    if (deger) {
        m.ptr = (char *)malloc(6);
        if (m.ptr) memcpy(m.ptr, "do\xc4\x9fru", 6);
        m.len = 6;
    } else {
        m.ptr = (char *)malloc(7);
        if (m.ptr) memcpy(m.ptr, "yanl\xc4\xb1\xc5\x9f", 7);
        m.len = 7;
    }
    return m;
}
/* === Gelismis Metin Islemleri === */

/* bol(metin, ayirici) -> TrDizi (metin parcalari olarak diziler dondurur) */
/* Not: Bu fonksiyon her parcayi ayri bir metin olarak heap'te saklar.
   Her eleman = (ptr<<32 | len) olarak tek bir long long'a paketlenir.
   Basitlik icin: her parca icin 2 slot kullanilir (ptr, len).
   Geri donus: ptr=dizi_ptr, count=parca_sayisi*2  */

/* Basitlesmis bol: sadece parca sayisini dondurur */
/* Gercek implementasyon: her parca icin heap'te 16 byte (ptr+len) tutar */
TrDizi _tr_bol(const char *ptr, long long len,
               const char *ayirici_ptr, long long ayirici_len) {
    /* Parca sayisini hesapla */
    long long parca = 1;
    for (long long i = 0; i <= len - ayirici_len; i++) {
        if (memcmp(ptr + i, ayirici_ptr, ayirici_len) == 0) {
            parca++;
            i += ayirici_len - 1;
        }
    }

    /* Dizi icin bellek ayir: her eleman bir tam sayi (metin ptr) */
    long long boyut = parca * 2 * sizeof(long long);  /* ptr + len ciftleri */
    long long *sonuç = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!sonuç) {
        TrDizi d = {NULL, 0};
        return d;
    }

    long long idx = 0;
    long long başlangıç = 0;
    for (long long i = 0; i <= len - ayirici_len; i++) {
        if (memcmp(ptr + i, ayirici_ptr, ayirici_len) == 0) {
            long long parca_len = i - başlangıç;
            char *parca_ptr = (char *)malloc(parca_len);
            if (parca_ptr) memcpy(parca_ptr, ptr + başlangıç, parca_len);
            sonuç[idx * 2] = (long long)parca_ptr;
            sonuç[idx * 2 + 1] = parca_len;
            idx++;
            başlangıç = i + ayirici_len;
            i += ayirici_len - 1;
        }
    }
    /* Son parca */
    {
        long long parca_len = len - başlangıç;
        char *parca_ptr = (char *)malloc(parca_len);
        if (parca_ptr) memcpy(parca_ptr, ptr + başlangıç, parca_len);
        sonuç[idx * 2] = (long long)parca_ptr;
        sonuç[idx * 2 + 1] = parca_len;
    }

    TrDizi d;
    d.ptr = sonuç;
    d.count = parca;
    return d;
}

/* birlestir_metin(dizi, ayirici) -> metin */
/* Dizi elemanlari: her eleman (ptr, len) cifti. count = eleman sayisi.
   Dizideki her i-th eleman icin: ptr_array[i*2]=ptr, ptr_array[i*2+1]=len */
TrMetin _tr_birlestir_metin(long long *dizi_ptr, long long dizi_count,
                              const char *ayirici_ptr, long long ayirici_len) {
    TrMetin m = {NULL, 0};
    if (dizi_count <= 0) return m;

    /* Toplam uzunlugu hesapla */
    long long toplam = 0;
    for (long long i = 0; i < dizi_count; i++) {
        toplam += dizi_ptr[i * 2 + 1];  /* len */
        if (i < dizi_count - 1) toplam += ayirici_len;
    }

    m.ptr = (char *)malloc(toplam);
    if (!m.ptr) return m;
    m.len = toplam;

    long long pos = 0;
    for (long long i = 0; i < dizi_count; i++) {
        char *p = (char *)dizi_ptr[i * 2];
        long long l = dizi_ptr[i * 2 + 1];
        memcpy(m.ptr + pos, p, l);
        pos += l;
        if (i < dizi_count - 1) {
            memcpy(m.ptr + pos, ayirici_ptr, ayirici_len);
            pos += ayirici_len;
        }
    }
    return m;
}

/* === Metin metotlari (string methods) === */

/* metin.say(alt_metin) -> tam (alt metin kac kez gectigini say) */
long long _tr_metin_say(const char *ptr, long long len,
                         const char *alt_ptr, long long alt_len) {
    if (alt_len <= 0 || alt_len > len) return 0;
    long long sayac = 0;
    for (long long i = 0; i <= len - alt_len; i++) {
        if (memcmp(ptr + i, alt_ptr, (size_t)alt_len) == 0) {
            sayac++;
            i += alt_len - 1;  /* overlap olmadan say */
        }
    }
    return sayac;
}

/* metin.bosmu() -> mantik */
long long _tr_metin_bosmu(const char *ptr, long long len) {
    (void)ptr;
    return len == 0 ? 1 : 0;
}

/* metin.rakammi() -> mantik (tum karakterler rakam mi) */
long long _tr_metin_rakammi(const char *ptr, long long len) {
    if (len == 0) return 0;
    for (long long i = 0; i < len; i++) {
        if (ptr[i] < '0' || ptr[i] > '9') return 0;
    }
    return 1;
}

/* metin.harfmi() -> mantik (tum karakterler harf mi, ASCII) */
long long _tr_metin_harfmi(const char *ptr, long long len) {
    if (len == 0) return 0;
    for (long long i = 0; i < len; i++) {
        char c = ptr[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (unsigned char)c >= 0xC0)) return 0;  /* UTF-8 harf olabilir */
    }
    return 1;
}

/* metin.boslukmu() -> mantik (tum karakterler bosluk mu) */
long long _tr_metin_boslukmu(const char *ptr, long long len) {
    if (len == 0) return 0;
    for (long long i = 0; i < len; i++) {
        if (ptr[i] != ' ' && ptr[i] != '\t' && ptr[i] != '\n' && ptr[i] != '\r') return 0;
    }
    return 1;
}

/* metin.icerir(alt_metin) -> mantik */
long long _tr_metin_icerir(const char *ptr, long long len,
                            const char *alt_ptr, long long alt_len) {
    if (alt_len <= 0) return 1;
    if (alt_len > len) return 0;
    for (long long i = 0; i <= len - alt_len; i++) {
        if (memcmp(ptr + i, alt_ptr, (size_t)alt_len) == 0) return 1;
    }
    return 0;
}

/* biçimle() — format specifiers */
/* Tam sayı biçimlendirme: biçimle(42, "05") → "00042" */
TrMetin _tr_bicimle_tam(long long deger, const char *spec, long long spec_len) {
    char fmt[64] = "%";
    int fi = 1;
    /* spec ayrıştır: [dolgu_karakter][genişlik] */
    int i = 0;
    char dolgu = ' ';
    int genislik = 0;

    if (i < spec_len && spec[i] == '0') { dolgu = '0'; i++; }
    else if (i < spec_len && (spec[i] == '<' || spec[i] == '>' || spec[i] == '^')) i++;

    while (i < spec_len && spec[i] >= '0' && spec[i] <= '9') {
        genislik = genislik * 10 + (spec[i] - '0');
        i++;
    }

    if (dolgu == '0' && genislik > 0) { fmt[fi++] = '0'; }
    if (genislik > 0) { fi += snprintf(fmt + fi, sizeof(fmt) - fi, "%d", genislik); }
    fmt[fi++] = 'l'; fmt[fi++] = 'l'; fmt[fi++] = 'd'; fmt[fi] = '\0';

    char buf[128];
    int n = snprintf(buf, sizeof(buf), fmt, deger);
    char *sonuç = malloc(n + 1);
    if (sonuç) memcpy(sonuç, buf, n + 1);
    TrMetin m = {sonuç, n};
    return m;
}

/* Ondalık biçimlendirme: biçimle(3.14, ".2f") → "3.14" */
TrMetin _tr_bicimle_ondalik(double deger, const char *spec, long long spec_len) {
    char fmt[64] = "%";
    int fi = 1;
    int i = 0;
    int hassasiyet = -1;
    int genislik = 0;

    /* Genişlik */
    while (i < spec_len && spec[i] >= '0' && spec[i] <= '9') {
        genislik = genislik * 10 + (spec[i] - '0');
        i++;
    }
    /* Hassasiyet */
    if (i < spec_len && spec[i] == '.') {
        i++;
        hassasiyet = 0;
        while (i < spec_len && spec[i] >= '0' && spec[i] <= '9') {
            hassasiyet = hassasiyet * 10 + (spec[i] - '0');
            i++;
        }
    }

    if (genislik > 0) { fi += snprintf(fmt + fi, sizeof(fmt) - fi, "%d", genislik); }
    if (hassasiyet >= 0) { fi += snprintf(fmt + fi, sizeof(fmt) - fi, ".%d", hassasiyet); }
    /* f veya e tipi */
    char tip = 'f';
    if (i < spec_len && (spec[i] == 'f' || spec[i] == 'e' || spec[i] == 'g')) tip = spec[i];
    fmt[fi++] = tip; fmt[fi] = '\0';

    char buf[128];
    int n = snprintf(buf, sizeof(buf), fmt, deger);
    char *sonuç = malloc(n + 1);
    if (sonuç) memcpy(sonuç, buf, n + 1);
    TrMetin m = {sonuç, n};
    return m;
}

/* Metin biçimlendirme: biçimle("abc", ">10") → "       abc" */
TrMetin _tr_bicimle_metin(const char *ptr, long long len,
                           const char *spec, long long spec_len) {
    int i = 0;
    char hiza = '>'; /* varsayılan sağa hizala */
    char dolgu = ' ';
    int genislik = 0;

    if (i < spec_len && (spec[i] == '<' || spec[i] == '>' || spec[i] == '^')) {
        hiza = spec[i]; i++;
    }
    while (i < spec_len && spec[i] >= '0' && spec[i] <= '9') {
        genislik = genislik * 10 + (spec[i] - '0');
        i++;
    }

    if (genislik <= len) {
        /* Genişlik metin uzunluğundan küçükse olduğu gibi döndür */
        char *sonuç = malloc(len);
        if (sonuç) memcpy(sonuç, ptr, len);
        TrMetin m = {sonuç, len};
        return m;
    }

    char *sonuç = malloc(genislik);
    if (!sonuç) { TrMetin m = {NULL, 0}; return m; }
    int bosluk = genislik - len;
    if (hiza == '<') {
        memcpy(sonuç, ptr, len);
        memset(sonuç + len, dolgu, bosluk);
    } else if (hiza == '>') {
        memset(sonuç, dolgu, bosluk);
        memcpy(sonuç + bosluk, ptr, len);
    } else { /* ^ */
        int sol = bosluk / 2;
        int sag = bosluk - sol;
        memset(sonuç, dolgu, sol);
        memcpy(sonuç + sol, ptr, len);
        memset(sonuç + sol + len, dolgu, sag);
    }
    TrMetin m = {sonuç, genislik};
    return m;
}

/* metin.birlestir(dizi) -> metin (join: ayirici.birlestir(dizi)) */
TrMetin _tr_metin_birlestir_ayirici(const char *ayirici_ptr, long long ayirici_len,
                                      long long *dizi_ptr, long long dizi_count) {
    TrMetin m = {NULL, 0};
    if (dizi_count <= 0) return m;
    /* Toplam uzunlugu hesapla */
    /* Her dizi elemani bir metin fat ptr cifti: ptr=dizi_ptr[i*2], len=dizi_ptr[i*2+1] (sıkı paketlenmemiş)
       Aslinda dizi elemanlari tek bir long long, metin ptr+len iki slot.
       Ama mevcut dizi yapısında elemanlar tek long long, metin fat ptr ise 2 slot.
       Basit yaklasim: her eleman bir TrMetin struct */
    /* Not: Tonyukuk'ta metin dizileri her eleman icin 2 slot (ptr, len) tutar.
       Ama runtime seviyesinde dizi her eleman 8 byte.
       Bu fonksiyon simdilik dizi elemanlari sayisal olarak davranir.
       Daha iyi yaklasim: bu fonksiyonu kodgen seviyesinde ozel handle edelim. */
    /* Simdilik stub olarak bos dondur */
    return m;
}

/* UTF-8 yardimci fonksiyonlari (inline) */
static uint32_t _utf8_oku(const char *s, int *pos) {
    unsigned char c = (unsigned char)s[*pos];
    uint32_t kod; int uz;
    if (c < 0x80) { kod = c; uz = 1; }
    else if ((c & 0xE0) == 0xC0) { kod = c & 0x1F; uz = 2; }
    else if ((c & 0xF0) == 0xE0) { kod = c & 0x0F; uz = 3; }
    else if ((c & 0xF8) == 0xF0) { kod = c & 0x07; uz = 4; }
    else { (*pos)++; return c; }
    for (int i = 1; i < uz; i++) kod = (kod << 6) | ((unsigned char)s[*pos + i] & 0x3F);
    *pos += uz;
    return kod;
}
static int _utf8_yaz(char *h, uint32_t k) {
    if (k < 0x80) { h[0] = (char)k; return 1; }
    if (k < 0x800) { h[0] = (char)(0xC0|(k>>6)); h[1] = (char)(0x80|(k&0x3F)); return 2; }
    if (k < 0x10000) { h[0]=(char)(0xE0|(k>>12)); h[1]=(char)(0x80|((k>>6)&0x3F)); h[2]=(char)(0x80|(k&0x3F)); return 3; }
    h[0]=(char)(0xF0|(k>>18)); h[1]=(char)(0x80|((k>>12)&0x3F)); h[2]=(char)(0x80|((k>>6)&0x3F)); h[3]=(char)(0x80|(k&0x3F)); return 4;
}
static uint32_t _turkce_buyuk(uint32_t k) {
    if (k == 'i') return 0x0130;   /* i -> İ */
    if (k == 0x0131) return 'I';   /* ı -> I */
    if (k >= 'a' && k <= 'z') return k - 32;
    if (k == 0x00E7) return 0x00C7; /* ç -> Ç */
    if (k == 0x011F) return 0x011E; /* ğ -> Ğ */
    if (k == 0x00F6) return 0x00D6; /* ö -> Ö */
    if (k == 0x015F) return 0x015E; /* ş -> Ş */
    if (k == 0x00FC) return 0x00DC; /* ü -> Ü */
    return k;
}
static uint32_t _turkce_kucuk(uint32_t k) {
    if (k == 'I') return 0x0131;   /* I -> ı */
    if (k == 0x0130) return 'i';   /* İ -> i */
    if (k >= 'A' && k <= 'Z') return k + 32;
    if (k == 0x00C7) return 0x00E7; /* Ç -> ç */
    if (k == 0x011E) return 0x011F; /* Ğ -> ğ */
    if (k == 0x00D6) return 0x00F6; /* Ö -> ö */
    if (k == 0x015E) return 0x015F; /* Ş -> ş */
    if (k == 0x00DC) return 0x00FC; /* Ü -> ü */
    return k;
}

/* buyuk_harf(metin) -> metin  (UTF-8 Turkce destekli) */
TrMetin _tr_buyuk_harf(const char *ptr, long long len) {
    TrMetin m;
    m.ptr = (char *)malloc(len * 2 + 1);
    m.len = 0;
    if (m.ptr) {
        int pos = 0;
        while (pos < (int)len) {
            uint32_t kod = _utf8_oku(ptr, &pos);
            kod = _turkce_buyuk(kod);
            m.len += _utf8_yaz(m.ptr + m.len, kod);
        }
    }
    return m;
}

/* kucuk_harf(metin) -> metin (UTF-8 Turkce destekli) */
TrMetin _tr_kucuk_harf(const char *ptr, long long len) {
    TrMetin m;
    m.ptr = (char *)malloc(len * 2 + 1);
    m.len = 0;
    if (m.ptr) {
        int pos = 0;
        while (pos < (int)len) {
            uint32_t kod = _utf8_oku(ptr, &pos);
            kod = _turkce_kucuk(kod);
            m.len += _utf8_yaz(m.ptr + m.len, kod);
        }
    }
    return m;
}

/* ==== UTF-8 Karakter Tabanli Metin Fonksiyonlari ==== */

/* Karakter sayisi (codepoint) */
long long _tr_utf8_karakter_say(const char *ptr, long long byte_len) {
    long long sayac = 0;
    int pos = 0;
    while (pos < (int)byte_len) {
        _utf8_oku(ptr, &pos);
        sayac++;
    }
    return sayac;
}

/* Karakter indeksini byte offset'e cevir. Negatif indeks destekli.
   Sinir disiysa -1 (negatif asim) veya byte_len (pozitif asim) dondurur. */
long long _tr_utf8_karakter_byte_offset(const char *ptr, long long byte_len,
                                          long long karakter_indeks) {
    if (karakter_indeks < 0) {
        long long toplam = _tr_utf8_karakter_say(ptr, byte_len);
        karakter_indeks = toplam + karakter_indeks;
        if (karakter_indeks < 0) return -1;
    }
    int pos = 0;
    long long sayac = 0;
    while (pos < (int)byte_len && sayac < karakter_indeks) {
        _utf8_oku(ptr, &pos);
        sayac++;
    }
    if (sayac < karakter_indeks) return byte_len;
    return (long long)pos;
}

/* i. karakteri tek karakterlik metin olarak dondur.
   Negatif indeks destekler. Sinir disiysa bos metin dondurur. */
TrMetin _tr_utf8_karakter_al(const char *ptr, long long byte_len,
                               long long karakter_indeks) {
    TrMetin m = {NULL, 0};
    if (karakter_indeks < 0) {
        long long toplam = _tr_utf8_karakter_say(ptr, byte_len);
        karakter_indeks = toplam + karakter_indeks;
    }
    if (karakter_indeks < 0) return m;

    int pos = 0;
    long long sayac = 0;
    while (pos < (int)byte_len && sayac < karakter_indeks) {
        _utf8_oku(ptr, &pos);
        sayac++;
    }
    if (pos >= (int)byte_len) return m;

    int başlangıç = pos;
    _utf8_oku(ptr, &pos);
    int byte_say = pos - başlangıç;

    m.ptr = (char *)malloc(byte_say);
    if (m.ptr) memcpy(m.ptr, ptr + başlangıç, byte_say);
    m.len = byte_say;
    return m;
}

/* Karakter tabanli dilim: metin[bas:son]
   Negatif indeks destekler. Sinir clamping yapar. */
TrMetin _tr_utf8_dilim(const char *ptr, long long byte_len,
                         long long karakter_bas, long long karakter_son) {
    TrMetin m = {NULL, 0};
    long long toplam = _tr_utf8_karakter_say(ptr, byte_len);

    if (karakter_bas < 0) karakter_bas = toplam + karakter_bas;
    if (karakter_son < 0) karakter_son = toplam + karakter_son;
    if (karakter_bas < 0) karakter_bas = 0;
    if (karakter_son > toplam) karakter_son = toplam;
    if (karakter_bas >= karakter_son) return m;

    int pos = 0;
    long long sayac = 0;
    while (pos < (int)byte_len && sayac < karakter_bas) {
        _utf8_oku(ptr, &pos);
        sayac++;
    }
    int byte_bas = pos;

    while (pos < (int)byte_len && sayac < karakter_son) {
        _utf8_oku(ptr, &pos);
        sayac++;
    }
    int byte_son = pos;

    long long yeni_len = byte_son - byte_bas;
    if (yeni_len <= 0) return m;
    m.ptr = (char *)malloc(yeni_len);
    if (m.ptr) memcpy(m.ptr, ptr + byte_bas, yeni_len);
    m.len = yeni_len;
    return m;
}

/* Karakter tabanli tersle (UTF-8 codepoint sirasini ters cevirir) */
TrMetin _tr_utf8_tersle(const char *ptr, long long byte_len) {
    TrMetin m;
    m.ptr = (char *)malloc(byte_len);
    m.len = byte_len;
    if (!m.ptr) { m.len = 0; return m; }
    if (byte_len == 0) return m;

    /* Codepoint başlangıç offsetlerini topla */
    int stack_poz[4096];
    int *poz = stack_poz;
    int *heap_poz = NULL;
    int kap = 4096;
    int karakter_sayisi = 0;
    int pos = 0;

    while (pos < (int)byte_len) {
        if (karakter_sayisi >= kap) {
            kap *= 2;
            if (heap_poz) {
                heap_poz = (int *)realloc(heap_poz, kap * sizeof(int));
            } else {
                heap_poz = (int *)malloc(kap * sizeof(int));
                if (heap_poz) memcpy(heap_poz, stack_poz, karakter_sayisi * sizeof(int));
            }
            if (!heap_poz) { free(m.ptr); m.ptr = NULL; m.len = 0; return m; }
            poz = heap_poz;
        }
        poz[karakter_sayisi++] = pos;
        _utf8_oku(ptr, &pos);
    }

    /* Ters sirada yaz */
    int yaz_pos = 0;
    for (int i = karakter_sayisi - 1; i >= 0; i--) {
        int kar_bas = poz[i];
        int kar_son = (i + 1 < karakter_sayisi) ? poz[i + 1] : (int)byte_len;
        int kar_len = kar_son - kar_bas;
        memcpy(m.ptr + yaz_pos, ptr + kar_bas, kar_len);
        yaz_pos += kar_len;
    }

    if (heap_poz) free(heap_poz);
    return m;
}

/* Karakter tabanli bul: aranan metnin karakter pozisyonunu dondur.
   Bulunamazsa -1. */
long long _tr_utf8_bul(const char *ptr, long long byte_len,
                        const char *aranan_ptr, long long aranan_byte_len) {
    if (aranan_byte_len <= 0) return 0;
    if (aranan_byte_len > byte_len) return -1;

    int pos = 0;
    long long karakter_indeks = 0;
    while (pos <= (int)(byte_len - aranan_byte_len)) {
        if (memcmp(ptr + pos, aranan_ptr, (size_t)aranan_byte_len) == 0) {
            return karakter_indeks;
        }
        _utf8_oku(ptr, &pos);
        karakter_indeks++;
    }
    return -1;
}

/* Karakter tabanli kes(metin, karakter_baslangic, karakter_uzunluk) -> metin */
TrMetin _tr_utf8_kes(const char *ptr, long long byte_len,
                       long long karakter_bas, long long karakter_uzunluk) {
    return _tr_utf8_dilim(ptr, byte_len, karakter_bas, karakter_bas + karakter_uzunluk);
}

TrDizi _tr_dosya_satirlar(const char *dosya_ptr, long long dosya_len) {
    TrDizi d = {NULL, 0};
    char dosya_adi[512];
    int n = (int)dosya_len;
    if (n >= (int)sizeof(dosya_adi)) n = (int)sizeof(dosya_adi) - 1;
    memcpy(dosya_adi, dosya_ptr, n);
    dosya_adi[n] = '\0';

    FILE *f = fopen(dosya_adi, "r");
    if (!f) return d;

    /* Satir sayisini bul */
    long long satir_sayisi = 0;
    char buf[4096];
    while (fgets(buf, (int)sizeof(buf), f)) satir_sayisi++;
    rewind(f);

    if (satir_sayisi == 0) { fclose(f); return d; }

    /* Bellek ayir: her satir icin 2 slot (ptr, len) */
    long long boyut = satir_sayisi * 2 * sizeof(long long);
    long long *sonuç = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!sonuç) { fclose(f); return d; }

    long long idx = 0;
    while (fgets(buf, (int)sizeof(buf), f) && idx < satir_sayisi) {
        int slen = (int)strlen(buf);
        /* Satir sonu karakterlerini sil */
        while (slen > 0 && (buf[slen-1] == '\n' || buf[slen-1] == '\r')) slen--;
        char *satir = (char *)malloc(slen);
        if (satir) memcpy(satir, buf, slen);
        sonuç[idx * 2] = (long long)satir;
        sonuç[idx * 2 + 1] = slen;
        idx++;
    }
    fclose(f);

    d.ptr = sonuç;
    d.count = idx;
    return d;
}

/* dosya_ekle(dosya_adi, icerik) -> tam (1=basarili, 0=hata) */
long long _tr_dosya_ekle(const char *dosya_ptr, long long dosya_len,
                          const char *icerik_ptr, long long icerik_len) {
    char dosya_adi[512];
    int n = (int)dosya_len;
    if (n >= (int)sizeof(dosya_adi)) n = (int)sizeof(dosya_adi) - 1;
    memcpy(dosya_adi, dosya_ptr, n);
    dosya_adi[n] = '\0';
    FILE *f = fopen(dosya_adi, "a");
    if (!f) return 0;
    fwrite(icerik_ptr, 1, icerik_len, f);
    fclose(f);
    return 1;
}
