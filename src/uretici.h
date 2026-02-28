#ifndef ÜRETİCİ_H
#define ÜRETİCİ_H

#include "agac.h"
#include "tablo.h"
#include "metin.h"
#include "bellek.h"

typedef struct {
    Metin   cikti;           /* assembly çıktısı */
    Metin   veri_bolumu;     /* .data / .rodata bölümü */
    Metin   bss_bolumu;      /* .bss bölümü (global değişkenler) */
    Metin   yardimcilar;     /* yardımcı fonksiyonlar (_yazdir_tam vs.) */
    int     etiket_sayac;    /* benzersiz etiket üretimi */
    int     metin_sayac;     /* string literal sayacı */
    Arena  *arena;
    Kapsam *kapsam;

    /* Yardımcı fonksiyon üretildi mi? */
    int     yazdir_tam_uretildi;
    int     yazdir_metin_uretildi;
    int     yazdir_ondalik_uretildi;
    int     ondalik_sayac;       /* .rodata'daki double sabit sayacı */

    /* Döngü etiketleri (kır/devam için) */
    int     dongu_baslangic_etiket;
    int     dongu_bitis_etiket;
    int     dongu_yoksa_etiket;   /* kır ile yoksa bloğunu atla */

    /* Etiketli kır/devam için döngü yığını */
    #define URETICI_MAKS_DONGU 32
    struct {
        char *isim;
        int   bitis_etiket;
        int   baslangic_etiket;
    } dongu_yigini[URETICI_MAKS_DONGU];
    int     dongu_derinligi;

    /* Mevcut işlev bilgisi */
    TipTürü mevcut_islev_donus_tipi;

    /* Sınıf metot üretimi */
    char   *mevcut_sinif;          /* metot üretilirken sınıf adı */

    /* matematik modülü yardımcıları */
    int     mat_mutlak_uretildi;
    int     mat_kuvvet_uretildi;
    int     mat_karekok_uretildi;
    int     mat_min_uretildi;
    int     mat_maks_uretildi;
    int     mat_mod_uretildi;

    /* metin modülü yardımcıları */
    int     mtn_harf_buyut_uretildi;
    int     mtn_harf_kucult_uretildi;
    int     mtn_kes_uretildi;
    int     mtn_bul_uretildi;
    int     mtn_icerir_uretildi;

    /* Runtime hata yardımcıları */
    int     hata_bolme_sifir_uretildi;
    int     hata_dizi_sinir_uretildi;
    int     hata_bellek_uretildi;

    /* Exception handling (dene/yakala) */
    int     istisna_bss_uretildi;   /* _istisna_cerceve BSS üretildi mi */

    /* her...icin dongu stack temizligi */
    int     her_icin_temizlik;

    /* Debug modu (DWARF bilgisi) */
    int     debug_modu;
    const char *kaynak_dosya;

    /* Profil modu (-profil bayrağı) */
    int     profil_modu;
    char   *profil_mevcut_islev;  /* profil modu: mevcut işlev adı */
    int     profil_isim_sayac;    /* profil modu: fonksiyon isim string sayacı */
    int     profil_isim_idx;      /* profil modu: mevcut işlev isim label indeksi */

    /* Kaynak harita modu (-harita bayrağı) */
    int     harita_modu;
    int     asm_satir_sayac;   /* assembly satır sayacı (kaynak harita için) */

    /* Test modu (--test bayrağı) */
    int     test_modu;
    int     test_sayaci;       /* test fonksiyon sayacı */

    /* Monomorphization: generic özelleştirmeler */
    void   *generic_ozellestirilmisler;   /* GenericÖzelleştirme dizisi */
    int     generic_ozellestirme_sayisi;
} Üretici;

/* AST'den x86_64 assembly üret */
void kod_üret(Üretici *u, Düğüm *program, Arena *arena);

/* Üretilen assembly'i dosyaya yaz */
int assembly_yaz(Üretici *u, const char *dosya_adi);

/* Temizlik */
void üretici_serbest(Üretici *u);

#endif
