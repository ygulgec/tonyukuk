/*
 * uretici_elf.h — Doğrudan ELF64 İkili Üretim Backend'i
 *
 * Tonyukuk derleyicisi için harici araç (as, gcc) gerektirmeden
 * doğrudan ELF64 çalıştırılabilir dosya üreten backend başlığı.
 */

#ifndef ÜRETİCİ_ELF_H
#define ÜRETİCİ_ELF_H

#include "agac.h"
#include "tablo.h"
#include "bellek.h"
#include "x86_kodlayici.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *                     ELF ÜRETİCİ YAPISI
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    İkilKod  kod;               /* .text bölümü: makine kodu */
    İkilKod  salt_okunur;       /* .rodata bölümü: sabit veriler */
    int      bss_boyut;         /* .bss boyutu (sıfırlanmış global veriler) */

    Arena   *arena;
    Kapsam  *kapsam;

    /* ---- Etiket sistemi ---- */
    int      etiket_sayaç;
    int     *etiket_ofsetler;   /* etiket_no → kod buffer ofseti (-1 = tanımsız) */
    int      etiket_kapasite;

    /* ---- İleri referans yamaları (atlama/çağrı) ---- */
    struct {
        int kod_ofseti;         /* rel32 alanının kod buffer'ındaki ofseti */
        int etiket_no;          /* hedef etiket numarası */
    } *yamalar;
    int     yama_sayısı;
    int     yama_kapasite;

    /* ---- İşlev tablosu ---- */
    struct {
        char *isim;
        int   etiket;
    } *işlev_tablosu;
    int     işlev_sayısı;
    int     işlev_kapasite;

    /* ---- Metin literal tablosu ---- */
    struct {
        int rodata_ofseti;
        int uzunluk;
    } *metin_tablosu;
    int     metin_sayaç;
    int     metin_kapasite;

    /* ---- Ondalık sabit tablosu ---- */
    struct {
        int rodata_ofseti;
    } *ondalık_tablosu;
    int     ondalık_sayaç;
    int     ondalık_kapasite;

    /* ---- Global değişken tablosu (.bss) ---- */
    struct {
        char *isim;
        int   bss_ofseti;
        int   boyut;
    } *genel_değişkenler;
    int     genel_değişken_sayısı;
    int     genel_değişken_kapasite;

    /* ---- RIP-bağıl yeniden konumlama yamaları ---- */

    /* Kod → Rodata yamaları */
    struct {
        int kod_ofseti;         /* disp32 alanının kod buffer ofseti */
        int rodata_ofseti;      /* hedef rodata ofseti */
    } *rodata_yamaları;
    int     rodata_yama_sayısı;
    int     rodata_yama_kapasite;

    /* Kod → BSS yamaları */
    struct {
        int kod_ofseti;
        int bss_indeksi;        /* genel_değişkenler[] indeksi */
        int ek_ofset;           /* alan içi ek ofset (örn: +8 metin uzunluğu) */
    } *bss_yamaları;
    int     bss_yama_sayısı;
    int     bss_yama_kapasite;

    /* ---- Yardımcı fonksiyon bayrakları ---- */
    int     yazdır_tam_üretildi;
    int     yazdır_metin_üretildi;
    int     yazdır_ondalık_üretildi;
    int     bellek_ayır_üretildi;
    int     metin_birleştir_üretildi;
    int     metin_karşılaştır_üretildi;
    int     hata_bölme_sıfır_üretildi;

    /* Yardımcı fonksiyon etiketleri */
    int     yazdır_tam_etiket;
    int     yazdır_metin_etiket;
    int     yazdır_ondalık_etiket;
    int     bellek_ayır_etiket;
    int     metin_birleştir_etiket;
    int     metin_karşılaştır_etiket;
    int     hata_bölme_sıfır_etiket;

    /* ---- Döngü etiketleri ---- */
    int     döngü_başlangıç_etiket;
    int     döngü_bitiş_etiket;

    #define ELF_MAKS_DÖNGÜ 32
    struct {
        char *isim;
        int   başlangıç;
        int   bitiş;
    } döngü_yığını[ELF_MAKS_DÖNGÜ];
    int     döngü_derinliği;

    /* ---- İşlev bağlamı ---- */
    TipTürü mevcut_işlev_dönüş_tipi;
    char   *mevcut_sınıf;

    /* ---- Giriş noktası ---- */
    int     ana_gövde_etiket;

    /* ---- Yeni satır rodata ofseti ---- */
    int     yeni_satır_ofseti;

    /* ---- Monomorphization ---- */
    void   *generic_özelleştirilmişler;
    int     generic_özelleştirme_sayısı;
} ElfÜretici;

/* ═══════════════════════════════════════════════════════════════════════════
 *                     GENEL API
 * ═══════════════════════════════════════════════════════════════════════════ */

/* AST'den doğrudan ELF64 makine kodu üret */
void kod_üret_elf64(ElfÜretici *eü, Düğüm *program, Arena *arena);

/* Üretilen makine kodunu ELF64 formatında dosyaya yaz */
int elf64_dosya_yaz(ElfÜretici *eü, const char *dosya_adı);

/* Temizlik */
void elf_üretici_serbest(ElfÜretici *eü);

#endif /* ÜRETİCİ_ELF_H */
