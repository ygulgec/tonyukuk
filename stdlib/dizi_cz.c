/* Dizi modülü — çalışma zamanı implementasyonu */
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

/* Karşılaştırma fonksiyonu (qsort için) */
static int _tr_karsilastir(const void *a, const void *b) {
    long long va = *(const long long *)a;
    long long vb = *(const long long *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

/* sırala(dizi) -> dizi (küçükten büyüğe, in-place) */
TrDizi _tr_sirala(long long *ptr, long long count) {
    if (count > 1) {
        qsort(ptr, (size_t)count, sizeof(long long), _tr_karsilastir);
    }
    TrDizi sonuç;
    sonuç.ptr = ptr;
    sonuç.count = count;
    return sonuç;
}

/* ekle(dizi, eleman) -> dizi (yeni dizi, sona eleman ekle) */
TrDizi _tr_ekle(long long *ptr, long long count, long long eleman) {
    long long yeni_boyut = (count + 1) * sizeof(long long);
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, yeni_boyut);
    if (!yeni) {
        TrDizi sonuç = {ptr, count};
        return sonuç;
    }
    for (long long i = 0; i < count; i++) {
        yeni[i] = ptr[i];
    }
    yeni[count] = eleman;
    TrDizi sonuç;
    sonuç.ptr = yeni;
    sonuç.count = count + 1;
    return sonuç;
}

/* çıkar(dizi) -> dizi (son elemanı çıkar, aslında count-1 döner) */
TrDizi _tr_cikar(long long *ptr, long long count) {
    TrDizi sonuç;
    sonuç.ptr = ptr;
    sonuç.count = (count > 0) ? count - 1 : 0;
    return sonuç;
}

/* birleştir(dizi1, dizi2) -> dizi (iki diziyi birleştir) */
TrDizi _tr_birlestir_dizi(long long *ptr1, long long count1,
                           long long *ptr2, long long count2) {
    long long toplam = count1 + count2;
    long long boyut = toplam * sizeof(long long);
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!yeni) {
        TrDizi sonuç = {ptr1, count1};
        return sonuç;
    }
    for (long long i = 0; i < count1; i++) {
        yeni[i] = ptr1[i];
    }
    for (long long i = 0; i < count2; i++) {
        yeni[count1 + i] = ptr2[i];
    }
    TrDizi sonuç;
    sonuç.ptr = yeni;
    sonuç.count = toplam;
    return sonuç;
}
/* === Tekrarlayıcı genişletme === */

/* al_iken(fn, dizi) -> dizi (takewhile) */
TrDizi _tr_al_iken(long long fn_ptr, long long *dizi_ptr, long long dizi_count) {
    TrDizi sonuç = {NULL, 0};
    if (!dizi_ptr || dizi_count <= 0) return sonuç;

    long long *yeni = (long long *)_tr_nesne_olustur(1, dizi_count * sizeof(long long));
    if (!yeni) return sonuç;

    long long idx = 0;
    for (long long i = 0; i < dizi_count; i++) {
        /* fn'yi çağır: fn(dizi[i]) -> sonuç */
        long long (*fn)(long long) = (long long (*)(long long))fn_ptr;
        long long r = fn(dizi_ptr[i]);
        if (!r) break;  /* koşul sağlanmıyor, dur */
        yeni[idx++] = dizi_ptr[i];
    }
    sonuç.ptr = yeni;
    sonuç.count = idx;
    return sonuç;
}

/* at_iken(fn, dizi) -> dizi (dropwhile) */
TrDizi _tr_at_iken(long long fn_ptr, long long *dizi_ptr, long long dizi_count) {
    TrDizi sonuç = {NULL, 0};
    if (!dizi_ptr || dizi_count <= 0) return sonuç;

    long long bas = 0;
    long long (*fn)(long long) = (long long (*)(long long))fn_ptr;
    while (bas < dizi_count && fn(dizi_ptr[bas])) bas++;

    long long kalan = dizi_count - bas;
    long long *yeni = (long long *)_tr_nesne_olustur(1, kalan * sizeof(long long));
    if (!yeni) return sonuç;

    for (long long i = 0; i < kalan; i++) yeni[i] = dizi_ptr[bas + i];
    sonuç.ptr = yeni;
    sonuç.count = kalan;
    return sonuç;
}

/* === Birinci Sinif Fonksiyon Destegi (map/filter/reduce) === */

/* esle(dizi, fonk_ptr) -> dizi: her elemana fonksiyonu uygula */
TrDizi _tr_esle(long long *ptr, long long count, long long (*fn)(long long)) {
    TrDizi sonuç = {NULL, 0};
    if (count <= 0) return sonuç;
    long long boyut = count * sizeof(long long);
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!yeni) return sonuç;
    for (long long i = 0; i < count; i++) {
        yeni[i] = fn(ptr[i]);
    }
    sonuç.ptr = yeni;
    sonuç.count = count;
    return sonuç;
}

/* filtre(dizi, fonk_ptr) -> dizi: fonksiyon sifir-olmayan dondurenleri tut */
TrDizi _tr_filtre(long long *ptr, long long count, long long (*fn)(long long)) {
    TrDizi sonuç = {NULL, 0};
    if (count <= 0) return sonuç;
    long long boyut = count * sizeof(long long);
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!yeni) return sonuç;
    long long j = 0;
    for (long long i = 0; i < count; i++) {
        if (fn(ptr[i])) {
            yeni[j++] = ptr[i];
        }
    }
    sonuç.ptr = yeni;
    sonuç.count = j;
    return sonuç;
}

/* indirge(dizi, başlangıç, fonk_ptr) -> tam: sola katlama */
long long _tr_indirge(long long *ptr, long long count, long long başlangıç,
                       long long (*fn)(long long, long long)) {
    long long acc = başlangıç;
    for (long long i = 0; i < count; i++) {
        acc = fn(acc, ptr[i]);
    }
    return acc;
}

/* Sozluk sil: bir anahtari kaldir */
long long _tr_sozluk_sil(long long sozluk_ptr,
                           const char *anahtar_ptr, long long anahtar_len) {
    long long *blok = (long long *)sozluk_ptr;
    long long say = blok[1];
    for (long long i = 0; i < say; i++) {
        char *k = (char *)blok[2 + i * 3];
        long long kl = blok[2 + i * 3 + 1];
        if (kl == anahtar_len && memcmp(k, anahtar_ptr, anahtar_len) == 0) {
            /* Son elemanla degistir */
            if (i < say - 1) {
                blok[2 + i * 3] = blok[2 + (say-1) * 3];
                blok[2 + i * 3 + 1] = blok[2 + (say-1) * 3 + 1];
                blok[2 + i * 3 + 2] = blok[2 + (say-1) * 3 + 2];
            }
            blok[1] = say - 1;
            return 1;
        }
    }
    return 0;
}
/* ========== Dilim (Slice) İşlemleri ========== */

/* Dizi dilimi: dizi[baş:son] -> yeni dizi */
TrDizi _tr_dizi_dilim(long long *ptr, long long count,
                       long long başlangıç, long long bitis) {
    TrDizi sonuç = {NULL, 0};
    /* Negatif indeks desteği */
    if (başlangıç < 0) başlangıç = count + başlangıç;
    if (bitis < 0) bitis = count + bitis;
    /* Sınır kontrolü */
    if (başlangıç < 0) başlangıç = 0;
    if (bitis > count) bitis = count;
    if (başlangıç >= bitis) return sonuç;

    long long yeni_count = bitis - başlangıç;
    long long boyut = yeni_count * sizeof(long long);
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, boyut);
    if (!yeni) return sonuç;
    for (long long i = 0; i < yeni_count; i++) {
        yeni[i] = ptr[başlangıç + i];
    }
    sonuç.ptr = yeni;
    sonuç.count = yeni_count;
    return sonuç;
}

/* ========== Test Çerçevesi ========== */
/* === Numarala/Eşle/Ters/Dilimle (her zaman kullanılabilir) === */

TrDizi _tr_numarala(long long *ptr, long long count) {
    TrDizi sonuç = {NULL, 0};
    if (count <= 0) return sonuç;
    /* Her eleman 2 elemanlı alt dizi: [indeks, deger] */
    long long *dis = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, count * sizeof(long long));
    if (!dis) return sonuç;
    for (long long i = 0; i < count; i++) {
        long long *ic = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, 2 * sizeof(long long));
        if (ic) { ic[0] = i; ic[1] = ptr[i]; }
        dis[i] = (long long)ic;
    }
    sonuç.ptr = dis;
    sonuç.count = count;
    return sonuç;
}

TrDizi _tr_eslestir(long long *ptr1, long long c1, long long *ptr2, long long c2) {
    TrDizi sonuç = {NULL, 0};
    long long min_c = c1 < c2 ? c1 : c2;
    if (min_c <= 0) return sonuç;
    long long *dis = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, min_c * sizeof(long long));
    if (!dis) return sonuç;
    for (long long i = 0; i < min_c; i++) {
        long long *ic = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, 2 * sizeof(long long));
        if (ic) { ic[0] = ptr1[i]; ic[1] = ptr2[i]; }
        dis[i] = (long long)ic;
    }
    sonuç.ptr = dis;
    sonuç.count = min_c;
    return sonuç;
}

TrDizi _tr_ters_dizi(long long *ptr, long long count) {
    TrDizi sonuç = {NULL, 0};
    if (count <= 0) return sonuç;
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, count * sizeof(long long));
    if (!yeni) return sonuç;
    for (long long i = 0; i < count; i++) {
        yeni[i] = ptr[count - 1 - i];
    }
    sonuç.ptr = yeni;
    sonuç.count = count;
    return sonuç;
}

TrDizi _tr_dilimle(long long *ptr, long long count, long long bas, long long bit) {
    TrDizi sonuç = {NULL, 0};
    if (bas < 0) bas = 0;
    if (bit > count) bit = count;
    if (bas >= bit) return sonuç;
    long long yeni_c = bit - bas;
    long long *yeni = (long long *)_tr_nesne_olustur(NESNE_TIP_DIZI, yeni_c * sizeof(long long));
    if (!yeni) return sonuç;
    for (long long i = 0; i < yeni_c; i++) yeni[i] = ptr[bas + i];
    sonuç.ptr = yeni;
    sonuç.count = yeni_c;
    return sonuç;
}

/* Metin dilimi: metin[baş:son] -> yeni metin */
TrMetin _tr_metin_dilim(const char *ptr, long long len,
                          long long başlangıç, long long bitis) {
    TrMetin m = {NULL, 0};
    /* Negatif indeks desteği */
    if (başlangıç < 0) başlangıç = len + başlangıç;
    if (bitis < 0) bitis = len + bitis;
    /* Sınır kontrolü */
    if (başlangıç < 0) başlangıç = 0;
    if (bitis > len) bitis = len;
    if (başlangıç >= bitis) return m;

    long long yeni_len = bitis - başlangıç;
    m.ptr = (char *)malloc(yeni_len);
    if (m.ptr) memcpy(m.ptr, ptr + başlangıç, yeni_len);
    m.len = yeni_len;
    return m;
}
