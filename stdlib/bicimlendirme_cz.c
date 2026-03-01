/* Biçimlendirme modülü — çalışma zamanı implementasyonu */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { char *ptr; long long len; } TrMetin;

/* sıfır_doldur(sayı, genişlik) -> metin: "00042" */
TrMetin _tr_sifir_doldur(long long sayi, long long genislik) {
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%0*lld", (int)genislik, sayi);
    char *sonuc = (char *)malloc(n);
    if (sonuc) memcpy(sonuc, buf, n);
    TrMetin m = {sonuc, n};
    return m;
}

/* ondalık_biçimle(sayı, basamak) -> metin: "3.14" */
TrMetin _tr_ondalik_bicimle2(double sayi, long long basamak) {
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%.*f", (int)basamak, sayi);
    char *sonuc = (char *)malloc(n);
    if (sonuc) memcpy(sonuc, buf, n);
    TrMetin m = {sonuc, n};
    return m;
}

/* Yardımcı: tam kısmı binlik noktalı formata çevir */
static int tam_binlik_format(long long tam, char *buf, int buf_boyut) {
    if (tam == 0) { buf[0] = '0'; return 1; }

    char rakamlar[32];
    int ri = 0;
    while (tam > 0) {
        rakamlar[ri++] = '0' + (int)(tam % 10);
        tam /= 10;
    }

    int bi = 0;
    for (int i = ri - 1; i >= 0 && bi < buf_boyut - 1; i--) {
        int konum = ri - 1 - i;  /* soldan kaçıncı rakam */
        if (konum > 0 && (ri - konum) % 3 == 0)
            buf[bi++] = '.';
        buf[bi++] = rakamlar[i];
    }
    return bi;
}

/* para_biçimle(sayı) -> metin: "1.234.567,89" (Türk formatı) */
TrMetin _tr_para_bicimle(double sayi) {
    int negatif = sayi < 0;
    if (negatif) sayi = -sayi;

    long long tam = (long long)sayi;
    int ondalik = (int)round((sayi - (double)tam) * 100.0);
    if (ondalik >= 100) { tam++; ondalik -= 100; }
    if (ondalik < 0) ondalik = 0;

    char tam_buf[64];
    int ti = tam_binlik_format(tam, tam_buf, sizeof(tam_buf));
    tam_buf[ti] = '\0';

    char buf[128];
    int n;
    if (negatif)
        n = snprintf(buf, sizeof(buf), "-%s,%02d", tam_buf, ondalik);
    else
        n = snprintf(buf, sizeof(buf), "%s,%02d", tam_buf, ondalik);

    char *sonuc = (char *)malloc(n);
    if (sonuc) memcpy(sonuc, buf, n);
    TrMetin m = {sonuc, n};
    return m;
}

/* para_biçimle_sembol(sayı, sembol) -> metin: "1.234,56 TL" */
TrMetin _tr_para_bicimle_sembol(double sayi,
                                const char *sembol_ptr, long long sembol_len) {
    TrMetin para = _tr_para_bicimle(sayi);
    long long toplam = para.len + 1 + sembol_len;
    char *sonuc = (char *)malloc(toplam);
    if (sonuc) {
        memcpy(sonuc, para.ptr, para.len);
        sonuc[para.len] = ' ';
        memcpy(sonuc + para.len + 1, sembol_ptr, sembol_len);
    }
    free(para.ptr);
    TrMetin m = {sonuc, toplam};
    return m;
}

/* binlik_ayır(sayı) -> metin: "1.234.567" */
TrMetin _tr_binlik_ayir(long long sayi) {
    int negatif = sayi < 0;
    if (negatif) sayi = -sayi;

    char buf[64];
    int bi = 0;
    if (negatif) buf[bi++] = '-';
    bi += tam_binlik_format(sayi, buf + bi, sizeof(buf) - bi);

    char *sonuc = (char *)malloc(bi);
    if (sonuc) memcpy(sonuc, buf, bi);
    TrMetin m = {sonuc, bi};
    return m;
}

/* sağa_hizala(metin, genişlik) -> metin */
TrMetin _tr_saga_hizala(const char *ptr, long long len, long long genislik) {
    if (genislik <= len) {
        char *sonuc = (char *)malloc(len);
        if (sonuc) memcpy(sonuc, ptr, len);
        TrMetin m = {sonuc, len};
        return m;
    }
    long long bosluk = genislik - len;
    char *sonuc = (char *)malloc(genislik);
    if (sonuc) {
        memset(sonuc, ' ', bosluk);
        memcpy(sonuc + bosluk, ptr, len);
    }
    TrMetin m = {sonuc, genislik};
    return m;
}

/* sola_hizala(metin, genişlik) -> metin */
TrMetin _tr_sola_hizala(const char *ptr, long long len, long long genislik) {
    if (genislik <= len) {
        char *sonuc = (char *)malloc(len);
        if (sonuc) memcpy(sonuc, ptr, len);
        TrMetin m = {sonuc, len};
        return m;
    }
    char *sonuc = (char *)malloc(genislik);
    if (sonuc) {
        memcpy(sonuc, ptr, len);
        memset(sonuc + len, ' ', genislik - len);
    }
    TrMetin m = {sonuc, genislik};
    return m;
}

/* ortala(metin, genişlik) -> metin */
TrMetin _tr_ortala(const char *ptr, long long len, long long genislik) {
    if (genislik <= len) {
        char *sonuc = (char *)malloc(len);
        if (sonuc) memcpy(sonuc, ptr, len);
        TrMetin m = {sonuc, len};
        return m;
    }
    long long bosluk = genislik - len;
    long long sol = bosluk / 2;
    long long sag = bosluk - sol;
    char *sonuc = (char *)malloc(genislik);
    if (sonuc) {
        memset(sonuc, ' ', sol);
        memcpy(sonuc + sol, ptr, len);
        memset(sonuc + sol + len, ' ', sag);
    }
    TrMetin m = {sonuc, genislik};
    return m;
}

/* ondalık_binlik(sayı, basamak) -> metin: "1.234,56" */
TrMetin _tr_ondalik_binlik(double sayi, long long basamak) {
    int negatif = sayi < 0;
    if (negatif) sayi = -sayi;

    double carpan = pow(10.0, (double)basamak);
    long long tam = (long long)sayi;
    long long ond = (long long)round((sayi - (double)tam) * carpan);
    if ((double)ond >= carpan) { tam++; ond -= (long long)carpan; }
    if (ond < 0) ond = 0;

    char tam_buf[64];
    int ti = tam_binlik_format(tam, tam_buf, sizeof(tam_buf));
    tam_buf[ti] = '\0';

    char buf[128];
    int n;
    if (basamak > 0) {
        if (negatif)
            n = snprintf(buf, sizeof(buf), "-%s,%0*lld", tam_buf, (int)basamak, ond);
        else
            n = snprintf(buf, sizeof(buf), "%s,%0*lld", tam_buf, (int)basamak, ond);
    } else {
        if (negatif)
            n = snprintf(buf, sizeof(buf), "-%s", tam_buf);
        else
            n = snprintf(buf, sizeof(buf), "%s", tam_buf);
    }

    char *sonuc = (char *)malloc(n);
    if (sonuc) memcpy(sonuc, buf, n);
    TrMetin m = {sonuc, n};
    return m;
}
