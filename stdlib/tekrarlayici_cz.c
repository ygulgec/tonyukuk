/* Tekrarlayıcı modülü — çalışma zamanı implementasyonu */
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

/* === Tekrarlayıcı Araçları (kullan tekrarlayıcı) === */

TrDizi _tr_zincir(long long *ptr1, long long c1, long long *ptr2, long long c2) {
    long long toplam = c1 + c2;
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, toplam * sizeof(long long));
    if (!yeni) { TrDizi d = {NULL, 0}; return d; }
    for (long long i = 0; i < c1; i++) yeni[i] = ptr1[i];
    for (long long i = 0; i < c2; i++) yeni[c1 + i] = ptr2[i];
    TrDizi d = {yeni, toplam};
    return d;
}

TrDizi _tr_tekrarla_dizi(long long *ptr, long long count, long long kez) {
    long long toplam = count * kez;
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, toplam * sizeof(long long));
    if (!yeni) { TrDizi d = {NULL, 0}; return d; }
    for (long long k = 0; k < kez; k++)
        for (long long i = 0; i < count; i++)
            yeni[k * count + i] = ptr[i];
    TrDizi d = {yeni, toplam};
    return d;
}

TrDizi _tr_parcala(long long *ptr, long long count, long long boyut) {
    if (boyut <= 0) boyut = 1;
    long long parca_sayisi = (count + boyut - 1) / boyut;
    long long *dis = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, parca_sayisi * sizeof(long long));
    if (!dis) { TrDizi d = {NULL, 0}; return d; }
    for (long long p = 0; p < parca_sayisi; p++) {
        long long bas = p * boyut;
        long long bit = bas + boyut;
        if (bit > count) bit = count;
        long long n = bit - bas;
        long long *ic = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, n * sizeof(long long));
        if (ic) for (long long i = 0; i < n; i++) ic[i] = ptr[bas + i];
        dis[p] = (long long)ic;
    }
    TrDizi d = {dis, parca_sayisi};
    return d;
}

static void _perm_helper(long long *arr, long long n, long long k,
                          long long **sonuç, long long *idx, long long max) {
    if (k == n) {
        if (*idx >= max) return;
        long long *kopya = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, n * sizeof(long long));
        if (kopya) { for (long long i = 0; i < n; i++) kopya[i] = arr[i]; }
        sonuç[*idx] = kopya;
        (*idx)++;
        return;
    }
    for (long long i = k; i < n; i++) {
        long long tmp = arr[k]; arr[k] = arr[i]; arr[i] = tmp;
        _perm_helper(arr, n, k + 1, sonuç, idx, max);
        tmp = arr[k]; arr[k] = arr[i]; arr[i] = tmp;
    }
}

TrDizi _tr_permutasyonlar(long long *ptr, long long count) {
    /* n! olabilir, max 10! = 3628800 */
    long long faktoriyel = 1;
    for (long long i = 2; i <= count && i <= 10; i++) faktoriyel *= i;
    if (count > 10) faktoriyel = 3628800;
    long long *kopya = (long long *)malloc(count * sizeof(long long));
    if (!kopya) { TrDizi d = {NULL, 0}; return d; }
    for (long long i = 0; i < count; i++) kopya[i] = ptr[i];
    long long *dis = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, faktoriyel * sizeof(long long));
    if (!dis) { free(kopya); TrDizi d = {NULL, 0}; return d; }
    long long idx = 0;
    _perm_helper(kopya, count, 0, (long long **)dis, &idx, faktoriyel);
    free(kopya);
    TrDizi d = {dis, idx};
    return d;
}

static void _komb_helper(long long *arr, long long n, long long r,
                          long long *gecici, long long derinlik, long long bas,
                          long long **sonuç, long long *idx, long long max) {
    if (derinlik == r) {
        if (*idx >= max) return;
        long long *kopya = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, r * sizeof(long long));
        if (kopya) { for (long long i = 0; i < r; i++) kopya[i] = gecici[i]; }
        sonuç[*idx] = kopya;
        (*idx)++;
        return;
    }
    for (long long i = bas; i < n; i++) {
        gecici[derinlik] = arr[i];
        _komb_helper(arr, n, r, gecici, derinlik + 1, i + 1, sonuç, idx, max);
    }
}

TrDizi _tr_kombinasyonlar(long long *ptr, long long count, long long r) {
    /* C(n, r) */
    long long cnr = 1;
    for (long long i = 0; i < r; i++) {
        cnr = cnr * (count - i) / (i + 1);
    }
    if (cnr > 100000) cnr = 100000;
    long long *dis = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, cnr * sizeof(long long));
    if (!dis) { TrDizi d = {NULL, 0}; return d; }
    long long *gecici = (long long *)malloc(r * sizeof(long long));
    long long idx = 0;
    if (gecici) {
        _komb_helper(ptr, count, r, gecici, 0, 0, (long long **)dis, &idx, cnr);
        free(gecici);
    }
    TrDizi d = {dis, idx};
    return d;
}

TrDizi _tr_duz(long long *ptr, long long count) {
    /* Birinci seviye düzleştirme: iç dizileri aç */
    /* Önce toplam eleman sayısını bul */
    long long toplam = 0;
    for (long long i = 0; i < count; i++) {
        /* Her eleman bir iç dizi olabilir; basit yaklaşım: sadece 1 seviye */
        toplam++;  /* fallback: her elemanı olduğu gibi tut */
    }
    /* Basit düzleştirme: elemanları doğrudan kopyala */
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, toplam * sizeof(long long));
    if (!yeni) { TrDizi d = {NULL, 0}; return d; }
    for (long long i = 0; i < count; i++) yeni[i] = ptr[i];
    TrDizi d = {yeni, toplam};
    return d;
}
