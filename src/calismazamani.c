/*
 * Tonyukuk Programlama Dili - Calisma Zamani Kutuphanesi (Cekirdek)
 *
 * Sadece core fonksiyonlar: GC, I/O, string donusum, profil, test.
 * Modül fonksiyonları stdlib/*_cz.c dosyalarında.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

/* Metin donusu: 16 byte struct -> rax=ptr, rdx=len (System V ABI) */
typedef struct { char *ptr; long long len; } TrMetin;

/* ---- Referans Sayaci GC ---- */

/* 24-byte header: her heap nesnesinin oncesinde */
typedef struct {
    long long ref_sayisi;
    long long tip;
    long long boyut;       /* header dahil toplam allocation boyutu */
} NesneBaslik;

#define NESNE_BASLIK_BOYUT 24
#define NESNE_BASLIK(ptr) ((NesneBaslik *)((char *)(ptr) - NESNE_BASLIK_BOYUT))

/* Nesne tip sabitleri */
#define NESNE_TIP_METIN   0
#define NESNE_TIP_DIZI    1
#define NESNE_TIP_SOZLUK  2
#define NESNE_TIP_SINIF   3
#define NESNE_TIP_KUME    4
#define NESNE_TIP_KAPANIS 5

/* Kapanış ortam işaretçisi (closure environment) */
long long *_tr_kapanis_ortam = NULL;

/* Yonetilen nesne olustur: header + kullanici alani */
void *_tr_nesne_olustur(long long tip, long long boyut) {
    long long toplam = boyut + NESNE_BASLIK_BOYUT;
    void *raw = mmap(NULL, (size_t)toplam, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) return NULL;
    NesneBaslik *b = (NesneBaslik *)raw;
    b->ref_sayisi = 1;
    b->tip = tip;
    b->boyut = toplam;
    memset((char *)raw + NESNE_BASLIK_BOYUT, 0, (size_t)boyut);
    return (char *)raw + NESNE_BASLIK_BOYUT;
}

/* Referans artir */
void _tr_ref_artir(void *ptr) {
    if (!ptr) return;
    NESNE_BASLIK(ptr)->ref_sayisi++;
}

/* Referans azalt, 0'a duserse serbest birak */
void _tr_ref_azalt(void *ptr) {
    if (!ptr) return;
    NesneBaslik *b = NESNE_BASLIK(ptr);
    if (--b->ref_sayisi <= 0) {
        munmap(b, (size_t)b->boyut);
    }
}

/* ---- I/O ---- */

/* Satirdan oku */
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

/* Dosya fonksiyonları artık stdlib/dosya_cz.c'de */

/* ---- String Donusum ---- */

/* Tam sayi -> metin */
TrMetin _tr_tam_metin(long long sayi) {
    TrMetin m;
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", sayi);
    m.ptr = (char *)malloc(len);
    if (m.ptr) memcpy(m.ptr, buf, len);
    m.len = len;
    return m;
}

/* Ondalik sayi -> metin */
TrMetin _tr_ondalik_metin(double sayi) {
    TrMetin m;
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%.6g", sayi);
    m.ptr = (char *)malloc(len);
    if (m.ptr) memcpy(m.ptr, buf, len);
    m.len = len;
    return m;
}

/* Metin -> tam sayi */
long long _tr_metin_tam(const char *ptr, long long len) {
    char buf[64];
    int n = (int)len;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    memcpy(buf, ptr, n);
    buf[n] = '\0';
    return atoll(buf);
}

/* Metin -> ondalık dönüşümü: stdlib/metin_cz.c'de tanımlı */

/* ========== Profil Entegrasyonu ========== */

#define PROFIL_MAKS_ISLEV 256

typedef struct {
    char isim[128];
    long long toplam_ns;
    long long cagri_sayisi;
    struct timespec başlangıç;
} ProfilGirdi;

static ProfilGirdi profil_tablo[PROFIL_MAKS_ISLEV];
static int profil_islev_sayisi = 0;

static ProfilGirdi *profil_bul_veya_ekle(const char *isim, long long isim_len) {
    /* Mevcut girdide ara */
    for (int i = 0; i < profil_islev_sayisi; i++) {
        if (strncmp(profil_tablo[i].isim, isim, (size_t)isim_len) == 0 &&
            profil_tablo[i].isim[isim_len] == '\0') {
            return &profil_tablo[i];
        }
    }
    /* Yeni girdi ekle */
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
    ProfilGirdi *g = profil_bul_veya_ekle(isim, isim_len);
    if (!g) return;
    clock_gettime(CLOCK_MONOTONIC, &g->başlangıç);
}

void _tr_profil_cikis(const char *isim, long long isim_len) {
    struct timespec bitis;
    clock_gettime(CLOCK_MONOTONIC, &bitis);
    ProfilGirdi *g = profil_bul_veya_ekle(isim, isim_len);
    if (!g) return;
    long long ns = (bitis.tv_sec - g->başlangıç.tv_sec) * 1000000000LL +
                   (bitis.tv_nsec - g->başlangıç.tv_nsec);
    g->toplam_ns += ns;
    g->cagri_sayisi++;
}

void _tr_profil_rapor(void) {
    if (profil_islev_sayisi == 0) return;
    fprintf(stderr, "\n=== Profil Raporu ===\n");
    fprintf(stderr, "%-30s %12s %12s %12s\n",
            "Islev", "Cagri", "Toplam(ms)", "Ort(us)");
    fprintf(stderr, "%-30s %12s %12s %12s\n",
            "-----", "-----", "----------", "------");
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
        fprintf(stderr, "  \033[31mBA\xc5\x9e" "ARISIZ\033[0m: ");
        if (isim_ptr && isim_len > 0) {
            fwrite(isim_ptr, 1, (size_t)isim_len, stderr);
        }
        fprintf(stderr, " (sat\xc4\xb1r %lld)\n", satir);
    }
}

void _tr_test_rapor(void) {
    int toplam = _test_basarili + _test_basarisiz;
    fprintf(stderr, "\n=== Test Raporu ===\n");
    fprintf(stderr, "Toplam: %d | ", toplam);
    fprintf(stderr, "\033[32mBa\xc5\x9f" "ar\xc4\xb1l\xc4\xb1: %d\033[0m | ", _test_basarili);
    if (_test_basarisiz > 0) {
        fprintf(stderr, "\033[31mBa\xc5\x9f" "ar\xc4\xb1s\xc4\xb1z: %d\033[0m\n", _test_basarisiz);
    } else {
        fprintf(stderr, "Ba\xc5\x9f" "ar\xc4\xb1s\xc4\xb1z: 0\n");
    }
    fprintf(stderr, "===================\n");
}

/* doğrula_eşit: tam sayı karşılaştırmalı assertion */
void _tr_dogrula_esit_tam(long long beklenen, long long gercek,
                           const char *isim_ptr, long long isim_len, long long satir) {
    if (beklenen == gercek) {
        _test_basarili++;
    } else {
        _test_basarisiz++;
        fprintf(stderr, "  \033[31mBA\xc5\x9e" "ARISIZ\033[0m: ");
        if (isim_ptr && isim_len > 0) {
            fwrite(isim_ptr, 1, (size_t)isim_len, stderr);
        }
        fprintf(stderr, " (sat\xc4\xb1r %lld)\n", satir);
        fprintf(stderr, "    Beklenen: %lld\n    Ger\xc3\xa7" "ek:   %lld\n", beklenen, gercek);
    }
}

/* doğrula_eşit: metin karşılaştırmalı assertion */
void _tr_dogrula_esit_metin(const char *beklenen_ptr, long long beklenen_len,
                              const char *gercek_ptr, long long gercek_len,
                              const char *isim_ptr, long long isim_len, long long satir) {
    int esit = (beklenen_len == gercek_len);
    if (esit && beklenen_len > 0) {
        esit = (memcmp(beklenen_ptr, gercek_ptr, (size_t)beklenen_len) == 0);
    }
    if (esit) {
        _test_basarili++;
    } else {
        _test_basarisiz++;
        fprintf(stderr, "  \033[31mBA\xc5\x9e" "ARISIZ\033[0m: ");
        if (isim_ptr && isim_len > 0) {
            fwrite(isim_ptr, 1, (size_t)isim_len, stderr);
        }
        fprintf(stderr, " (sat\xc4\xb1r %lld)\n", satir);
        fprintf(stderr, "    Beklenen: \"");
        if (beklenen_ptr && beklenen_len > 0)
            fwrite(beklenen_ptr, 1, (size_t)beklenen_len, stderr);
        fprintf(stderr, "\"\n    Ger\xc3\xa7" "ek:   \"");
        if (gercek_ptr && gercek_len > 0)
            fwrite(gercek_ptr, 1, (size_t)gercek_len, stderr);
        fprintf(stderr, "\"\n");
    }
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

/* Tam sayı yazdırma (newline ile) */
void _yazdir_tam(long long sayi) {
    printf("%lld\n", sayi);
}

/* Metin yazdırma (ptr, len) */
void _yazdir_metin(const char *ptr, long long len) {
    if (ptr && len > 0) {
        fwrite(ptr, 1, (size_t)len, stdout);
    }
    printf("\n");
}

/* Ondalık sayı yazdırma — x86 backend ile uyumlu format */
void _yazdir_ondalik(double sayi) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%f", sayi);
    /* Sondaki gereksiz sıfırları kaldır ama en az bir ondalık basamak kalsın */
    char *nokta = strchr(buf, '.');
    if (nokta) {
        char *son = buf + strlen(buf) - 1;
        while (son > nokta + 1 && *son == '0') {
            *son = '\0';
            son--;
        }
    }
    printf("%s\n", buf);
}

/* Mantık değeri yazdırma */
void _yazdir_mantik(long long deger) {
    printf("%s\n", deger ? "dogru" : "yanlis");
}

/* ========== Dizi Sınır Hatası (LLVM backend) ========== */

void _tr_dizi_sinir_hatasi(void) {
    fprintf(stderr, "Hata: dizi sinir asimi\n");
    exit(1);
}

/* ========== İstisna İşleme (LLVM backend) ========== */

#include <setjmp.h>

#define ISTISNA_MAKS_DERINLIK 32

typedef struct {
    jmp_buf buf;
    long long deger;
    long long deger_len;
    long long tip;
} TrIstisnaCerceve;

static TrIstisnaCerceve _istisna_yigin[ISTISNA_MAKS_DERINLIK];
static int _istisna_derinlik = 0;

/* dene bloğu başlangıcı: 0=normal, 1=istisna yakalandı */
int _tr_dene_baslat(void) {
    if (_istisna_derinlik >= ISTISNA_MAKS_DERINLIK) return -1;
    TrIstisnaCerceve *c = &_istisna_yigin[_istisna_derinlik++];
    c->deger = 0;
    c->tip = 0;
    return setjmp(c->buf);
}

/* dene bloğu normal tamamlandı */
void _tr_dene_bitir(void) {
    if (_istisna_derinlik > 0) _istisna_derinlik--;
}

/* İstisna fırlat */
void _tr_firlat_deger(long long deger) {
    if (_istisna_derinlik > 0) {
        _istisna_derinlik--;
        TrIstisnaCerceve *c = &_istisna_yigin[_istisna_derinlik];
        c->deger = deger;
        c->deger_len = 0;
        c->tip = 0;
        longjmp(c->buf, 1);
    } else {
        fprintf(stderr, "Hata: Yakalanmam\xc4\xb1\xc5\x9f istisna!\n");
        exit(1);
    }
}

/* Tipli istisna fırlat */
void _tr_firlat_tipli(long long deger, long long deger_len, long long tip) {
    if (_istisna_derinlik > 0) {
        _istisna_derinlik--;
        TrIstisnaCerceve *c = &_istisna_yigin[_istisna_derinlik];
        c->deger = deger;
        c->deger_len = deger_len;
        c->tip = tip;
        longjmp(c->buf, 1);
    } else {
        fprintf(stderr, "Hata: Yakalanmam\xc4\xb1\xc5\x9f istisna!\n");
        exit(1);
    }
}

/* Yakalanan istisna değeri */
long long _tr_istisna_deger(void) {
    if (_istisna_derinlik >= 0 && _istisna_derinlik < ISTISNA_MAKS_DERINLIK) {
        return _istisna_yigin[_istisna_derinlik].deger;
    }
    return 0;
}

/* Yakalanan istisna tip kodu */
long long _tr_istisna_tip(void) {
    if (_istisna_derinlik >= 0 && _istisna_derinlik < ISTISNA_MAKS_DERINLIK) {
        return _istisna_yigin[_istisna_derinlik].tip;
    }
    return 0;
}

/* Yakalanan istisna metin uzunluğu */
long long _tr_istisna_deger_len(void) {
    if (_istisna_derinlik >= 0 && _istisna_derinlik < ISTISNA_MAKS_DERINLIK) {
        return _istisna_yigin[_istisna_derinlik].deger_len;
    }
    return 0;
}
