/* Küme modülü — çalışma zamanı implementasyonu */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

/* === Küme (Set) modülü === */

typedef struct {
    long long *anahtarlar;
    long long *durumlar;    /* 0=bos, 1=dolu, 2=silinmis */
    long long kapasite;
    long long sayi;
} TrKume;

static unsigned long long _kume_hash(long long deger) {
    unsigned long long h = 14695981039346656037ULL;
    unsigned char *p = (unsigned char *)&deger;
    for (int i = 0; i < 8; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static void _kume_genislet(TrKume *k) {
    long long yeni_kap = k->kapasite * 2;
    long long *yeni_a = (long long *)calloc(yeni_kap, sizeof(long long));
    long long *yeni_d = (long long *)calloc(yeni_kap, sizeof(long long));
    if (!yeni_a || !yeni_d) return;
    for (long long i = 0; i < k->kapasite; i++) {
        if (k->durumlar[i] == 1) {
            unsigned long long h = _kume_hash(k->anahtarlar[i]) % (unsigned long long)yeni_kap;
            while (yeni_d[h] == 1) h = (h + 1) % (unsigned long long)yeni_kap;
            yeni_a[h] = k->anahtarlar[i];
            yeni_d[h] = 1;
        }
    }
    free(k->anahtarlar);
    free(k->durumlar);
    k->anahtarlar = yeni_a;
    k->durumlar = yeni_d;
    k->kapasite = yeni_kap;
}

long long _tr_kume_yeni(void) {
    TrKume *k = (TrKume *)_tr_nesne_olustur(NESNE_TIP_KUME, (long long)sizeof(TrKume));
    if (!k) return 0;
    k->kapasite = 16;
    k->sayi = 0;
    k->anahtarlar = (long long *)calloc(16, sizeof(long long));
    k->durumlar = (long long *)calloc(16, sizeof(long long));
    return (long long)k;
}

void _tr_kume_ekle(long long kume_ptr, long long deger) {
    TrKume *k = (TrKume *)kume_ptr;
    if (!k) return;
    if (k->sayi * 4 >= k->kapasite * 3) _kume_genislet(k);
    unsigned long long h = _kume_hash(deger) % (unsigned long long)k->kapasite;
    while (k->durumlar[h] == 1) {
        if (k->anahtarlar[h] == deger) return;  /* zaten var */
        h = (h + 1) % (unsigned long long)k->kapasite;
    }
    k->anahtarlar[h] = deger;
    k->durumlar[h] = 1;
    k->sayi++;
}

void _tr_kume_sil(long long kume_ptr, long long deger) {
    TrKume *k = (TrKume *)kume_ptr;
    if (!k) return;
    unsigned long long h = _kume_hash(deger) % (unsigned long long)k->kapasite;
    for (long long i = 0; i < k->kapasite; i++) {
        unsigned long long idx = (h + (unsigned long long)i) % (unsigned long long)k->kapasite;
        if (k->durumlar[idx] == 0) return;  /* bulunamadi */
        if (k->durumlar[idx] == 1 && k->anahtarlar[idx] == deger) {
            k->durumlar[idx] = 2;  /* silinmis olarak isaretle */
            k->sayi--;
            return;
        }
    }
}

long long _tr_kume_var_mi(long long kume_ptr, long long deger) {
    TrKume *k = (TrKume *)kume_ptr;
    if (!k) return 0;
    unsigned long long h = _kume_hash(deger) % (unsigned long long)k->kapasite;
    for (long long i = 0; i < k->kapasite; i++) {
        unsigned long long idx = (h + (unsigned long long)i) % (unsigned long long)k->kapasite;
        if (k->durumlar[idx] == 0) return 0;
        if (k->durumlar[idx] == 1 && k->anahtarlar[idx] == deger) return 1;
    }
    return 0;
}

long long _tr_kume_uzunluk(long long kume_ptr) {
    TrKume *k = (TrKume *)kume_ptr;
    return k ? k->sayi : 0;
}

long long _tr_kume_birlesim(long long a_ptr, long long b_ptr) {
    long long yeni = _tr_kume_yeni();
    TrKume *a = (TrKume *)a_ptr;
    TrKume *b = (TrKume *)b_ptr;
    if (a) for (long long i = 0; i < a->kapasite; i++)
        if (a->durumlar[i] == 1) _tr_kume_ekle(yeni, a->anahtarlar[i]);
    if (b) for (long long i = 0; i < b->kapasite; i++)
        if (b->durumlar[i] == 1) _tr_kume_ekle(yeni, b->anahtarlar[i]);
    return yeni;
}

long long _tr_kume_kesisim(long long a_ptr, long long b_ptr) {
    long long yeni = _tr_kume_yeni();
    TrKume *a = (TrKume *)a_ptr;
    if (a) for (long long i = 0; i < a->kapasite; i++)
        if (a->durumlar[i] == 1 && _tr_kume_var_mi(b_ptr, a->anahtarlar[i]))
            _tr_kume_ekle(yeni, a->anahtarlar[i]);
    return yeni;
}

long long _tr_kume_fark(long long a_ptr, long long b_ptr) {
    long long yeni = _tr_kume_yeni();
    TrKume *a = (TrKume *)a_ptr;
    if (a) for (long long i = 0; i < a->kapasite; i++)
        if (a->durumlar[i] == 1 && !_tr_kume_var_mi(b_ptr, a->anahtarlar[i]))
            _tr_kume_ekle(yeni, a->anahtarlar[i]);
    return yeni;
}

void _tr_kume_yazdir(long long kume_ptr) {
    TrKume *k = (TrKume *)kume_ptr;
    if (!k) { printf("{}\n"); return; }
    printf("{");
    int ilk = 1;
    for (long long i = 0; i < k->kapasite; i++) {
        if (k->durumlar[i] == 1) {
            if (!ilk) printf(", ");
            printf("%lld", k->anahtarlar[i]);
            ilk = 0;
        }
    }
    printf("}\n");
}
