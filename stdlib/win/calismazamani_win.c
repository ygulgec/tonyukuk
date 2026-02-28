/*
 * Tonyukuk Programlama Dili - Çalışma Zamanı Kütüphanesi (Windows)
 *
 * Core fonksiyonlar: GC, I/O, string dönüşüm, profil, test.
 * mmap/munmap yerine VirtualAlloc/VirtualFree kullanır.
 * clock_gettime yerine QueryPerformanceCounter kullanır.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Metin dönüşü: 16 byte struct */
typedef struct { char *ptr; long long len; } TrMetin;

/* ---- Referans Sayaci GC ---- */

typedef struct {
    long long ref_sayisi;
    long long tip;
    long long boyut;
} NesneBaslik;

#define NESNE_BASLIK_BOYUT 24
#define NESNE_BASLIK(ptr) ((NesneBaslik *)((char *)(ptr) - NESNE_BASLIK_BOYUT))

#define NESNE_TIP_METIN   0
#define NESNE_TIP_DIZI    1
#define NESNE_TIP_SOZLUK  2
#define NESNE_TIP_SINIF   3
#define NESNE_TIP_KUME    4
#define NESNE_TIP_KAPANIS 5

/* Kapanış ortam işaretçisi */
long long *_tr_kapanis_ortam = NULL;

/* Yönetilen nesne oluştur: VirtualAlloc kullanır */
void *_tr_nesne_olustur(long long tip, long long boyut) {
    long long toplam = boyut + NESNE_BASLIK_BOYUT;
    void *raw = VirtualAlloc(NULL, (SIZE_T)toplam,
                             MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!raw) return NULL;
    NesneBaslik *b = (NesneBaslik *)raw;
    b->ref_sayisi = 1;
    b->tip = tip;
    b->boyut = toplam;
    memset((char *)raw + NESNE_BASLIK_BOYUT, 0, (size_t)boyut);
    return (char *)raw + NESNE_BASLIK_BOYUT;
}

/* Referans artır */
void _tr_ref_artir(void *ptr) {
    if (!ptr) return;
    NESNE_BASLIK(ptr)->ref_sayisi++;
}

/* Referans azalt, 0'a düşerse serbest bırak */
void _tr_ref_azalt(void *ptr) {
    if (!ptr) return;
    NesneBaslik *b = NESNE_BASLIK(ptr);
    if (--b->ref_sayisi <= 0) {
        VirtualFree(b, 0, MEM_RELEASE);
    }
}

/* ---- I/O ---- */

/* Satırdan oku */
TrMetin _tr_satiroku(void) {
    TrMetin m = {NULL, 0};
    char buf[4096];
    if (fgets(buf, (int)sizeof(buf), stdin) == NULL) return m;
    int len = (int)strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') { buf[len - 1] = '\0'; len--; }
    m.ptr = (char *)malloc(len);
    if (m.ptr) memcpy(m.ptr, buf, len);
    m.len = len;
    return m;
}

/* ---- String Dönüşüm ---- */

TrMetin _tr_tam_metin(long long sayi) {
    TrMetin m;
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", sayi);
    m.ptr = (char *)malloc(len);
    if (m.ptr) memcpy(m.ptr, buf, len);
    m.len = len;
    return m;
}

TrMetin _tr_ondalik_metin(double sayi) {
    TrMetin m;
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%.6g", sayi);
    m.ptr = (char *)malloc(len);
    if (m.ptr) memcpy(m.ptr, buf, len);
    m.len = len;
    return m;
}

long long _tr_metin_tam(const char *ptr, long long len) {
    char buf[64];
    int n = (int)len;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    memcpy(buf, ptr, n);
    buf[n] = '\0';
    return atoll(buf);
}

/* ========== Profil Entegrasyonu ========== */

#define PROFIL_MAKS_ISLEV 256

typedef struct {
    char isim[128];
    long long toplam_ns;
    long long cagri_sayisi;
    LARGE_INTEGER başlangıç;
} ProfilGirdi;

static ProfilGirdi profil_tablo[PROFIL_MAKS_ISLEV];
static int profil_islev_sayisi = 0;
static LARGE_INTEGER qpc_frekans = {0};

static ProfilGirdi *profil_bul_veya_ekle(const char *isim, long long isim_len) {
    for (int i = 0; i < profil_islev_sayisi; i++) {
        if (strncmp(profil_tablo[i].isim, isim, (size_t)isim_len) == 0 &&
            profil_tablo[i].isim[isim_len] == '\0') {
            return &profil_tablo[i];
        }
    }
    if (profil_islev_sayisi >= PROFIL_MAKS_ISLEV) return NULL;
    ProfilGirdi *g = &profil_tablo[profil_islev_sayisi++];
    int n = (int)isim_len;
    if (n >= (int)sizeof(g->isim)) n = (int)sizeof(g->isim) - 1;
    memcpy(g->isim, isim, n);
    g->isim[n] = '\0';
    g->toplam_ns = 0;
    g->cagri_sayisi = 0;
    return g;
}

void _tr_profil_giris(const char *isim, long long isim_len) {
    if (qpc_frekans.QuadPart == 0) QueryPerformanceFrequency(&qpc_frekans);
    ProfilGirdi *g = profil_bul_veya_ekle(isim, isim_len);
    if (!g) return;
    QueryPerformanceCounter(&g->başlangıç);
}

void _tr_profil_cikis(const char *isim, long long isim_len) {
    LARGE_INTEGER bitis;
    QueryPerformanceCounter(&bitis);
    ProfilGirdi *g = profil_bul_veya_ekle(isim, isim_len);
    if (!g) return;
    long long ns = (bitis.QuadPart - g->başlangıç.QuadPart) * 1000000000LL / qpc_frekans.QuadPart;
    g->toplam_ns += ns;
    g->cagri_sayisi++;
}

void _tr_profil_rapor(void) {
    if (profil_islev_sayisi == 0) return;
    fprintf(stderr, "\n=== Profil Raporu ===\n");
    fprintf(stderr, "%-30s %12s %12s %12s\n", "Islev", "Cagri", "Toplam(ms)", "Ort(us)");
    fprintf(stderr, "%-30s %12s %12s %12s\n", "-----", "-----", "----------", "------");
    for (int i = 0; i < profil_islev_sayisi; i++) {
        ProfilGirdi *g = &profil_tablo[i];
        double toplam_ms = (double)g->toplam_ns / 1000000.0;
        double ort_us = g->cagri_sayisi > 0 ?
            (double)g->toplam_ns / (double)g->cagri_sayisi / 1000.0 : 0.0;
        fprintf(stderr, "%-30s %12lld %12.3f %12.3f\n",
                g->isim, g->cagri_sayisi, toplam_ms, ort_us);
    }
    fprintf(stderr, "=====================\n");
}

/* ========== Test Çerçevesi ========== */

static int _test_basarili = 0;
static int _test_basarisiz = 0;

void _tr_dogrula(long long kosul, const char *isim_ptr, long long isim_len, long long satir) {
    if (kosul) {
        _test_basarili++;
    } else {
        _test_basarisiz++;
        fprintf(stderr, "  BASARISIZ: ");
        if (isim_ptr && isim_len > 0) {
            fwrite(isim_ptr, 1, (size_t)isim_len, stderr);
        }
        fprintf(stderr, " (satir %lld)\n", satir);
    }
}

void _tr_test_rapor(void) {
    int toplam = _test_basarili + _test_basarisiz;
    fprintf(stderr, "\n=== Test Raporu ===\n");
    fprintf(stderr, "Toplam: %d | Basarili: %d | Basarisiz: %d\n",
            toplam, _test_basarili, _test_basarisiz);
    fprintf(stderr, "===================\n");
}

/* ========== Metin Birleştirme (LLVM backend) ========== */

TrMetin _metin_birlestir(const char *ptr1, long long len1,
                          const char *ptr2, long long len2) {
    TrMetin m;
    m.len = len1 + len2;
    m.ptr = (char *)malloc((size_t)m.len);
    if (m.ptr) {
        if (ptr1 && len1 > 0) memcpy(m.ptr, ptr1, (size_t)len1);
        if (ptr2 && len2 > 0) memcpy(m.ptr + len1, ptr2, (size_t)len2);
    }
    return m;
}

/* ========== LLVM Backend Uyumlu I/O Fonksiyonları ========== */

void _yazdir_tam(long long sayi) {
    printf("%lld\n", sayi);
}

void _yazdir_metin(const char *ptr, long long len) {
    if (ptr && len > 0) {
        fwrite(ptr, 1, (size_t)len, stdout);
        printf("\n");
    }
}

void _yazdir_ondalik(double sayi) {
    printf("%g\n", sayi);
}

void _yazdir_mantik(long long deger) {
    printf("%s\n", deger ? "dogru" : "yanlis");
}
