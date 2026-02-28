/* Matematik modülü — çalışma zamanı implementasyonu */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "runtime.h"

/* ---- Matematik ---- */

double _tr_sin(double x)  { return sin(x); }
double _tr_cos(double x)  { return cos(x); }
double _tr_tan(double x)  { return tan(x); }
double _tr_log(double x)  { return log(x); }
double _tr_log10(double x) { return log10(x); }
double _tr_ust(double x, double y) { return pow(x, y); }
double _tr_taban(double x) { return floor(x); }
double _tr_tavan(double x) { return ceil(x); }
double _tr_yuvarla(double x) { return round(x); }

long long _tr_rastgele(long long maks) {
    static int ilk = 1;
    if (ilk) { srand((unsigned int)time(NULL)); ilk = 0; }
    if (maks <= 0) return 0;
    return rand() % maks;
}

double _tr_pi(void) { return 3.14159265358979323846; }
double _tr_e(void)  { return 2.71828182845904523536; }

/* Ters trigonometrik */
double _tr_asin(double x)  { return asin(x); }
double _tr_acos(double x)  { return acos(x); }
double _tr_atan(double x)  { return atan(x); }
double _tr_atan2(double x, double y) { return atan2(x, y); }

/* Hiperbolik */
double _tr_sinh(double x)  { return sinh(x); }
double _tr_cosh(double x)  { return cosh(x); }
double _tr_tanh(double x)  { return tanh(x); }

/* Üstel / Logaritmik */
double _tr_ustel(double x) { return exp(x); }
double _tr_log2_(double x) { return log2(x); }

/* Yuvarlama */
double _tr_buda(double x)  { return trunc(x); }

/* Diğer */
double _tr_kupkok(double x) { return cbrt(x); }
double _tr_hipot(double x, double y) { return hypot(x, y); }
double _tr_fmod(double x, double y)  { return fmod(x, y); }
long long _tr_isaret(long long x) { return (x > 0) - (x < 0); }
/* faktöriyel(n) -> tam */
long long _tr_faktoriyel(long long n) {
    if (n < 0) return 0;
    if (n <= 1) return 1;
    long long sonuç = 1;
    for (long long i = 2; i <= n; i++) sonuç *= i;
    return sonuç;
}

/* obeb(a, b) -> tam (GCD) */
long long _tr_obeb(long long a, long long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/* okek(a, b) -> tam (LCM) */
long long _tr_okek(long long a, long long b) {
    if (a == 0 || b == 0) return 0;
    long long g = _tr_obeb(a, b);
    return (a / g) * b;
}

/* sonsuz_mu(x) -> mantık */
long long _tr_sonsuz_mu(double x) {
    return __builtin_isinf(x) ? 1 : 0;
}

/* sayidegil_mi(x) -> mantık */
long long _tr_sayidegil_mi(double x) {
    return __builtin_isnan(x) ? 1 : 0;
}
/* === İleri düzey matematik === */

/* --- Matris yardımcıları --- */
/* Matris formatı: düz dizi [satır, sütun, veri...] */
#define MAT_S(p) ((p)[0])
#define MAT_N(p) ((p)[1])
#define MAT_V(p, i, j) ((p)[2 + (i) * MAT_N(p) + (j)])

static TrDizi mat_yeni(long long s, long long n) {
    long long toplam = s * n + 2;
    long long *ptr = (long long *)_tr_nesne_olustur(1, toplam * sizeof(long long));
    TrDizi d = {NULL, 0};
    if (!ptr) return d;
    for (long long i = 0; i < toplam; i++) ptr[i] = 0;
    ptr[0] = s; ptr[1] = n;
    d.ptr = ptr; d.count = toplam;
    return d;
}

TrDizi _tr_matris(long long s, long long n) {
    return mat_yeni(s, n);
}

/* matris_birim(n) -> dizi */
TrDizi _tr_matris_birim(long long n) {
    TrDizi d = mat_yeni(n, n);
    if (!d.ptr) return d;
    for (long long i = 0; i < n; i++)
        MAT_V(d.ptr, i, i) = 1;
    return d;
}

/* matris_topla(a, b) -> dizi */
TrDizi _tr_matris_topla(long long *ap, long long ac, long long *bp, long long bc) {
    (void)ac; (void)bc;
    long long s = MAT_S(ap), n = MAT_N(ap);
    TrDizi d = mat_yeni(s, n);
    if (!d.ptr) return d;
    for (long long i = 0; i < s; i++)
        for (long long j = 0; j < n; j++)
            MAT_V(d.ptr, i, j) = MAT_V(ap, i, j) + MAT_V(bp, i, j);
    return d;
}

/* matris_cikar(a, b) -> dizi */
TrDizi _tr_matris_cikar(long long *ap, long long ac, long long *bp, long long bc) {
    (void)ac; (void)bc;
    long long s = MAT_S(ap), n = MAT_N(ap);
    TrDizi d = mat_yeni(s, n);
    if (!d.ptr) return d;
    for (long long i = 0; i < s; i++)
        for (long long j = 0; j < n; j++)
            MAT_V(d.ptr, i, j) = MAT_V(ap, i, j) - MAT_V(bp, i, j);
    return d;
}

/* matris_carp(a, b) -> dizi — a(m×n) * b(n×p) = c(m×p) */
TrDizi _tr_matris_carp(long long *ap, long long ac, long long *bp, long long bc) {
    (void)ac; (void)bc;
    long long m = MAT_S(ap), n = MAT_N(ap), p = MAT_N(bp);
    TrDizi d = mat_yeni(m, p);
    if (!d.ptr) return d;
    for (long long i = 0; i < m; i++)
        for (long long j = 0; j < p; j++) {
            long long t = 0;
            for (long long k = 0; k < n; k++)
                t += MAT_V(ap, i, k) * MAT_V(bp, k, j);
            MAT_V(d.ptr, i, j) = t;
        }
    return d;
}

/* matris_skaler(a, k) -> dizi */
TrDizi _tr_matris_skaler(long long *ap, long long ac, long long k) {
    (void)ac;
    long long s = MAT_S(ap), n = MAT_N(ap);
    TrDizi d = mat_yeni(s, n);
    if (!d.ptr) return d;
    for (long long i = 0; i < s; i++)
        for (long long j = 0; j < n; j++)
            MAT_V(d.ptr, i, j) = MAT_V(ap, i, j) * k;
    return d;
}

/* matris_transpoz(a) -> dizi */
TrDizi _tr_matris_transpoz(long long *ap, long long ac) {
    (void)ac;
    long long s = MAT_S(ap), n = MAT_N(ap);
    TrDizi d = mat_yeni(n, s);
    if (!d.ptr) return d;
    for (long long i = 0; i < s; i++)
        for (long long j = 0; j < n; j++)
            MAT_V(d.ptr, j, i) = MAT_V(ap, i, j);
    return d;
}

/* matris_determinant(a) -> tam — LU ayrışımı ile */
long long _tr_matris_determinant(long long *ap, long long ac) {
    (void)ac;
    long long n = MAT_S(ap);
    /* Çalışma kopyası */
    double *m = (double *)malloc((size_t)(n * n) * sizeof(double));
    if (!m) return 0;
    for (long long i = 0; i < n * n; i++)
        m[i] = (double)ap[2 + i];

    double det = 1.0;
    for (long long i = 0; i < n; i++) {
        /* Pivot seçimi */
        long long maks_satir = i;
        double maks_deger = fabs(m[i * n + i]);
        for (long long k = i + 1; k < n; k++) {
            if (fabs(m[k * n + i]) > maks_deger) {
                maks_deger = fabs(m[k * n + i]);
                maks_satir = k;
            }
        }
        if (maks_deger < 1e-12) { free(m); return 0; }
        if (maks_satir != i) {
            for (long long j = 0; j < n; j++) {
                double tmp = m[i * n + j];
                m[i * n + j] = m[maks_satir * n + j];
                m[maks_satir * n + j] = tmp;
            }
            det *= -1;
        }
        det *= m[i * n + i];
        for (long long k = i + 1; k < n; k++) {
            double f = m[k * n + i] / m[i * n + i];
            for (long long j = i; j < n; j++)
                m[k * n + j] -= f * m[i * n + j];
        }
    }
    free(m);
    return (long long)round(det);
}

/* matris_ters(a) -> dizi — Gauss-Jordan */
TrDizi _tr_matris_ters(long long *ap, long long ac) {
    (void)ac;
    long long n = MAT_S(ap);
    double *m = (double *)malloc((size_t)(n * 2 * n) * sizeof(double));
    TrDizi d = mat_yeni(n, n);
    if (!m || !d.ptr) { free(m); return d; }

    /* [A | I] oluştur */
    for (long long i = 0; i < n; i++)
        for (long long j = 0; j < n; j++) {
            m[i * 2 * n + j] = (double)MAT_V(ap, i, j);
            m[i * 2 * n + n + j] = (i == j) ? 1.0 : 0.0;
        }

    for (long long i = 0; i < n; i++) {
        long long maks_satir = i;
        for (long long k = i + 1; k < n; k++)
            if (fabs(m[k * 2 * n + i]) > fabs(m[maks_satir * 2 * n + i]))
                maks_satir = k;
        if (fabs(m[maks_satir * 2 * n + i]) < 1e-12) { free(m); return d; }
        if (maks_satir != i)
            for (long long j = 0; j < 2 * n; j++) {
                double tmp = m[i * 2 * n + j];
                m[i * 2 * n + j] = m[maks_satir * 2 * n + j];
                m[maks_satir * 2 * n + j] = tmp;
            }
        double pivot = m[i * 2 * n + i];
        for (long long j = 0; j < 2 * n; j++)
            m[i * 2 * n + j] /= pivot;
        for (long long k = 0; k < n; k++) {
            if (k == i) continue;
            double f = m[k * 2 * n + i];
            for (long long j = 0; j < 2 * n; j++)
                m[k * 2 * n + j] -= f * m[i * 2 * n + j];
        }
    }

    for (long long i = 0; i < n; i++)
        for (long long j = 0; j < n; j++)
            MAT_V(d.ptr, i, j) = (long long)round(m[i * 2 * n + n + j]);
    free(m);
    return d;
}

/* matris_oku(m, i, j) -> tam */
long long _tr_matris_oku(long long *ap, long long ac, long long i, long long j) {
    (void)ac;
    return MAT_V(ap, i, j);
}

/* matris_yaz(m, i, j, v) -> dizi (yeni kopya döndürür) */
TrDizi _tr_matris_yaz(long long *ap, long long ac, long long i, long long j, long long v) {
    TrDizi d;
    long long *yeni = (long long *)_tr_nesne_olustur(1, ac * (long long)sizeof(long long));
    if (!yeni) { d.ptr = ap; d.count = ac; return d; }
    for (long long k = 0; k < ac; k++) yeni[k] = ap[k];
    MAT_V(yeni, i, j) = v;
    d.ptr = yeni; d.count = ac;
    return d;
}

/* matris_iz(a) -> tam (trace) */
long long _tr_matris_iz(long long *ap, long long ac) {
    (void)ac;
    long long n = MAT_S(ap);
    long long iz = 0;
    for (long long i = 0; i < n; i++)
        iz += MAT_V(ap, i, i);
    return iz;
}

/* --- İstatistik --- */

/* toplam(d) -> tam */
long long _tr_toplam(long long *ptr, long long count) {
    long long t = 0;
    for (long long i = 0; i < count; i++) t += ptr[i];
    return t;
}

/* ortalama(d) -> ondalık */
double _tr_ortalama(long long *ptr, long long count) {
    if (count == 0) return 0.0;
    double t = 0.0;
    for (long long i = 0; i < count; i++) t += (double)ptr[i];
    return t / (double)count;
}

/* medyan(d) -> ondalık */
double _tr_medyan(long long *ptr, long long count) {
    if (count == 0) return 0.0;
    /* Kopyala ve sırala */
    long long *tmp = (long long *)malloc((size_t)count * sizeof(long long));
    if (!tmp) return 0.0;
    for (long long i = 0; i < count; i++) tmp[i] = ptr[i];
    /* Basit sıralama */
    for (long long i = 0; i < count - 1; i++)
        for (long long j = i + 1; j < count; j++)
            if (tmp[j] < tmp[i]) { long long t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }
    double sonuç;
    if (count % 2 == 1)
        sonuç = (double)tmp[count / 2];
    else
        sonuç = ((double)tmp[count / 2 - 1] + (double)tmp[count / 2]) / 2.0;
    free(tmp);
    return sonuç;
}

/* varyans(d) -> ondalık */
double _tr_varyans(long long *ptr, long long count) {
    if (count == 0) return 0.0;
    double ort = _tr_ortalama(ptr, count);
    double t = 0.0;
    for (long long i = 0; i < count; i++) {
        double fark = (double)ptr[i] - ort;
        t += fark * fark;
    }
    return t / (double)count;
}

/* std_sapma(d) -> ondalık */
double _tr_std_sapma(long long *ptr, long long count) {
    return sqrt(_tr_varyans(ptr, count));
}

/* en_kucuk(d) -> tam */
long long _tr_en_kucuk(long long *ptr, long long count) {
    if (count == 0) return 0;
    long long m = ptr[0];
    for (long long i = 1; i < count; i++)
        if (ptr[i] < m) m = ptr[i];
    return m;
}

/* en_buyuk(d) -> tam */
long long _tr_en_buyuk(long long *ptr, long long count) {
    if (count == 0) return 0;
    long long m = ptr[0];
    for (long long i = 1; i < count; i++)
        if (ptr[i] > m) m = ptr[i];
    return m;
}

/* kombinasyon(n, k) -> tam — C(n,k) */
long long _tr_kombinasyon(long long n, long long k) {
    if (k < 0 || k > n) return 0;
    if (k == 0 || k == n) return 1;
    if (k > n - k) k = n - k;
    long long sonuç = 1;
    for (long long i = 0; i < k; i++) {
        sonuç *= (n - i);
        sonuç /= (i + 1);
    }
    return sonuç;
}

/* permutasyon(n, k) -> tam — P(n,k) */
long long _tr_permutasyon(long long n, long long k) {
    if (k < 0 || k > n) return 0;
    long long sonuç = 1;
    for (long long i = 0; i < k; i++)
        sonuç *= (n - i);
    return sonuç;
}

/* korelasyon(x, y) -> ondalık — Pearson */
double _tr_korelasyon(long long *xp, long long xc, long long *yp, long long yc) {
    long long n = xc < yc ? xc : yc;
    if (n == 0) return 0.0;
    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
    for (long long i = 0; i < n; i++) {
        double x = (double)xp[i], y = (double)yp[i];
        sx += x; sy += y;
        sxx += x * x; syy += y * y;
        sxy += x * y;
    }
    double nd = (double)n;
    double pay = nd * sxy - sx * sy;
    double payda = sqrt((nd * sxx - sx * sx) * (nd * syy - sy * sy));
    if (payda < 1e-12) return 0.0;
    return pay / payda;
}

/* --- Sayısal Analiz --- */

/* polinom_hesapla(katsayılar, x) -> tam — Horner yöntemi */
/* katsayılar: [a_n, a_{n-1}, ..., a_1, a_0] */
long long _tr_polinom_hesapla(long long *ptr, long long count, long long x) {
    long long sonuç = 0;
    for (long long i = 0; i < count; i++)
        sonuç = sonuç * x + ptr[i];
    return sonuç;
}

/* polinom_turev(katsayılar) -> dizi */
TrDizi _tr_polinom_turev(long long *ptr, long long count) {
    TrDizi d = {NULL, 0};
    if (count <= 1) {
        long long *yeni = (long long *)_tr_nesne_olustur(1, sizeof(long long));
        if (yeni) { yeni[0] = 0; d.ptr = yeni; d.count = 1; }
        return d;
    }
    long long yeni_say = count - 1;
    long long *yeni = (long long *)_tr_nesne_olustur(1, yeni_say * (long long)sizeof(long long));
    if (!yeni) return d;
    for (long long i = 0; i < yeni_say; i++)
        yeni[i] = ptr[i] * (count - 1 - i);
    d.ptr = yeni; d.count = yeni_say;
    return d;
}

/* polinom_integral(katsayılar) -> dizi (sabit = 0) */
TrDizi _tr_polinom_integral(long long *ptr, long long count) {
    TrDizi d = {NULL, 0};
    long long yeni_say = count + 1;
    long long *yeni = (long long *)_tr_nesne_olustur(1, yeni_say * (long long)sizeof(long long));
    if (!yeni) return d;
    for (long long i = 0; i < count; i++)
        yeni[i] = ptr[i] / (count - i);
    yeni[count] = 0; /* integral sabiti */
    d.ptr = yeni; d.count = yeni_say;
    return d;
}

/* sayisal_integral(x_dizisi, y_dizisi) -> tam — yamuk kuralı */
long long _tr_sayisal_integral(long long *xp, long long xc, long long *yp, long long yc) {
    long long n = xc < yc ? xc : yc;
    if (n < 2) return 0;
    long long toplam = 0;
    for (long long i = 0; i < n - 1; i++)
        toplam += (yp[i] + yp[i + 1]) * (xp[i + 1] - xp[i]);
    return toplam / 2;
}

/* lineer_regresyon(x, y) -> dizi [eğim, kesişim] */
TrDizi _tr_lineer_regresyon(long long *xp, long long xc, long long *yp, long long yc) {
    TrDizi d = {NULL, 0};
    long long n = xc < yc ? xc : yc;
    if (n < 2) return d;
    long long *yeni = (long long *)_tr_nesne_olustur(1, 2 * (long long)sizeof(long long));
    if (!yeni) return d;

    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (long long i = 0; i < n; i++) {
        double x = (double)xp[i], y = (double)yp[i];
        sx += x; sy += y;
        sxx += x * x; sxy += x * y;
    }
    double nd = (double)n;
    double payda = nd * sxx - sx * sx;
    if (fabs(payda) < 1e-12) { yeni[0] = 0; yeni[1] = 0; }
    else {
        double egim = (nd * sxy - sx * sy) / payda;
        double kesisim = (sy - egim * sx) / nd;
        yeni[0] = (long long)round(egim);
        yeni[1] = (long long)round(kesisim);
    }
    d.ptr = yeni; d.count = 2;
    return d;
}
