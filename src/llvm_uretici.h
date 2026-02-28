/*
 * Tonyukuk Programlama Dili - LLVM IR Üretici
 *
 * Bu modül, Tonyukuk AST'sini LLVM IR'a dönüştürür.
 * Desteklenen hedef platformlar:
 *   - x86_64-pc-linux-gnu (Linux)
 *   - x86_64-pc-windows-msvc (Windows)
 *   - aarch64-apple-darwin (macOS ARM64)
 *   - wasm32-unknown-unknown (WebAssembly)
 *   - riscv64-unknown-linux-gnu (RISC-V)
 *
 * Yazar: Tonyukuk Geliştirici Ekibi
 * Tarih: 2026
 */

#ifndef LLVM_ÜRETİCİ_H
#define LLVM_ÜRETİCİ_H

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/DebugInfo.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/LLJIT.h>
#include "agac.h"
#include "tablo.h"
#include "bellek.h"

/* Hata ayıklama (Debug) bilgisi yapıları */
typedef struct LLVMDebugBilgisi {
    LLVMDIBuilderRef di_builder;     /* Debug info builder */
    LLVMMetadataRef derleme_birimi;  /* Compile unit */
    LLVMMetadataRef dosya;           /* File metadata */
    LLVMMetadataRef *kapsam_yigini;  /* Scope stack */
    int kapsam_derinlik;             /* Current scope depth */
} LLVMDebugBilgisi;

/* JIT bileşenleri */
typedef struct LLVMJITBilgisi {
    LLVMOrcThreadSafeContextRef ts_context;
    LLVMOrcLLJITRef lljit;
    int aktif;
} LLVMJITBilgisi;

/* ═══════════════════════════════════════════════════════════════════════════
 *                         TİP TANIMLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Sembol girişi - değişken veya işlev için */
typedef struct LLVMSembolGirişi {
    char *isim;                    /* Sembol adı */
    LLVMValueRef deger;            /* LLVM değeri (alloca veya global) */
    LLVMTypeRef tip;               /* LLVM tipi */
    int parametre_mi;              /* Parametre mi? */
    int global_mi;                 /* Global mi? */
    int metin_dizisi;              /* Metin elemanları olan dizi mi? */
    struct LLVMSembolGirişi *sonraki;
} LLVMSembolGirişi;

/* Sembol tablosu */
typedef struct LLVMSembolTablosu {
    LLVMSembolGirişi *girisler;    /* Bağlı liste */
    struct LLVMSembolTablosu *ust; /* Üst kapsam */
} LLVMSembolTablosu;

/* Sınıf bilgisi */
typedef struct LLVMSınıfBilgisi {
    char *isim;                    /* Sınıf adı */
    LLVMTypeRef struct_tipi;       /* LLVM struct tipi */
    char **alan_isimleri;          /* Alan isimleri */
    LLVMTypeRef *alan_tipleri;     /* Alan tipleri */
    int *alan_statik;              /* Statik alan mı? (1=statik, 0=değil) */
    LLVMValueRef *statik_globals;  /* Statik alanlar için LLVM global referansları */
    int alan_sayisi;               /* Alan sayısı */
    struct LLVMSınıfBilgisi *sonraki;
} LLVMSınıfBilgisi;

/* Metin sabiti girişi */
typedef struct LLVMMetinSabiti {
    char *metin;                   /* Orijinal metin */
    LLVMValueRef global_ref;       /* Global string referansı */
    struct LLVMMetinSabiti *sonraki;
} LLVMMetinSabiti;

/* Ana LLVM Üretici yapısı */
typedef struct {
    /* LLVM temel bileşenleri */
    LLVMContextRef baglam;         /* LLVM bağlamı */
    LLVMModuleRef modul;           /* LLVM modülü */
    LLVMBuilderRef olusturucu;     /* IR oluşturucu */

    /* Hedef makine */
    LLVMTargetMachineRef hedef_makine;
    char *hedef_uclu;              /* Target triple (ör: x86_64-pc-linux-gnu) */

    /* Sembol yönetimi */
    LLVMSembolTablosu *sembol_tablosu;  /* Mevcut kapsam */
    LLVMSınıfBilgisi *siniflar;         /* Sınıf tanımları */
    LLVMMetinSabiti *metin_sabitleri;   /* String literaller */
    int metin_sayaci;                    /* Metin sabiti sayacı */

    /* Mevcut işlev bağlamı */
    LLVMValueRef mevcut_islev;     /* Şu an derlenen işlev */
    char *mevcut_sinif;            /* Şu an derlenen sınıfın adı (metot için) */

    /* Döngü kontrol blokları */
    LLVMBasicBlockRef dongu_cikis; /* kır (break) için */
    LLVMBasicBlockRef dongu_devam; /* devam (continue) için */

    /* Etiketli kır/devam için döngü yığını */
    #define LLVM_MAKS_DONGU 32
    struct {
        char *isim;
        LLVMBasicBlockRef cikis;
        LLVMBasicBlockRef devam;
    } dongu_yigini[LLVM_MAKS_DONGU];
    int dongu_derinligi;

    /* Etiket sayacı */
    int etiket_sayaci;

    /* Ayarlar */
    int optimizasyon_seviyesi;     /* 0, 1, 2, 3 */
    int hata_ayiklama;             /* Debug bilgisi ekle */
    int ir_dogrula;                /* IR doğrulama aktif mi */

    /* Debug bilgisi (DWARF) */
    LLVMDebugBilgisi *debug_bilgi; /* NULL ise debug devre dışı */

    /* JIT desteği */
    LLVMJITBilgisi *jit_bilgi;     /* NULL ise JIT devre dışı */

    /* Bellek yönetimi */
    Arena *arena;                  /* Bellek havuzu */

    /* Kaynak dosya bilgisi */
    const char *kaynak_dosya;
    int mevcut_satir;              /* Debug için mevcut satır numarası */
    int mevcut_sutun;              /* Debug için mevcut sütun numarası */

    /* Test modu (--test/--sına bayrağı) */
    int test_modu;

    /* Monomorphization: generic özelleştirmeler */
    void *generic_ozellestirilmisler;   /* GenericÖzelleştirme dizisi */
    int   generic_ozellestirme_sayisi;

    /* Dizi eleman tipi takibi */
    int son_dizi_metin;                /* Son üretilen dizi literal metin elemanı mı içeriyor */
} LLVMÜretici;

/* ═══════════════════════════════════════════════════════════════════════════
 *                         ANA FONKSİYONLAR
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Üretici oluşturma ve yok etme */
LLVMÜretici *llvm_üretici_oluştur(
    Arena *arena,
    const char *modül_adı,
    const char *hedef_uclu,        /* NULL ise varsayılan: x86_64-pc-linux-gnu */
    int optimizasyon_seviyesi,     /* 0-3 */
    int hata_ayiklama              /* 1 ise debug bilgisi ekle */
);

void llvm_uretici_yok_et(LLVMÜretici *u);

/* ═══════════════════════════════════════════════════════════════════════════
 *                         KOD ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Program üretimi (ana giriş noktası) */
void llvm_program_üret(LLVMÜretici *u, Düğüm *program);

/* Üst düzey tanımlar */
void llvm_işlev_üret(LLVMÜretici *u, Düğüm *dugum);
void llvm_sınıf_üret(LLVMÜretici *u, Düğüm *dugum);
void llvm_global_degisken_uret(LLVMÜretici *u, Düğüm *dugum);
void llvm_sayim_uret(LLVMÜretici *u, Düğüm *dugum);

/* İfade üretimi - sonuç LLVMValueRef olarak döner */
LLVMValueRef llvm_ifade_uret(LLVMÜretici *u, Düğüm *dugum);

/* Deyim üretimi */
void llvm_deyim_uret(LLVMÜretici *u, Düğüm *dugum);
void llvm_blok_uret(LLVMÜretici *u, Düğüm *dugum);

/* ═══════════════════════════════════════════════════════════════════════════
 *                         TİP DÖNÜŞÜMLERİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Tonyukuk tipini LLVM tipine dönüştür */
LLVMTypeRef llvm_tip_dönüştür(LLVMÜretici *u, const char *tip_adı);
LLVMTypeRef llvm_tip_turu_donustur(LLVMÜretici *u, TipTürü tip);

/* Sınıf tipi oluştur */
LLVMTypeRef llvm_sinif_tipi_olustur(LLVMÜretici *u, const char *sınıf_adı);

/* Metin (TrMetin) tipi - { i8*, i64 } */
LLVMTypeRef llvm_metin_tipi_al(LLVMÜretici *u);

/* Dizi (TrDizi) tipi - { i64*, i64 } */
LLVMTypeRef llvm_dizi_tipi_al(LLVMÜretici *u);

/* ═══════════════════════════════════════════════════════════════════════════
 *                         SEMBOL YÖNETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Kapsam yönetimi */
void llvm_kapsam_gir(LLVMÜretici *u);
void llvm_kapsam_cik(LLVMÜretici *u);

/* Sembol ekleme ve arama */
void llvm_sembol_ekle(LLVMÜretici *u, const char *isim, LLVMValueRef deger,
                      LLVMTypeRef tip, int parametre_mi, int global_mi);
LLVMSembolGirişi *llvm_sembol_bul(LLVMÜretici *u, const char *isim);

/* Sınıf bilgisi */
void llvm_sınıf_bilgisi_ekle(LLVMÜretici *u, LLVMSınıfBilgisi *bilgi);
LLVMSınıfBilgisi *llvm_sinif_bilgisi_bul(LLVMÜretici *u, const char *isim);

/* ═══════════════════════════════════════════════════════════════════════════
 *                         YARDIMCI FONKSİYONLAR
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Metin sabiti oluştur */
LLVMValueRef llvm_metin_sabiti_oluştur(LLVMÜretici *u, const char *metin);

/* Yeni blok oluştur */
LLVMBasicBlockRef llvm_blok_olustur(LLVMÜretici *u, const char *isim);

/* Standart kütüphane fonksiyonlarını tanımla */
void llvm_stdlib_tanimla(LLVMÜretici *u);

/* Tonyukuk runtime fonksiyonlarını tanımla */
void llvm_runtime_tanimla(LLVMÜretici *u);

/* ═══════════════════════════════════════════════════════════════════════════
 *                         DOSYA ÇIKTISI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* LLVM IR metin dosyasına yaz (.ll) */
int llvm_ir_dosyaya_yaz(LLVMÜretici *u, const char *dosya_adi);

/* LLVM bitcode dosyasına yaz (.bc) */
int llvm_bitcode_dosyaya_yaz(LLVMÜretici *u, const char *dosya_adi);

/* Nesne dosyası üret (.o) */
int llvm_nesne_dosyasi_uret(LLVMÜretici *u, const char *dosya_adi);

/* Assembly dosyası üret (.s) */
int llvm_asm_dosyasi_uret(LLVMÜretici *u, const char *dosya_adi);

/* ═══════════════════════════════════════════════════════════════════════════
 *                         OPTİMİZASYON VE DOĞRULAMA
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Modülü doğrula - detaylı hata mesajları ile */
int llvm_modul_dogrula(LLVMÜretici *u);

/* IR doğrulamayı etkinleştir/devre dışı bırak */
void llvm_ir_dogrulama_ayarla(LLVMÜretici *u, int etkin);

/* Optimizasyonları uygula */
void llvm_optimizasyonlari_uygula(LLVMÜretici *u);

/* Frontend seviyesi optimizasyonlar */
void llvm_frontend_optimizasyonlari(LLVMÜretici *u, Düğüm *program);

/* IR'ı konsola yazdır (debug için) */
void llvm_ir_yazdir(LLVMÜretici *u);

/* ═══════════════════════════════════════════════════════════════════════════
 *                         DEBUG BİLGİSİ (DWARF)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Debug bilgisini başlat */
int llvm_debug_baslat(LLVMÜretici *u, const char *kaynak_dosya);

/* Debug bilgisini sonlandır */
void llvm_debug_sonlandir(LLVMÜretici *u);

/* Satır bilgisi ekle */
void llvm_debug_satir_ayarla(LLVMÜretici *u, int satir, int sutun);

/* Fonksiyon debug kapsamı */
LLVMMetadataRef llvm_debug_fonksiyon_olustur(LLVMÜretici *u, const char *isim,
                                              int satir, LLVMTypeRef tip);

/* Değişken debug bilgisi */
void llvm_debug_degisken_tanimla(LLVMÜretici *u, const char *isim,
                                  LLVMValueRef alloca, int satir);

/* ═══════════════════════════════════════════════════════════════════════════
 *                         JIT (Just-In-Time) DERLEME
 * ═══════════════════════════════════════════════════════════════════════════ */

/* JIT'i başlat */
int llvm_jit_baslat(LLVMÜretici *u);

/* JIT'i sonlandır */
void llvm_jit_sonlandir(LLVMÜretici *u);

/* Modülü JIT'e ekle */
int llvm_jit_modul_ekle(LLVMÜretici *u);

/* JIT'ten fonksiyon al ve çalıştır */
void *llvm_jit_fonksiyon_al(LLVMÜretici *u, const char *isim);

/* JIT ile ana programı çalıştır */
int llvm_jit_calistir(LLVMÜretici *u);

/* ═══════════════════════════════════════════════════════════════════════════
 *                         TİP DÖNÜŞÜM YARDIMCILARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Örtük (implicit) tip dönüşümü */
LLVMValueRef llvm_örtük_dönüşüm(LLVMÜretici *u, LLVMValueRef deger,
                                 LLVMTypeRef hedef_tip);

/* Açık (explicit) tip dönüşümü */
LLVMValueRef llvm_açık_dönüşüm(LLVMÜretici *u, LLVMValueRef deger,
                                LLVMTypeRef hedef_tip, const char *tip_adı);

/* Tip uyumluluğu kontrolü */
int llvm_tipler_uyumlu_mu(LLVMTypeRef tip1, LLVMTypeRef tip2);

#endif /* LLVM_ÜRETİCİ_H */
