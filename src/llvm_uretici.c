/*
 * Tonyukuk Programlama Dili - LLVM IR Üretici İmplementasyonu
 *
 * Bu dosya, Tonyukuk AST'sini LLVM IR'a dönüştüren kod üreticiyi içerir.
 *
 * Bölümler:
 *   1. Başlatma ve Temizlik
 *   2. Tip Dönüşümleri
 *   3. Sembol Yönetimi
 *   4. İşlev Üretimi
 *   5. İfade Üretimi
 *   6. Deyim Üretimi
 *   7. Sınıf Üretimi
 *   8. Döngü ve Kontrol Akışı
 *   9. Dosya Çıktısı
 *  10. Optimizasyon ve Doğrulama
 */

#include "llvm_uretici.h"
#include "hata.h"
#include "modul.h"
#include "anlam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 1: BAŞLATMA VE TEMİZLİK
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_üretici_oluştur - Yeni bir LLVM üretici oluşturur
 *
 * Parametreler:
 *   arena              : Bellek havuzu
 *   modül_adı          : LLVM modül adı
 *   hedef_uclu         : Hedef platform (NULL ise x86_64-pc-linux-gnu)
 *   optimizasyon_seviyesi : 0-3 arası optimizasyon seviyesi
 *   hata_ayiklama      : 1 ise debug bilgisi ekler
 *
 * Döndürür: Yeni LLVMÜretici örneği
 */
LLVMÜretici *llvm_üretici_oluştur(
    Arena *arena,
    const char *modül_adı,
    const char *hedef_uclu,
    int optimizasyon_seviyesi,
    int hata_ayiklama
) {
    /* Bellek ayır */
    LLVMÜretici *u = (LLVMÜretici *)arena_ayir(arena, sizeof(LLVMÜretici));
    if (!u) return NULL;

    /* Arena'yı kaydet */
    u->arena = arena;

    /* LLVM başlatma - tüm hedefleri etkinleştir */
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    /* LLVM bağlamı oluştur */
    u->baglam = LLVMContextCreate();
    if (!u->baglam) {
        fprintf(stderr, "Hata: LLVM bağlamı oluşturulamadı\n");
        return NULL;
    }

    /* LLVM modülü oluştur */
    u->modul = LLVMModuleCreateWithNameInContext(
        modül_adı ? modül_adı : "tonyukuk_modul",
        u->baglam
    );
    if (!u->modul) {
        fprintf(stderr, "Hata: LLVM modülü oluşturulamadı\n");
        LLVMContextDispose(u->baglam);
        return NULL;
    }

    /* IR oluşturucu */
    u->olusturucu = LLVMCreateBuilderInContext(u->baglam);
    if (!u->olusturucu) {
        fprintf(stderr, "Hata: LLVM builder oluşturulamadı\n");
        LLVMDisposeModule(u->modul);
        LLVMContextDispose(u->baglam);
        return NULL;
    }

    /* Hedef üçlü (target triple) ayarla */
    if (hedef_uclu) {
        u->hedef_uclu = arena_strdup(arena, hedef_uclu);
    } else {
        /* Varsayılan: Mevcut platform */
        char *varsayilan = LLVMGetDefaultTargetTriple();
        u->hedef_uclu = arena_strdup(arena, varsayilan);
        LLVMDisposeMessage(varsayilan);
    }
    LLVMSetTarget(u->modul, u->hedef_uclu);

    /* Veri düzeni (data layout) ayarla */
    if (strstr(u->hedef_uclu, "wasm32")) {
        /* WebAssembly 32-bit */
        LLVMSetDataLayout(u->modul,
            "e-m:e-p:32:32-i64:64-n32:64-S128");
    } else if (strstr(u->hedef_uclu, "aarch64") ||
               strstr(u->hedef_uclu, "arm64")) {
        /* ARM64 (macOS, Linux) */
        LLVMSetDataLayout(u->modul,
            "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128");
    } else if (strstr(u->hedef_uclu, "riscv64")) {
        /* RISC-V 64-bit */
        LLVMSetDataLayout(u->modul,
            "e-m:e-p:64:64-i64:64-i128:128-n64-S128");
    } else if (strstr(u->hedef_uclu, "windows")) {
        /* Windows x86_64 */
        LLVMSetDataLayout(u->modul,
            "e-m:w-i64:64-f80:128-n8:16:32:64-S128");
    } else {
        /* Linux x86_64 (varsayılan) */
        LLVMSetDataLayout(u->modul,
            "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128");
    }

    /* Hedef makine oluştur */
    char *hata_mesaji = NULL;
    LLVMTargetRef hedef;
    if (LLVMGetTargetFromTriple(u->hedef_uclu, &hedef, &hata_mesaji) != 0) {
        fprintf(stderr, "Hata: Hedef bulunamadı: %s\n", hata_mesaji);
        LLVMDisposeMessage(hata_mesaji);
        u->hedef_makine = NULL;
    } else {
        /* Optimizasyon seviyesine göre kod üretim seviyesi */
        LLVMCodeGenOptLevel kod_seviyesi;
        switch (optimizasyon_seviyesi) {
            case 0: kod_seviyesi = LLVMCodeGenLevelNone; break;
            case 1: kod_seviyesi = LLVMCodeGenLevelLess; break;
            case 2: kod_seviyesi = LLVMCodeGenLevelDefault; break;
            case 3: kod_seviyesi = LLVMCodeGenLevelAggressive; break;
            default: kod_seviyesi = LLVMCodeGenLevelDefault; break;
        }

        u->hedef_makine = LLVMCreateTargetMachine(
            hedef,
            u->hedef_uclu,
            "generic",           /* CPU */
            "",                  /* Özellikler */
            kod_seviyesi,
            LLVMRelocDefault,    /* Relocation model */
            LLVMCodeModelDefault /* Kod modeli */
        );
    }

    /* Sembol tablosu başlat */
    u->sembol_tablosu = (LLVMSembolTablosu *)arena_ayir(arena, sizeof(LLVMSembolTablosu));
    u->sembol_tablosu->girisler = NULL;
    u->sembol_tablosu->ust = NULL;

    /* Sınıf listesi */
    u->siniflar = NULL;

    /* Metin sabitleri */
    u->metin_sabitleri = NULL;
    u->metin_sayaci = 0;

    /* Mevcut işlev bağlamı */
    u->mevcut_islev = NULL;
    u->mevcut_sinif = NULL;

    /* Döngü kontrol blokları */
    u->dongu_cikis = NULL;
    u->dongu_devam = NULL;
    u->dongu_derinligi = 0;
    memset(u->dongu_yigini, 0, sizeof(u->dongu_yigini));

    /* Etiket sayacı */
    u->etiket_sayaci = 0;

    /* Ayarlar */
    u->optimizasyon_seviyesi = optimizasyon_seviyesi;
    u->hata_ayiklama = hata_ayiklama;
    u->kaynak_dosya = NULL;

    /* Standart kütüphane fonksiyonlarını tanımla */
    llvm_stdlib_tanimla(u);

    /* Tonyukuk runtime fonksiyonlarını tanımla */
    llvm_runtime_tanimla(u);

    return u;
}

/*
 * llvm_uretici_yok_et - LLVM üreticiyi temizler
 */
void llvm_uretici_yok_et(LLVMÜretici *u) {
    if (!u) return;

    /* LLVM kaynaklarını serbest bırak */
    if (u->olusturucu) {
        LLVMDisposeBuilder(u->olusturucu);
    }
    if (u->hedef_makine) {
        LLVMDisposeTargetMachine(u->hedef_makine);
    }
    if (u->modul) {
        LLVMDisposeModule(u->modul);
    }
    if (u->baglam) {
        LLVMContextDispose(u->baglam);
    }

    /* Arena tarafından yönetilen bellek otomatik temizlenir */
}

/*
 * llvm_stdlib_tanimla - C standart kütüphane fonksiyonlarını tanımlar
 */
void llvm_stdlib_tanimla(LLVMÜretici *u) {
    LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(u->baglam);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
    LLVMTypeRef void_tip = LLVMVoidTypeInContext(u->baglam);

    /* puts(char*) -> int */
    {
        LLVMTypeRef param_tipleri[] = { i8_ptr };
        LLVMTypeRef fn_tip = LLVMFunctionType(i32, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "puts", fn_tip);
        llvm_sembol_ekle(u, "puts", fn, fn_tip, 0, 1);
    }

    /* printf(char*, ...) -> int */
    {
        LLVMTypeRef param_tipleri[] = { i8_ptr };
        LLVMTypeRef fn_tip = LLVMFunctionType(i32, param_tipleri, 1, 1); /* varargs */
        LLVMValueRef fn = LLVMAddFunction(u->modul, "printf", fn_tip);
        llvm_sembol_ekle(u, "printf", fn, fn_tip, 0, 1);
    }

    /* malloc(size_t) -> void* */
    {
        LLVMTypeRef param_tipleri[] = { i64 };
        LLVMTypeRef fn_tip = LLVMFunctionType(i8_ptr, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "malloc", fn_tip);
        llvm_sembol_ekle(u, "malloc", fn, fn_tip, 0, 1);
    }

    /* free(void*) */
    {
        LLVMTypeRef param_tipleri[] = { i8_ptr };
        LLVMTypeRef fn_tip = LLVMFunctionType(void_tip, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "free", fn_tip);
        llvm_sembol_ekle(u, "free", fn, fn_tip, 0, 1);
    }

    /* memcpy(void*, void*, size_t) -> void* */
    {
        LLVMTypeRef param_tipleri[] = { i8_ptr, i8_ptr, i64 };
        LLVMTypeRef fn_tip = LLVMFunctionType(i8_ptr, param_tipleri, 3, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "memcpy", fn_tip);
        llvm_sembol_ekle(u, "memcpy", fn, fn_tip, 0, 1);
    }

    /* strlen(char*) -> size_t */
    {
        LLVMTypeRef param_tipleri[] = { i8_ptr };
        LLVMTypeRef fn_tip = LLVMFunctionType(i64, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "strlen", fn_tip);
        llvm_sembol_ekle(u, "strlen", fn, fn_tip, 0, 1);
    }

    /* strcmp(char*, char*) -> int */
    {
        LLVMTypeRef param_tipleri[] = { i8_ptr, i8_ptr };
        LLVMTypeRef fn_tip = LLVMFunctionType(i32, param_tipleri, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "strcmp", fn_tip);
        llvm_sembol_ekle(u, "strcmp", fn, fn_tip, 0, 1);
    }

    /* exit(int) */
    {
        LLVMTypeRef param_tipleri[] = { i32 };
        LLVMTypeRef fn_tip = LLVMFunctionType(void_tip, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "exit", fn_tip);
        llvm_sembol_ekle(u, "exit", fn, fn_tip, 0, 1);
    }
}

/*
 * llvm_runtime_tanimla - Tonyukuk runtime fonksiyonlarını tanımlar
 */
void llvm_runtime_tanimla(LLVMÜretici *u) {
    LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
    LLVMTypeRef void_tip = LLVMVoidTypeInContext(u->baglam);

    /* Metin tipi: { i8*, i64 } */
    LLVMTypeRef metin_tipi = llvm_metin_tipi_al(u);

    /* _tr_nesne_olustur(tip: i64, boyut: i64) -> i8* */
    {
        LLVMTypeRef param_tipleri[] = { i64, i64 };
        LLVMTypeRef fn_tip = LLVMFunctionType(i8_ptr, param_tipleri, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_nesne_olustur", fn_tip);
        llvm_sembol_ekle(u, "_tr_nesne_olustur", fn, fn_tip, 0, 1);
    }

    /* _tr_tam_metin(sayi: i64) -> metin */
    {
        LLVMTypeRef param_tipleri[] = { i64 };
        LLVMTypeRef fn_tip = LLVMFunctionType(metin_tipi, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_tam_metin", fn_tip);
        llvm_sembol_ekle(u, "_tr_tam_metin", fn, fn_tip, 0, 1);
    }

    /* _yazdir_tam(sayi: i64) */
    {
        LLVMTypeRef param_tipleri[] = { i64 };
        LLVMTypeRef fn_tip = LLVMFunctionType(void_tip, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_yazdir_tam", fn_tip);
        llvm_sembol_ekle(u, "_yazdir_tam", fn, fn_tip, 0, 1);
    }

    /* _yazdir_metin(ptr: i8*, len: i64) */
    {
        LLVMTypeRef param_tipleri[] = { i8_ptr, i64 };
        LLVMTypeRef fn_tip = LLVMFunctionType(void_tip, param_tipleri, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_yazdir_metin", fn_tip);
        llvm_sembol_ekle(u, "_yazdir_metin", fn, fn_tip, 0, 1);
    }

    /* _metin_birlestir(ptr1, len1, ptr2, len2) -> metin */
    {
        LLVMTypeRef param_tipleri[] = { i8_ptr, i64, i8_ptr, i64 };
        LLVMTypeRef fn_tip = LLVMFunctionType(metin_tipi, param_tipleri, 4, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_metin_birlestir", fn_tip);
        llvm_sembol_ekle(u, "_metin_birlestir", fn, fn_tip, 0, 1);
    }

    /* _yazdir_ondalik(sayi: double) */
    {
        LLVMTypeRef double_tip = LLVMDoubleTypeInContext(u->baglam);
        LLVMTypeRef param_tipleri[] = { double_tip };
        LLVMTypeRef fn_tip = LLVMFunctionType(void_tip, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_yazdir_ondalik", fn_tip);
        llvm_sembol_ekle(u, "_yazdir_ondalik", fn, fn_tip, 0, 1);
    }

    /* _yazdir_mantik(deger: i64) */
    {
        LLVMTypeRef param_tipleri[] = { i64 };
        LLVMTypeRef fn_tip = LLVMFunctionType(void_tip, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_yazdir_mantik", fn_tip);
        llvm_sembol_ekle(u, "_yazdir_mantik", fn, fn_tip, 0, 1);
    }

    /* _tr_ondalik_metin(sayi: double) -> metin */
    {
        LLVMTypeRef double_tip = LLVMDoubleTypeInContext(u->baglam);
        LLVMTypeRef param_tipleri[] = { double_tip };
        LLVMTypeRef fn_tip = LLVMFunctionType(metin_tipi, param_tipleri, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_ondalik_metin", fn_tip);
        llvm_sembol_ekle(u, "_tr_ondalik_metin", fn, fn_tip, 0, 1);
    }

    /* _tr_metin_tam(ptr: i8*, len: i64) -> i64 */
    {
        LLVMTypeRef param_tipleri[] = { i8_ptr, i64 };
        LLVMTypeRef fn_tip = LLVMFunctionType(i64, param_tipleri, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_metin_tam", fn_tip);
        llvm_sembol_ekle(u, "_tr_metin_tam", fn, fn_tip, 0, 1);
    }

    /* _tr_satiroku() -> metin */
    {
        LLVMTypeRef fn_tip = LLVMFunctionType(metin_tipi, NULL, 0, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_satiroku", fn_tip);
        llvm_sembol_ekle(u, "_tr_satiroku", fn, fn_tip, 0, 1);
    }

    /* Built-in fonksiyon isimleri (Tonyukuk adları → runtime) */
    /* tam_metin(sayi: i64) -> metin */
    {
        LLVMSembolGirişi *s = llvm_sembol_bul(u, "_tr_tam_metin");
        if (s) llvm_sembol_ekle(u, "tam_metin", s->deger, s->tip, 0, 1);
    }
    /* ondalık_metin / ondalik_metin */
    {
        LLVMSembolGirişi *s = llvm_sembol_bul(u, "_tr_ondalik_metin");
        if (s) {
            llvm_sembol_ekle(u, "ondal\xc4\xb1k_metin", s->deger, s->tip, 0, 1);
            llvm_sembol_ekle(u, "ondalik_metin", s->deger, s->tip, 0, 1);
        }
    }
    /* metin_tam */
    {
        LLVMSembolGirişi *s = llvm_sembol_bul(u, "_tr_metin_tam");
        if (s) llvm_sembol_ekle(u, "metin_tam", s->deger, s->tip, 0, 1);
    }
    /* satiroku / satıroku */
    {
        LLVMSembolGirişi *s = llvm_sembol_bul(u, "_tr_satiroku");
        if (s) {
            llvm_sembol_ekle(u, "sat\xc4\xb1roku", s->deger, s->tip, 0, 1);
            llvm_sembol_ekle(u, "satiroku", s->deger, s->tip, 0, 1);
        }
    }

    /* ==== UTF-8 karakter tabanli metin fonksiyonlari ==== */
    /* _tr_utf8_karakter_say(ptr, byte_len) -> i64 */
    {
        LLVMTypeRef pt[] = { i8_ptr, i64 };
        LLVMTypeRef ft = LLVMFunctionType(i64, pt, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_utf8_karakter_say", ft);
        llvm_sembol_ekle(u, "_tr_utf8_karakter_say", fn, ft, 0, 1);
    }
    /* _tr_utf8_karakter_al(ptr, byte_len, indeks) -> metin */
    {
        LLVMTypeRef pt[] = { i8_ptr, i64, i64 };
        LLVMTypeRef ft = LLVMFunctionType(metin_tipi, pt, 3, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_utf8_karakter_al", ft);
        llvm_sembol_ekle(u, "_tr_utf8_karakter_al", fn, ft, 0, 1);
    }
    /* _tr_utf8_dilim(ptr, byte_len, bas, son) -> metin */
    {
        LLVMTypeRef pt[] = { i8_ptr, i64, i64, i64 };
        LLVMTypeRef ft = LLVMFunctionType(metin_tipi, pt, 4, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_utf8_dilim", ft);
        llvm_sembol_ekle(u, "_tr_utf8_dilim", fn, ft, 0, 1);
    }
    /* _tr_utf8_bul(ptr, byte_len, aranan_ptr, aranan_len) -> i64 */
    {
        LLVMTypeRef pt[] = { i8_ptr, i64, i8_ptr, i64 };
        LLVMTypeRef ft = LLVMFunctionType(i64, pt, 4, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_utf8_bul", ft);
        llvm_sembol_ekle(u, "_tr_utf8_bul", fn, ft, 0, 1);
    }
    /* _tr_utf8_kes(ptr, byte_len, bas, uzunluk) -> metin */
    {
        LLVMTypeRef pt[] = { i8_ptr, i64, i64, i64 };
        LLVMTypeRef ft = LLVMFunctionType(metin_tipi, pt, 4, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_utf8_kes", ft);
        llvm_sembol_ekle(u, "_tr_utf8_kes", fn, ft, 0, 1);
    }
    /* _tr_utf8_tersle(ptr, byte_len) -> metin */
    {
        LLVMTypeRef pt[] = { i8_ptr, i64 };
        LLVMTypeRef ft = LLVMFunctionType(metin_tipi, pt, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_utf8_tersle", ft);
        llvm_sembol_ekle(u, "_tr_utf8_tersle", fn, ft, 0, 1);
    }

    /* ==== İstisna İşleme runtime fonksiyonları ==== */

    /* _tr_dene_baslat() -> i32 (0=normal, 1=istisna) */
    {
        LLVMTypeRef i32_tip = LLVMInt32TypeInContext(u->baglam);
        LLVMTypeRef ft = LLVMFunctionType(i32_tip, NULL, 0, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_dene_baslat", ft);
        /* setjmp wrapper: returns_twice attribute gerekli */
        unsigned kind = LLVMGetEnumAttributeKindForName("returns_twice", 13);
        LLVMAttributeRef attr = LLVMCreateEnumAttribute(u->baglam, kind, 0);
        LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex, attr);
        llvm_sembol_ekle(u, "_tr_dene_baslat", fn, ft, 0, 1);
    }
    /* _tr_dene_bitir() -> void */
    {
        LLVMTypeRef ft = LLVMFunctionType(void_tip, NULL, 0, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_dene_bitir", ft);
        llvm_sembol_ekle(u, "_tr_dene_bitir", fn, ft, 0, 1);
    }
    /* _tr_firlat_deger(deger: i64) -> void */
    {
        LLVMTypeRef pt[] = { i64 };
        LLVMTypeRef ft = LLVMFunctionType(void_tip, pt, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_firlat_deger", ft);
        /* noreturn attribute */
        unsigned kind = LLVMGetEnumAttributeKindForName("noreturn", 8);
        LLVMAttributeRef attr = LLVMCreateEnumAttribute(u->baglam, kind, 0);
        LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex, attr);
        llvm_sembol_ekle(u, "_tr_firlat_deger", fn, ft, 0, 1);
    }
    /* _tr_istisna_deger() -> i64 */
    {
        LLVMTypeRef ft = LLVMFunctionType(i64, NULL, 0, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_istisna_deger", ft);
        llvm_sembol_ekle(u, "_tr_istisna_deger", fn, ft, 0, 1);
    }

    /* ==== Sözlük runtime fonksiyonları ==== */

    /* _tr_sozluk_yeni() -> i64 */
    {
        LLVMTypeRef ft = LLVMFunctionType(i64, NULL, 0, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_sozluk_yeni", ft);
        llvm_sembol_ekle(u, "_tr_sozluk_yeni", fn, ft, 0, 1);
    }
    /* _tr_sozluk_ekle(sozluk: i64, anahtar_ptr: i8*, anahtar_len: i64, deger: i64) -> i64 */
    {
        LLVMTypeRef pt[] = { i64, i8_ptr, i64, i64 };
        LLVMTypeRef ft = LLVMFunctionType(i64, pt, 4, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_sozluk_ekle", ft);
        llvm_sembol_ekle(u, "_tr_sozluk_ekle", fn, ft, 0, 1);
    }
    /* _tr_sozluk_oku(sozluk: i64, anahtar_ptr: i8*, anahtar_len: i64) -> i64 */
    {
        LLVMTypeRef pt[] = { i64, i8_ptr, i64 };
        LLVMTypeRef ft = LLVMFunctionType(i64, pt, 3, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_sozluk_oku", ft);
        llvm_sembol_ekle(u, "_tr_sozluk_oku", fn, ft, 0, 1);
    }

    /* ==== Test çerçevesi runtime fonksiyonları ==== */

    /* _tr_dogrula(kosul: i64, isim_ptr: i8*, isim_len: i64, satir: i64) -> void */
    {
        LLVMTypeRef pt[] = { i64, i8_ptr, i64, i64 };
        LLVMTypeRef ft = LLVMFunctionType(void_tip, pt, 4, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_dogrula", ft);
        llvm_sembol_ekle(u, "_tr_dogrula", fn, ft, 0, 1);
    }

    /* _tr_test_rapor() -> void */
    {
        LLVMTypeRef ft = LLVMFunctionType(void_tip, NULL, 0, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_test_rapor", ft);
        llvm_sembol_ekle(u, "_tr_test_rapor", fn, ft, 0, 1);
    }

    /* _tr_dogrula_esit_tam(beklenen: i64, gercek: i64, isim_ptr: i8*, isim_len: i64, satir: i64) -> void */
    {
        LLVMTypeRef pt[] = { i64, i64, i8_ptr, i64, i64 };
        LLVMTypeRef ft = LLVMFunctionType(void_tip, pt, 5, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_dogrula_esit_tam", ft);
        llvm_sembol_ekle(u, "_tr_dogrula_esit_tam", fn, ft, 0, 1);
    }

    /* _tr_dogrula_esit_metin(beklenen_ptr: i8*, beklenen_len: i64, gercek_ptr: i8*, gercek_len: i64, isim_ptr: i8*, isim_len: i64, satir: i64) -> void */
    {
        LLVMTypeRef pt[] = { i8_ptr, i64, i8_ptr, i64, i8_ptr, i64, i64 };
        LLVMTypeRef ft = LLVMFunctionType(void_tip, pt, 7, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, "_tr_dogrula_esit_metin", ft);
        llvm_sembol_ekle(u, "_tr_dogrula_esit_metin", fn, ft, 0, 1);
    }

    /* ==== Kapanış (closure) global değişkeni ==== */
    /* _tr_kapanis_ortam: extern i64* (calismazamani.c'de tanımlı) */
    {
        LLVMTypeRef i64_ptr = LLVMPointerType(i64, 0);
        LLVMValueRef ortam_global = LLVMAddGlobal(u->modul, i64_ptr, "_tr_kapanis_ortam");
        LLVMSetLinkage(ortam_global, LLVMExternalLinkage);
    }
}

/*
 * llvm_modul_kaydet - Modül fonksiyonlarını LLVM sembol tablosuna kaydet
 */
void llvm_modul_kaydet(LLVMÜretici *u, const char *modül_adı) {
    const ModülTanım *mt = modul_bul(modül_adı);
    if (!mt) return;

    LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
    LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
    LLVMTypeRef double_tip = LLVMDoubleTypeInContext(u->baglam);
    LLVMTypeRef metin_tipi = llvm_metin_tipi_al(u);
    LLVMTypeRef dizi_tipi = llvm_dizi_tipi_al(u);

    for (int i = 0; i < mt->fonksiyon_sayisi; i++) {
        const ModülFonksiyon *mf = &mt->fonksiyonlar[i];

        /* Zaten kayıtlıysa atla */
        if (llvm_sembol_bul(u, mf->runtime_isim)) continue;

        /* Parametre tiplerini oluştur */
        /* Metin/dizi parametreleri flat olarak genişletilir: (ptr, len) */
        LLVMTypeRef param_tipleri[16];
        int llvm_param_sayisi = 0;

        for (int j = 0; j < mf->param_sayisi; j++) {
            switch (mf->param_tipleri[j]) {
                case TİP_TAM:
                    param_tipleri[llvm_param_sayisi++] = i64;
                    break;
                case TİP_ONDALIK:
                    param_tipleri[llvm_param_sayisi++] = double_tip;
                    break;
                case TİP_MANTIK:
                    param_tipleri[llvm_param_sayisi++] = i64;
                    break;
                case TİP_METİN:
                    param_tipleri[llvm_param_sayisi++] = i8_ptr;
                    param_tipleri[llvm_param_sayisi++] = i64;
                    break;
                case TİP_DİZİ:
                    param_tipleri[llvm_param_sayisi++] = LLVMPointerType(i64, 0);
                    param_tipleri[llvm_param_sayisi++] = i64;
                    break;
                default:
                    param_tipleri[llvm_param_sayisi++] = i64;
                    break;
            }
        }

        /* Dönüş tipini oluştur */
        LLVMTypeRef donus;
        switch (mf->dönüş_tipi) {
            case TİP_TAM:     donus = i64; break;
            case TİP_ONDALIK: donus = double_tip; break;
            case TİP_MANTIK:  donus = i64; break;
            case TİP_METİN:   donus = metin_tipi; break;
            case TİP_DİZİ:    donus = dizi_tipi; break;
            case TİP_BOŞLUK:  donus = LLVMVoidTypeInContext(u->baglam); break;
            default:          donus = i64; break;
        }

        /* Fonksiyon tanımla */
        LLVMTypeRef fn_tip = LLVMFunctionType(donus, param_tipleri, llvm_param_sayisi, 0);
        LLVMValueRef fn = LLVMAddFunction(u->modul, mf->runtime_isim, fn_tip);

        /* Sembol tablosuna kaydet (runtime, Türkçe, ASCII isimleri) */
        llvm_sembol_ekle(u, mf->runtime_isim, fn, fn_tip, 0, 1);
        llvm_sembol_ekle(u, mf->isim, fn, fn_tip, 0, 1);
        if (mf->ascii_isim) {
            llvm_sembol_ekle(u, mf->ascii_isim, fn, fn_tip, 0, 1);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 2: TİP DÖNÜŞÜMLERİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_metin_tipi_al - Tonyukuk metin tipi { i8*, i64 }
 */
LLVMTypeRef llvm_metin_tipi_al(LLVMÜretici *u) {
    LLVMTypeRef alanlar[2];
    alanlar[0] = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0); /* ptr */
    alanlar[1] = LLVMInt64TypeInContext(u->baglam);                     /* len */
    return LLVMStructTypeInContext(u->baglam, alanlar, 2, 0);
}

/*
 * llvm_dizi_tipi_al - Tonyukuk dizi tipi { i64*, i64 }
 */
LLVMTypeRef llvm_dizi_tipi_al(LLVMÜretici *u) {
    LLVMTypeRef alanlar[2];
    alanlar[0] = LLVMPointerType(LLVMInt64TypeInContext(u->baglam), 0); /* ptr */
    alanlar[1] = LLVMInt64TypeInContext(u->baglam);                      /* count */
    return LLVMStructTypeInContext(u->baglam, alanlar, 2, 0);
}

/*
 * llvm_tip_dönüştür - Tonyukuk tip adını LLVM tipine dönüştürür
 */
LLVMTypeRef llvm_tip_dönüştür(LLVMÜretici *u, const char *tip_adı) {
    if (!tip_adı) return LLVMVoidTypeInContext(u->baglam);

    /* Temel tipler */
    if (strcmp(tip_adı, "tam") == 0) {
        return LLVMInt64TypeInContext(u->baglam);
    }
    if (strcmp(tip_adı, "ondalik") == 0 ||
        strcmp(tip_adı, "ondal\xc4\xb1k") == 0) {  /* ondalık UTF-8 */
        return LLVMDoubleTypeInContext(u->baglam);
    }
    if (strcmp(tip_adı, "mantik") == 0 ||
        strcmp(tip_adı, "mant\xc4\xb1k") == 0) {   /* mantık UTF-8 */
        return LLVMInt1TypeInContext(u->baglam);
    }
    if (strcmp(tip_adı, "metin") == 0) {
        return llvm_metin_tipi_al(u);
    }
    if (strcmp(tip_adı, "dizi") == 0) {
        return llvm_dizi_tipi_al(u);
    }

    /* Sınıf tipi mi? */
    LLVMSınıfBilgisi *sinif = llvm_sinif_bilgisi_bul(u, tip_adı);
    if (sinif) {
        /* Sınıf pointer'ı olarak döndür */
        return LLVMPointerType(sinif->struct_tipi, 0);
    }

    /* Bilinmeyen tip - varsayılan olarak i64 */
    return LLVMInt64TypeInContext(u->baglam);
}

/*
 * llvm_tip_turu_donustur - TipTürü enum'unu LLVM tipine dönüştürür
 */
LLVMTypeRef llvm_tip_turu_donustur(LLVMÜretici *u, TipTürü tip) {
    switch (tip) {
        case TİP_TAM:
            return LLVMInt64TypeInContext(u->baglam);
        case TİP_ONDALIK:
            return LLVMDoubleTypeInContext(u->baglam);
        case TİP_MANTIK:
            return LLVMInt1TypeInContext(u->baglam);
        case TİP_METİN:
            return llvm_metin_tipi_al(u);
        case TİP_DİZİ:
            return llvm_dizi_tipi_al(u);
        case TİP_BOŞLUK:
            return LLVMVoidTypeInContext(u->baglam);
        case TİP_SINIF:
            /* Opak pointer - gerçek tip çağrı sırasında belirlenir */
            return LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
        default:
            return LLVMInt64TypeInContext(u->baglam);
    }
}

/*
 * llvm_sinif_tipi_olustur - Sınıf için LLVM struct tipi oluşturur
 */
LLVMTypeRef llvm_sinif_tipi_olustur(LLVMÜretici *u, const char *sınıf_adı) {
    /* Önce mevcut tanımı kontrol et */
    LLVMSınıfBilgisi *mevcut = llvm_sinif_bilgisi_bul(u, sınıf_adı);
    if (mevcut) {
        return mevcut->struct_tipi;
    }

    /* Yeni opak struct oluştur */
    LLVMTypeRef struct_tipi = LLVMStructCreateNamed(u->baglam, sınıf_adı);
    return struct_tipi;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 3: SEMBOL YÖNETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_kapsam_gir - Yeni bir kapsam oluştur (fonksiyon, blok için)
 */
void llvm_kapsam_gir(LLVMÜretici *u) {
    LLVMSembolTablosu *yeni = (LLVMSembolTablosu *)arena_ayir(
        u->arena, sizeof(LLVMSembolTablosu));
    yeni->girisler = NULL;
    yeni->ust = u->sembol_tablosu;
    u->sembol_tablosu = yeni;
}

/*
 * llvm_kapsam_cik - Mevcut kapsamdan çık
 */
void llvm_kapsam_cik(LLVMÜretici *u) {
    if (u->sembol_tablosu && u->sembol_tablosu->ust) {
        u->sembol_tablosu = u->sembol_tablosu->ust;
    }
}

/*
 * llvm_sembol_ekle - Sembol tablosuna yeni giriş ekle
 */
void llvm_sembol_ekle(LLVMÜretici *u, const char *isim, LLVMValueRef deger,
                      LLVMTypeRef tip, int parametre_mi, int global_mi) {
    LLVMSembolGirişi *giris = (LLVMSembolGirişi *)arena_ayir(
        u->arena, sizeof(LLVMSembolGirişi));

    giris->isim = arena_strdup(u->arena, isim);
    giris->deger = deger;
    giris->tip = tip;
    giris->parametre_mi = parametre_mi;
    giris->global_mi = global_mi;
    giris->metin_dizisi = 0;

    /* Listeye ekle */
    giris->sonraki = u->sembol_tablosu->girisler;
    u->sembol_tablosu->girisler = giris;
}

/*
 * llvm_sembol_bul - Sembol tablosunda ara (tüm kapsamlarda)
 */
LLVMSembolGirişi *llvm_sembol_bul(LLVMÜretici *u, const char *isim) {
    LLVMSembolTablosu *kapsam = u->sembol_tablosu;

    while (kapsam) {
        LLVMSembolGirişi *giris = kapsam->girisler;
        while (giris) {
            if (strcmp(giris->isim, isim) == 0) {
                return giris;
            }
            giris = giris->sonraki;
        }
        kapsam = kapsam->ust;
    }

    return NULL;
}

/*
 * llvm_sınıf_bilgisi_ekle - Sınıf bilgisi ekle
 */
void llvm_sınıf_bilgisi_ekle(LLVMÜretici *u, LLVMSınıfBilgisi *bilgi) {
    bilgi->sonraki = u->siniflar;
    u->siniflar = bilgi;
}

/*
 * llvm_sinif_bilgisi_bul - Sınıf bilgisi bul
 */
LLVMSınıfBilgisi *llvm_sinif_bilgisi_bul(LLVMÜretici *u, const char *isim) {
    LLVMSınıfBilgisi *bilgi = u->siniflar;
    while (bilgi) {
        if (strcmp(bilgi->isim, isim) == 0) {
            return bilgi;
        }
        bilgi = bilgi->sonraki;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 4: İŞLEV ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Forward declaration: varsayılan parametre yardımcıları */
static void llvm_varsayilan_kaydet(const char *isim, Düğüm *params_dugum);

/*
 * llvm_işlev_üret - İşlev tanımı üret
 */
void llvm_işlev_üret(LLVMÜretici *u, Düğüm *dugum) {
    if (!dugum || dugum->tur != DÜĞÜM_İŞLEV) return;

    char *isim = dugum->veri.islev.isim;
    char *dekorator = dugum->veri.islev.dekorator;
    char *donus_tipi_adi = dugum->veri.islev.dönüş_tipi;

    /* Dekoratör: orijinal fonksiyonu __dekoratsiz_<isim> olarak üret */
    char gercek_isim[256];
    if (dekorator) {
        snprintf(gercek_isim, sizeof(gercek_isim), "__dekoratsiz_%s", isim);
    } else {
        snprintf(gercek_isim, sizeof(gercek_isim), "%s", isim);
    }

    /* Generic fonksiyonları atla - özelleştirilmiş versiyonları ayrıca üretilir */
    if (dugum->veri.islev.tip_parametre != NULL) {
        return;
    }

    /* Dönüş tipi */
    LLVMTypeRef dönüş_tipi;
    if (donus_tipi_adi) {
        dönüş_tipi = llvm_tip_dönüştür(u, donus_tipi_adi);
    } else {
        dönüş_tipi = LLVMVoidTypeInContext(u->baglam);
    }

    /* Parametre tipleri */
    int param_sayisi = 0;
    LLVMTypeRef *param_tipleri = NULL;
    char **param_isimleri = NULL;

    if (dugum->çocuk_sayısı > 0 && dugum->çocuklar[0]) {
        Düğüm *params = dugum->çocuklar[0];
        param_sayisi = params->çocuk_sayısı;

        if (param_sayisi > 0) {
            param_tipleri = (LLVMTypeRef *)arena_ayir(
                u->arena, sizeof(LLVMTypeRef) * param_sayisi);
            param_isimleri = (char **)arena_ayir(
                u->arena, sizeof(char *) * param_sayisi);

            for (int i = 0; i < param_sayisi; i++) {
                Düğüm *param = params->çocuklar[i];
                param_isimleri[i] = param->veri.değişken.isim;
                param_tipleri[i] = llvm_tip_dönüştür(u, param->veri.değişken.tip);
            }
        }
    }

    /* Varsayılan parametre bilgisini kaydet */
    if (dugum->çocuk_sayısı > 0 && dugum->çocuklar[0]) {
        llvm_varsayilan_kaydet(isim, dugum->çocuklar[0]);
    }

    /* Metot ise "bu" parametresini başa ekle */
    int bu_param_var = 0;
    LLVMTypeRef bu_tipi = NULL;
    LLVMSınıfBilgisi *bu_sinif_bilgi = NULL;

    if (u->mevcut_sinif) {
        bu_sinif_bilgi = llvm_sinif_bilgisi_bul(u, u->mevcut_sinif);
        if (bu_sinif_bilgi) {
            bu_tipi = LLVMPointerType(bu_sinif_bilgi->struct_tipi, 0);
            bu_param_var = 1;
        }
    }

    int toplam_param = param_sayisi + bu_param_var;
    LLVMTypeRef *tum_param_tipleri = NULL;
    if (toplam_param > 0) {
        tum_param_tipleri = (LLVMTypeRef *)arena_ayir(
            u->arena, sizeof(LLVMTypeRef) * toplam_param);
        if (bu_param_var) {
            tum_param_tipleri[0] = bu_tipi;
        }
        for (int i = 0; i < param_sayisi; i++) {
            tum_param_tipleri[i + bu_param_var] = param_tipleri[i];
        }
    }

    /* İşlev tipi oluştur */
    LLVMTypeRef islev_tipi = LLVMFunctionType(
        dönüş_tipi,
        tum_param_tipleri,
        toplam_param,
        0  /* varargs değil */
    );

    /* İşlevi modüle ekle (dekoratörlü ise __dekoratsiz_ prefix ile) */
    LLVMValueRef islev = LLVMAddFunction(u->modul, gercek_isim, islev_tipi);

    /* Sembol tablosuna ekle (dekoratörlü ise gerçek isimle) */
    llvm_sembol_ekle(u, gercek_isim, islev, islev_tipi, 0, 1);

    /* Debug bilgisi: fonksiyon */
    LLVMMetadataRef debug_sp = NULL;
    if (u->hata_ayiklama && u->debug_bilgi) {
        debug_sp = llvm_debug_fonksiyon_olustur(u, isim, dugum->satir, islev_tipi);
        if (debug_sp) {
            LLVMSetSubprogram(islev, debug_sp);
        }
    }

    /* Giriş bloğu oluştur */
    LLVMBasicBlockRef giris_blok = LLVMAppendBasicBlockInContext(
        u->baglam, islev, "giris");
    LLVMPositionBuilderAtEnd(u->olusturucu, giris_blok);

    /* Mevcut işlevi kaydet */
    u->mevcut_islev = islev;

    /* Yeni kapsam */
    llvm_kapsam_gir(u);

    /* Debug bilgisi: satır ayarla */
    if (u->hata_ayiklama) {
        llvm_debug_satir_ayarla(u, dugum->satir, dugum->sutun);
    }

    /* Metot ise "bu" parametresini kaydet */
    if (bu_param_var && bu_sinif_bilgi) {
        LLVMValueRef bu_param = LLVMGetParam(islev, 0);
        LLVMSetValueName(bu_param, "bu");

        LLVMValueRef bu_alloca = LLVMBuildAlloca(u->olusturucu, bu_tipi, "bu");
        LLVMBuildStore(u->olusturucu, bu_param, bu_alloca);
        llvm_sembol_ekle(u, "bu", bu_alloca, bu_tipi, 1, 0);
    }

    /* Parametreleri yerel değişkenlere kopyala (alloca) */
    for (int i = 0; i < param_sayisi; i++) {
        LLVMValueRef param_deger = LLVMGetParam(islev, i + bu_param_var);
        LLVMSetValueName(param_deger, param_isimleri[i]);

        /* Alloca ile stack'te yer ayır */
        LLVMValueRef alloca = LLVMBuildAlloca(
            u->olusturucu, param_tipleri[i], param_isimleri[i]);

        /* Parametreyi alloca'ya kopyala */
        LLVMBuildStore(u->olusturucu, param_deger, alloca);

        /* Sembol tablosuna ekle */
        llvm_sembol_ekle(u, param_isimleri[i], alloca, param_tipleri[i], 1, 0);
    }

    /* Gövdeyi üret */
    if (dugum->çocuk_sayısı > 1 && dugum->çocuklar[1]) {
        llvm_blok_uret(u, dugum->çocuklar[1]);
    }

    /* Eğer son komut terminator değilse, varsayılan dönüş ekle */
    LLVMBasicBlockRef mevcut_blok = LLVMGetInsertBlock(u->olusturucu);
    if (!LLVMGetBasicBlockTerminator(mevcut_blok)) {
        if (LLVMGetTypeKind(dönüş_tipi) == LLVMVoidTypeKind) {
            LLVMBuildRetVoid(u->olusturucu);
        } else {
            /* Varsayılan değer döndür */
            LLVMValueRef varsayilan = LLVMConstNull(dönüş_tipi);
            LLVMBuildRet(u->olusturucu, varsayilan);
        }
    }

    /* Kapsamdan çık */
    llvm_kapsam_cik(u);

    /* Debug: scope yığınından çık ve konumu temizle */
    if (u->hata_ayiklama && u->debug_bilgi) {
        if (u->debug_bilgi->kapsam_derinlik > 0) {
            u->debug_bilgi->kapsam_derinlik--;
        }
        /* Debug konumunu temizle */
        LLVMSetCurrentDebugLocation2(u->olusturucu, NULL);
    }

    /* Dekoratör wrapper: <isim> çağrıldığında __dekoratsiz_<isim> + dekoratör çağır */
    if (dekorator) {
        /* Wrapper fonksiyonu oluştur: isim orijinal isimle */
        LLVMValueRef wrapper = LLVMAddFunction(u->modul, isim, islev_tipi);
        LLVMBasicBlockRef w_giris = LLVMAppendBasicBlockInContext(
            u->baglam, wrapper, "giris");
        LLVMPositionBuilderAtEnd(u->olusturucu, w_giris);

        /* Argümanları olduğu gibi orijinal fonksiyona aktar */
        unsigned arg_sayisi = (unsigned)toplam_param;
        LLVMValueRef *w_args = NULL;
        if (arg_sayisi > 0) {
            w_args = (LLVMValueRef *)arena_ayir(u->arena, sizeof(LLVMValueRef) * arg_sayisi);
            for (unsigned a = 0; a < arg_sayisi; a++) {
                w_args[a] = LLVMGetParam(wrapper, a);
            }
        }
        LLVMValueRef ic_sonuc = LLVMBuildCall2(u->olusturucu, islev_tipi,
            islev, w_args, arg_sayisi, "ic_sonuc");

        /* Sonucu dekoratör fonksiyonuna geçir */
        LLVMSembolGirişi *deko_fn = llvm_sembol_bul(u, dekorator);
        if (deko_fn) {
            LLVMValueRef deko_args[] = { ic_sonuc };
            LLVMValueRef deko_sonuc = LLVMBuildCall2(u->olusturucu, deko_fn->tip,
                deko_fn->deger, deko_args, 1, "deko_sonuc");
            LLVMBuildRet(u->olusturucu, deko_sonuc);
        } else {
            /* Dekoratör fonksiyonu henüz tanımlı değil, doğrudan döndür */
            LLVMBuildRet(u->olusturucu, ic_sonuc);
        }

        /* Wrapper'ı sembol tablosuna kaydet (asıl çağrı ismi ile) */
        llvm_sembol_ekle(u, isim, wrapper, islev_tipi, 0, 1);
    }

    u->mevcut_islev = NULL;
}

/* ---- Varsayılan parametre AST bilgisi (işlev adı → parametreler) ---- */
#define LLVM_MAKS_ISLEV_VARS 256

typedef struct {
    char isim[128];
    int param_sayisi;
    Düğüm *varsayilan_dugumler[32]; /* NULL ise varsayılan yok */
} LLVMIslevVarsayilan;

static LLVMIslevVarsayilan llvm_varsayilanlar[LLVM_MAKS_ISLEV_VARS];
static int llvm_varsayilan_sayisi = 0;

static void llvm_varsayilan_kaydet(const char *isim, Düğüm *params_dugum) {
    if (!isim || !params_dugum || llvm_varsayilan_sayisi >= LLVM_MAKS_ISLEV_VARS) return;
    int var_var = 0;
    for (int i = 0; i < params_dugum->çocuk_sayısı && i < 32; i++) {
        if (params_dugum->çocuklar[i]->çocuk_sayısı > 0) { var_var = 1; break; }
    }
    if (!var_var) return;  /* Varsayılan parametre yok, kaydetmeye gerek yok */

    LLVMIslevVarsayilan *v = &llvm_varsayilanlar[llvm_varsayilan_sayisi++];
    int n = (int)strlen(isim);
    if (n >= 128) n = 127;
    memcpy(v->isim, isim, n);
    v->isim[n] = '\0';
    v->param_sayisi = params_dugum->çocuk_sayısı;
    for (int i = 0; i < params_dugum->çocuk_sayısı && i < 32; i++) {
        Düğüm *p = params_dugum->çocuklar[i];
        v->varsayilan_dugumler[i] = (p->çocuk_sayısı > 0) ? p->çocuklar[0] : NULL;
    }
}

static LLVMIslevVarsayilan *llvm_varsayilan_bul(const char *isim) {
    for (int i = 0; i < llvm_varsayilan_sayisi; i++) {
        if (strcmp(llvm_varsayilanlar[i].isim, isim) == 0)
            return &llvm_varsayilanlar[i];
    }
    return NULL;
}

/*
 * llvm_kapanis_yakala_topla - Lambda gövdesindeki serbest değişkenleri topla
 * AST'yi yürür, parametrelerde olmayan ama dış kapsamda olan değişkenleri bulur.
 */
static void llvm_kapanis_yakala_topla(Düğüm *d, char **param_isimleri, int param_sayisi,
                                       LLVMÜretici *u, char ***isimler, int *sayac) {
    if (!d) return;

    if (d->tur == DÜĞÜM_TANIMLAYICI && d->veri.tanimlayici.isim) {
        const char *isim = d->veri.tanimlayici.isim;

        /* Parametrelerde mi? — atla */
        for (int i = 0; i < param_sayisi; i++) {
            if (param_isimleri[i] && strcmp(param_isimleri[i], isim) == 0) return;
        }

        /* Dış kapsamda yerel değişken mi? */
        LLVMSembolGirişi *s = llvm_sembol_bul(u, isim);
        if (s && !s->global_mi) {
            /* Zaten listeye eklendi mi kontrol et */
            for (int j = 0; j < *sayac; j++) {
                if (strcmp((*isimler)[j], isim) == 0) return;
            }
            if (*sayac < 32) {
                (*isimler)[*sayac] = arena_strdup(u->arena, isim);
                (*sayac)++;
            }
        }
    }

    /* Alt düğümleri tara */
    for (int i = 0; i < d->çocuk_sayısı; i++) {
        llvm_kapanis_yakala_topla(d->çocuklar[i], param_isimleri, param_sayisi,
                                   u, isimler, sayac);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 5: İFADE ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_ifade_uret - İfade üret, sonucu LLVMValueRef olarak döndür
 */
LLVMValueRef llvm_ifade_uret(LLVMÜretici *u, Düğüm *dugum) {
    if (!dugum) return NULL;

    switch (dugum->tur) {
        case DÜĞÜM_TAM_SAYI: {
            /* Tam sayı sabiti */
            return LLVMConstInt(
                LLVMInt64TypeInContext(u->baglam),
                dugum->veri.tam_deger,
                1  /* signed */
            );
        }

        case DÜĞÜM_ONDALIK_SAYI: {
            /* Ondalık sayı sabiti */
            return LLVMConstReal(
                LLVMDoubleTypeInContext(u->baglam),
                dugum->veri.ondalık_değer
            );
        }

        case DÜĞÜM_MANTIK_DEĞERİ: {
            /* Mantık (boolean) sabiti */
            return LLVMConstInt(
                LLVMInt1TypeInContext(u->baglam),
                dugum->veri.mantık_değer ? 1 : 0,
                0
            );
        }

        case DÜĞÜM_METİN_DEĞERİ: {
            /* Metin sabiti - global string oluştur */
            return llvm_metin_sabiti_oluştur(u, dugum->veri.metin_değer);
        }

        case DÜĞÜM_TANIMLAYICI: {
            /* Değişken erişimi */
            char *isim = dugum->veri.tanimlayici.isim;
            LLVMSembolGirişi *sembol = llvm_sembol_bul(u, isim);

            if (!sembol) {
                fprintf(stderr, "Hata: Tanımsız değişken: %s\n", isim);
                return LLVMConstNull(LLVMInt64TypeInContext(u->baglam));
            }

            if (sembol->global_mi) {
                /* Fonksiyon ise direkt döndür, global değişken ise load */
                if (LLVMIsAFunction(sembol->deger)) {
                    return sembol->deger;
                } else {
                    return LLVMBuildLoad2(u->olusturucu, sembol->tip,
                                          sembol->deger, isim);
                }
            } else {
                /* Yerel değişken - load */
                return LLVMBuildLoad2(u->olusturucu, sembol->tip,
                                      sembol->deger, isim);
            }
        }

        case DÜĞÜM_İKİLİ_İŞLEM: {
            /* Boş birleştirme operatörü: sol ?? sağ (short-circuit) */
            if (dugum->veri.islem.islem == TOK_SORU_SORU) {
                LLVMValueRef sol = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (!sol) return llvm_ifade_uret(u, dugum->çocuklar[1]);

                LLVMTypeRef sol_tip = LLVMTypeOf(sol);
                LLVMValueRef bos_mu = NULL;

                if (LLVMGetTypeKind(sol_tip) == LLVMStructTypeKind) {
                    /* Metin struct: ptr alanını kontrol et */
                    LLVMValueRef ptr = LLVMBuildExtractValue(u->olusturucu, sol, 0, "bos_ptr");
                    LLVMValueRef null_ptr = LLVMConstNull(LLVMTypeOf(ptr));
                    bos_mu = LLVMBuildICmp(u->olusturucu, LLVMIntEQ, ptr, null_ptr, "bos_mu");
                } else if (LLVMGetTypeKind(sol_tip) == LLVMPointerTypeKind) {
                    /* Raw pointer: null kontrolü */
                    LLVMValueRef null_ptr = LLVMConstNull(sol_tip);
                    bos_mu = LLVMBuildICmp(u->olusturucu, LLVMIntEQ, sol, null_ptr, "bos_mu");
                } else {
                    /* Integer: sıfır kontrolü */
                    LLVMValueRef sifir = LLVMConstNull(sol_tip);
                    bos_mu = LLVMBuildICmp(u->olusturucu, LLVMIntEQ, sol, sifir, "bos_mu");
                }

                /* Branching: boş ise sağı değerlendir */
                LLVMBasicBlockRef bos_blok = LLVMAppendBasicBlockInContext(
                    u->baglam, u->mevcut_islev, "bos_blok");
                LLVMBasicBlockRef dolu_blok = LLVMAppendBasicBlockInContext(
                    u->baglam, u->mevcut_islev, "dolu_blok");
                LLVMBasicBlockRef birlestir = LLVMAppendBasicBlockInContext(
                    u->baglam, u->mevcut_islev, "bos_birlestir");

                LLVMBuildCondBr(u->olusturucu, bos_mu, bos_blok, dolu_blok);

                /* Boş blok: sağ tarafı değerlendir */
                LLVMPositionBuilderAtEnd(u->olusturucu, bos_blok);
                LLVMValueRef sag = llvm_ifade_uret(u, dugum->çocuklar[1]);
                if (!sag) sag = LLVMConstNull(sol_tip);
                LLVMBasicBlockRef bos_son = LLVMGetInsertBlock(u->olusturucu);
                LLVMBuildBr(u->olusturucu, birlestir);

                /* Dolu blok: sol değeri al */
                LLVMPositionBuilderAtEnd(u->olusturucu, dolu_blok);
                LLVMBuildBr(u->olusturucu, birlestir);

                /* Birleştir: phi node */
                LLVMPositionBuilderAtEnd(u->olusturucu, birlestir);
                LLVMValueRef phi = LLVMBuildPhi(u->olusturucu, sol_tip, "bos_sonuc");
                LLVMValueRef degerler[] = { sag, sol };
                LLVMBasicBlockRef bloklar[] = { bos_son, dolu_blok };
                LLVMAddIncoming(phi, degerler, bloklar, 2);
                return phi;
            }

            /* İkili işlem: +, -, *, /, <, >, ==, !=, ve, veya */
            LLVMValueRef sol = llvm_ifade_uret(u, dugum->çocuklar[0]);
            LLVMValueRef sag = llvm_ifade_uret(u, dugum->çocuklar[1]);

            if (!sol || !sag) return NULL;

            /* İşlem türüne göre üret */
            SözcükTürü islem = dugum->veri.islem.islem;

            /* Sol operandın tipini kontrol et */
            LLVMTypeRef sol_tip = LLVMTypeOf(sol);
            LLVMTypeRef sag_tip = LLVMTypeOf(sag);
            int ondalik_mi = (LLVMGetTypeKind(sol_tip) == LLVMDoubleTypeKind);

            /* Integer genişlik uyumsuzluğu düzelt (i1 vs i64 vb.) */
            if (LLVMGetTypeKind(sol_tip) == LLVMIntegerTypeKind &&
                LLVMGetTypeKind(sag_tip) == LLVMIntegerTypeKind) {
                unsigned sol_w = LLVMGetIntTypeWidth(sol_tip);
                unsigned sag_w = LLVMGetIntTypeWidth(sag_tip);
                if (sol_w < sag_w) {
                    sol = LLVMBuildZExt(u->olusturucu, sol, sag_tip, "sol_ext");
                    sol_tip = sag_tip;
                } else if (sag_w < sol_w) {
                    sag = LLVMBuildZExt(u->olusturucu, sag, sol_tip, "sag_ext");
                }
            }

            /* Sınıf operatör aşırı yükleme: pointer + pointer → topla metotu */
            if (LLVMGetTypeKind(sol_tip) == LLVMPointerTypeKind &&
                LLVMGetTypeKind(sag_tip) == LLVMPointerTypeKind) {
                const char *metot_adi = NULL;
                switch (islem) {
                    case TOK_ARTI: metot_adi = "topla"; break;
                    case TOK_EKSI: metot_adi = "cikar"; break;
                    case TOK_ÇARPIM: metot_adi = "carp"; break;
                    case TOK_EŞİT_EŞİT: metot_adi = "esit"; break;
                    case TOK_EŞİT_DEĞİL: metot_adi = "esit_degil"; break;
                    default: break;
                }
                if (metot_adi) {
                    /* Sınıf ara: SinifAdi_metotAdi */
                    LLVMSınıfBilgisi *sinif_it = u->siniflar;
                    LLVMSembolGirişi *metot_fn = NULL;
                    while (sinif_it) {
                        char mangled[256];
                        snprintf(mangled, sizeof(mangled), "%s_%s", sinif_it->isim, metot_adi);
                        metot_fn = llvm_sembol_bul(u, mangled);
                        if (metot_fn) break;
                        sinif_it = sinif_it->sonraki;
                    }
                    if (metot_fn) {
                        LLVMValueRef args[] = { sol, sag };
                        LLVMTypeRef donus_tip = LLVMGetReturnType(metot_fn->tip);
                        const char *cagri_adi = (LLVMGetTypeKind(donus_tip) == LLVMVoidTypeKind) ? "" : "op_sonuc";
                        return LLVMBuildCall2(u->olusturucu, metot_fn->tip,
                                             metot_fn->deger, args, 2, cagri_adi);
                    }
                }
            }

            switch (islem) {
                case TOK_ARTI:
                    if (LLVMGetTypeKind(sol_tip) == LLVMStructTypeKind) {
                        /* Metin birleştirme */
                        LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_metin_birlestir");
                        if (fn) {
                            LLVMValueRef ptr1 = LLVMBuildExtractValue(u->olusturucu, sol, 0, "sol_ptr");
                            LLVMValueRef len1 = LLVMBuildExtractValue(u->olusturucu, sol, 1, "sol_len");
                            LLVMValueRef ptr2 = LLVMBuildExtractValue(u->olusturucu, sag, 0, "sag_ptr");
                            LLVMValueRef len2 = LLVMBuildExtractValue(u->olusturucu, sag, 1, "sag_len");
                            LLVMValueRef args[] = { ptr1, len1, ptr2, len2 };
                            return LLVMBuildCall2(u->olusturucu, fn->tip,
                                                 fn->deger, args, 4, "birlestir");
                        }
                    }
                    if (ondalik_mi)
                        return LLVMBuildFAdd(u->olusturucu, sol, sag, "toplam");
                    else
                        return LLVMBuildAdd(u->olusturucu, sol, sag, "toplam");

                case TOK_EKSI:
                    if (ondalik_mi)
                        return LLVMBuildFSub(u->olusturucu, sol, sag, "fark");
                    else
                        return LLVMBuildSub(u->olusturucu, sol, sag, "fark");

                case TOK_ÇARPIM:
                    if (ondalik_mi)
                        return LLVMBuildFMul(u->olusturucu, sol, sag, "carpim");
                    else
                        return LLVMBuildMul(u->olusturucu, sol, sag, "carpim");

                case TOK_BÖLME:
                    if (ondalik_mi)
                        return LLVMBuildFDiv(u->olusturucu, sol, sag, "bolum");
                    else
                        return LLVMBuildSDiv(u->olusturucu, sol, sag, "bolum");

                case TOK_YÜZDE:
                    return LLVMBuildSRem(u->olusturucu, sol, sag, "kalan");

                case TOK_KÜÇÜK:
                    if (ondalik_mi)
                        return LLVMBuildFCmp(u->olusturucu, LLVMRealOLT,
                                             sol, sag, "kucuk");
                    else
                        return LLVMBuildICmp(u->olusturucu, LLVMIntSLT,
                                             sol, sag, "kucuk");

                case TOK_BÜYÜK:
                    if (ondalik_mi)
                        return LLVMBuildFCmp(u->olusturucu, LLVMRealOGT,
                                             sol, sag, "buyuk");
                    else
                        return LLVMBuildICmp(u->olusturucu, LLVMIntSGT,
                                             sol, sag, "buyuk");

                case TOK_KÜÇÜK_EŞİT:
                    if (ondalik_mi)
                        return LLVMBuildFCmp(u->olusturucu, LLVMRealOLE,
                                             sol, sag, "kucuk_esit");
                    else
                        return LLVMBuildICmp(u->olusturucu, LLVMIntSLE,
                                             sol, sag, "kucuk_esit");

                case TOK_BÜYÜK_EŞİT:
                    if (ondalik_mi)
                        return LLVMBuildFCmp(u->olusturucu, LLVMRealOGE,
                                             sol, sag, "buyuk_esit");
                    else
                        return LLVMBuildICmp(u->olusturucu, LLVMIntSGE,
                                             sol, sag, "buyuk_esit");

                case TOK_EŞİT_EŞİT:
                    if (LLVMGetTypeKind(sol_tip) == LLVMStructTypeKind) {
                        /* Metin karşılaştırma: strcmp(ptr1, ptr2) == 0 */
                        LLVMSembolGirişi *fn = llvm_sembol_bul(u, "strcmp");
                        if (fn) {
                            LLVMValueRef ptr1 = LLVMBuildExtractValue(u->olusturucu, sol, 0, "cmp_ptr1");
                            LLVMValueRef ptr2 = LLVMBuildExtractValue(u->olusturucu, sag, 0, "cmp_ptr2");
                            LLVMValueRef args[] = { ptr1, ptr2 };
                            LLVMValueRef cmp = LLVMBuildCall2(u->olusturucu, fn->tip,
                                                              fn->deger, args, 2, "strcmp_sonuc");
                            return LLVMBuildICmp(u->olusturucu, LLVMIntEQ, cmp,
                                LLVMConstInt(LLVMInt32TypeInContext(u->baglam), 0, 0), "esit");
                        }
                    }
                    if (ondalik_mi)
                        return LLVMBuildFCmp(u->olusturucu, LLVMRealOEQ,
                                             sol, sag, "esit");
                    else
                        return LLVMBuildICmp(u->olusturucu, LLVMIntEQ,
                                             sol, sag, "esit");

                case TOK_EŞİT_DEĞİL:
                    if (LLVMGetTypeKind(sol_tip) == LLVMStructTypeKind) {
                        /* Metin karşılaştırma: strcmp(ptr1, ptr2) != 0 */
                        LLVMSembolGirişi *fn = llvm_sembol_bul(u, "strcmp");
                        if (fn) {
                            LLVMValueRef ptr1 = LLVMBuildExtractValue(u->olusturucu, sol, 0, "cmp_ptr1");
                            LLVMValueRef ptr2 = LLVMBuildExtractValue(u->olusturucu, sag, 0, "cmp_ptr2");
                            LLVMValueRef args[] = { ptr1, ptr2 };
                            LLVMValueRef cmp = LLVMBuildCall2(u->olusturucu, fn->tip,
                                                              fn->deger, args, 2, "strcmp_sonuc");
                            return LLVMBuildICmp(u->olusturucu, LLVMIntNE, cmp,
                                LLVMConstInt(LLVMInt32TypeInContext(u->baglam), 0, 0), "esit_degil");
                        }
                    }
                    if (ondalik_mi)
                        return LLVMBuildFCmp(u->olusturucu, LLVMRealONE,
                                             sol, sag, "esit_degil");
                    else
                        return LLVMBuildICmp(u->olusturucu, LLVMIntNE,
                                             sol, sag, "esit_degil");

                case TOK_VE:
                    return LLVMBuildAnd(u->olusturucu, sol, sag, "ve");

                case TOK_VEYA:
                    return LLVMBuildOr(u->olusturucu, sol, sag, "veya");

                case TOK_BİT_VE:
                    return LLVMBuildAnd(u->olusturucu, sol, sag, "bit_ve");

                case TOK_BİT_VEYA:
                    return LLVMBuildOr(u->olusturucu, sol, sag, "bit_veya");

                case TOK_BİT_XOR:
                    return LLVMBuildXor(u->olusturucu, sol, sag, "bit_xor");

                case TOK_SOL_KAYDIR:
                    return LLVMBuildShl(u->olusturucu, sol, sag, "sol_kaydir");

                case TOK_SAĞ_KAYDIR:
                    return LLVMBuildLShr(u->olusturucu, sol, sag, "sag_kaydir");

                default:
                    fprintf(stderr, "Hata: Bilinmeyen ikili işlem\n");
                    return NULL;
            }
        }

        case DÜĞÜM_TEKLİ_İŞLEM: {
            /* Tekli işlem: -, değil */
            LLVMValueRef operand = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (!operand) return NULL;

            SözcükTürü islem = dugum->veri.islem.islem;

            switch (islem) {
                case TOK_EKSI:
                    return LLVMBuildNeg(u->olusturucu, operand, "negatif");

                case TOK_DEĞİL:
                    return LLVMBuildNot(u->olusturucu, operand, "degil");

                case TOK_BİT_DEĞİL:
                    return LLVMBuildNot(u->olusturucu, operand, "bit_degil");

                default:
                    return operand;
            }
        }

        case DÜĞÜM_ÇAĞRI: {
            /* Fonksiyon çağrısı */
            char *isim = dugum->veri.tanimlayici.isim;

            /* "yazdır" özel durumu */
            if (strcmp(isim, "yazd\xc4\xb1r") == 0 ||
                strcmp(isim, "yazdir") == 0) {
                if (dugum->çocuk_sayısı > 0) {
                    LLVMValueRef arg = llvm_ifade_uret(u, dugum->çocuklar[0]);
                    if (arg) {
                        LLVMTypeRef arg_tip = LLVMTypeOf(arg);
                        LLVMTypeKind tip_turu = LLVMGetTypeKind(arg_tip);

                        if (tip_turu == LLVMIntegerTypeKind) {
                            unsigned bit_genisligi = LLVMGetIntTypeWidth(arg_tip);
                            if (bit_genisligi == 1) {
                                /* Mantık değeri yazdır */
                                LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_yazdir_mantik");
                                if (fn) {
                                    LLVMValueRef ext = LLVMBuildZExt(u->olusturucu, arg,
                                        LLVMInt64TypeInContext(u->baglam), "mantik_ext");
                                    LLVMValueRef args[] = { ext };
                                    return LLVMBuildCall2(u->olusturucu, fn->tip,
                                                         fn->deger, args, 1, "");
                                }
                            } else {
                                /* Tam sayı yazdır */
                                LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_yazdir_tam");
                                if (fn) {
                                    LLVMValueRef tam_arg = arg;
                                    if (bit_genisligi != 64) {
                                        tam_arg = LLVMBuildSExt(u->olusturucu, arg,
                                            LLVMInt64TypeInContext(u->baglam), "tam_ext");
                                    }
                                    LLVMValueRef args[] = { tam_arg };
                                    return LLVMBuildCall2(u->olusturucu, fn->tip,
                                                         fn->deger, args, 1, "");
                                }
                            }
                        } else if (tip_turu == LLVMDoubleTypeKind) {
                            /* Ondalık sayı yazdır */
                            LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_yazdir_ondalik");
                            if (fn) {
                                LLVMValueRef args[] = { arg };
                                return LLVMBuildCall2(u->olusturucu, fn->tip,
                                                     fn->deger, args, 1, "");
                            }
                        } else if (tip_turu == LLVMStructTypeKind) {
                            /* Metin struct { i8*, i64 } yazdır */
                            LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_yazdir_metin");
                            if (fn) {
                                LLVMValueRef ptr = LLVMBuildExtractValue(
                                    u->olusturucu, arg, 0, "metin_ptr");
                                LLVMValueRef len = LLVMBuildExtractValue(
                                    u->olusturucu, arg, 1, "metin_len");
                                LLVMValueRef args[] = { ptr, len };
                                return LLVMBuildCall2(u->olusturucu, fn->tip,
                                                     fn->deger, args, 2, "");
                            }
                        } else if (tip_turu == LLVMPointerTypeKind) {
                            /* Raw pointer - puts kullan */
                            LLVMSembolGirişi *fn = llvm_sembol_bul(u, "puts");
                            if (fn) {
                                LLVMValueRef args[] = { arg };
                                return LLVMBuildCall2(u->olusturucu, fn->tip,
                                                     fn->deger, args, 1, "");
                            }
                        }
                    }
                }
                return NULL;
            }

            /* doğrula() — test assertion */
            if (strcmp(isim, "do\xc4\x9frula") == 0) {
                if (dugum->çocuk_sayısı > 0) {
                    LLVMValueRef kosul = llvm_ifade_uret(u, dugum->çocuklar[0]);
                    if (kosul) {
                        LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_dogrula");
                        if (fn) {
                            /* Koşulu i64'e genişlet */
                            LLVMTypeRef i64_tip = LLVMInt64TypeInContext(u->baglam);
                            LLVMTypeKind tk = LLVMGetTypeKind(LLVMTypeOf(kosul));
                            LLVMValueRef kosul64 = kosul;
                            if (tk == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(LLVMTypeOf(kosul)) != 64) {
                                kosul64 = LLVMBuildZExt(u->olusturucu, kosul, i64_tip, "kosul_ext");
                            }

                            /* Test isim ptr/len global'lardan oku (varsa) */
                            LLVMValueRef isim_ptr_val;
                            LLVMValueRef isim_len_val;
                            LLVMValueRef g_ptr = LLVMGetNamedGlobal(u->modul, "_test_isim_ptr");
                            LLVMValueRef g_len = LLVMGetNamedGlobal(u->modul, "_test_isim_len");
                            LLVMTypeRef i8_ptr_tip = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
                            if (g_ptr && g_len) {
                                isim_ptr_val = LLVMBuildLoad2(u->olusturucu, i8_ptr_tip, g_ptr, "t_isim_ptr");
                                isim_len_val = LLVMBuildLoad2(u->olusturucu, i64_tip, g_len, "t_isim_len");
                            } else {
                                isim_ptr_val = LLVMConstNull(i8_ptr_tip);
                                isim_len_val = LLVMConstInt(i64_tip, 0, 0);
                            }

                            LLVMValueRef satir_val = LLVMConstInt(i64_tip, (unsigned long long)dugum->satir, 0);
                            LLVMValueRef args[] = { kosul64, isim_ptr_val, isim_len_val, satir_val };
                            LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 4, "");
                        }
                    }
                }
                return NULL;
            }

            /* doğrula_eşit(beklenen, gerçek) — test assertion with diff */
            if (strcmp(isim, "do\xc4\x9frula_e\xc5\x9fit") == 0 ||
                strcmp(isim, "dogrula_esit") == 0) {
                if (dugum->çocuk_sayısı >= 2) {
                    LLVMValueRef beklenen = llvm_ifade_uret(u, dugum->çocuklar[0]);
                    LLVMValueRef gercek = llvm_ifade_uret(u, dugum->çocuklar[1]);
                    if (beklenen && gercek) {
                        LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_dogrula_esit_tam");
                        if (fn) {
                            LLVMTypeRef i64_tip = LLVMInt64TypeInContext(u->baglam);
                            /* i64'e genişlet */
                            if (LLVMGetTypeKind(LLVMTypeOf(beklenen)) == LLVMIntegerTypeKind &&
                                LLVMGetIntTypeWidth(LLVMTypeOf(beklenen)) != 64)
                                beklenen = LLVMBuildSExt(u->olusturucu, beklenen, i64_tip, "bek_ext");
                            if (LLVMGetTypeKind(LLVMTypeOf(gercek)) == LLVMIntegerTypeKind &&
                                LLVMGetIntTypeWidth(LLVMTypeOf(gercek)) != 64)
                                gercek = LLVMBuildSExt(u->olusturucu, gercek, i64_tip, "ger_ext");

                            LLVMTypeRef i8_ptr_tip = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
                            LLVMValueRef isim_ptr_val, isim_len_val;
                            LLVMValueRef g_ptr = LLVMGetNamedGlobal(u->modul, "_test_isim_ptr");
                            LLVMValueRef g_len = LLVMGetNamedGlobal(u->modul, "_test_isim_len");
                            if (g_ptr && g_len) {
                                isim_ptr_val = LLVMBuildLoad2(u->olusturucu, i8_ptr_tip, g_ptr, "t_ip");
                                isim_len_val = LLVMBuildLoad2(u->olusturucu, i64_tip, g_len, "t_il");
                            } else {
                                isim_ptr_val = LLVMConstNull(i8_ptr_tip);
                                isim_len_val = LLVMConstInt(i64_tip, 0, 0);
                            }

                            LLVMValueRef satir_val = LLVMConstInt(i64_tip, (unsigned long long)dugum->satir, 0);
                            LLVMValueRef args[] = { beklenen, gercek, isim_ptr_val, isim_len_val, satir_val };
                            LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 5, "");
                        }
                    }
                }
                return NULL;
            }

            /* doğrula_farklı(a, b) — a != b assertion */
            if (strcmp(isim, "do\xc4\x9frula_farkl\xc4\xb1") == 0 ||
                strcmp(isim, "dogrula_farkli") == 0) {
                if (dugum->çocuk_sayısı >= 2) {
                    LLVMValueRef a = llvm_ifade_uret(u, dugum->çocuklar[0]);
                    LLVMValueRef b = llvm_ifade_uret(u, dugum->çocuklar[1]);
                    if (a && b) {
                        LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_dogrula");
                        if (fn) {
                            LLVMTypeRef i64_tip = LLVMInt64TypeInContext(u->baglam);
                            /* Karşılaştır: a != b -> 1, a == b -> 0 */
                            LLVMValueRef cmp = LLVMBuildICmp(u->olusturucu, LLVMIntNE, a, b, "farkli");
                            LLVMValueRef kosul64 = LLVMBuildZExt(u->olusturucu, cmp, i64_tip, "farkli_ext");

                            LLVMTypeRef i8_ptr_tip = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
                            LLVMValueRef isim_ptr_val, isim_len_val;
                            LLVMValueRef g_ptr = LLVMGetNamedGlobal(u->modul, "_test_isim_ptr");
                            LLVMValueRef g_len = LLVMGetNamedGlobal(u->modul, "_test_isim_len");
                            if (g_ptr && g_len) {
                                isim_ptr_val = LLVMBuildLoad2(u->olusturucu, i8_ptr_tip, g_ptr, "t_ip2");
                                isim_len_val = LLVMBuildLoad2(u->olusturucu, i64_tip, g_len, "t_il2");
                            } else {
                                isim_ptr_val = LLVMConstNull(i8_ptr_tip);
                                isim_len_val = LLVMConstInt(i64_tip, 0, 0);
                            }

                            LLVMValueRef satir_val = LLVMConstInt(i64_tip, (unsigned long long)dugum->satir, 0);
                            LLVMValueRef args[] = { kosul64, isim_ptr_val, isim_len_val, satir_val };
                            LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 4, "");
                        }
                    }
                }
                return NULL;
            }

            /* uzunluk() özel durumu — karakter tabanlı */
            if ((strcmp(isim, "uzunluk") == 0 || strcmp(isim, "metin_uzunluk") == 0) &&
                dugum->çocuk_sayısı > 0) {
                LLVMValueRef arg = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (arg && LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMStructTypeKind &&
                    dugum->çocuklar[0]->sonuç_tipi == TİP_METİN) {
                    LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_utf8_karakter_say");
                    if (fn) {
                        LLVMValueRef ptr = LLVMBuildExtractValue(u->olusturucu, arg, 0, "m_ptr");
                        LLVMValueRef len = LLVMBuildExtractValue(u->olusturucu, arg, 1, "m_len");
                        LLVMValueRef args[] = { ptr, len };
                        return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 2, "karakter_say");
                    }
                }
                /* Dizi uzunluk: struct ikinci alanı */
                if (arg && LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMStructTypeKind) {
                    return LLVMBuildExtractValue(u->olusturucu, arg, 1, "uzunluk");
                }
            }

            /* byte_uzunluk() özel durumu — byte cinsinden */
            if (strcmp(isim, "byte_uzunluk") == 0 && dugum->çocuk_sayısı > 0) {
                LLVMValueRef arg = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (arg && LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMStructTypeKind) {
                    return LLVMBuildExtractValue(u->olusturucu, arg, 1, "byte_len");
                }
            }

            /* bul() özel durumu — karakter tabanlı */
            if (strcmp(isim, "bul") == 0 && dugum->çocuk_sayısı >= 2) {
                LLVMValueRef haystack = llvm_ifade_uret(u, dugum->çocuklar[0]);
                LLVMValueRef needle = llvm_ifade_uret(u, dugum->çocuklar[1]);
                if (haystack && needle &&
                    LLVMGetTypeKind(LLVMTypeOf(haystack)) == LLVMStructTypeKind &&
                    LLVMGetTypeKind(LLVMTypeOf(needle)) == LLVMStructTypeKind) {
                    LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_utf8_bul");
                    if (fn) {
                        LLVMValueRef h_ptr = LLVMBuildExtractValue(u->olusturucu, haystack, 0, "h_ptr");
                        LLVMValueRef h_len = LLVMBuildExtractValue(u->olusturucu, haystack, 1, "h_len");
                        LLVMValueRef n_ptr = LLVMBuildExtractValue(u->olusturucu, needle, 0, "n_ptr");
                        LLVMValueRef n_len = LLVMBuildExtractValue(u->olusturucu, needle, 1, "n_len");
                        LLVMValueRef args[] = { h_ptr, h_len, n_ptr, n_len };
                        return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 4, "bul_sonuc");
                    }
                }
            }

            /* kes() özel durumu — karakter tabanlı */
            if (strcmp(isim, "kes") == 0 && dugum->çocuk_sayısı >= 3) {
                LLVMValueRef metin_val = llvm_ifade_uret(u, dugum->çocuklar[0]);
                LLVMValueRef bas = llvm_ifade_uret(u, dugum->çocuklar[1]);
                LLVMValueRef uzn = llvm_ifade_uret(u, dugum->çocuklar[2]);
                if (metin_val && bas && uzn &&
                    LLVMGetTypeKind(LLVMTypeOf(metin_val)) == LLVMStructTypeKind) {
                    LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_utf8_kes");
                    if (fn) {
                        LLVMValueRef ptr = LLVMBuildExtractValue(u->olusturucu, metin_val, 0, "k_ptr");
                        LLVMValueRef len = LLVMBuildExtractValue(u->olusturucu, metin_val, 1, "k_len");
                        LLVMValueRef args[] = { ptr, len, bas, uzn };
                        return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 4, "kes_sonuc");
                    }
                }
            }

            /* === Birinci Sınıf Fonksiyon Desteği (eşlem/filtre/indirge) === */

            /* eşlem/dönüştür(d: dizi, fonk_adi) -> dizi (map) */
            if ((strcmp(isim, "e\xc5\x9flem") == 0 ||
                 strcmp(isim, "d\xc3\xb6n\xc3\xbc\xc5\x9ft\xc3\xbcr") == 0) && dugum->çocuk_sayısı >= 2) {
                LLVMValueRef dizi_val = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (dizi_val && LLVMGetTypeKind(LLVMTypeOf(dizi_val)) == LLVMStructTypeKind) {
                    LLVMValueRef dizi_ptr = LLVMBuildExtractValue(u->olusturucu, dizi_val, 0, "d_ptr");
                    LLVMValueRef dizi_cnt = LLVMBuildExtractValue(u->olusturucu, dizi_val, 1, "d_cnt");
                    /* Fonksiyon pointer: isim ise leaq, değilse ifade üret */
                    LLVMValueRef fn_ptr = NULL;
                    if (dugum->çocuklar[1]->tur == DÜĞÜM_TANIMLAYICI) {
                        char *fn_isim = dugum->çocuklar[1]->veri.tanimlayici.isim;
                        LLVMValueRef fn_val = LLVMGetNamedFunction(u->modul, fn_isim);
                        if (fn_val) fn_ptr = fn_val;
                    }
                    if (!fn_ptr) fn_ptr = llvm_ifade_uret(u, dugum->çocuklar[1]);
                    if (fn_ptr) {
                        /* _tr_esle(ptr, count, fn_ptr) -> {i64*, i64} */
                        LLVMTypeRef i64_t = LLVMInt64TypeInContext(u->baglam);
                        LLVMTypeRef i64p_t = LLVMPointerType(i64_t, 0);
                        LLVMTypeRef fn_cb_t = LLVMPointerType(LLVMFunctionType(i64_t, &i64_t, 1, 0), 0);
                        LLVMTypeRef diz_t = llvm_dizi_tipi_al(u);
                        LLVMTypeRef p_tipleri[] = { i64p_t, i64_t, fn_cb_t };
                        LLVMTypeRef esle_fn_tip = LLVMFunctionType(diz_t, p_tipleri, 3, 0);
                        LLVMValueRef esle_fn = LLVMGetNamedFunction(u->modul, "_tr_esle");
                        if (!esle_fn) esle_fn = LLVMAddFunction(u->modul, "_tr_esle", esle_fn_tip);
                        LLVMValueRef fn_cast;
                        if (LLVMGetTypeKind(LLVMTypeOf(fn_ptr)) == LLVMIntegerTypeKind)
                            fn_cast = LLVMBuildIntToPtr(u->olusturucu, fn_ptr, fn_cb_t, "fn_cast");
                        else
                            fn_cast = LLVMBuildBitCast(u->olusturucu, fn_ptr, fn_cb_t, "fn_cast");
                        LLVMValueRef args[] = { dizi_ptr, dizi_cnt, fn_cast };
                        return LLVMBuildCall2(u->olusturucu, esle_fn_tip, esle_fn, args, 3, "esle_sonuc");
                    }
                }
            }

            /* filtre/filtrele(d: dizi, fonk_adi) -> dizi */
            if ((strcmp(isim, "filtre") == 0 ||
                 strcmp(isim, "filtrele") == 0) && dugum->çocuk_sayısı >= 2) {
                LLVMValueRef dizi_val = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (dizi_val && LLVMGetTypeKind(LLVMTypeOf(dizi_val)) == LLVMStructTypeKind) {
                    LLVMValueRef dizi_ptr = LLVMBuildExtractValue(u->olusturucu, dizi_val, 0, "d_ptr");
                    LLVMValueRef dizi_cnt = LLVMBuildExtractValue(u->olusturucu, dizi_val, 1, "d_cnt");
                    LLVMValueRef fn_ptr = NULL;
                    if (dugum->çocuklar[1]->tur == DÜĞÜM_TANIMLAYICI) {
                        char *fn_isim = dugum->çocuklar[1]->veri.tanimlayici.isim;
                        LLVMValueRef fn_val = LLVMGetNamedFunction(u->modul, fn_isim);
                        if (fn_val) fn_ptr = fn_val;
                    }
                    if (!fn_ptr) fn_ptr = llvm_ifade_uret(u, dugum->çocuklar[1]);
                    if (fn_ptr) {
                        LLVMTypeRef i64_t = LLVMInt64TypeInContext(u->baglam);
                        LLVMTypeRef i64p_t = LLVMPointerType(i64_t, 0);
                        LLVMTypeRef fn_cb_t = LLVMPointerType(LLVMFunctionType(i64_t, &i64_t, 1, 0), 0);
                        LLVMTypeRef diz_t = llvm_dizi_tipi_al(u);
                        LLVMTypeRef p_tipleri[] = { i64p_t, i64_t, fn_cb_t };
                        LLVMTypeRef filtre_fn_tip = LLVMFunctionType(diz_t, p_tipleri, 3, 0);
                        LLVMValueRef filtre_fn = LLVMGetNamedFunction(u->modul, "_tr_filtre");
                        if (!filtre_fn) filtre_fn = LLVMAddFunction(u->modul, "_tr_filtre", filtre_fn_tip);
                        LLVMValueRef fn_cast;
                        if (LLVMGetTypeKind(LLVMTypeOf(fn_ptr)) == LLVMIntegerTypeKind)
                            fn_cast = LLVMBuildIntToPtr(u->olusturucu, fn_ptr, fn_cb_t, "fn_cast");
                        else
                            fn_cast = LLVMBuildBitCast(u->olusturucu, fn_ptr, fn_cb_t, "fn_cast");
                        LLVMValueRef args[] = { dizi_ptr, dizi_cnt, fn_cast };
                        return LLVMBuildCall2(u->olusturucu, filtre_fn_tip, filtre_fn, args, 3, "filtre_sonuc");
                    }
                }
            }

            /* indirge/biriktir(d: dizi, başlangıç: tam, fonk_adi) -> tam */
            if ((strcmp(isim, "indirge") == 0 ||
                 strcmp(isim, "biriktir") == 0) && dugum->çocuk_sayısı >= 3) {
                LLVMValueRef dizi_val = llvm_ifade_uret(u, dugum->çocuklar[0]);
                LLVMValueRef baslangic = llvm_ifade_uret(u, dugum->çocuklar[1]);
                if (dizi_val && baslangic && LLVMGetTypeKind(LLVMTypeOf(dizi_val)) == LLVMStructTypeKind) {
                    LLVMValueRef dizi_ptr = LLVMBuildExtractValue(u->olusturucu, dizi_val, 0, "d_ptr");
                    LLVMValueRef dizi_cnt = LLVMBuildExtractValue(u->olusturucu, dizi_val, 1, "d_cnt");
                    LLVMValueRef fn_ptr = NULL;
                    if (dugum->çocuklar[2]->tur == DÜĞÜM_TANIMLAYICI) {
                        char *fn_isim = dugum->çocuklar[2]->veri.tanimlayici.isim;
                        LLVMValueRef fn_val = LLVMGetNamedFunction(u->modul, fn_isim);
                        if (fn_val) fn_ptr = fn_val;
                    }
                    if (!fn_ptr) fn_ptr = llvm_ifade_uret(u, dugum->çocuklar[2]);
                    if (fn_ptr) {
                        LLVMTypeRef i64_t = LLVMInt64TypeInContext(u->baglam);
                        LLVMTypeRef i64p_t = LLVMPointerType(i64_t, 0);
                        LLVMTypeRef i64_pair[] = { i64_t, i64_t };
                        LLVMTypeRef fn_cb_t = LLVMPointerType(LLVMFunctionType(i64_t, i64_pair, 2, 0), 0);
                        LLVMTypeRef p_tipleri[] = { i64p_t, i64_t, i64_t, fn_cb_t };
                        LLVMTypeRef indirge_fn_tip = LLVMFunctionType(i64_t, p_tipleri, 4, 0);
                        LLVMValueRef indirge_fn = LLVMGetNamedFunction(u->modul, "_tr_indirge");
                        if (!indirge_fn) indirge_fn = LLVMAddFunction(u->modul, "_tr_indirge", indirge_fn_tip);
                        LLVMValueRef fn_cast;
                        if (LLVMGetTypeKind(LLVMTypeOf(fn_ptr)) == LLVMIntegerTypeKind)
                            fn_cast = LLVMBuildIntToPtr(u->olusturucu, fn_ptr, fn_cb_t, "fn_cast");
                        else
                            fn_cast = LLVMBuildBitCast(u->olusturucu, fn_ptr, fn_cb_t, "fn_cast");
                        LLVMValueRef args[] = { dizi_ptr, dizi_cnt, baslangic, fn_cast };
                        return LLVMBuildCall2(u->olusturucu, indirge_fn_tip, indirge_fn, args, 4, "indirge_sonuc");
                    }
                }
            }

            /* her_biri(d: dizi, fonk_adi) -> tam (forEach) */
            if (strcmp(isim, "her_biri") == 0 && dugum->çocuk_sayısı >= 2) {
                LLVMValueRef dizi_val = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (dizi_val && LLVMGetTypeKind(LLVMTypeOf(dizi_val)) == LLVMStructTypeKind) {
                    LLVMValueRef dizi_ptr = LLVMBuildExtractValue(u->olusturucu, dizi_val, 0, "d_ptr");
                    LLVMValueRef dizi_cnt = LLVMBuildExtractValue(u->olusturucu, dizi_val, 1, "d_cnt");
                    LLVMValueRef fn_ptr = NULL;
                    if (dugum->çocuklar[1]->tur == DÜĞÜM_TANIMLAYICI) {
                        char *fn_isim = dugum->çocuklar[1]->veri.tanimlayici.isim;
                        LLVMValueRef fn_val = LLVMGetNamedFunction(u->modul, fn_isim);
                        if (fn_val) fn_ptr = fn_val;
                    }
                    if (!fn_ptr) fn_ptr = llvm_ifade_uret(u, dugum->çocuklar[1]);
                    if (fn_ptr) {
                        LLVMTypeRef i64_t = LLVMInt64TypeInContext(u->baglam);
                        LLVMTypeRef i64p_t = LLVMPointerType(i64_t, 0);
                        LLVMTypeRef fn_cb_t = LLVMPointerType(LLVMFunctionType(i64_t, &i64_t, 1, 0), 0);
                        LLVMTypeRef p_tipleri[] = { i64p_t, i64_t, fn_cb_t };
                        LLVMTypeRef hb_fn_tip = LLVMFunctionType(i64_t, p_tipleri, 3, 0);
                        LLVMValueRef hb_fn = LLVMGetNamedFunction(u->modul, "_tr_her_biri");
                        if (!hb_fn) hb_fn = LLVMAddFunction(u->modul, "_tr_her_biri", hb_fn_tip);
                        /* fn_ptr i64 ise IntToPtr, pointer ise BitCast */
                        LLVMValueRef fn_cast;
                        if (LLVMGetTypeKind(LLVMTypeOf(fn_ptr)) == LLVMIntegerTypeKind)
                            fn_cast = LLVMBuildIntToPtr(u->olusturucu, fn_ptr, fn_cb_t, "fn_cast");
                        else
                            fn_cast = LLVMBuildBitCast(u->olusturucu, fn_ptr, fn_cb_t, "fn_cast");
                        LLVMValueRef args[] = { dizi_ptr, dizi_cnt, fn_cast };
                        return LLVMBuildCall2(u->olusturucu, hb_fn_tip, hb_fn, args, 3, "hb_sonuc");
                    }
                }
            }

            /* Metot çağrısı: nesne.metot(args...) */
            if (dugum->veri.tanimlayici.tip &&
                strcmp(dugum->veri.tanimlayici.tip, "metot") == 0 &&
                dugum->çocuk_sayısı > 0) {

                /* Metin metot çağrıları */
                if (dugum->çocuklar[0]->sonuç_tipi == TİP_METİN) {
                    LLVMValueRef nesne = llvm_ifade_uret(u, dugum->çocuklar[0]);
                    if (nesne && LLVMGetTypeKind(LLVMTypeOf(nesne)) == LLVMStructTypeKind) {
                        LLVMValueRef ptr = LLVMBuildExtractValue(u->olusturucu, nesne, 0, "m_ptr");
                        LLVMValueRef len = LLVMBuildExtractValue(u->olusturucu, nesne, 1, "m_len");
                        /* m.uzunluk() */
                        if (strcmp(isim, "uzunluk") == 0) {
                            LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_utf8_karakter_say");
                            if (fn) {
                                LLVMValueRef args[] = { ptr, len };
                                return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 2, "karakter_say");
                            }
                        }
                        /* m.byte_uzunluk() */
                        if (strcmp(isim, "byte_uzunluk") == 0) {
                            return len;
                        }
                        /* m.tersle() */
                        if (strcmp(isim, "tersle") == 0) {
                            LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_utf8_tersle");
                            if (fn) {
                                LLVMValueRef args[] = { ptr, len };
                                return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 2, "tersle_sonuc");
                            }
                        }
                    }
                }

                /* Receiver nesneyi üret */
                LLVMValueRef nesne = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (!nesne) return NULL;

                /* Sınıf adını bul: tüm sınıflarda mangled ismi ara */
                LLVMSembolGirişi *metot_fn = NULL;
                LLVMSınıfBilgisi *sinif_it = u->siniflar;
                while (sinif_it) {
                    char mangled[256];
                    snprintf(mangled, sizeof(mangled), "%s_%s", sinif_it->isim, isim);
                    metot_fn = llvm_sembol_bul(u, mangled);
                    if (metot_fn) break;
                    sinif_it = sinif_it->sonraki;
                }

                if (metot_fn) {
                    /* Argümanları hazırla: [nesne(bu), arg1, arg2, ...] */
                    LLVMValueRef args[32];
                    args[0] = nesne;
                    for (int i = 1; i < dugum->çocuk_sayısı && i < 31; i++) {
                        args[i] = llvm_ifade_uret(u, dugum->çocuklar[i]);
                        if (!args[i]) args[i] = LLVMConstNull(LLVMInt64TypeInContext(u->baglam));
                    }

                    LLVMTypeRef donus_tip = LLVMGetReturnType(metot_fn->tip);
                    const char *cagri_adi = (LLVMGetTypeKind(donus_tip) == LLVMVoidTypeKind) ? "" : "metot_sonuc";
                    return LLVMBuildCall2(u->olusturucu, metot_fn->tip, metot_fn->deger,
                                         args, dugum->çocuk_sayısı, cagri_adi);
                } else {
                    fprintf(stderr, "Hata: Tanımsız metot: %s\n", isim);
                    return NULL;
                }
            }

            /* Normal fonksiyon çağrısı */
            LLVMSembolGirişi *fn = llvm_sembol_bul(u, isim);
            if (!fn) {
                /* Stdlib fallback: modul_fonksiyon_bul ile dene */
                const ModülFonksiyon *mf = modul_fonksiyon_bul(isim);
                if (mf) {
                    /* Fonksiyonu LLVM'e dinamik olarak kaydet */
                    LLVMSembolGirişi *rt = llvm_sembol_bul(u, mf->runtime_isim);
                    if (!rt) {
                        /* Henüz tanımlı değil — LLVM'e ekle */
                        LLVMTypeRef i64_t = LLVMInt64TypeInContext(u->baglam);
                        LLVMTypeRef i8p_t = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
                        LLVMTypeRef dbl_t = LLVMDoubleTypeInContext(u->baglam);
                        LLVMTypeRef mtn_t = llvm_metin_tipi_al(u);
                        LLVMTypeRef diz_t = llvm_dizi_tipi_al(u);
                        LLVMTypeRef pt[16]; int pc = 0;
                        for (int j = 0; j < mf->param_sayisi; j++) {
                            switch (mf->param_tipleri[j]) {
                                case TİP_TAM:     pt[pc++] = i64_t; break;
                                case TİP_ONDALIK: pt[pc++] = dbl_t; break;
                                case TİP_MANTIK:  pt[pc++] = i64_t; break;
                                case TİP_METİN:   pt[pc++] = i8p_t; pt[pc++] = i64_t; break;
                                case TİP_DİZİ:    pt[pc++] = LLVMPointerType(i64_t, 0); pt[pc++] = i64_t; break;
                                default:          pt[pc++] = i64_t; break;
                            }
                        }
                        LLVMTypeRef dt;
                        switch (mf->dönüş_tipi) {
                            case TİP_TAM:     dt = i64_t; break;
                            case TİP_ONDALIK: dt = dbl_t; break;
                            case TİP_MANTIK:  dt = i64_t; break;
                            case TİP_METİN:   dt = mtn_t; break;
                            case TİP_DİZİ:    dt = diz_t; break;
                            case TİP_BOŞLUK:  dt = LLVMVoidTypeInContext(u->baglam); break;
                            default:          dt = i64_t; break;
                        }
                        LLVMTypeRef ft = LLVMFunctionType(dt, pt, pc, 0);
                        LLVMValueRef fv = LLVMAddFunction(u->modul, mf->runtime_isim, ft);
                        llvm_sembol_ekle(u, mf->runtime_isim, fv, ft, 0, 1);
                        llvm_sembol_ekle(u, isim, fv, ft, 0, 1);
                        fn = llvm_sembol_bul(u, isim);
                    } else {
                        /* Runtime isimle zaten var — Tonyukuk isim alias'ı ekle */
                        llvm_sembol_ekle(u, isim, rt->deger, rt->tip, 0, 1);
                        fn = llvm_sembol_bul(u, isim);
                    }
                }
                if (!fn) {
                    fprintf(stderr, "Hata: Tanımsız işlev: %s\n", isim);
                    return NULL;
                }
            }

            /* Dolaylı çağrı kontrolü: fn->deger fonksiyon değilse pointer'dan çağır */
            if (!LLVMIsAFunction(fn->deger)) {
                LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
                LLVMValueRef fn_ptr_val = LLVMBuildLoad2(u->olusturucu, i64,
                    fn->deger, "fn_ptr_yukle");

                /* Fonksiyon tipini argümanlardan oluştur */
                int arg_say = dugum->çocuk_sayısı < 32 ? dugum->çocuk_sayısı : 32;
                LLVMTypeRef param_t[32];
                for (int i = 0; i < arg_say; i++) param_t[i] = i64;
                LLVMTypeRef fn_tip = LLVMFunctionType(i64, param_t, arg_say, 0);
                LLVMTypeRef fn_ptr_tip = LLVMPointerType(fn_tip, 0);

                /* i64 → fonksiyon pointer'ına dönüştür */
                LLVMValueRef fn_ptr = LLVMBuildIntToPtr(u->olusturucu,
                    fn_ptr_val, fn_ptr_tip, "fn_ptr");

                /* Argümanları üret */
                LLVMValueRef args[32];
                for (int i = 0; i < arg_say; i++) {
                    args[i] = llvm_ifade_uret(u, dugum->çocuklar[i]);
                    if (!args[i]) args[i] = LLVMConstInt(i64, 0, 0);
                }

                return LLVMBuildCall2(u->olusturucu, fn_tip, fn_ptr,
                    args, arg_say, "dolayli_cagri");
            }

            /* Fonksiyonun parametre tiplerini al */
            unsigned fn_param_sayisi = LLVMCountParamTypes(fn->tip);
            LLVMTypeRef fn_param_tipleri[32];
            if (fn_param_sayisi > 0 && fn_param_sayisi <= 32) {
                LLVMGetParamTypes(fn->tip, fn_param_tipleri);
            }

            /* Argümanları üret - metin/dizi struct'ları flat (ptr, len) olarak genişlet */
            int kaynak_arg_sayisi = dugum->çocuk_sayısı;
            LLVMValueRef genisletilmis_args[32];
            int gen_idx = 0;

            for (int i = 0; i < kaynak_arg_sayisi && gen_idx < 32; i++) {
                LLVMValueRef arg = llvm_ifade_uret(u, dugum->çocuklar[i]);
                if (!arg) {
                    genisletilmis_args[gen_idx++] = LLVMConstNull(LLVMInt64TypeInContext(u->baglam));
                    continue;
                }

                LLVMTypeRef arg_tip = LLVMTypeOf(arg);
                LLVMTypeKind arg_kind = LLVMGetTypeKind(arg_tip);

                /* Beklenen parametre tipini kontrol et */
                if ((unsigned)gen_idx < fn_param_sayisi) {
                    LLVMTypeRef beklenen = fn_param_tipleri[gen_idx];
                    LLVMTypeKind bek_kind = LLVMGetTypeKind(beklenen);

                    /* Struct arg ama flat pointer param bekleniyor → metin/dizi genişlet */
                    if (arg_kind == LLVMStructTypeKind && bek_kind == LLVMPointerTypeKind) {
                        genisletilmis_args[gen_idx++] =
                            LLVMBuildExtractValue(u->olusturucu, arg, 0, "arg_ptr");
                        if ((unsigned)gen_idx < fn_param_sayisi) {
                            genisletilmis_args[gen_idx++] =
                                LLVMBuildExtractValue(u->olusturucu, arg, 1, "arg_len");
                        }
                        continue;
                    }

                    /* Integer genişlik uyumsuzluğu (i1 → i64 vb.) */
                    if (arg_kind == LLVMIntegerTypeKind && bek_kind == LLVMIntegerTypeKind) {
                        unsigned arg_w = LLVMGetIntTypeWidth(arg_tip);
                        unsigned bek_w = LLVMGetIntTypeWidth(beklenen);
                        if (arg_w < bek_w) {
                            arg = LLVMBuildZExt(u->olusturucu, arg, beklenen, "zext_arg");
                        } else if (arg_w > bek_w) {
                            arg = LLVMBuildTrunc(u->olusturucu, arg, beklenen, "trunc_arg");
                        }
                    }
                }

                genisletilmis_args[gen_idx++] = arg;
            }

            /* Eksik argümanlar için varsayılan değer ekle */
            if ((unsigned)gen_idx < fn_param_sayisi) {
                LLVMIslevVarsayilan *vars = llvm_varsayilan_bul(isim);
                int kaynak_idx = kaynak_arg_sayisi;  /* hangi parametreden başlayacağız */
                while ((unsigned)gen_idx < fn_param_sayisi) {
                    int eklendi = 0;
                    if (vars && kaynak_idx < vars->param_sayisi &&
                        vars->varsayilan_dugumler[kaynak_idx]) {
                        LLVMValueRef vd = llvm_ifade_uret(u, vars->varsayilan_dugumler[kaynak_idx]);
                        if (vd) {
                            /* Struct → flat genişletme (metin varsayılan parametresi) */
                            if (LLVMGetTypeKind(LLVMTypeOf(vd)) == LLVMStructTypeKind &&
                                (unsigned)gen_idx < fn_param_sayisi &&
                                LLVMGetTypeKind(fn_param_tipleri[gen_idx]) == LLVMPointerTypeKind) {
                                genisletilmis_args[gen_idx++] =
                                    LLVMBuildExtractValue(u->olusturucu, vd, 0, "def_ptr");
                                if ((unsigned)gen_idx < fn_param_sayisi) {
                                    genisletilmis_args[gen_idx++] =
                                        LLVMBuildExtractValue(u->olusturucu, vd, 1, "def_len");
                                }
                            } else {
                                genisletilmis_args[gen_idx++] = vd;
                            }
                            eklendi = 1;
                        }
                    }
                    if (!eklendi) {
                        genisletilmis_args[gen_idx] = LLVMConstNull(fn_param_tipleri[gen_idx]);
                        gen_idx++;
                    }
                    kaynak_idx++;
                }
            }

            /* Void fonksiyonlar için isim boş olmalı */
            LLVMTypeRef donus_tip = LLVMGetReturnType(fn->tip);
            const char *cagri_adi = (LLVMGetTypeKind(donus_tip) == LLVMVoidTypeKind) ? "" : "cagri_sonuc";
            return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger,
                                 genisletilmis_args, gen_idx, cagri_adi);
        }

        case DÜĞÜM_DİZİ_ERİŞİM: {
            /* Dizi erişimi: dizi[indeks] */
            LLVMValueRef dizi = llvm_ifade_uret(u, dugum->çocuklar[0]);
            LLVMValueRef indeks = llvm_ifade_uret(u, dugum->çocuklar[1]);

            if (!dizi || !indeks) return NULL;

            LLVMTypeRef i64_tip = LLVMInt64TypeInContext(u->baglam);

            /* Metin karakter indeksleme: metin[i] -> tek karakter metin */
            if (dugum->çocuklar[0]->sonuç_tipi == TİP_METİN &&
                LLVMGetTypeKind(LLVMTypeOf(dizi)) == LLVMStructTypeKind) {
                LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_utf8_karakter_al");
                if (fn) {
                    LLVMValueRef ptr = LLVMBuildExtractValue(u->olusturucu, dizi, 0, "m_ptr");
                    LLVMValueRef len = LLVMBuildExtractValue(u->olusturucu, dizi, 1, "m_len");
                    /* indeks i64'e genislet */
                    if (LLVMGetIntTypeWidth(LLVMTypeOf(indeks)) != 64) {
                        indeks = LLVMBuildSExt(u->olusturucu, indeks, i64_tip, "idx_ext");
                    }
                    LLVMValueRef args[] = { ptr, len, indeks };
                    return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 3, "karakter");
                }
            }

            /* Dizi struct {i64*, i64} ise pointer alanını çıkar */
            LLVMTypeRef dizi_tip = LLVMTypeOf(dizi);
            LLVMValueRef veri_ptr;
            LLVMValueRef dizi_uzunluk = NULL;
            if (LLVMGetTypeKind(dizi_tip) == LLVMStructTypeKind) {
                veri_ptr = LLVMBuildExtractValue(u->olusturucu, dizi, 0, "dizi_ptr");
                dizi_uzunluk = LLVMBuildExtractValue(u->olusturucu, dizi, 1, "dizi_len");
            } else {
                veri_ptr = dizi;
            }

            /* Sınır kontrolü: indeks < 0 || indeks >= uzunluk */
            if (dizi_uzunluk) {
                LLVMValueRef sifir = LLVMConstInt(i64_tip, 0, 0);
                LLVMValueRef negatif = LLVMBuildICmp(u->olusturucu, LLVMIntSLT, indeks, sifir, "neg_chk");
                LLVMValueRef fazla = LLVMBuildICmp(u->olusturucu, LLVMIntSGE, indeks, dizi_uzunluk, "ub_chk");
                LLVMValueRef hata = LLVMBuildOr(u->olusturucu, negatif, fazla, "sinir_hata");

                LLVMBasicBlockRef hata_bb = LLVMAppendBasicBlockInContext(
                    u->baglam, u->mevcut_islev, "sinir_hata");
                LLVMBasicBlockRef devam_bb = LLVMAppendBasicBlockInContext(
                    u->baglam, u->mevcut_islev, "sinir_ok");
                LLVMBuildCondBr(u->olusturucu, hata, hata_bb, devam_bb);

                /* Hata bloğu: stderr'e mesaj yazdır + exit(1) */
                LLVMPositionBuilderAtEnd(u->olusturucu, hata_bb);
                /* _tr_dizi_sinir_hatasi() C runtime fonksiyonu çağır */
                LLVMValueRef sinir_fn = LLVMGetNamedFunction(u->modul, "_tr_dizi_sinir_hatasi");
                if (!sinir_fn) {
                    LLVMTypeRef void_t = LLVMVoidTypeInContext(u->baglam);
                    LLVMTypeRef ft = LLVMFunctionType(void_t, NULL, 0, 0);
                    sinir_fn = LLVMAddFunction(u->modul, "_tr_dizi_sinir_hatasi", ft);
                    LLVMAddAttributeAtIndex(sinir_fn, LLVMAttributeFunctionIndex,
                        LLVMCreateEnumAttribute(u->baglam,
                            LLVMGetEnumAttributeKindForName("noreturn", 8), 0));
                }
                LLVMTypeRef void_t2 = LLVMVoidTypeInContext(u->baglam);
                LLVMTypeRef sinir_ft = LLVMFunctionType(void_t2, NULL, 0, 0);
                LLVMBuildCall2(u->olusturucu, sinir_ft, sinir_fn, NULL, 0, "");
                LLVMBuildUnreachable(u->olusturucu);

                LLVMPositionBuilderAtEnd(u->olusturucu, devam_bb);
            }

            /* Metin dizisi mi kontrol et */
            int metin_dizi_erisim = 0;
            if (dugum->çocuklar[0]->tur == DÜĞÜM_TANIMLAYICI) {
                LLVMSembolGirişi *dizi_sem = llvm_sembol_bul(u,
                    dugum->çocuklar[0]->veri.tanimlayici.isim);
                if (dizi_sem && dizi_sem->metin_dizisi) {
                    metin_dizi_erisim = 1;
                }
            }

            /* GEP ile element adresini al */
            LLVMTypeRef eleman_tipi = metin_dizi_erisim ? llvm_metin_tipi_al(u) : i64_tip;
            LLVMValueRef eleman_ptr = LLVMBuildGEP2(
                u->olusturucu, eleman_tipi, veri_ptr,
                &indeks, 1, "eleman_ptr"
            );

            return LLVMBuildLoad2(u->olusturucu, eleman_tipi, eleman_ptr, "eleman");
        }

        case DÜĞÜM_DİLİM: {
            /* Dilim: metin[bas:son] veya dizi[bas:son] */
            if (dugum->çocuk_sayısı >= 3) {
                LLVMValueRef kaynak = llvm_ifade_uret(u, dugum->çocuklar[0]);
                LLVMValueRef bas = llvm_ifade_uret(u, dugum->çocuklar[1]);
                LLVMValueRef son = llvm_ifade_uret(u, dugum->çocuklar[2]);
                if (kaynak && bas && son &&
                    LLVMGetTypeKind(LLVMTypeOf(kaynak)) == LLVMStructTypeKind) {
                    if (dugum->çocuklar[0]->sonuç_tipi == TİP_METİN) {
                        /* Metin dilimi */
                        LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_utf8_dilim");
                        if (fn) {
                            LLVMValueRef ptr = LLVMBuildExtractValue(u->olusturucu, kaynak, 0, "d_ptr");
                            LLVMValueRef len = LLVMBuildExtractValue(u->olusturucu, kaynak, 1, "d_len");
                            LLVMValueRef args[] = { ptr, len, bas, son };
                            return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 4, "dilim");
                        }
                    } else {
                        /* Dizi dilimi */
                        LLVMSembolGirişi *fn = llvm_sembol_bul(u, "_tr_dizi_dilim");
                        if (!fn) {
                            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
                            LLVMTypeRef i64p = LLVMPointerType(i64, 0);
                            LLVMTypeRef pt[] = { i64p, i64, i64, i64 };
                            LLVMTypeRef ret_f[] = { i64p, i64 };
                            LLVMTypeRef ret_t = LLVMStructTypeInContext(u->baglam, ret_f, 2, 0);
                            LLVMTypeRef ft = LLVMFunctionType(ret_t, pt, 4, 0);
                            LLVMValueRef fv = LLVMAddFunction(u->modul, "_tr_dizi_dilim", ft);
                            llvm_sembol_ekle(u, "_tr_dizi_dilim", fv, ft, 0, 1);
                            fn = llvm_sembol_bul(u, "_tr_dizi_dilim");
                        }
                        if (fn) {
                            LLVMValueRef ptr = LLVMBuildExtractValue(u->olusturucu, kaynak, 0, "d_ptr");
                            LLVMValueRef len = LLVMBuildExtractValue(u->olusturucu, kaynak, 1, "d_len");
                            LLVMValueRef args[] = { ptr, len, bas, son };
                            return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 4, "dizi_dilim");
                        }
                    }
                }
            }
            return NULL;
        }

        case DÜĞÜM_ERİŞİM: {
            /* Alan erişimi: nesne.alan */
            LLVMValueRef nesne = llvm_ifade_uret(u, dugum->çocuklar[0]);
            char *alan_adi = dugum->veri.tanimlayici.isim;

            if (!nesne) return NULL;

            /* Nesne pointer tipini al ve sınıf bilgisini bul */
            LLVMTypeRef nesne_tip = LLVMTypeOf(nesne);
            LLVMTypeKind nesne_kind = LLVMGetTypeKind(nesne_tip);

            /* Sınıf bilgisini tüm sınıflarda ara */
            LLVMSınıfBilgisi *sinif = u->siniflar;
            int alan_indeks = -1;
            LLVMTypeRef alan_tipi = LLVMInt64TypeInContext(u->baglam);

            while (sinif) {
                for (int i = 0; i < sinif->alan_sayisi; i++) {
                    if (strcmp(sinif->alan_isimleri[i], alan_adi) == 0) {
                        alan_indeks = i;
                        alan_tipi = sinif->alan_tipleri[i];
                        break;
                    }
                }
                if (alan_indeks >= 0) break;
                sinif = sinif->sonraki;
            }

            if (alan_indeks < 0) {
                /* Getter fallback: al_<alan_adi> metot ara */
                char getter_adi[256];
                snprintf(getter_adi, sizeof(getter_adi), "al_%s", alan_adi);
                LLVMSınıfBilgisi *s_it = u->siniflar;
                while (s_it) {
                    char mangled[256];
                    snprintf(mangled, sizeof(mangled), "%s_%s", s_it->isim, getter_adi);
                    LLVMSembolGirişi *getter_fn = llvm_sembol_bul(u, mangled);
                    if (getter_fn) {
                        /* Getter bulundu: getter(bu) çağır */
                        LLVMValueRef args[] = { nesne };
                        LLVMTypeRef donus_tip = LLVMGetReturnType(getter_fn->tip);
                        const char *cagri_adi = (LLVMGetTypeKind(donus_tip) == LLVMVoidTypeKind) ? "" : "getter_sonuc";
                        return LLVMBuildCall2(u->olusturucu, getter_fn->tip,
                                             getter_fn->deger, args, 1, cagri_adi);
                    }
                    s_it = s_it->sonraki;
                }
                fprintf(stderr, "Hata: Bilinmeyen alan: %s\n", alan_adi);
                return NULL;
            }

            /* Statik alan: global değişkenden oku */
            if (sinif->alan_statik && sinif->alan_statik[alan_indeks] &&
                sinif->statik_globals && sinif->statik_globals[alan_indeks]) {
                return LLVMBuildLoad2(u->olusturucu, alan_tipi,
                                      sinif->statik_globals[alan_indeks], alan_adi);
            }

            /* Nesne pointer üzerinden struct alanına GEP */
            LLVMValueRef sifir = LLVMConstInt(LLVMInt32TypeInContext(u->baglam), 0, 0);
            LLVMValueRef idx = LLVMConstInt(LLVMInt32TypeInContext(u->baglam), alan_indeks, 0);
            LLVMValueRef indices[] = { sifir, idx };

            LLVMValueRef alan_ptr = LLVMBuildGEP2(
                u->olusturucu,
                sinif->struct_tipi,
                nesne,
                indices, 2,
                alan_adi
            );

            return LLVMBuildLoad2(u->olusturucu, alan_tipi, alan_ptr, alan_adi);
        }

        case DÜĞÜM_DİZİ_DEĞERİ: {
            /* Dizi literal: [1, 2, 3] veya ["a", "b", "c"] */
            int eleman_sayisi = dugum->çocuk_sayısı;
            LLVMTypeRef i64_tip = LLVMInt64TypeInContext(u->baglam);

            /* Metin dizisi mi kontrol et (ilk elemanın sonuç tipine bak) */
            int metin_eleman_mi = 0;
            if (eleman_sayisi > 0 && dugum->çocuklar[0]->sonuç_tipi == TİP_METİN) {
                metin_eleman_mi = 1;
            }

            /* Eleman boyutu: metin={ptr,i64}=16 byte, diğerleri=i64=8 byte */
            int eleman_boyut = metin_eleman_mi ? 16 : 8;
            LLVMTypeRef eleman_tip = metin_eleman_mi ? llvm_metin_tipi_al(u) : i64_tip;

            /* malloc ile heap'te alan ayır */
            LLVMValueRef boyut = LLVMConstInt(i64_tip, eleman_sayisi * eleman_boyut, 0);
            LLVMSembolGirişi *malloc_fn = llvm_sembol_bul(u, "malloc");
            if (!malloc_fn) return NULL;

            LLVMValueRef malloc_args[] = { boyut };
            LLVMValueRef raw_ptr = LLVMBuildCall2(u->olusturucu, malloc_fn->tip,
                                                   malloc_fn->deger, malloc_args, 1, "dizi_raw");
            LLVMValueRef dizi_ptr = LLVMBuildBitCast(u->olusturucu, raw_ptr,
                LLVMPointerType(eleman_tip, 0), "dizi_ptr");

            /* Her elemanı yaz */
            for (int i = 0; i < eleman_sayisi; i++) {
                LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[i]);
                if (!deger) deger = LLVMConstInt(i64_tip, 0, 0);

                /* Integer genişlik uyumsuzluğu düzelt (metin olmayan elemanlar için) */
                if (!metin_eleman_mi) {
                    LLVMTypeRef deger_tip = LLVMTypeOf(deger);
                    if (LLVMGetTypeKind(deger_tip) == LLVMIntegerTypeKind &&
                        LLVMGetIntTypeWidth(deger_tip) != 64) {
                        deger = LLVMBuildZExt(u->olusturucu, deger, i64_tip, "dizi_elem_ext");
                    }
                }

                LLVMValueRef idx = LLVMConstInt(i64_tip, i, 0);
                LLVMValueRef elem_ptr = LLVMBuildGEP2(u->olusturucu, eleman_tip,
                                                       dizi_ptr, &idx, 1, "elem_ptr");
                LLVMBuildStore(u->olusturucu, deger, elem_ptr);
            }

            /* son_dizi_metin bayrağını ayarla (değişken tanımında kullanılır) */
            u->son_dizi_metin = metin_eleman_mi;

            /* Dizi struct { ptr, i64 } oluştur */
            LLVMTypeRef dizi_struct_tipi = llvm_dizi_tipi_al(u);
            LLVMValueRef dizi_struct = LLVMGetUndef(dizi_struct_tipi);
            dizi_struct = LLVMBuildInsertValue(u->olusturucu, dizi_struct,
                                                dizi_ptr, 0, "dizi_s_ptr");
            LLVMValueRef eleman_sayisi_val = LLVMConstInt(i64_tip, eleman_sayisi, 0);
            dizi_struct = LLVMBuildInsertValue(u->olusturucu, dizi_struct,
                                                eleman_sayisi_val, 1, "dizi_s_len");
            return dizi_struct;
        }

        case DÜĞÜM_SÖZLÜK_DEĞERİ: {
            /* {k1: v1, k2: v2, ...} → sözlük oluştur */
            LLVMTypeRef i64_tip = LLVMInt64TypeInContext(u->baglam);

            /* _tr_sozluk_yeni() çağır */
            LLVMSembolGirişi *yeni_fn = llvm_sembol_bul(u, "_tr_sozluk_yeni");
            if (!yeni_fn) return NULL;
            LLVMValueRef sozluk = LLVMBuildCall2(u->olusturucu, yeni_fn->tip,
                yeni_fn->deger, NULL, 0, "sozluk_yeni");

            /* Her çift için _tr_sozluk_ekle çağır */
            LLVMSembolGirişi *ekle_fn = llvm_sembol_bul(u, "_tr_sozluk_ekle");
            if (ekle_fn) {
                for (int i = 0; i + 1 < dugum->çocuk_sayısı; i += 2) {
                    LLVMValueRef anahtar = llvm_ifade_uret(u, dugum->çocuklar[i]);
                    LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[i + 1]);
                    if (!anahtar || !deger) continue;
                    /* Anahtar metin struct {ptr, len} */
                    LLVMValueRef aptr = LLVMBuildExtractValue(u->olusturucu, anahtar, 0, "a_ptr");
                    LLVMValueRef alen = LLVMBuildExtractValue(u->olusturucu, anahtar, 1, "a_len");
                    LLVMValueRef args[] = { sozluk, aptr, alen, deger };
                    sozluk = LLVMBuildCall2(u->olusturucu, ekle_fn->tip,
                        ekle_fn->deger, args, 4, "sozluk_ekle");
                }
            }
            return sozluk;
        }

        case DÜĞÜM_SÖZLÜK_ERİŞİM: {
            /* sözlük["anahtar"] → _tr_sozluk_oku(sozluk, ptr, len) */
            if (dugum->çocuk_sayısı < 2) return NULL;
            LLVMValueRef sozluk = llvm_ifade_uret(u, dugum->çocuklar[0]);
            LLVMValueRef anahtar = llvm_ifade_uret(u, dugum->çocuklar[1]);
            if (!sozluk || !anahtar) return NULL;

            LLVMSembolGirişi *oku_fn = llvm_sembol_bul(u, "_tr_sozluk_oku");
            if (!oku_fn) return NULL;

            LLVMValueRef aptr = LLVMBuildExtractValue(u->olusturucu, anahtar, 0, "s_ptr");
            LLVMValueRef alen = LLVMBuildExtractValue(u->olusturucu, anahtar, 1, "s_len");
            LLVMValueRef args[] = { sozluk, aptr, alen };
            return LLVMBuildCall2(u->olusturucu, oku_fn->tip,
                oku_fn->deger, args, 3, "sozluk_oku");
        }

        case DÜĞÜM_BOŞ_DEĞER: {
            /* boş (null) */
            return LLVMConstNull(
                LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0));
        }

        case DÜĞÜM_İFADE_BİLDİRİMİ: {
            /* İfade bildirimi - içindeki ifadeyi üret */
            if (dugum->çocuk_sayısı > 0 && dugum->çocuklar[0]) {
                return llvm_ifade_uret(u, dugum->çocuklar[0]);
            }
            return NULL;
        }

        case DÜĞÜM_ATAMA: {
            /* Atama: değişken = değer
             * Parser format: veri.tanimlayici.isim = variable name
             *                çocuklar[0] = value expression
             */
            if (dugum->çocuk_sayısı < 1) return NULL;

            char *isim = dugum->veri.tanimlayici.isim;
            Düğüm *deger = dugum->çocuklar[0];

            LLVMValueRef deger_ref = llvm_ifade_uret(u, deger);
            if (!deger_ref) return NULL;

            /* Hedef değişkeni bul */
            LLVMSembolGirişi *sembol = llvm_sembol_bul(u, isim);
            if (sembol && sembol->deger) {
                LLVMBuildStore(u->olusturucu, deger_ref, sembol->deger);
                return deger_ref;
            }
            return NULL;
        }

        case DÜĞÜM_ÜÇLÜ: {
            /* Üçlü ifade: koşul ? değer1 : değer2 */
            if (dugum->çocuk_sayısı < 3) return NULL;

            LLVMValueRef kosul = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (!kosul) return NULL;

            /* i1'e dönüştür */
            LLVMTypeRef kosul_tip = LLVMTypeOf(kosul);
            if (LLVMGetTypeKind(kosul_tip) != LLVMIntegerTypeKind ||
                LLVMGetIntTypeWidth(kosul_tip) != 1) {
                kosul = LLVMBuildICmp(u->olusturucu, LLVMIntNE, kosul,
                    LLVMConstInt(LLVMInt64TypeInContext(u->baglam), 0, 0), "uclu_kosul");
            }

            LLVMBasicBlockRef dogru_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "uclu_dogru");
            LLVMBasicBlockRef yanlis_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "uclu_yanlis");
            LLVMBasicBlockRef birlesim = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "uclu_son");

            LLVMBuildCondBr(u->olusturucu, kosul, dogru_blok, yanlis_blok);

            /* Doğru değer */
            LLVMPositionBuilderAtEnd(u->olusturucu, dogru_blok);
            LLVMValueRef dogru_val = llvm_ifade_uret(u, dugum->çocuklar[1]);
            if (!dogru_val) dogru_val = LLVMConstInt(LLVMInt64TypeInContext(u->baglam), 0, 0);
            LLVMBasicBlockRef dogru_son = LLVMGetInsertBlock(u->olusturucu);
            LLVMBuildBr(u->olusturucu, birlesim);

            /* Yanlış değer */
            LLVMPositionBuilderAtEnd(u->olusturucu, yanlis_blok);
            LLVMValueRef yanlis_val = llvm_ifade_uret(u, dugum->çocuklar[2]);
            if (!yanlis_val) yanlis_val = LLVMConstInt(LLVMInt64TypeInContext(u->baglam), 0, 0);
            LLVMBasicBlockRef yanlis_son = LLVMGetInsertBlock(u->olusturucu);
            LLVMBuildBr(u->olusturucu, birlesim);

            /* PHI node ile birleştir */
            LLVMPositionBuilderAtEnd(u->olusturucu, birlesim);
            LLVMTypeRef sonuc_tip = LLVMTypeOf(dogru_val);
            LLVMValueRef phi = LLVMBuildPhi(u->olusturucu, sonuc_tip, "uclu_sonuc");
            LLVMValueRef gelen_degerler[] = { dogru_val, yanlis_val };
            LLVMBasicBlockRef gelen_bloklar[] = { dogru_son, yanlis_son };
            LLVMAddIncoming(phi, gelen_degerler, gelen_bloklar, 2);
            return phi;
        }

        case DÜĞÜM_BORU: {
            /* a |> b  =>  b(a) */
            if (dugum->çocuk_sayısı < 2) return NULL;

            LLVMValueRef arg = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (!arg) return NULL;

            /* Sağ taraf fonksiyon ismi olmalı */
            if (dugum->çocuklar[1]->tur == DÜĞÜM_TANIMLAYICI) {
                char *fn_isim = dugum->çocuklar[1]->veri.tanimlayici.isim;
                LLVMSembolGirişi *fn = llvm_sembol_bul(u, fn_isim);
                if (fn) {
                    LLVMValueRef args[] = { arg };
                    return LLVMBuildCall2(u->olusturucu, fn->tip, fn->deger, args, 1, "boru_sonuc");
                }
                /* Doğrudan fonksiyon adı ile dene */
                LLVMValueRef fn_ref = LLVMGetNamedFunction(u->modul, fn_isim);
                if (fn_ref) {
                    LLVMTypeRef fn_tip = LLVMGlobalGetValueType(fn_ref);
                    LLVMValueRef args[] = { arg };
                    return LLVMBuildCall2(u->olusturucu, fn_tip, fn_ref, args, 1, "boru_sonuc");
                }
            }
            return NULL;
        }

        case DÜĞÜM_WALRUS: {
            /* isim := ifade → değeri ata ve döndür */
            LLVMValueRef deger = NULL;
            if (dugum->çocuk_sayısı > 0) {
                deger = llvm_ifade_uret(u, dugum->çocuklar[0]);
            }
            if (!deger) deger = LLVMConstInt(LLVMInt64TypeInContext(u->baglam), 0, 0);

            if (dugum->veri.tanimlayici.isim) {
                LLVMSembolGirişi *s = llvm_sembol_bul(u, dugum->veri.tanimlayici.isim);
                if (s) {
                    LLVMBuildStore(u->olusturucu, deger, s->deger);
                } else {
                    /* Yeni değişken oluştur */
                    LLVMTypeRef tip = LLVMTypeOf(deger);
                    LLVMValueRef alloca = LLVMBuildAlloca(u->olusturucu, tip, dugum->veri.tanimlayici.isim);
                    LLVMBuildStore(u->olusturucu, deger, alloca);
                    llvm_sembol_ekle(u, dugum->veri.tanimlayici.isim, alloca, tip, 0, 0);
                }
            }
            return deger;
        }

        case DÜĞÜM_DEMET: {
            /* Demet: {çocuklar[0], çocuklar[1]} → struct {i64, i64} */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
            LLVMTypeRef demet_elems[] = { i64, i64 };
            LLVMTypeRef demet_tipi = LLVMStructTypeInContext(u->baglam, demet_elems, 2, 0);
            LLVMValueRef sonuc = LLVMGetUndef(demet_tipi);

            if (dugum->çocuk_sayısı >= 1) {
                LLVMValueRef v0 = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (v0) {
                    if (LLVMGetTypeKind(LLVMTypeOf(v0)) != LLVMIntegerTypeKind ||
                        LLVMGetIntTypeWidth(LLVMTypeOf(v0)) != 64)
                        v0 = LLVMBuildZExt(u->olusturucu, v0, i64, "demet_ext0");
                    sonuc = LLVMBuildInsertValue(u->olusturucu, sonuc, v0, 0, "demet0");
                }
            }
            if (dugum->çocuk_sayısı >= 2) {
                LLVMValueRef v1 = llvm_ifade_uret(u, dugum->çocuklar[1]);
                if (v1) {
                    if (LLVMGetTypeKind(LLVMTypeOf(v1)) != LLVMIntegerTypeKind ||
                        LLVMGetIntTypeWidth(LLVMTypeOf(v1)) != 64)
                        v1 = LLVMBuildZExt(u->olusturucu, v1, i64, "demet_ext1");
                    sonuc = LLVMBuildInsertValue(u->olusturucu, sonuc, v1, 1, "demet1");
                }
            }
            return sonuc;
        }

        case DÜĞÜM_SONUÇ_OLUŞTUR: {
            /* Tamam(değer) veya Hata(değer)
             * x86 uyumu: tag rax'a (0=Tamam, 1=Hata), data rbx'e gider.
             * Değişkene atanınca sadece tag kaydedilir.
             * İki değer birden lazımsa _tr_sonuc_data global'i kullanılır. */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
            int varyant = dugum->veri.sonuç_seçenek.varyant;

            /* Veriyi hesapla ve global _tr_sonuc_data'ya kaydet */
            if (dugum->çocuk_sayısı > 0) {
                LLVMValueRef v = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (v) {
                    if (LLVMGetTypeKind(LLVMTypeOf(v)) != LLVMIntegerTypeKind ||
                        LLVMGetIntTypeWidth(LLVMTypeOf(v)) != 64)
                        v = LLVMBuildZExt(u->olusturucu, v, i64, "sonuc_ext");
                    LLVMValueRef data_g = LLVMGetNamedGlobal(u->modul, "_tr_sonuc_data");
                    if (!data_g) {
                        data_g = LLVMAddGlobal(u->modul, i64, "_tr_sonuc_data");
                        LLVMSetInitializer(data_g, LLVMConstInt(i64, 0, 0));
                        LLVMSetLinkage(data_g, LLVMInternalLinkage);
                    }
                    LLVMBuildStore(u->olusturucu, v, data_g);
                }
            }
            return LLVMConstInt(i64, varyant, 0);
        }

        case DÜĞÜM_SEÇENEK_OLUŞTUR: {
            /* Bir(değer) veya Hiç
             * tag: 0=Bir, 1=Hiç. Data → _tr_sonuc_data global. */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
            int varyant = dugum->veri.sonuç_seçenek.varyant;

            if (varyant == 0 && dugum->çocuk_sayısı > 0) {
                LLVMValueRef v = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (v) {
                    if (LLVMGetTypeKind(LLVMTypeOf(v)) != LLVMIntegerTypeKind ||
                        LLVMGetIntTypeWidth(LLVMTypeOf(v)) != 64)
                        v = LLVMBuildZExt(u->olusturucu, v, i64, "bir_ext");
                    LLVMValueRef data_g = LLVMGetNamedGlobal(u->modul, "_tr_sonuc_data");
                    if (!data_g) {
                        data_g = LLVMAddGlobal(u->modul, i64, "_tr_sonuc_data");
                        LLVMSetInitializer(data_g, LLVMConstInt(i64, 0, 0));
                        LLVMSetLinkage(data_g, LLVMInternalLinkage);
                    }
                    LLVMBuildStore(u->olusturucu, v, data_g);
                }
                return LLVMConstInt(i64, 0, 0);
            } else {
                return LLVMConstInt(i64, 1, 0);
            }
        }

        case DÜĞÜM_SORU_OP: {
            /* ifade? — Hata yayılımı operatörü
             * tag==0 ise _tr_sonuc_data'daki değeri döndür, aksi halde fonksiyondan dön */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);

            LLVMValueRef tag = NULL;
            if (dugum->çocuk_sayısı > 0) {
                tag = llvm_ifade_uret(u, dugum->çocuklar[0]);
            }
            if (!tag) return LLVMConstInt(i64, 0, 0);
            if (LLVMGetTypeKind(LLVMTypeOf(tag)) != LLVMIntegerTypeKind ||
                LLVMGetIntTypeWidth(LLVMTypeOf(tag)) != 64)
                tag = LLVMBuildZExt(u->olusturucu, tag, i64, "soru_ext");

            LLVMValueRef cmp = LLVMBuildICmp(u->olusturucu, LLVMIntEQ, tag,
                LLVMConstInt(i64, 0, 0), "soru_cmp");

            LLVMBasicBlockRef basari_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "soru_basari");
            LLVMBasicBlockRef hata_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "soru_hata");
            LLVMBasicBlockRef devam_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "soru_devam");

            LLVMBuildCondBr(u->olusturucu, cmp, basari_blok, hata_blok);

            /* Hata: fonksiyondan dön */
            LLVMPositionBuilderAtEnd(u->olusturucu, hata_blok);
            LLVMTypeRef fn_dönüş = LLVMGetReturnType(
                LLVMGetElementType(LLVMTypeOf(u->mevcut_islev)));
            if (LLVMGetTypeKind(fn_dönüş) == LLVMVoidTypeKind) {
                LLVMBuildRetVoid(u->olusturucu);
            } else {
                LLVMBuildRet(u->olusturucu, LLVMConstNull(fn_dönüş));
            }

            /* Başarı: _tr_sonuc_data'dan veriyi oku */
            LLVMPositionBuilderAtEnd(u->olusturucu, basari_blok);
            LLVMValueRef data_g = LLVMGetNamedGlobal(u->modul, "_tr_sonuc_data");
            LLVMValueRef data_val;
            if (data_g) {
                data_val = LLVMBuildLoad2(u->olusturucu, i64, data_g, "soru_data");
            } else {
                data_val = LLVMConstInt(i64, 0, 0);
            }
            LLVMBuildBr(u->olusturucu, devam_blok);

            LLVMPositionBuilderAtEnd(u->olusturucu, devam_blok);
            LLVMValueRef phi = LLVMBuildPhi(u->olusturucu, i64, "soru_sonuc");
            LLVMValueRef phi_vals[] = { data_val };
            LLVMBasicBlockRef phi_bloklar[] = { basari_blok };
            LLVMAddIncoming(phi, phi_vals, phi_bloklar, 1);
            return phi;
        }

        case DÜĞÜM_KÜME_DEĞERİ: {
            /* Küme literal: küme{e1, e2, ...} */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);

            /* _tr_kume_yeni() çağır */
            LLVMValueRef kume_fn = LLVMGetNamedFunction(u->modul, "_tr_kume_yeni");
            if (!kume_fn) {
                LLVMTypeRef ft = LLVMFunctionType(i64, NULL, 0, 0);
                kume_fn = LLVMAddFunction(u->modul, "_tr_kume_yeni", ft);
            }
            LLVMTypeRef kume_yeni_ft = LLVMFunctionType(i64, NULL, 0, 0);
            LLVMValueRef kume = LLVMBuildCall2(u->olusturucu,
                kume_yeni_ft, kume_fn, NULL, 0, "kume");

            /* Her eleman için _tr_kume_ekle(kume, deger) */
            LLVMValueRef ekle_fn = LLVMGetNamedFunction(u->modul, "_tr_kume_ekle");
            if (!ekle_fn) {
                LLVMTypeRef pt[] = { i64, i64 };
                LLVMTypeRef ft = LLVMFunctionType(LLVMVoidTypeInContext(u->baglam), pt, 2, 0);
                ekle_fn = LLVMAddFunction(u->modul, "_tr_kume_ekle", ft);
            }
            LLVMTypeRef ekle_pt[] = { i64, i64 };
            LLVMTypeRef ekle_ft = LLVMFunctionType(
                LLVMVoidTypeInContext(u->baglam), ekle_pt, 2, 0);
            for (int i = 0; i < dugum->çocuk_sayısı; i++) {
                LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[i]);
                if (deger) {
                    if (LLVMGetTypeKind(LLVMTypeOf(deger)) != LLVMIntegerTypeKind ||
                        LLVMGetIntTypeWidth(LLVMTypeOf(deger)) != 64)
                        deger = LLVMBuildZExt(u->olusturucu, deger, i64, "kume_ext");
                    LLVMValueRef args[] = { kume, deger };
                    LLVMBuildCall2(u->olusturucu, ekle_ft, ekle_fn, args, 2, "");
                }
            }
            return kume;
        }

        case DÜĞÜM_LİSTE_ÜRETİMİ: {
            /* [ifade her x için kaynak eğer koşul]
             * çocuklar[0]=ifade, [1]=kaynak, [2]=filtre(opsiyonel) */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
            LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);

            /* Kaynak diziyi değerlendir → struct {ptr, count} */
            LLVMValueRef kaynak = llvm_ifade_uret(u, dugum->çocuklar[1]);
            if (!kaynak) return NULL;
            LLVMValueRef kaynak_ptr = LLVMBuildExtractValue(u->olusturucu, kaynak, 0, "lu_kptr");
            LLVMValueRef kaynak_count = LLVMBuildExtractValue(u->olusturucu, kaynak, 1, "lu_kcount");

            /* Sonuç dizisi oluştur: _tr_nesne_olustur(1, count*8) */
            LLVMValueRef nesne_fn = LLVMGetNamedFunction(u->modul, "_tr_nesne_olustur");
            LLVMTypeRef nesne_pt[] = { i64, i64 };
            LLVMTypeRef nesne_ft = LLVMFunctionType(i8_ptr, nesne_pt, 2, 0);
            LLVMValueRef boyut = LLVMBuildMul(u->olusturucu, kaynak_count,
                LLVMConstInt(i64, 8, 0), "lu_boyut");
            /* En az 8 byte */
            LLVMValueRef min_cmp = LLVMBuildICmp(u->olusturucu, LLVMIntSLT, boyut,
                LLVMConstInt(i64, 8, 0), "lu_min_cmp");
            boyut = LLVMBuildSelect(u->olusturucu, min_cmp,
                LLVMConstInt(i64, 8, 0), boyut, "lu_boyut_min");
            LLVMValueRef nesne_args[] = { LLVMConstInt(i64, 1, 0), boyut };
            LLVMValueRef sonuc_raw = LLVMBuildCall2(u->olusturucu,
                nesne_ft, nesne_fn, nesne_args, 2, "lu_sonuc");
            LLVMValueRef sonuc_ptr = LLVMBuildBitCast(u->olusturucu, sonuc_raw,
                LLVMPointerType(i64, 0), "lu_sonuc_ptr");

            /* Alloca: kaynak index ve sonuç index */
            LLVMValueRef kaynak_idx = LLVMBuildAlloca(u->olusturucu, i64, "lu_kidx");
            LLVMValueRef sonuc_idx = LLVMBuildAlloca(u->olusturucu, i64, "lu_sidx");
            LLVMBuildStore(u->olusturucu, LLVMConstInt(i64, 0, 0), kaynak_idx);
            LLVMBuildStore(u->olusturucu, LLVMConstInt(i64, 0, 0), sonuc_idx);

            /* Döngü blokları */
            LLVMBasicBlockRef baslik = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "lu_baslik");
            LLVMBasicBlockRef govde = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "lu_govde");
            LLVMBasicBlockRef atla = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "lu_atla");
            LLVMBasicBlockRef son = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "lu_son");

            LLVMBuildBr(u->olusturucu, baslik);

            /* Başlık: idx < count? */
            LLVMPositionBuilderAtEnd(u->olusturucu, baslik);
            LLVMValueRef idx_val = LLVMBuildLoad2(u->olusturucu, i64, kaynak_idx, "lu_idx");
            LLVMValueRef cmp = LLVMBuildICmp(u->olusturucu, LLVMIntSLT,
                idx_val, kaynak_count, "lu_cmp");
            LLVMBuildCondBr(u->olusturucu, cmp, govde, son);

            /* Gövde: döngü değişkenini ayarla */
            LLVMPositionBuilderAtEnd(u->olusturucu, govde);
            llvm_kapsam_gir(u);
            LLVMValueRef elem_ptr = LLVMBuildGEP2(u->olusturucu, i64, kaynak_ptr,
                &idx_val, 1, "lu_elem_ptr");
            LLVMValueRef elem_val = LLVMBuildLoad2(u->olusturucu, i64, elem_ptr, "lu_elem");

            LLVMValueRef dongu_var = LLVMBuildAlloca(u->olusturucu, i64,
                dugum->veri.dongu.isim);
            LLVMBuildStore(u->olusturucu, elem_val, dongu_var);
            llvm_sembol_ekle(u, dugum->veri.dongu.isim, dongu_var, i64, 0, 0);

            /* Filtre kontrolü */
            LLVMBasicBlockRef ekle_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "lu_ekle");
            if (dugum->çocuk_sayısı > 2) {
                LLVMValueRef filtre = llvm_ifade_uret(u, dugum->çocuklar[2]);
                if (filtre) {
                    if (LLVMGetTypeKind(LLVMTypeOf(filtre)) == LLVMIntegerTypeKind &&
                        LLVMGetIntTypeWidth(LLVMTypeOf(filtre)) == 1)
                        filtre = LLVMBuildZExt(u->olusturucu, filtre, i64, "lu_fext");
                    LLVMValueRef fcmp = LLVMBuildICmp(u->olusturucu, LLVMIntNE, filtre,
                        LLVMConstInt(i64, 0, 0), "lu_fcmp");
                    LLVMBuildCondBr(u->olusturucu, fcmp, ekle_blok, atla);
                } else {
                    LLVMBuildBr(u->olusturucu, ekle_blok);
                }
            } else {
                LLVMBuildBr(u->olusturucu, ekle_blok);
            }

            /* Eleman ekle */
            LLVMPositionBuilderAtEnd(u->olusturucu, ekle_blok);
            LLVMValueRef ifade_val = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (ifade_val) {
                if (LLVMGetTypeKind(LLVMTypeOf(ifade_val)) != LLVMIntegerTypeKind ||
                    LLVMGetIntTypeWidth(LLVMTypeOf(ifade_val)) != 64)
                    ifade_val = LLVMBuildZExt(u->olusturucu, ifade_val, i64, "lu_vext");
                LLVMValueRef sidx = LLVMBuildLoad2(u->olusturucu, i64, sonuc_idx, "lu_sidx_v");
                LLVMValueRef dest = LLVMBuildGEP2(u->olusturucu, i64, sonuc_ptr,
                    &sidx, 1, "lu_dest");
                LLVMBuildStore(u->olusturucu, ifade_val, dest);
                LLVMValueRef sidx_inc = LLVMBuildAdd(u->olusturucu, sidx,
                    LLVMConstInt(i64, 1, 0), "lu_sidx_inc");
                LLVMBuildStore(u->olusturucu, sidx_inc, sonuc_idx);
            }
            LLVMBuildBr(u->olusturucu, atla);

            /* Atla: idx++ */
            LLVMPositionBuilderAtEnd(u->olusturucu, atla);
            LLVMValueRef idx_cur = LLVMBuildLoad2(u->olusturucu, i64, kaynak_idx, "lu_idx_cur");
            LLVMValueRef idx_inc = LLVMBuildAdd(u->olusturucu, idx_cur,
                LLVMConstInt(i64, 1, 0), "lu_idx_inc");
            LLVMBuildStore(u->olusturucu, idx_inc, kaynak_idx);
            llvm_kapsam_cik(u);
            LLVMBuildBr(u->olusturucu, baslik);

            /* Son: sonuç struct {ptr, count} */
            LLVMPositionBuilderAtEnd(u->olusturucu, son);
            LLVMValueRef son_count = LLVMBuildLoad2(u->olusturucu, i64, sonuc_idx, "lu_son_count");
            LLVMTypeRef dizi_elems[] = { LLVMPointerType(i64, 0), i64 };
            LLVMTypeRef dizi_tipi = LLVMStructTypeInContext(u->baglam, dizi_elems, 2, 0);
            LLVMValueRef dizi = LLVMGetUndef(dizi_tipi);
            dizi = LLVMBuildInsertValue(u->olusturucu, dizi, sonuc_ptr, 0, "lu_dizi_ptr");
            dizi = LLVMBuildInsertValue(u->olusturucu, dizi, son_count, 1, "lu_dizi_count");
            return dizi;
        }

        case DÜĞÜM_SÖZLÜK_ÜRETİMİ: {
            /* {k: v her x için kaynak eğer koşul}
             * çocuklar[0]=key_expr, [1]=val_expr, [2]=kaynak, [3]=filtre(opsiyonel) */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
            LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);

            /* Kaynak diziyi değerlendir */
            LLVMValueRef kaynak = llvm_ifade_uret(u, dugum->çocuklar[2]);
            if (!kaynak) return NULL;
            LLVMValueRef kaynak_ptr = LLVMBuildExtractValue(u->olusturucu, kaynak, 0, "su_kptr");
            LLVMValueRef kaynak_count = LLVMBuildExtractValue(u->olusturucu, kaynak, 1, "su_kcount");

            /* Yeni sözlük oluştur */
            LLVMValueRef sozluk_fn = LLVMGetNamedFunction(u->modul, "_tr_sozluk_yeni");
            LLVMTypeRef sozluk_ft = LLVMFunctionType(i64, NULL, 0, 0);
            if (!sozluk_fn) {
                sozluk_fn = LLVMAddFunction(u->modul, "_tr_sozluk_yeni", sozluk_ft);
            }
            LLVMValueRef sozluk = LLVMBuildCall2(u->olusturucu,
                sozluk_ft, sozluk_fn, NULL, 0, "su_sozluk");

            /* Alloca: kaynak index */
            LLVMValueRef kaynak_idx = LLVMBuildAlloca(u->olusturucu, i64, "su_kidx");
            LLVMBuildStore(u->olusturucu, LLVMConstInt(i64, 0, 0), kaynak_idx);

            /* Döngü blokları */
            LLVMBasicBlockRef baslik = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "su_baslik");
            LLVMBasicBlockRef govde = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "su_govde");
            LLVMBasicBlockRef atla = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "su_atla");
            LLVMBasicBlockRef son = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "su_son");

            LLVMBuildBr(u->olusturucu, baslik);

            /* Başlık: idx < count? */
            LLVMPositionBuilderAtEnd(u->olusturucu, baslik);
            LLVMValueRef idx_val = LLVMBuildLoad2(u->olusturucu, i64, kaynak_idx, "su_idx");
            LLVMValueRef cmp = LLVMBuildICmp(u->olusturucu, LLVMIntSLT,
                idx_val, kaynak_count, "su_cmp");
            LLVMBuildCondBr(u->olusturucu, cmp, govde, son);

            /* Gövde */
            LLVMPositionBuilderAtEnd(u->olusturucu, govde);
            llvm_kapsam_gir(u);
            LLVMValueRef elem_ptr = LLVMBuildGEP2(u->olusturucu, i64, kaynak_ptr,
                &idx_val, 1, "su_elem_ptr");
            LLVMValueRef elem_val = LLVMBuildLoad2(u->olusturucu, i64, elem_ptr, "su_elem");

            LLVMValueRef dongu_var = LLVMBuildAlloca(u->olusturucu, i64,
                dugum->veri.dongu.isim);
            LLVMBuildStore(u->olusturucu, elem_val, dongu_var);
            llvm_sembol_ekle(u, dugum->veri.dongu.isim, dongu_var, i64, 0, 0);

            /* Filtre */
            LLVMBasicBlockRef ekle_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "su_ekle");
            if (dugum->çocuk_sayısı > 3) {
                LLVMValueRef filtre = llvm_ifade_uret(u, dugum->çocuklar[3]);
                if (filtre) {
                    if (LLVMGetTypeKind(LLVMTypeOf(filtre)) == LLVMIntegerTypeKind &&
                        LLVMGetIntTypeWidth(LLVMTypeOf(filtre)) == 1)
                        filtre = LLVMBuildZExt(u->olusturucu, filtre, i64, "su_fext");
                    LLVMValueRef fcmp = LLVMBuildICmp(u->olusturucu, LLVMIntNE, filtre,
                        LLVMConstInt(i64, 0, 0), "su_fcmp");
                    LLVMBuildCondBr(u->olusturucu, fcmp, ekle_blok, atla);
                } else {
                    LLVMBuildBr(u->olusturucu, ekle_blok);
                }
            } else {
                LLVMBuildBr(u->olusturucu, ekle_blok);
            }

            /* Ekle: anahtar ve değer üret, _tr_sozluk_ekle çağır */
            LLVMPositionBuilderAtEnd(u->olusturucu, ekle_blok);
            LLVMValueRef anahtar = llvm_ifade_uret(u, dugum->çocuklar[0]);
            LLVMValueRef anahtar_ptr_v, anahtar_len;
            if (anahtar && LLVMGetTypeKind(LLVMTypeOf(anahtar)) == LLVMStructTypeKind) {
                anahtar_ptr_v = LLVMBuildExtractValue(u->olusturucu, anahtar, 0, "su_aptr");
                anahtar_len = LLVMBuildExtractValue(u->olusturucu, anahtar, 1, "su_alen");
            } else {
                anahtar_ptr_v = LLVMConstNull(i8_ptr);
                anahtar_len = LLVMConstInt(i64, 0, 0);
            }
            LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[1]);
            if (!deger) deger = LLVMConstInt(i64, 0, 0);
            if (LLVMGetTypeKind(LLVMTypeOf(deger)) != LLVMIntegerTypeKind ||
                LLVMGetIntTypeWidth(LLVMTypeOf(deger)) != 64)
                deger = LLVMBuildZExt(u->olusturucu, deger, i64, "su_vext");

            LLVMValueRef ekle_fn = LLVMGetNamedFunction(u->modul, "_tr_sozluk_ekle");
            LLVMTypeRef ekle_pt[] = { i64, i8_ptr, i64, i64 };
            LLVMTypeRef ekle_ft = LLVMFunctionType(i64, ekle_pt, 4, 0);
            if (!ekle_fn) {
                ekle_fn = LLVMAddFunction(u->modul, "_tr_sozluk_ekle", ekle_ft);
            }
            LLVMValueRef ekle_args[] = { sozluk, anahtar_ptr_v, anahtar_len, deger };
            LLVMBuildCall2(u->olusturucu, ekle_ft, ekle_fn, ekle_args, 4, "su_ekle");
            LLVMBuildBr(u->olusturucu, atla);

            /* Atla: idx++ */
            LLVMPositionBuilderAtEnd(u->olusturucu, atla);
            LLVMValueRef idx_cur = LLVMBuildLoad2(u->olusturucu, i64, kaynak_idx, "su_idx_cur");
            LLVMValueRef idx_inc = LLVMBuildAdd(u->olusturucu, idx_cur,
                LLVMConstInt(i64, 1, 0), "su_idx_inc");
            LLVMBuildStore(u->olusturucu, idx_inc, kaynak_idx);
            llvm_kapsam_cik(u);
            LLVMBuildBr(u->olusturucu, baslik);

            /* Son */
            LLVMPositionBuilderAtEnd(u->olusturucu, son);
            return sozluk;
        }

        case DÜĞÜM_BEKLE: {
            /* bekle ifade → async task oluştur ve bekle */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);

            if (dugum->çocuk_sayısı > 0) {
                Düğüm *cagri = dugum->çocuklar[0];
                if (cagri->tur == DÜĞÜM_ÇAĞRI && cagri->veri.tanimlayici.isim) {
                    int arg_sayisi = cagri->çocuk_sayısı;

                    /* Argümanları hesapla */
                    LLVMValueRef arg_vals[4] = {
                        LLVMConstInt(i64, 0, 0), LLVMConstInt(i64, 0, 0),
                        LLVMConstInt(i64, 0, 0), LLVMConstInt(i64, 0, 0)
                    };
                    for (int i = 0; i < arg_sayisi && i < 4; i++) {
                        LLVMValueRef v = llvm_ifade_uret(u, cagri->çocuklar[i]);
                        if (v) {
                            if (LLVMGetTypeKind(LLVMTypeOf(v)) != LLVMIntegerTypeKind ||
                                LLVMGetIntTypeWidth(LLVMTypeOf(v)) != 64)
                                v = LLVMBuildZExt(u->olusturucu, v, i64, "bekle_ext");
                            arg_vals[i] = v;
                        }
                    }

                    /* Fonksiyon pointer'ı */
                    LLVMValueRef fn = LLVMGetNamedFunction(u->modul,
                        cagri->veri.tanimlayici.isim);
                    LLVMValueRef fn_ptr;
                    if (fn) {
                        fn_ptr = LLVMBuildPtrToInt(u->olusturucu, fn, i64, "bekle_fn");
                    } else {
                        fn_ptr = LLVMConstInt(i64, 0, 0);
                    }

                    /* _tr_async_olustur(fn, arg_count, a0, a1, a2, a3) */
                    LLVMValueRef olustur_fn = LLVMGetNamedFunction(u->modul, "_tr_async_olustur");
                    if (!olustur_fn) {
                        LLVMTypeRef pt[] = { i64, i64, i64, i64, i64, i64 };
                        LLVMTypeRef ft = LLVMFunctionType(i64, pt, 6, 0);
                        olustur_fn = LLVMAddFunction(u->modul, "_tr_async_olustur", ft);
                    }
                    LLVMTypeRef olustur_pt[] = { i64, i64, i64, i64, i64, i64 };
                    LLVMTypeRef olustur_ft = LLVMFunctionType(i64, olustur_pt, 6, 0);
                    LLVMValueRef olustur_args[] = {
                        fn_ptr, LLVMConstInt(i64, arg_sayisi, 0),
                        arg_vals[0], arg_vals[1], arg_vals[2], arg_vals[3]
                    };
                    LLVMValueRef handle = LLVMBuildCall2(u->olusturucu,
                        olustur_ft, olustur_fn, olustur_args, 6, "bekle_handle");

                    /* _tr_async_bekle(handle) */
                    LLVMValueRef bekle_fn = LLVMGetNamedFunction(u->modul, "_tr_async_bekle");
                    if (!bekle_fn) {
                        LLVMTypeRef pt[] = { i64 };
                        LLVMTypeRef ft = LLVMFunctionType(i64, pt, 1, 0);
                        bekle_fn = LLVMAddFunction(u->modul, "_tr_async_bekle", ft);
                    }
                    LLVMTypeRef bekle_pt[] = { i64 };
                    LLVMTypeRef bekle_ft = LLVMFunctionType(i64, bekle_pt, 1, 0);
                    LLVMValueRef bekle_args[] = { handle };
                    return LLVMBuildCall2(u->olusturucu,
                        bekle_ft, bekle_fn, bekle_args, 1, "bekle_sonuc");
                } else {
                    /* Fallback: ifadeyi doğrudan çalıştır */
                    return llvm_ifade_uret(u, dugum->çocuklar[0]);
                }
            }
            return LLVMConstInt(i64, 0, 0);
        }

        case DÜĞÜM_ARAYÜZ:
        case DÜĞÜM_TİP_TANIMI:
        case DÜĞÜM_SAYIM:
            /* Derleme zamanı yapıları — codegen gerektirmez */
            return LLVMConstInt(LLVMInt64TypeInContext(u->baglam), 0, 0);

        case DÜĞÜM_LAMBDA: {
            /* Lambda (anonim işlev): fonksiyonu üret, adresi i64 olarak döndür */
            char *lambda_isim = dugum->veri.islev.isim;
            char *donus_tipi_adi = dugum->veri.islev.dönüş_tipi;
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
            LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);

            /* Parametre bilgilerini topla */
            int param_sayisi = 0;
            LLVMTypeRef *param_tipleri = NULL;
            char **param_isimleri = NULL;

            if (dugum->çocuk_sayısı > 0 && dugum->çocuklar[0]) {
                Düğüm *params = dugum->çocuklar[0];
                param_sayisi = params->çocuk_sayısı;
                if (param_sayisi > 0) {
                    param_tipleri = (LLVMTypeRef *)arena_ayir(
                        u->arena, sizeof(LLVMTypeRef) * param_sayisi);
                    param_isimleri = (char **)arena_ayir(
                        u->arena, sizeof(char *) * param_sayisi);
                    for (int i = 0; i < param_sayisi; i++) {
                        Düğüm *param = params->çocuklar[i];
                        param_isimleri[i] = param->veri.değişken.isim;
                        param_tipleri[i] = llvm_tip_dönüştür(u, param->veri.değişken.tip);
                    }
                }
            }

            /* Yakalanan (captured) değişkenleri tespit et */
            char **yak_isimler = (char **)arena_ayir(u->arena, 32 * sizeof(char *));
            int yak_sayisi = 0;
            if (dugum->çocuk_sayısı > 1 && dugum->çocuklar[1]) {
                llvm_kapanis_yakala_topla(dugum->çocuklar[1], param_isimleri,
                    param_sayisi, u, &yak_isimler, &yak_sayisi);
            }

            /* Yakalanan değişkenlerin değerlerini ŞİMDİ oku (kontekst değişmeden) */
            LLVMValueRef *yakalanan_degerler = NULL;
            if (yak_sayisi > 0) {
                yakalanan_degerler = (LLVMValueRef *)arena_ayir(
                    u->arena, yak_sayisi * sizeof(LLVMValueRef));
                for (int ci = 0; ci < yak_sayisi; ci++) {
                    LLVMSembolGirişi *s = llvm_sembol_bul(u, yak_isimler[ci]);
                    if (s) {
                        yakalanan_degerler[ci] = LLVMBuildLoad2(u->olusturucu,
                            i64, s->deger, "yak_oku");
                    } else {
                        yakalanan_degerler[ci] = LLVMConstInt(i64, 0, 0);
                    }
                }
            }

            /* Dönüş tipi */
            LLVMTypeRef dönüş_tipi;
            if (donus_tipi_adi) {
                dönüş_tipi = llvm_tip_dönüştür(u, donus_tipi_adi);
            } else {
                dönüş_tipi = LLVMVoidTypeInContext(u->baglam);
            }

            /* LLVM fonksiyonu oluştur */
            LLVMTypeRef islev_tipi = LLVMFunctionType(
                dönüş_tipi, param_tipleri, param_sayisi, 0);
            LLVMValueRef lambda_fn = LLVMAddFunction(u->modul, lambda_isim, islev_tipi);
            LLVMSetLinkage(lambda_fn, LLVMPrivateLinkage);

            /* Mevcut durumu kaydet */
            LLVMValueRef onceki_islev = u->mevcut_islev;
            LLVMBasicBlockRef onceki_blok = LLVMGetInsertBlock(u->olusturucu);

            /* Lambda giriş bloğu */
            LLVMBasicBlockRef giris_blok = LLVMAppendBasicBlockInContext(
                u->baglam, lambda_fn, "giris");
            LLVMPositionBuilderAtEnd(u->olusturucu, giris_blok);
            u->mevcut_islev = lambda_fn;

            /* Yeni kapsam */
            llvm_kapsam_gir(u);

            /* Yakalanan değişkenleri ortamdan yükle */
            if (yak_sayisi > 0) {
                LLVMValueRef ortam_global = LLVMGetNamedGlobal(u->modul, "_tr_kapanis_ortam");
                LLVMTypeRef i64_ptr = LLVMPointerType(i64, 0);
                LLVMValueRef ortam_ptr = LLVMBuildLoad2(u->olusturucu,
                    i64_ptr, ortam_global, "ortam");

                for (int ci = 0; ci < yak_sayisi; ci++) {
                    LLVMValueRef idx = LLVMConstInt(i64, ci + 2, 0);
                    LLVMValueRef elem_ptr = LLVMBuildGEP2(u->olusturucu,
                        i64, ortam_ptr, &idx, 1, "yak_ptr");
                    LLVMValueRef val = LLVMBuildLoad2(u->olusturucu,
                        i64, elem_ptr, "yak_val");

                    LLVMValueRef alloca = LLVMBuildAlloca(u->olusturucu,
                        i64, yak_isimler[ci]);
                    LLVMBuildStore(u->olusturucu, val, alloca);
                    llvm_sembol_ekle(u, yak_isimler[ci], alloca, i64, 0, 0);
                }
            }

            /* Parametreleri yerel değişkenlere kopyala */
            for (int i = 0; i < param_sayisi; i++) {
                LLVMValueRef param_deger = LLVMGetParam(lambda_fn, i);
                LLVMSetValueName(param_deger, param_isimleri[i]);

                LLVMValueRef alloca = LLVMBuildAlloca(
                    u->olusturucu, param_tipleri[i], param_isimleri[i]);
                LLVMBuildStore(u->olusturucu, param_deger, alloca);
                llvm_sembol_ekle(u, param_isimleri[i], alloca, param_tipleri[i], 1, 0);
            }

            /* Gövdeyi üret */
            if (dugum->çocuk_sayısı > 1 && dugum->çocuklar[1]) {
                llvm_blok_uret(u, dugum->çocuklar[1]);
            }

            /* Varsayılan terminator */
            LLVMBasicBlockRef mevcut_blok = LLVMGetInsertBlock(u->olusturucu);
            if (!LLVMGetBasicBlockTerminator(mevcut_blok)) {
                if (LLVMGetTypeKind(dönüş_tipi) == LLVMVoidTypeKind) {
                    LLVMBuildRetVoid(u->olusturucu);
                } else {
                    LLVMBuildRet(u->olusturucu, LLVMConstNull(dönüş_tipi));
                }
            }

            /* Kapsamdan çık */
            llvm_kapsam_cik(u);

            /* Eski durumu geri yükle */
            u->mevcut_islev = onceki_islev;
            LLVMPositionBuilderAtEnd(u->olusturucu, onceki_blok);

            /* Kapanış nesnesi oluştur veya sadece adres döndür */
            if (yak_sayisi > 0) {
                /* _tr_nesne_olustur(5, (yak_sayisi+2)*8) */
                LLVMValueRef nesne_fn = LLVMGetNamedFunction(u->modul, "_tr_nesne_olustur");
                LLVMTypeRef nesne_pt[] = { i64, i64 };
                LLVMTypeRef nesne_ft = LLVMFunctionType(i8_ptr, nesne_pt, 2, 0);
                LLVMValueRef nesne_args[] = {
                    LLVMConstInt(i64, 5, 0),  /* NESNE_TIP_KAPANIS */
                    LLVMConstInt(i64, (yak_sayisi + 2) * 8, 0)
                };
                LLVMValueRef ortam = LLVMBuildCall2(u->olusturucu,
                    nesne_ft, nesne_fn, nesne_args, 2, "kapanis_ortam");

                /* i8* → i64* */
                LLVMTypeRef i64_ptr = LLVMPointerType(i64, 0);
                LLVMValueRef ortam_i64 = LLVMBuildBitCast(u->olusturucu,
                    ortam, i64_ptr, "ortam_i64");

                /* [0] = fn_ptr */
                LLVMValueRef fn_as_i64 = LLVMBuildPtrToInt(u->olusturucu,
                    lambda_fn, i64, "fn_ptr");
                LLVMValueRef idx0 = LLVMConstInt(i64, 0, 0);
                LLVMValueRef slot0 = LLVMBuildGEP2(u->olusturucu,
                    i64, ortam_i64, &idx0, 1, "slot0");
                LLVMBuildStore(u->olusturucu, fn_as_i64, slot0);

                /* [1] = count */
                LLVMValueRef idx1 = LLVMConstInt(i64, 1, 0);
                LLVMValueRef slot1 = LLVMBuildGEP2(u->olusturucu,
                    i64, ortam_i64, &idx1, 1, "slot1");
                LLVMBuildStore(u->olusturucu, LLVMConstInt(i64, yak_sayisi, 0), slot1);

                /* [2..] = yakalanan değerler */
                for (int ci = 0; ci < yak_sayisi; ci++) {
                    LLVMValueRef idx = LLVMConstInt(i64, ci + 2, 0);
                    LLVMValueRef slot = LLVMBuildGEP2(u->olusturucu,
                        i64, ortam_i64, &idx, 1, "slot");
                    LLVMBuildStore(u->olusturucu, yakalanan_degerler[ci], slot);
                }

                /* Global _tr_kapanis_ortam'a kaydet */
                LLVMValueRef ortam_global = LLVMGetNamedGlobal(u->modul, "_tr_kapanis_ortam");
                LLVMBuildStore(u->olusturucu, ortam_i64, ortam_global);

                /* fn_ptr döndür (i64) */
                return fn_as_i64;
            } else {
                /* Yakalama yok: sadece fonksiyon adresi */
                return LLVMBuildPtrToInt(u->olusturucu, lambda_fn, i64, "lambda_ptr");
            }
        }

        default:
            fprintf(stderr, "Hata: Desteklenmeyen ifade türü: %d\n", dugum->tur);
            return NULL;
    }
}

/*
 * llvm_metin_sabiti_oluştur - Global string sabiti oluştur
 */
LLVMValueRef llvm_metin_sabiti_oluştur(LLVMÜretici *u, const char *metin) {
    /* Önce cache'te ara */
    LLVMMetinSabiti *mevcut = u->metin_sabitleri;
    while (mevcut) {
        if (strcmp(mevcut->metin, metin) == 0) {
            return mevcut->global_ref;
        }
        mevcut = mevcut->sonraki;
    }

    /* Escape sequence'leri işle: \" → ", \\ → \, \n → newline, \t → tab, \r → CR, \0 → NUL */
    int ham_uzunluk = strlen(metin);
    char *islenmis = (char *)arena_ayir(u->arena, ham_uzunluk + 1);
    int uzunluk = 0;
    for (int i = 0; i < ham_uzunluk; i++) {
        if (metin[i] == '\\' && i + 1 < ham_uzunluk) {
            switch (metin[i + 1]) {
                case 'n':  islenmis[uzunluk++] = '\n'; i++; break;
                case 't':  islenmis[uzunluk++] = '\t'; i++; break;
                case 'r':  islenmis[uzunluk++] = '\r'; i++; break;
                case '\\': islenmis[uzunluk++] = '\\'; i++; break;
                case '"':  islenmis[uzunluk++] = '"';  i++; break;
                case '0':  islenmis[uzunluk++] = '\0'; i++; break;
                default:   islenmis[uzunluk++] = metin[i]; break;
            }
        } else {
            islenmis[uzunluk++] = metin[i];
        }
    }
    islenmis[uzunluk] = '\0';

    /* Yeni global string oluştur */
    LLVMValueRef str_sabit = LLVMConstStringInContext(
        u->baglam, islenmis, uzunluk, 0);

    char isim[64];
    snprintf(isim, sizeof(isim), ".metin_%d", u->metin_sayaci++);

    LLVMTypeRef dizi_tipi = LLVMArrayType(LLVMInt8TypeInContext(u->baglam), uzunluk + 1);
    LLVMValueRef global = LLVMAddGlobal(u->modul, dizi_tipi, isim);

    LLVMSetInitializer(global, str_sabit);
    LLVMSetGlobalConstant(global, 1);
    LLVMSetLinkage(global, LLVMPrivateLinkage);
    LLVMSetUnnamedAddress(global, LLVMGlobalUnnamedAddr);

    /* i8* pointer'ı al */
    LLVMValueRef sifir = LLVMConstInt(LLVMInt64TypeInContext(u->baglam), 0, 0);
    LLVMValueRef indices[] = { sifir, sifir };
    LLVMValueRef str_ptr = LLVMConstGEP2(dizi_tipi, global, indices, 2);

    /* Metin struct { i8*, i64 } sabiti oluştur */
    LLVMValueRef uzunluk_val = LLVMConstInt(
        LLVMInt64TypeInContext(u->baglam), uzunluk, 0);
    LLVMValueRef alanlar[] = { str_ptr, uzunluk_val };
    LLVMValueRef metin_struct = LLVMConstStructInContext(
        u->baglam, alanlar, 2, 0);

    /* Cache'e ekle */
    LLVMMetinSabiti *yeni = (LLVMMetinSabiti *)arena_ayir(
        u->arena, sizeof(LLVMMetinSabiti));
    yeni->metin = arena_strdup(u->arena, metin);
    yeni->global_ref = metin_struct;
    yeni->sonraki = u->metin_sabitleri;
    u->metin_sabitleri = yeni;

    return metin_struct;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 6: DEYİM ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_deyim_uret - Deyim üret
 */
void llvm_deyim_uret(LLVMÜretici *u, Düğüm *dugum) {
    if (!dugum) return;

    /* Debug: satır bilgisini ayarla */
    if (u->hata_ayiklama && dugum->satir > 0) {
        llvm_debug_satir_ayarla(u, dugum->satir, dugum->sutun);
    }

    switch (dugum->tur) {
        case DÜĞÜM_DEĞİŞKEN: {
            /* Yerel değişken tanımı */
            char *isim = dugum->veri.değişken.isim;
            char *tip_adı = dugum->veri.değişken.tip;
            LLVMTypeRef tip = llvm_tip_dönüştür(u, tip_adı);

            /* Çoklu dönüş: ikinci değişken (_tr_coklu_donus'tan oku) */
            if (dugum->veri.değişken.genel == 2) {
                LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
                LLVMValueRef alloca2 = LLVMBuildAlloca(u->olusturucu, tip, isim);
                LLVMValueRef coklu_g = LLVMGetNamedGlobal(u->modul, "_tr_coklu_donus");
                if (coklu_g) {
                    LLVMValueRef val = LLVMBuildLoad2(u->olusturucu, i64, coklu_g, "coklu_val");
                    LLVMBuildStore(u->olusturucu, val, alloca2);
                }
                llvm_sembol_ekle(u, isim, alloca2, tip, 0, 0);
                break;
            }

            /* Alloca ile stack'te yer ayır */
            LLVMValueRef alloca = LLVMBuildAlloca(u->olusturucu, tip, isim);

            /* Başlangıç değeri varsa ata */
            if (dugum->çocuk_sayısı > 0 && dugum->çocuklar[0]) {
                LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (deger) {
                    /* Struct → i64 dönüşümü (Demet, Sonuç, Seçenek uyumu) */
                    if (LLVMGetTypeKind(LLVMTypeOf(deger)) == LLVMStructTypeKind &&
                        LLVMGetTypeKind(tip) == LLVMIntegerTypeKind) {
                        deger = LLVMBuildExtractValue(u->olusturucu, deger, 0, "struct_v0");
                    }
                    LLVMBuildStore(u->olusturucu, deger, alloca);
                }
            }

            /* Sembol tablosuna ekle */
            llvm_sembol_ekle(u, isim, alloca, tip, 0, 0);

            /* Metin dizisi bayrağını ayarla */
            if (u->son_dizi_metin) {
                LLVMSembolGirişi *sem = llvm_sembol_bul(u, isim);
                if (sem) sem->metin_dizisi = 1;
                u->son_dizi_metin = 0;
            }

            /* Debug: değişken bilgisi */
            if (u->hata_ayiklama) {
                llvm_debug_degisken_tanimla(u, isim, alloca, dugum->satir);
            }
            break;
        }

        case DÜĞÜM_ATAMA: {
            /* Atama: değişken = ifade
             * Parser format: veri.tanimlayici.isim = variable name
             *                çocuklar[0] = value expression
             */
            if (dugum->çocuk_sayısı < 1) break;

            char *isim = dugum->veri.tanimlayici.isim;
            LLVMSembolGirişi *sembol = llvm_sembol_bul(u, isim);

            if (!sembol) {
                fprintf(stderr, "Hata: Tanımsız değişken: %s\n", isim);
                break;
            }

            LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (deger) {
                /* Struct → i64 dönüşümü */
                if (LLVMGetTypeKind(LLVMTypeOf(deger)) == LLVMStructTypeKind &&
                    LLVMGetTypeKind(sembol->tip) == LLVMIntegerTypeKind) {
                    deger = LLVMBuildExtractValue(u->olusturucu, deger, 0, "atama_v0");
                }
                LLVMBuildStore(u->olusturucu, deger, sembol->deger);
            }
            break;
        }

        case DÜĞÜM_DÖNDÜR: {
            /* Dönüş deyimi */
            if (dugum->çocuk_sayısı >= 2) {
                /* Çoklu dönüş: ilk değer return, ikinci değer _tr_coklu_donus global'ına */
                LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
                LLVMValueRef deger1 = llvm_ifade_uret(u, dugum->çocuklar[0]);
                LLVMValueRef deger2 = llvm_ifade_uret(u, dugum->çocuklar[1]);
                if (deger2) {
                    LLVMValueRef coklu_g = LLVMGetNamedGlobal(u->modul, "_tr_coklu_donus");
                    if (!coklu_g) {
                        coklu_g = LLVMAddGlobal(u->modul, i64, "_tr_coklu_donus");
                        LLVMSetInitializer(coklu_g, LLVMConstInt(i64, 0, 0));
                        LLVMSetLinkage(coklu_g, LLVMInternalLinkage);
                    }
                    LLVMBuildStore(u->olusturucu, deger2, coklu_g);
                }
                if (deger1) {
                    LLVMBuildRet(u->olusturucu, deger1);
                } else {
                    LLVMBuildRetVoid(u->olusturucu);
                }
            } else if (dugum->çocuk_sayısı > 0 && dugum->çocuklar[0]) {
                LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[0]);
                if (deger) {
                    LLVMBuildRet(u->olusturucu, deger);
                } else {
                    LLVMBuildRetVoid(u->olusturucu);
                }
            } else {
                LLVMBuildRetVoid(u->olusturucu);
            }
            break;
        }

        case DÜĞÜM_EĞER: {
            /* eğer/değilse */
            LLVMValueRef kosul = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (!kosul) break;

            /* i1'e dönüştür */
            LLVMTypeRef kosul_tip = LLVMTypeOf(kosul);
            if (LLVMGetTypeKind(kosul_tip) != LLVMIntegerTypeKind ||
                LLVMGetIntTypeWidth(kosul_tip) != 1) {
                kosul = LLVMBuildICmp(u->olusturucu, LLVMIntNE, kosul,
                    LLVMConstInt(LLVMInt64TypeInContext(u->baglam), 0, 0),
                    "kosul_bool");
            }

            /* Blokları oluştur */
            LLVMBasicBlockRef dogru_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "eger_dogru");
            LLVMBasicBlockRef yanlis_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "eger_yanlis");
            LLVMBasicBlockRef birlesim_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "eger_son");

            /* Koşullu dallanma */
            LLVMBuildCondBr(u->olusturucu, kosul, dogru_blok, yanlis_blok);

            /* Doğru dal */
            LLVMPositionBuilderAtEnd(u->olusturucu, dogru_blok);
            if (dugum->çocuk_sayısı > 1 && dugum->çocuklar[1]) {
                llvm_blok_uret(u, dugum->çocuklar[1]);
            }
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                LLVMBuildBr(u->olusturucu, birlesim_blok);
            }

            /* Yanlış dal (değilse) */
            LLVMPositionBuilderAtEnd(u->olusturucu, yanlis_blok);
            if (dugum->çocuk_sayısı > 2 && dugum->çocuklar[2]) {
                llvm_blok_uret(u, dugum->çocuklar[2]);
            }
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                LLVMBuildBr(u->olusturucu, birlesim_blok);
            }

            /* Birleşim noktası */
            LLVMPositionBuilderAtEnd(u->olusturucu, birlesim_blok);
            break;
        }

        case DÜĞÜM_İKEN: {
            /* iken döngüsü */
            LLVMBasicBlockRef kosul_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "iken_kosul");
            LLVMBasicBlockRef govde_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "iken_govde");
            LLVMBasicBlockRef son_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "iken_son");

            /* Döngü kontrol bloklarını kaydet */
            LLVMBasicBlockRef onceki_cikis = u->dongu_cikis;
            LLVMBasicBlockRef onceki_devam = u->dongu_devam;
            u->dongu_cikis = son_blok;
            u->dongu_devam = kosul_blok;

            /* Koşul bloğuna atla */
            LLVMBuildBr(u->olusturucu, kosul_blok);

            /* Koşul kontrolü */
            LLVMPositionBuilderAtEnd(u->olusturucu, kosul_blok);
            LLVMValueRef kosul = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (kosul) {
                /* i1'e dönüştür */
                LLVMTypeRef kosul_tip = LLVMTypeOf(kosul);
                if (LLVMGetTypeKind(kosul_tip) != LLVMIntegerTypeKind ||
                    LLVMGetIntTypeWidth(kosul_tip) != 1) {
                    kosul = LLVMBuildICmp(u->olusturucu, LLVMIntNE, kosul,
                        LLVMConstInt(LLVMInt64TypeInContext(u->baglam), 0, 0),
                        "kosul_bool");
                }
                LLVMBuildCondBr(u->olusturucu, kosul, govde_blok, son_blok);
            } else {
                LLVMBuildBr(u->olusturucu, son_blok);
            }

            /* Döngü gövdesi */
            LLVMPositionBuilderAtEnd(u->olusturucu, govde_blok);
            if (dugum->çocuk_sayısı > 1 && dugum->çocuklar[1]) {
                llvm_blok_uret(u, dugum->çocuklar[1]);
            }
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                LLVMBuildBr(u->olusturucu, kosul_blok);
            }

            /* Döngü sonu */
            LLVMPositionBuilderAtEnd(u->olusturucu, son_blok);

            /* Önceki kontrol bloklarını geri yükle */
            u->dongu_cikis = onceki_cikis;
            u->dongu_devam = onceki_devam;
            break;
        }

        case DÜĞÜM_DÖNGÜ: {
            /* döngü i = başlangıç, bitis ise ... son
             * çocuklar[0] = başlangıç değeri
             * çocuklar[1] = bitiş değeri
             * çocuklar[2] = gövde bloğu
             * veri.dongu.isim = sayaç değişkeni adı
             */
            LLVMBasicBlockRef kosul_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "dongu_kosul");
            LLVMBasicBlockRef govde_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "dongu_govde");
            LLVMBasicBlockRef artir_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "dongu_artir");
            LLVMBasicBlockRef son_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "dongu_son");

            /* Döngü kontrol bloklarını kaydet */
            LLVMBasicBlockRef onceki_cikis = u->dongu_cikis;
            LLVMBasicBlockRef onceki_devam = u->dongu_devam;
            u->dongu_cikis = son_blok;
            u->dongu_devam = artir_blok;

            /* Etiketli kır/devam için yığına it */
            int onceki_derinlik = u->dongu_derinligi;
            if (dugum->veri.dongu.isim && u->dongu_derinligi < LLVM_MAKS_DONGU) {
                u->dongu_yigini[u->dongu_derinligi].isim = dugum->veri.dongu.isim;
                u->dongu_yigini[u->dongu_derinligi].cikis = son_blok;
                u->dongu_yigini[u->dongu_derinligi].devam = artir_blok;
                u->dongu_derinligi++;
            }

            /* Yeni kapsam — döngü değişkeni burada yaşar */
            llvm_kapsam_gir(u);

            LLVMTypeRef i64_tipi = LLVMInt64TypeInContext(u->baglam);

            /* Sayaç değişkeni oluştur (alloca) */
            LLVMValueRef sayac = LLVMBuildAlloca(u->olusturucu, i64_tipi,
                                                  dugum->veri.dongu.isim);

            /* Başlangıç değerini hesapla ve sakla */
            LLVMValueRef başlangıç = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (!başlangıç)
                başlangıç = LLVMConstInt(i64_tipi, 0, 0);
            LLVMBuildStore(u->olusturucu, başlangıç, sayac);

            /* Bitiş değerini hesapla ve sakla */
            LLVMValueRef bitis_alloca = LLVMBuildAlloca(u->olusturucu, i64_tipi,
                                                         "dongu_bitis");
            LLVMValueRef bitis = llvm_ifade_uret(u, dugum->çocuklar[1]);
            if (!bitis)
                bitis = LLVMConstInt(i64_tipi, 0, 0);
            LLVMBuildStore(u->olusturucu, bitis, bitis_alloca);

            /* Adım değeri: 4 çocuk varsa çocuklar[2] adım, çocuklar[3] gövde
             *              3 çocuk varsa adım=1, çocuklar[2] gövde */
            int adim_var = (dugum->çocuk_sayısı > 3);
            int govde_idx = adim_var ? 3 : 2;
            LLVMValueRef adim_alloca = LLVMBuildAlloca(u->olusturucu, i64_tipi,
                                                        "dongu_adim");
            if (adim_var) {
                LLVMValueRef adim = llvm_ifade_uret(u, dugum->çocuklar[2]);
                if (!adim) adim = LLVMConstInt(i64_tipi, 1, 0);
                LLVMBuildStore(u->olusturucu, adim, adim_alloca);
            } else {
                LLVMBuildStore(u->olusturucu, LLVMConstInt(i64_tipi, 1, 0), adim_alloca);
            }

            /* Sayaç değişkenini sembol tablosuna ekle */
            llvm_sembol_ekle(u, dugum->veri.dongu.isim, sayac, i64_tipi, 0, 0);

            /* Koşul bloğuna atla */
            LLVMBuildBr(u->olusturucu, kosul_blok);

            /* Koşul: adım >= 0 ise sayac <= bitis, değilse sayac >= bitis */
            LLVMPositionBuilderAtEnd(u->olusturucu, kosul_blok);
            LLVMValueRef sayac_degeri = LLVMBuildLoad2(u->olusturucu, i64_tipi,
                                                        sayac, "sayac_val");
            LLVMValueRef bitis_degeri = LLVMBuildLoad2(u->olusturucu, i64_tipi,
                                                        bitis_alloca, "bitis_val");
            LLVMValueRef adim_degeri = LLVMBuildLoad2(u->olusturucu, i64_tipi,
                                                        adim_alloca, "adim_val");
            LLVMValueRef sifir = LLVMConstInt(i64_tipi, 0, 0);
            LLVMValueRef pozitif_mi = LLVMBuildICmp(u->olusturucu, LLVMIntSGE,
                                                      adim_degeri, sifir, "pozitif_mi");
            LLVMValueRef kosul_ileri = LLVMBuildICmp(u->olusturucu, LLVMIntSLE,
                                                      sayac_degeri, bitis_degeri, "kosul_ileri");
            LLVMValueRef kosul_geri = LLVMBuildICmp(u->olusturucu, LLVMIntSGE,
                                                      sayac_degeri, bitis_degeri, "kosul_geri");
            LLVMValueRef karsilastirma = LLVMBuildSelect(u->olusturucu, pozitif_mi,
                                                          kosul_ileri, kosul_geri, "dongu_kosul");
            LLVMBuildCondBr(u->olusturucu, karsilastirma, govde_blok, son_blok);

            /* Gövde */
            LLVMPositionBuilderAtEnd(u->olusturucu, govde_blok);
            if (dugum->çocuk_sayısı > govde_idx && dugum->çocuklar[govde_idx]) {
                llvm_blok_uret(u, dugum->çocuklar[govde_idx]);
            }
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                LLVMBuildBr(u->olusturucu, artir_blok);
            }

            /* Sayacı adım kadar artır */
            LLVMPositionBuilderAtEnd(u->olusturucu, artir_blok);
            LLVMValueRef mevcut = LLVMBuildLoad2(u->olusturucu, i64_tipi,
                                                   sayac, "mevcut");
            LLVMValueRef adim_val = LLVMBuildLoad2(u->olusturucu, i64_tipi,
                                                     adim_alloca, "adim");
            LLVMValueRef artmis = LLVMBuildAdd(u->olusturucu, mevcut, adim_val,
                                                "artmis");
            LLVMBuildStore(u->olusturucu, artmis, sayac);
            LLVMBuildBr(u->olusturucu, kosul_blok);

            /* Döngü sonu */
            LLVMPositionBuilderAtEnd(u->olusturucu, son_blok);

            /* Kapsamı kapat, kontrol bloklarını geri yükle */
            llvm_kapsam_cik(u);
            u->dongu_derinligi = onceki_derinlik;
            u->dongu_cikis = onceki_cikis;
            u->dongu_devam = onceki_devam;
            break;
        }

        case DÜĞÜM_KIR: {
            /* kır (break) — opsiyonel etiketli */
            LLVMBasicBlockRef hedef = u->dongu_cikis;
            if (dugum->veri.tanimlayici.isim) {
                /* Etiketli: döngü yığınında ara */
                for (int i = u->dongu_derinligi - 1; i >= 0; i--) {
                    if (u->dongu_yigini[i].isim &&
                        strcmp(u->dongu_yigini[i].isim, dugum->veri.tanimlayici.isim) == 0) {
                        hedef = u->dongu_yigini[i].cikis;
                        break;
                    }
                }
            }
            if (hedef) {
                LLVMBuildBr(u->olusturucu, hedef);
            }
            break;
        }

        case DÜĞÜM_DEVAM: {
            /* devam (continue) — opsiyonel etiketli */
            LLVMBasicBlockRef hedef = u->dongu_devam;
            if (dugum->veri.tanimlayici.isim) {
                for (int i = u->dongu_derinligi - 1; i >= 0; i--) {
                    if (u->dongu_yigini[i].isim &&
                        strcmp(u->dongu_yigini[i].isim, dugum->veri.tanimlayici.isim) == 0) {
                        hedef = u->dongu_yigini[i].devam;
                        break;
                    }
                }
            }
            if (hedef) {
                LLVMBuildBr(u->olusturucu, hedef);
            }
            break;
        }

        case DÜĞÜM_DİZİ_ATAMA: {
            /* Dizi eleman ataması: dizi[indeks] = değer */
            if (dugum->çocuk_sayısı < 3) break;
            LLVMValueRef dizi = llvm_ifade_uret(u, dugum->çocuklar[0]);
            LLVMValueRef indeks = llvm_ifade_uret(u, dugum->çocuklar[1]);
            LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[2]);
            if (!dizi || !indeks || !deger) break;

            LLVMTypeRef i64_tip = LLVMInt64TypeInContext(u->baglam);

            /* Dizi struct ise pointer alanını çıkar */
            LLVMValueRef veri_ptr;
            if (LLVMGetTypeKind(LLVMTypeOf(dizi)) == LLVMStructTypeKind) {
                veri_ptr = LLVMBuildExtractValue(u->olusturucu, dizi, 0, "dizi_ptr");
            } else {
                veri_ptr = dizi;
            }

            /* Metin dizisi mi kontrol et */
            int metin_dizi_ata = 0;
            if (dugum->çocuklar[0]->tur == DÜĞÜM_TANIMLAYICI) {
                LLVMSembolGirişi *dizi_sem = llvm_sembol_bul(u,
                    dugum->çocuklar[0]->veri.tanimlayici.isim);
                if (dizi_sem && dizi_sem->metin_dizisi) {
                    metin_dizi_ata = 1;
                }
            }
            LLVMTypeRef ata_eleman_tip = metin_dizi_ata ? llvm_metin_tipi_al(u) : i64_tip;

            LLVMValueRef elem_ptr = LLVMBuildGEP2(u->olusturucu, ata_eleman_tip,
                                                    veri_ptr, &indeks, 1, "elem_ptr");
            LLVMBuildStore(u->olusturucu, deger, elem_ptr);
            break;
        }

        case DÜĞÜM_ERİŞİM_ATAMA: {
            /* Alan ataması: nesne.alan = değer */
            if (dugum->çocuk_sayısı < 2) break;
            LLVMValueRef nesne = llvm_ifade_uret(u, dugum->çocuklar[0]);
            LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[1]);
            char *alan_adi = dugum->veri.tanimlayici.isim;
            if (!nesne || !deger) break;

            /* Alan indeksini bul */
            LLVMSınıfBilgisi *sinif = u->siniflar;
            int alan_indeks = -1;
            while (sinif) {
                for (int i = 0; i < sinif->alan_sayisi; i++) {
                    if (strcmp(sinif->alan_isimleri[i], alan_adi) == 0) {
                        alan_indeks = i;
                        break;
                    }
                }
                if (alan_indeks >= 0) break;
                sinif = sinif->sonraki;
            }

            if (alan_indeks >= 0 && sinif) {
                /* Statik alan: global değişkene yaz */
                if (sinif->alan_statik && sinif->alan_statik[alan_indeks] &&
                    sinif->statik_globals && sinif->statik_globals[alan_indeks]) {
                    LLVMBuildStore(u->olusturucu, deger, sinif->statik_globals[alan_indeks]);
                } else {
                    LLVMValueRef sifir = LLVMConstInt(LLVMInt32TypeInContext(u->baglam), 0, 0);
                    LLVMValueRef idx = LLVMConstInt(LLVMInt32TypeInContext(u->baglam), alan_indeks, 0);
                    LLVMValueRef indices[] = { sifir, idx };
                    LLVMValueRef alan_ptr = LLVMBuildGEP2(u->olusturucu, sinif->struct_tipi,
                                                           nesne, indices, 2, "alan_ptr");
                    LLVMBuildStore(u->olusturucu, deger, alan_ptr);
                }
            } else {
                /* Setter fallback: koy_<alan_adi> metot ara */
                char setter_adi[256];
                snprintf(setter_adi, sizeof(setter_adi), "koy_%s", alan_adi);
                LLVMSınıfBilgisi *s_it = u->siniflar;
                while (s_it) {
                    char mangled[256];
                    snprintf(mangled, sizeof(mangled), "%s_%s", s_it->isim, setter_adi);
                    LLVMSembolGirişi *setter_fn = llvm_sembol_bul(u, mangled);
                    if (setter_fn) {
                        /* Setter bulundu: setter(bu, deger) çağır */
                        LLVMValueRef args[] = { nesne, deger };
                        LLVMBuildCall2(u->olusturucu, setter_fn->tip,
                                      setter_fn->deger, args, 2, "");
                        break;
                    }
                    s_it = s_it->sonraki;
                }
            }
            break;
        }

        case DÜĞÜM_İFADE_BİLDİRİMİ: {
            /* İfade deyimi (sonucu atılır) */
            if (dugum->çocuk_sayısı > 0) {
                Düğüm *cocuk = dugum->çocuklar[0];
                /* Deyim türündeki düğümler ifade olarak değil deyim olarak işlenmeli */
                if (cocuk->tur == DÜĞÜM_PAKET_AÇ ||
                    cocuk->tur == DÜĞÜM_İLE_İSE ||
                    cocuk->tur == DÜĞÜM_ÜRET ||
                    cocuk->tur == DÜĞÜM_ERİŞİM_ATAMA) {
                    llvm_deyim_uret(u, cocuk);
                } else {
                    llvm_ifade_uret(u, cocuk);
                }
            }
            break;
        }

        case DÜĞÜM_BLOK: {
            /* Çoklu dönüş bloğu: tam x, tam y = fonk() */
            if (dugum->çocuk_sayısı == 2 &&
                dugum->çocuklar[0] && dugum->çocuklar[0]->tur == DÜĞÜM_DEĞİŞKEN &&
                dugum->çocuklar[1] && dugum->çocuklar[1]->tur == DÜĞÜM_DEĞİŞKEN &&
                dugum->çocuklar[1]->veri.değişken.genel == 2) {
                /* İlk değişkeni normal üret (fonksiyon çağrısı yapılır, sonuç var1'e atanır) */
                llvm_deyim_uret(u, dugum->çocuklar[0]);
                /* İkinci değişkeni üret (genel==2 olduğu için _tr_coklu_donus'tan okuyacak) */
                llvm_deyim_uret(u, dugum->çocuklar[1]);
            } else {
                llvm_blok_uret(u, dugum);
            }
            break;
        }

        case DÜĞÜM_ÇAĞRI: {
            /* Fonksiyon çağrısı (sonucu atılır) */
            llvm_ifade_uret(u, dugum);
            break;
        }

        case DÜĞÜM_HER_İÇİN: {
            /* her eleman için dizi ise ... son */
            if (dugum->çocuk_sayısı < 2) break;

            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);

            /* Dizi ifadesini üret → struct {ptr, count} */
            LLVMValueRef dizi_val = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (!dizi_val) break;

            LLVMValueRef dizi_ptr = LLVMBuildExtractValue(u->olusturucu, dizi_val, 0, "her_ptr");
            LLVMValueRef dizi_say = LLVMBuildExtractValue(u->olusturucu, dizi_val, 1, "her_say");

            /* Sayaç: alloca i64 = 0 */
            LLVMValueRef idx_alloca = LLVMBuildAlloca(u->olusturucu, i64, "her_idx");
            LLVMBuildStore(u->olusturucu, LLVMConstInt(i64, 0, 0), idx_alloca);

            /* Bloklar */
            LLVMBasicBlockRef kosul_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "her_kosul");
            LLVMBasicBlockRef govde_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "her_govde");
            LLVMBasicBlockRef artir_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "her_artir");
            LLVMBasicBlockRef son_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "her_son");

            LLVMBuildBr(u->olusturucu, kosul_blok);

            /* Koşul: idx < count */
            LLVMPositionBuilderAtEnd(u->olusturucu, kosul_blok);
            LLVMValueRef idx_val = LLVMBuildLoad2(u->olusturucu, i64, idx_alloca, "idx");
            LLVMValueRef cmp = LLVMBuildICmp(u->olusturucu, LLVMIntSLT, idx_val, dizi_say, "her_cmp");
            LLVMBuildCondBr(u->olusturucu, cmp, govde_blok, son_blok);

            /* Gövde */
            LLVMPositionBuilderAtEnd(u->olusturucu, govde_blok);

            /* Döngü kontrol bloklarını kaydet */
            LLVMBasicBlockRef onceki_cikis = u->dongu_cikis;
            LLVMBasicBlockRef onceki_devam = u->dongu_devam;
            u->dongu_cikis = son_blok;
            u->dongu_devam = artir_blok;

            llvm_kapsam_gir(u);

            /* Eleman değeri: dizi_ptr[idx] */
            LLVMValueRef idx_yeniden = LLVMBuildLoad2(u->olusturucu, i64, idx_alloca, "idx2");
            LLVMTypeRef i64_ptr = LLVMPointerType(i64, 0);
            LLVMValueRef eleman_ptr = LLVMBuildGEP2(u->olusturucu, i64,
                LLVMBuildBitCast(u->olusturucu, dizi_ptr, i64_ptr, "dp"),
                &idx_yeniden, 1, "elem_ptr");
            LLVMValueRef eleman_val = LLVMBuildLoad2(u->olusturucu, i64, eleman_ptr, "eleman");

            /* Döngü değişkenini oluştur */
            if (dugum->veri.dongu.isim) {
                LLVMValueRef var_alloca = LLVMBuildAlloca(u->olusturucu, i64, dugum->veri.dongu.isim);
                LLVMBuildStore(u->olusturucu, eleman_val, var_alloca);
                llvm_sembol_ekle(u, dugum->veri.dongu.isim, var_alloca, i64, 0, 0);
            }

            /* Gövde bloğu */
            if (dugum->çocuk_sayısı > 1) {
                llvm_blok_uret(u, dugum->çocuklar[1]);
            }

            llvm_kapsam_cik(u);

            /* Terminatör yoksa artır bloğuna atla */
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                LLVMBuildBr(u->olusturucu, artir_blok);
            }

            /* idx++ */
            LLVMPositionBuilderAtEnd(u->olusturucu, artir_blok);
            LLVMValueRef idx3 = LLVMBuildLoad2(u->olusturucu, i64, idx_alloca, "idx3");
            LLVMValueRef idx_artmis = LLVMBuildAdd(u->olusturucu, idx3,
                LLVMConstInt(i64, 1, 0), "idx_art");
            LLVMBuildStore(u->olusturucu, idx_artmis, idx_alloca);
            LLVMBuildBr(u->olusturucu, kosul_blok);

            /* Son */
            LLVMPositionBuilderAtEnd(u->olusturucu, son_blok);
            u->dongu_cikis = onceki_cikis;
            u->dongu_devam = onceki_devam;
            break;
        }

        case DÜĞÜM_EŞLE: {
            /* eşle ifade ise durum1: blok1, durum2: blok2, ... varsayılan: blok son */
            if (dugum->çocuk_sayısı < 1) break;

            LLVMValueRef esle_val = llvm_ifade_uret(u, dugum->çocuklar[0]);
            if (!esle_val) break;

            LLVMBasicBlockRef son_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "esle_son");

            int i = 1;
            while (i + 1 < dugum->çocuk_sayısı) {
                if (dugum->çocuklar[i]->tur == DÜĞÜM_BLOK) {
                    break;  /* Varsayılan blok */
                }
                LLVMBasicBlockRef eslesme_blok = LLVMAppendBasicBlockInContext(
                    u->baglam, u->mevcut_islev, "esle_eslesme");
                LLVMBasicBlockRef sonraki_blok = LLVMAppendBasicBlockInContext(
                    u->baglam, u->mevcut_islev, "esle_sonraki");

                /* Durum değerini karşılaştır */
                if (dugum->çocuklar[i]->tur == DÜĞÜM_ARALIK) {
                    /* Aralık deseni: alt..üst -> val >= alt && val <= üst */
                    Düğüm *aralik = dugum->çocuklar[i];
                    LLVMValueRef alt = llvm_ifade_uret(u, aralik->çocuklar[0]);
                    LLVMValueRef ust = llvm_ifade_uret(u, aralik->çocuklar[1]);
                    if (alt && ust) {
                        LLVMValueRef ge = LLVMBuildICmp(u->olusturucu, LLVMIntSGE,
                            esle_val, alt, "aralik_ge");
                        LLVMValueRef le = LLVMBuildICmp(u->olusturucu, LLVMIntSLE,
                            esle_val, ust, "aralik_le");
                        LLVMValueRef cmp = LLVMBuildAnd(u->olusturucu, ge, le, "aralik_cmp");
                        LLVMBuildCondBr(u->olusturucu, cmp, eslesme_blok, sonraki_blok);
                    } else {
                        LLVMBuildBr(u->olusturucu, sonraki_blok);
                    }
                } else {
                    LLVMValueRef durum_val = llvm_ifade_uret(u, dugum->çocuklar[i]);
                    if (durum_val) {
                        LLVMValueRef cmp = LLVMBuildICmp(u->olusturucu, LLVMIntEQ,
                            esle_val, durum_val, "esle_cmp");
                        LLVMBuildCondBr(u->olusturucu, cmp, eslesme_blok, sonraki_blok);
                    } else {
                        LLVMBuildBr(u->olusturucu, sonraki_blok);
                    }
                }

                /* Eşleşen blok */
                LLVMPositionBuilderAtEnd(u->olusturucu, eslesme_blok);
                llvm_blok_uret(u, dugum->çocuklar[i + 1]);
                if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                    LLVMBuildBr(u->olusturucu, son_blok);
                }

                LLVMPositionBuilderAtEnd(u->olusturucu, sonraki_blok);
                i += 2;
            }

            /* Varsayılan blok */
            if (i < dugum->çocuk_sayısı && dugum->çocuklar[i]->tur == DÜĞÜM_BLOK) {
                llvm_blok_uret(u, dugum->çocuklar[i]);
            }

            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                LLVMBuildBr(u->olusturucu, son_blok);
            }

            LLVMPositionBuilderAtEnd(u->olusturucu, son_blok);
            break;
        }

        case DÜĞÜM_DENE_YAKALA: {
            /* dene ... yakala ... [sonunda ...] son */
            /* _tr_dene_baslat() çağır: 0=normal, 1=istisna */
            LLVMSembolGirişi *dene_fn = llvm_sembol_bul(u, "_tr_dene_baslat");
            LLVMSembolGirişi *bitir_fn = llvm_sembol_bul(u, "_tr_dene_bitir");
            if (!dene_fn || !bitir_fn) break;

            LLVMValueRef sonuc = LLVMBuildCall2(u->olusturucu, dene_fn->tip,
                dene_fn->deger, NULL, 0, "dene_sonuc");
            /* returns_twice attribute on call site */
            unsigned kind = LLVMGetEnumAttributeKindForName("returns_twice", 13);
            LLVMAttributeRef attr = LLVMCreateEnumAttribute(u->baglam, kind, 0);
            LLVMAddCallSiteAttribute(sonuc, LLVMAttributeFunctionIndex, attr);

            LLVMValueRef cmp = LLVMBuildICmp(u->olusturucu, LLVMIntEQ, sonuc,
                LLVMConstInt(LLVMInt32TypeInContext(u->baglam), 0, 0), "dene_cmp");

            LLVMBasicBlockRef dene_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "dene_govde");
            LLVMBasicBlockRef yakala_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "yakala_govde");
            LLVMBasicBlockRef son_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "dene_son");

            LLVMBuildCondBr(u->olusturucu, cmp, dene_blok, yakala_blok);

            /* Dene gövdesi */
            LLVMPositionBuilderAtEnd(u->olusturucu, dene_blok);
            if (dugum->çocuk_sayısı > 0) {
                llvm_blok_uret(u, dugum->çocuklar[0]);
            }
            /* Normal tamamlanma: _tr_dene_bitir() */
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                LLVMBuildCall2(u->olusturucu, bitir_fn->tip,
                    bitir_fn->deger, NULL, 0, "");
                LLVMBuildBr(u->olusturucu, son_blok);
            }

            /* Yakala gövdesi */
            LLVMPositionBuilderAtEnd(u->olusturucu, yakala_blok);
            if (dugum->çocuk_sayısı > 1) {
                /* İlk yakala bloğu (çocuklar[1]) */
                llvm_blok_uret(u, dugum->çocuklar[1]);
            }
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                LLVMBuildBr(u->olusturucu, son_blok);
            }

            LLVMPositionBuilderAtEnd(u->olusturucu, son_blok);
            break;
        }

        case DÜĞÜM_FIRLAT: {
            /* fırlat ifade → _tr_firlat_deger(deger) */
            LLVMSembolGirişi *firlat_fn = llvm_sembol_bul(u, "_tr_firlat_deger");
            if (!firlat_fn) break;

            LLVMValueRef deger = NULL;
            if (dugum->çocuk_sayısı > 0) {
                deger = llvm_ifade_uret(u, dugum->çocuklar[0]);
            }
            if (!deger) {
                deger = LLVMConstInt(LLVMInt64TypeInContext(u->baglam), 0, 0);
            }
            /* Metin struct {ptr, i64} ise pointer'ı çıkarıp i64'e dönüştür */
            LLVMTypeRef deger_tipi = LLVMTypeOf(deger);
            if (LLVMGetTypeKind(deger_tipi) == LLVMStructTypeKind) {
                /* Struct'ın ilk elemanını (ptr) çıkar ve ptrtoint ile i64'e dönüştür */
                LLVMValueRef ptr_val = LLVMBuildExtractValue(u->olusturucu, deger, 0, "firlat_ptr");
                deger = LLVMBuildPtrToInt(u->olusturucu, ptr_val,
                    LLVMInt64TypeInContext(u->baglam), "firlat_i64");
            }
            LLVMValueRef args[] = { deger };
            LLVMBuildCall2(u->olusturucu, firlat_fn->tip,
                firlat_fn->deger, args, 1, "");
            LLVMBuildUnreachable(u->olusturucu);
            /* Unreachable sonrası yeni blok lazım (LLVM gereksinimi) */
            LLVMBasicBlockRef devam_blok = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "firlat_sonrasi");
            LLVMPositionBuilderAtEnd(u->olusturucu, devam_blok);
            break;
        }

        case DÜĞÜM_İLE_İSE: {
            /* ile ifade olarak d ise ... son
             * çocuklar[0]=kaynak ifade, çocuklar[1]=gövde */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
            LLVMValueRef kaynak = NULL;

            if (dugum->çocuk_sayısı > 0) {
                kaynak = llvm_ifade_uret(u, dugum->çocuklar[0]);
            }

            /* Yeni kapsam */
            llvm_kapsam_gir(u);

            /* Değişken varsa ata */
            if (dugum->veri.tanimlayici.isim && kaynak) {
                LLVMValueRef val = kaynak;
                if (LLVMGetTypeKind(LLVMTypeOf(val)) != LLVMIntegerTypeKind ||
                    LLVMGetIntTypeWidth(LLVMTypeOf(val)) != 64)
                    val = LLVMBuildZExt(u->olusturucu, val, i64, "ile_ext");
                LLVMValueRef alloca = LLVMBuildAlloca(u->olusturucu, i64,
                    dugum->veri.tanimlayici.isim);
                LLVMBuildStore(u->olusturucu, val, alloca);
                llvm_sembol_ekle(u, dugum->veri.tanimlayici.isim, alloca, i64, 0, 0);
            }

            /* Gövdeyi çalıştır */
            if (dugum->çocuk_sayısı > 1) {
                llvm_blok_uret(u, dugum->çocuklar[1]);
            }

            llvm_kapsam_cik(u);
            break;
        }

        case DÜĞÜM_PAKET_AÇ: {
            /* [a, b, c] = ifade veya x, y = ifade
             * Son çocuk = kaynak, diğerleri = hedef değişkenler */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
            int hedef_sayisi = dugum->çocuk_sayısı - 1;
            if (hedef_sayisi <= 0) break;
            Düğüm *kaynak = dugum->çocuklar[hedef_sayisi];

            /* 2 değerli demet unpacking: struct {i64, i64} */
            if (hedef_sayisi == 2 && kaynak->tur == DÜĞÜM_ÇAĞRI) {
                LLVMValueRef sonuc = llvm_ifade_uret(u, kaynak);
                if (!sonuc) break;

                for (int i = 0; i < 2; i++) {
                    Düğüm *hedef = dugum->çocuklar[i];
                    if (hedef->tur == DÜĞÜM_TANIMLAYICI) {
                        LLVMValueRef val;
                        if (LLVMGetTypeKind(LLVMTypeOf(sonuc)) == LLVMStructTypeKind) {
                            val = LLVMBuildExtractValue(u->olusturucu, sonuc, i, "paket_v");
                        } else {
                            val = sonuc; /* fallback */
                        }
                        LLVMSembolGirişi *s = llvm_sembol_bul(u, hedef->veri.tanimlayici.isim);
                        if (s) {
                            LLVMBuildStore(u->olusturucu, val, s->deger);
                        } else {
                            LLVMValueRef alloca = LLVMBuildAlloca(u->olusturucu, i64,
                                hedef->veri.tanimlayici.isim);
                            LLVMBuildStore(u->olusturucu, val, alloca);
                            llvm_sembol_ekle(u, hedef->veri.tanimlayici.isim, alloca, i64, 0, 0);
                        }
                    }
                }
                break;
            }

            /* Dizi unpacking: kaynak → struct {ptr, count} */
            LLVMValueRef kaynak_val = llvm_ifade_uret(u, kaynak);
            if (!kaynak_val) break;

            LLVMValueRef dizi_ptr, dizi_count;
            if (LLVMGetTypeKind(LLVMTypeOf(kaynak_val)) == LLVMStructTypeKind) {
                dizi_ptr = LLVMBuildExtractValue(u->olusturucu, kaynak_val, 0, "pak_ptr");
                dizi_count = LLVMBuildExtractValue(u->olusturucu, kaynak_val, 1, "pak_count");
            } else {
                /* tek değer — fallback */
                dizi_ptr = kaynak_val;
                dizi_count = LLVMConstInt(i64, 1, 0);
            }
            (void)dizi_count;

            for (int i = 0; i < hedef_sayisi; i++) {
                Düğüm *hedef = dugum->çocuklar[i];
                if (hedef->tur != DÜĞÜM_TANIMLAYICI) continue;

                /* Rest pattern: ...değişken */
                const char *hisim = hedef->veri.tanimlayici.isim;
                if (hisim[0] == '.' && hisim[1] == '.' && hisim[2] == '.') {
                    /* Rest: kalan elemanları yeni diziye kopyala */
                    /* Basitleştirilmiş: şimdilik atla */
                    continue;
                }

                /* dizi[i] → değişken */
                LLVMValueRef idx = LLVMConstInt(i64, i, 0);
                LLVMValueRef elem_ptr = LLVMBuildGEP2(u->olusturucu, i64,
                    dizi_ptr, &idx, 1, "pak_elem");
                LLVMValueRef elem_val = LLVMBuildLoad2(u->olusturucu, i64, elem_ptr, "pak_val");

                LLVMSembolGirişi *s = llvm_sembol_bul(u, hisim);
                if (s) {
                    LLVMBuildStore(u->olusturucu, elem_val, s->deger);
                } else {
                    LLVMValueRef alloca = LLVMBuildAlloca(u->olusturucu, i64, hisim);
                    LLVMBuildStore(u->olusturucu, elem_val, alloca);
                    llvm_sembol_ekle(u, hisim, alloca, i64, 0, 0);
                }
            }
            break;
        }

        case DÜĞÜM_ÜRET: {
            /* Üreteç yield: basitleştirilmiş — değeri döndür */
            LLVMValueRef deger = NULL;
            if (dugum->çocuk_sayısı > 0) {
                deger = llvm_ifade_uret(u, dugum->çocuklar[0]);
            }
            if (deger) {
                LLVMBuildRet(u->olusturucu, deger);
            } else {
                LLVMBuildRetVoid(u->olusturucu);
            }
            /* Unreachable sonrası yeni blok */
            LLVMBasicBlockRef devam = LLVMAppendBasicBlockInContext(
                u->baglam, u->mevcut_islev, "uret_sonrasi");
            LLVMPositionBuilderAtEnd(u->olusturucu, devam);
            break;
        }

        case DÜĞÜM_ARAYÜZ:
        case DÜĞÜM_TİP_TANIMI:
        case DÜĞÜM_KULLAN:
            /* Derleme zamanı yapıları — codegen gerektirmez */
            break;

        case DÜĞÜM_SAYIM: {
            /* Sayım (enum) tanımı: her değeri global olarak kaydet */
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
            for (int i = 0; i < dugum->çocuk_sayısı; i++) {
                Düğüm *deger = dugum->çocuklar[i];
                if (deger && deger->veri.tanimlayici.isim) {
                    const char *isim = deger->veri.tanimlayici.isim;
                    /* Global değişken oluştur veya mevcut olanı bul */
                    LLVMValueRef global = LLVMGetNamedGlobal(u->modul, isim);
                    if (!global) {
                        global = LLVMAddGlobal(u->modul, i64, isim);
                        LLVMSetInitializer(global, LLVMConstInt(i64, (unsigned long long)i, 0));
                    }
                    /* Sembol tablosuna ekle */
                    llvm_sembol_ekle(u, isim, global, i64, 0, 1);
                }
            }
            break;
        }

        default:
            /* Diğer deyimler - ifade olarak dene */
            llvm_ifade_uret(u, dugum);
            break;
    }
}

/*
 * llvm_blok_uret - Kod bloğu üret
 */
void llvm_blok_uret(LLVMÜretici *u, Düğüm *dugum) {
    if (!dugum) return;

    llvm_kapsam_gir(u);

    for (int i = 0; i < dugum->çocuk_sayısı; i++) {
        llvm_deyim_uret(u, dugum->çocuklar[i]);

        /* Terminator varsa devam etme */
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
            break;
        }
    }

    llvm_kapsam_cik(u);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 7: SINIF ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_sınıf_üret - Sınıf tanımı üret
 */
void llvm_sınıf_üret(LLVMÜretici *u, Düğüm *dugum) {
    if (!dugum || dugum->tur != DÜĞÜM_SINIF) return;

    char *sınıf_adı = dugum->veri.sinif.isim;

    /* Sınıf bilgisi oluştur */
    LLVMSınıfBilgisi *bilgi = (LLVMSınıfBilgisi *)arena_ayir(
        u->arena, sizeof(LLVMSınıfBilgisi));
    bilgi->isim = arena_strdup(u->arena, sınıf_adı);

    /* Ebeveyn sınıf alanlarını kontrol et */
    LLVMSınıfBilgisi *ebeveyn_bilgi = NULL;
    int ebeveyn_alan_sayisi = 0;
    if (dugum->veri.sinif.ebeveyn) {
        ebeveyn_bilgi = llvm_sinif_bilgisi_bul(u, dugum->veri.sinif.ebeveyn);
        if (ebeveyn_bilgi) {
            ebeveyn_alan_sayisi = ebeveyn_bilgi->alan_sayisi;
        }
    }

    /* Kendi alanlarını say */
    int kendi_alan_sayisi = 0;
    for (int i = 0; i < dugum->çocuk_sayısı; i++) {
        if (dugum->çocuklar[i]->tur == DÜĞÜM_DEĞİŞKEN) {
            kendi_alan_sayisi++;
        }
    }

    int alan_sayisi = ebeveyn_alan_sayisi + kendi_alan_sayisi;

    bilgi->alan_sayisi = alan_sayisi;
    int alloc_sz = alan_sayisi > 0 ? alan_sayisi : 1;
    bilgi->alan_isimleri = (char **)arena_ayir(u->arena, sizeof(char *) * alloc_sz);
    bilgi->alan_tipleri = (LLVMTypeRef *)arena_ayir(u->arena, sizeof(LLVMTypeRef) * alloc_sz);
    bilgi->alan_statik = (int *)arena_ayir(u->arena, sizeof(int) * alloc_sz);
    bilgi->statik_globals = (LLVMValueRef *)arena_ayir(u->arena, sizeof(LLVMValueRef) * alloc_sz);
    memset(bilgi->alan_statik, 0, sizeof(int) * alloc_sz);
    memset(bilgi->statik_globals, 0, sizeof(LLVMValueRef) * alloc_sz);

    /* Alan tiplerini topla */
    LLVMTypeRef *alan_llvm_tipleri = (LLVMTypeRef *)arena_ayir(
        u->arena, sizeof(LLVMTypeRef) * alloc_sz);

    int alan_idx = 0;

    /* Önce ebeveyn alanlarını ekle */
    if (ebeveyn_bilgi) {
        for (int i = 0; i < ebeveyn_alan_sayisi; i++) {
            bilgi->alan_isimleri[alan_idx] = arena_strdup(
                u->arena, ebeveyn_bilgi->alan_isimleri[i]);
            bilgi->alan_tipleri[alan_idx] = ebeveyn_bilgi->alan_tipleri[i];
            bilgi->alan_statik[alan_idx] = ebeveyn_bilgi->alan_statik ? ebeveyn_bilgi->alan_statik[i] : 0;
            bilgi->statik_globals[alan_idx] = ebeveyn_bilgi->statik_globals ? ebeveyn_bilgi->statik_globals[i] : NULL;
            alan_llvm_tipleri[alan_idx] = ebeveyn_bilgi->alan_tipleri[i];
            alan_idx++;
        }
    }

    /* Sonra kendi alanlarını ekle */
    for (int i = 0; i < dugum->çocuk_sayısı; i++) {
        Düğüm *cocuk = dugum->çocuklar[i];
        if (cocuk->tur == DÜĞÜM_DEĞİŞKEN) {
            bilgi->alan_isimleri[alan_idx] = arena_strdup(
                u->arena, cocuk->veri.değişken.isim);
            LLVMTypeRef alan_tipi = llvm_tip_dönüştür(u, cocuk->veri.değişken.tip);
            bilgi->alan_tipleri[alan_idx] = alan_tipi;
            alan_llvm_tipleri[alan_idx] = alan_tipi;

            /* Statik alan: global değişken oluştur */
            if (cocuk->veri.değişken.statik) {
                bilgi->alan_statik[alan_idx] = 1;
                char global_isim[256];
                snprintf(global_isim, sizeof(global_isim), "_%s_%s",
                         sınıf_adı, cocuk->veri.değişken.isim);
                LLVMValueRef global = LLVMAddGlobal(u->modul, alan_tipi, global_isim);
                LLVMSetInitializer(global, LLVMConstNull(alan_tipi));
                bilgi->statik_globals[alan_idx] = global;
            }
            alan_idx++;
        }
    }

    /* LLVM struct tipi oluştur */
    bilgi->struct_tipi = LLVMStructCreateNamed(u->baglam, sınıf_adı);
    LLVMStructSetBody(bilgi->struct_tipi, alan_llvm_tipleri, alan_sayisi, 0);

    /* Bilgiyi kaydet */
    llvm_sınıf_bilgisi_ekle(u, bilgi);

    /* Yapıcı (constructor) fonksiyonu üret: SinifAdi(alan1, alan2, ...) -> SinifAdi* */
    {
        LLVMTypeRef sinif_ptr_tipi = LLVMPointerType(bilgi->struct_tipi, 0);

        /* Parametre tipleri = alan tipleri */
        LLVMTypeRef *yapici_params = alan_llvm_tipleri;

        /* Yapıcı fonksiyon tipi */
        LLVMTypeRef yapici_tip = LLVMFunctionType(sinif_ptr_tipi, yapici_params, alan_sayisi, 0);
        LLVMValueRef yapici_fn = LLVMAddFunction(u->modul, sınıf_adı, yapici_tip);

        /* Sembol tablosuna ekle */
        llvm_sembol_ekle(u, sınıf_adı, yapici_fn, yapici_tip, 0, 1);

        /* Yapıcı gövdesi */
        LLVMBasicBlockRef giris = LLVMAppendBasicBlockInContext(u->baglam, yapici_fn, "giris");
        LLVMValueRef onceki_islev = u->mevcut_islev;
        u->mevcut_islev = yapici_fn;
        LLVMPositionBuilderAtEnd(u->olusturucu, giris);

        /* malloc ile nesne oluştur */
        LLVMTypeRef i64_tip = LLVMInt64TypeInContext(u->baglam);
        unsigned struct_boyut = LLVMStoreSizeOfType(LLVMGetModuleDataLayout(u->modul), bilgi->struct_tipi);
        LLVMValueRef boyut_val = LLVMConstInt(i64_tip, struct_boyut, 0);

        LLVMSembolGirişi *malloc_fn = llvm_sembol_bul(u, "malloc");
        if (malloc_fn) {
            LLVMValueRef malloc_args[] = { boyut_val };
            LLVMValueRef raw_ptr = LLVMBuildCall2(u->olusturucu, malloc_fn->tip,
                                                   malloc_fn->deger, malloc_args, 1, "raw_ptr");
            LLVMValueRef nesne_ptr = LLVMBuildBitCast(u->olusturucu, raw_ptr,
                                                       sinif_ptr_tipi, "nesne_ptr");

            /* Alanları doldur */
            for (int i = 0; i < alan_sayisi; i++) {
                LLVMValueRef param_deger = LLVMGetParam(yapici_fn, i);
                LLVMValueRef sifir = LLVMConstInt(LLVMInt32TypeInContext(u->baglam), 0, 0);
                LLVMValueRef idx = LLVMConstInt(LLVMInt32TypeInContext(u->baglam), i, 0);
                LLVMValueRef field_indices[] = { sifir, idx };
                LLVMValueRef alan_ptr = LLVMBuildGEP2(u->olusturucu, bilgi->struct_tipi,
                                                       nesne_ptr, field_indices, 2, "alan_ptr");
                LLVMBuildStore(u->olusturucu, param_deger, alan_ptr);
            }

            LLVMBuildRet(u->olusturucu, nesne_ptr);
        } else {
            LLVMBuildRet(u->olusturucu, LLVMConstNull(sinif_ptr_tipi));
        }

        u->mevcut_islev = onceki_islev;
    }

    /* Mevcut sınıf adını kaydet */
    u->mevcut_sinif = sınıf_adı;

    /* Ebeveyn metotlarını child sınıfa da kaydet (kalıtım) */
    if (ebeveyn_bilgi) {
        char *ebeveyn_adi = dugum->veri.sinif.ebeveyn;
        /* Tüm Ebeveyn_metot sembolleri için Cocuk_metot alias oluştur */
        LLVMSembolTablosu *tablo = u->sembol_tablosu;
        while (tablo) {
            LLVMSembolGirişi *g = tablo->girisler;
            while (g) {
                /* "EbeveynAdi_" ile başlayan sembolleri bul */
                int ebeveyn_len = (int)strlen(ebeveyn_adi);
                if (strncmp(g->isim, ebeveyn_adi, ebeveyn_len) == 0 &&
                    g->isim[ebeveyn_len] == '_') {
                    char *metot_adi = g->isim + ebeveyn_len + 1;
                    char mangled[256];
                    snprintf(mangled, sizeof(mangled), "%s_%s", sınıf_adı, metot_adi);
                    /* Eğer child zaten bu isimde metot tanımlamadıysa, alias ekle */
                    if (!llvm_sembol_bul(u, mangled)) {
                        llvm_sembol_ekle(u, mangled, g->deger, g->tip, 0, 1);
                    }
                }
                g = g->sonraki;
            }
            tablo = tablo->ust;
        }
    }

    /* Metotları üret */
    for (int i = 0; i < dugum->çocuk_sayısı; i++) {
        Düğüm *cocuk = dugum->çocuklar[i];
        if (cocuk->tur == DÜĞÜM_İŞLEV) {
            /* Metot adını mangle et: SinifAdi_metotAdi */
            char *orijinal_isim = cocuk->veri.islev.isim;
            char mangled[256];
            snprintf(mangled, sizeof(mangled), "%s_%s", sınıf_adı, orijinal_isim);
            cocuk->veri.islev.isim = arena_strdup(u->arena, mangled);

            llvm_işlev_üret(u, cocuk);
        }
    }

    u->mevcut_sinif = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 8: GLOBAL DEĞİŞKENLER
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_global_degisken_uret - Global değişken tanımı üret
 */
void llvm_global_degisken_uret(LLVMÜretici *u, Düğüm *dugum) {
    if (!dugum || dugum->tur != DÜĞÜM_DEĞİŞKEN) return;

    char *isim = dugum->veri.değişken.isim;
    char *tip_adı = dugum->veri.değişken.tip;
    LLVMTypeRef tip = llvm_tip_dönüştür(u, tip_adı);

    /* Global değişken oluştur */
    LLVMValueRef global = LLVMAddGlobal(u->modul, tip, isim);

    /* Başlangıç değeri */
    if (dugum->çocuk_sayısı > 0 && dugum->çocuklar[0]) {
        LLVMValueRef deger = llvm_ifade_uret(u, dugum->çocuklar[0]);
        if (deger) {
            LLVMSetInitializer(global, deger);
        } else {
            LLVMSetInitializer(global, LLVMConstNull(tip));
        }
    } else {
        LLVMSetInitializer(global, LLVMConstNull(tip));
    }

    /* Sembol tablosuna ekle */
    llvm_sembol_ekle(u, isim, global, tip, 0, 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 9: PROGRAM ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_program_üret - Tüm programı üret
 */
void llvm_program_üret(LLVMÜretici *u, Düğüm *program) {
    if (!program || program->tur != DÜĞÜM_PROGRAM) return;

    /* Statik durumları sıfırla */
    llvm_varsayilan_sayisi = 0;

    /* Ön geçiş: Modül fonksiyonlarını kaydet (kullan deyimleri) */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *dugum = program->çocuklar[i];
        if (dugum->tur == DÜĞÜM_KULLAN && dugum->veri.kullan.modul) {
            llvm_modul_kaydet(u, dugum->veri.kullan.modul);
        }
    }

    /* İlk geçiş: Sınıf tanımlarını topla */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *dugum = program->çocuklar[i];
        if (dugum->tur == DÜĞÜM_SINIF) {
            llvm_sınıf_üret(u, dugum);
        }
    }

    /* İkinci geçiş: TÜM üst düzey değişkenleri LLVM global olarak kaydet */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *dugum = program->çocuklar[i];
        if (dugum->tur == DÜĞÜM_DEĞİŞKEN) {
            char *isim = dugum->veri.değişken.isim;
            char *tip_adı = dugum->veri.değişken.tip;
            LLVMTypeRef tip = llvm_tip_dönüştür(u, tip_adı);
            LLVMValueRef global = LLVMAddGlobal(u->modul, tip, isim);
            LLVMSetInitializer(global, LLVMConstNull(tip));
            llvm_sembol_ekle(u, isim, global, tip, 0, 1);
        }
    }

    /* Üçüncü geçiş: Global başlatıcılar ve işlevler */
    int ana_islev_var = 0;
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *dugum = program->çocuklar[i];

        switch (dugum->tur) {
            case DÜĞÜM_İŞLEV:
                llvm_işlev_üret(u, dugum);
                if (strcmp(dugum->veri.islev.isim, "main") == 0) {
                    ana_islev_var = 1;
                }
                break;

            case DÜĞÜM_DEĞİŞKEN:
                /* Zaten ikinci geçişte kaydedildi, sabit başlatıcıyı güncelle.
                 * Karmaşık tipler (dizi, metin, vb.) builder gerektirdiğinden
                 * burada atlanır — main() içinde yerel olarak başlatılır. */
                if (dugum->çocuk_sayısı > 0 && dugum->çocuklar[0]) {
                    Düğüm *init = dugum->çocuklar[0];
                    /* Sadece sabit ifadeler: tam sayı, ondalık, mantık */
                    if (init->tur == DÜĞÜM_TAM_SAYI || init->tur == DÜĞÜM_ONDALIK_SAYI ||
                        init->tur == DÜĞÜM_MANTIK_DEĞERİ) {
                        char *isim = dugum->veri.değişken.isim;
                        LLVMValueRef global = LLVMGetNamedGlobal(u->modul, isim);
                        if (global) {
                            LLVMValueRef deger = llvm_ifade_uret(u, init);
                            if (deger) LLVMSetInitializer(global, deger);
                        }
                    }
                }
                break;

            case DÜĞÜM_SINIF:
                /* Zaten işlendi */
                break;

            case DÜĞÜM_SAYIM: {
                /* Sayım (enum) tanımı: her değeri global olarak kaydet */
                LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);
                for (int j = 0; j < dugum->çocuk_sayısı; j++) {
                    Düğüm *deger = dugum->çocuklar[j];
                    if (deger && deger->veri.tanimlayici.isim) {
                        const char *s_isim = deger->veri.tanimlayici.isim;
                        LLVMValueRef global = LLVMGetNamedGlobal(u->modul, s_isim);
                        if (!global) {
                            global = LLVMAddGlobal(u->modul, i64, s_isim);
                            LLVMSetInitializer(global, LLVMConstInt(i64, (unsigned long long)j, 0));
                        }
                        llvm_sembol_ekle(u, s_isim, global, i64, 0, 1);
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    /* Monomorphization: Özelleştirilmiş generic fonksiyonları üret */
    if (u->generic_ozellestirilmisler && u->generic_ozellestirme_sayisi > 0) {
        GenericÖzelleştirme *ozler = (GenericÖzelleştirme *)u->generic_ozellestirilmisler;
        for (int gi = 0; gi < u->generic_ozellestirme_sayisi; gi++) {
            GenericÖzelleştirme *oz = &ozler[gi];
            if (!oz->uretildi && oz->orijinal_dugum) {
                Düğüm *d = oz->orijinal_dugum;
                /* Orijinal isim ve tip_parametre'yi geçici olarak değiştir */
                char *eski_isim = d->veri.islev.isim;
                char *eski_tp = d->veri.islev.tip_parametre;
                char *eski_donus = d->veri.islev.dönüş_tipi;

                d->veri.islev.isim = oz->ozel_isim;
                d->veri.islev.tip_parametre = NULL; /* Generic bayrağını kaldır */

                /* Dönüş tipi T ise somut tipe çevir */
                if (eski_donus && oz->tip_parametre &&
                    strcmp(eski_donus, oz->tip_parametre) == 0) {
                    d->veri.islev.dönüş_tipi = oz->somut_tip;
                }

                /* Parametre tiplerini geçici olarak değiştir */
                char *param_eski_tipler[32];
                int param_sayisi = 0;
                if (d->çocuk_sayısı > 0 && d->çocuklar[0]) {
                    Düğüm *params = d->çocuklar[0];
                    param_sayisi = params->çocuk_sayısı;
                    for (int pi = 0; pi < param_sayisi && pi < 32; pi++) {
                        param_eski_tipler[pi] = params->çocuklar[pi]->veri.değişken.tip;
                        if (params->çocuklar[pi]->veri.değişken.tip &&
                            oz->tip_parametre &&
                            strcmp(params->çocuklar[pi]->veri.değişken.tip, oz->tip_parametre) == 0) {
                            params->çocuklar[pi]->veri.değişken.tip = oz->somut_tip;
                        }
                    }
                }

                /* Fonksiyonu üret */
                llvm_işlev_üret(u, d);
                oz->uretildi = 1;

                /* Orijinal değerleri geri yükle */
                d->veri.islev.isim = eski_isim;
                d->veri.islev.tip_parametre = eski_tp;
                d->veri.islev.dönüş_tipi = eski_donus;
                if (d->çocuk_sayısı > 0 && d->çocuklar[0]) {
                    Düğüm *params = d->çocuklar[0];
                    for (int pi = 0; pi < param_sayisi && pi < 32; pi++) {
                        params->çocuklar[pi]->veri.değişken.tip = param_eski_tipler[pi];
                    }
                }
            }
        }
    }

    /* Test modu: Test düğümlerini topla ve global'ları oluştur */
    int test_indeksler[256];
    int test_toplam = 0;

    if (u->test_modu) {
        for (int i = 0; i < program->çocuk_sayısı; i++) {
            if (program->çocuklar[i]->tur == DÜĞÜM_TEST && test_toplam < 256) {
                test_indeksler[test_toplam++] = i;
            }
        }

        if (test_toplam > 0) {
            /* Global değişkenler: _test_isim_ptr (i8*), _test_isim_len (i64) */
            LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
            LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);

            LLVMValueRef g_ptr = LLVMAddGlobal(u->modul, i8_ptr, "_test_isim_ptr");
            LLVMSetInitializer(g_ptr, LLVMConstNull(i8_ptr));

            LLVMValueRef g_len = LLVMAddGlobal(u->modul, i64, "_test_isim_len");
            LLVMSetInitializer(g_len, LLVMConstInt(i64, 0, 0));
        }
    }

    /* Test fonksiyonlarını üret (main'den önce) */
    LLVMValueRef test_fonksiyonlar[256];
    if (u->test_modu && test_toplam > 0) {
        LLVMTypeRef void_tip = LLVMVoidTypeInContext(u->baglam);
        LLVMTypeRef test_fn_tip = LLVMFunctionType(void_tip, NULL, 0, 0);
        LLVMTypeRef i8_ptr = LLVMPointerType(LLVMInt8TypeInContext(u->baglam), 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(u->baglam);

        for (int t = 0; t < test_toplam; t++) {
            Düğüm *td = program->çocuklar[test_indeksler[t]];
            char *test_isim = td->veri.test.isim;
            int isim_len = (int)strlen(test_isim);

            /* Test fonksiyonu oluştur: _test_N */
            char fn_adi[32];
            snprintf(fn_adi, sizeof(fn_adi), "_test_%d", t);
            LLVMValueRef test_fn = LLVMAddFunction(u->modul, fn_adi, test_fn_tip);
            test_fonksiyonlar[t] = test_fn;

            LLVMBasicBlockRef giris = LLVMAppendBasicBlockInContext(
                u->baglam, test_fn, "giris");
            LLVMPositionBuilderAtEnd(u->olusturucu, giris);

            u->mevcut_islev = test_fn;
            llvm_kapsam_gir(u);

            /* Test ismini global string olarak oluştur */
            LLVMValueRef isim_str = LLVMBuildGlobalStringPtr(
                u->olusturucu, test_isim, "test_isim");

            /* _test_isim_ptr ve _test_isim_len global'larını güncelle */
            LLVMValueRef g_ptr = LLVMGetNamedGlobal(u->modul, "_test_isim_ptr");
            LLVMValueRef g_len = LLVMGetNamedGlobal(u->modul, "_test_isim_len");
            LLVMBuildStore(u->olusturucu, isim_str, g_ptr);
            LLVMBuildStore(u->olusturucu,
                LLVMConstInt(i64, (unsigned long long)isim_len, 0), g_len);

            /* Test gövdesini üret */
            if (td->çocuk_sayısı > 0) {
                llvm_blok_uret(u, td->çocuklar[0]);
            }

            llvm_kapsam_cik(u);

            /* Terminatör yoksa ekle */
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
                LLVMBuildRetVoid(u->olusturucu);
            }
        }
    }

    /* Ana işlev yoksa oluştur (üst düzey kod için) */
    if (!ana_islev_var) {
        /* main işlevi oluştur */
        LLVMTypeRef main_tip = LLVMFunctionType(
            LLVMInt32TypeInContext(u->baglam),
            NULL, 0, 0
        );
        LLVMValueRef main_fn = LLVMAddFunction(u->modul, "main", main_tip);
        LLVMBasicBlockRef giris = LLVMAppendBasicBlockInContext(
            u->baglam, main_fn, "giris");
        LLVMPositionBuilderAtEnd(u->olusturucu, giris);

        u->mevcut_islev = main_fn;
        llvm_kapsam_gir(u);

        /* Üst düzey deyimleri işle */
        for (int i = 0; i < program->çocuk_sayısı; i++) {
            Düğüm *dugum = program->çocuklar[i];
            if (dugum->tur != DÜĞÜM_İŞLEV &&
                dugum->tur != DÜĞÜM_SINIF &&
                dugum->tur != DÜĞÜM_KULLAN &&
                dugum->tur != DÜĞÜM_TEST) {
                llvm_deyim_uret(u, dugum);
            }
        }

        /* Test modu: test fonksiyonlarını çağır + rapor */
        if (u->test_modu && test_toplam > 0) {
            LLVMTypeRef void_tip = LLVMVoidTypeInContext(u->baglam);
            LLVMTypeRef test_fn_tip = LLVMFunctionType(void_tip, NULL, 0, 0);
            for (int t = 0; t < test_toplam; t++) {
                LLVMBuildCall2(u->olusturucu, test_fn_tip,
                    test_fonksiyonlar[t], NULL, 0, "");
            }
            /* _tr_test_rapor() çağır */
            LLVMSembolGirişi *rapor_fn = llvm_sembol_bul(u, "_tr_test_rapor");
            if (rapor_fn) {
                LLVMBuildCall2(u->olusturucu, rapor_fn->tip,
                    rapor_fn->deger, NULL, 0, "");
            }
        }

        llvm_kapsam_cik(u);

        /* return 0 */
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(u->olusturucu))) {
            LLVMBuildRet(u->olusturucu,
                LLVMConstInt(LLVMInt32TypeInContext(u->baglam), 0, 0));
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 10: DOSYA ÇIKTISI VE OPTİMİZASYON
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_modul_dogrula - Modülü doğrula
 */
int llvm_modul_dogrula(LLVMÜretici *u) {
    char *hata = NULL;
    if (LLVMVerifyModule(u->modul, LLVMReturnStatusAction, &hata)) {
        fprintf(stderr, "LLVM doğrulama hatası:\n%s\n", hata);
        LLVMDisposeMessage(hata);
        return 0;
    }
    return 1;
}

/*
 * llvm_optimizasyonlari_uygula - Optimizasyonları uygula
 */
void llvm_optimizasyonlari_uygula(LLVMÜretici *u) {
    if (u->optimizasyon_seviyesi <= 0) return;

    /* LLVM pass builder kullanarak optimizasyonları uygula */
    LLVMPassBuilderOptionsRef secenekler = LLVMCreatePassBuilderOptions();

    /* Optimizasyon seviyesine göre pass string'i oluştur */
    char pass_str[64];
    switch (u->optimizasyon_seviyesi) {
        case 1:
            snprintf(pass_str, sizeof(pass_str), "default<O1>");
            break;
        case 2:
            snprintf(pass_str, sizeof(pass_str), "default<O2>");
            break;
        case 3:
            snprintf(pass_str, sizeof(pass_str), "default<O3>");
            break;
        default:
            snprintf(pass_str, sizeof(pass_str), "default<O2>");
            break;
    }

    LLVMErrorRef hata = LLVMRunPasses(u->modul, pass_str,
                                       u->hedef_makine, secenekler);
    if (hata) {
        char *mesaj = LLVMGetErrorMessage(hata);
        fprintf(stderr, "Optimizasyon hatası: %s\n", mesaj);
        LLVMDisposeErrorMessage(mesaj);
    }

    LLVMDisposePassBuilderOptions(secenekler);
}

/*
 * llvm_ir_yazdir - IR'ı konsola yazdır (debug için)
 */
void llvm_ir_yazdir(LLVMÜretici *u) {
    char *ir = LLVMPrintModuleToString(u->modul);
    printf("%s", ir);
    LLVMDisposeMessage(ir);
}

/*
 * llvm_ir_dosyaya_yaz - LLVM IR'ı .ll dosyasına yaz
 */
int llvm_ir_dosyaya_yaz(LLVMÜretici *u, const char *dosya_adi) {
    char *hata = NULL;
    if (LLVMPrintModuleToFile(u->modul, dosya_adi, &hata)) {
        fprintf(stderr, "IR yazma hatası: %s\n", hata);
        LLVMDisposeMessage(hata);
        return 0;
    }
    return 1;
}

/*
 * llvm_bitcode_dosyaya_yaz - LLVM bitcode'u .bc dosyasına yaz
 */
int llvm_bitcode_dosyaya_yaz(LLVMÜretici *u, const char *dosya_adi) {
    if (LLVMWriteBitcodeToFile(u->modul, dosya_adi) != 0) {
        fprintf(stderr, "Bitcode yazma hatası: %s\n", dosya_adi);
        return 0;
    }
    return 1;
}

/*
 * llvm_nesne_dosyasi_uret - Nesne dosyası (.o) üret
 */
int llvm_nesne_dosyasi_uret(LLVMÜretici *u, const char *dosya_adi) {
    if (!u->hedef_makine) {
        fprintf(stderr, "Hata: Hedef makine yapılandırılmamış\n");
        return 0;
    }

    char *hata = NULL;
    if (LLVMTargetMachineEmitToFile(u->hedef_makine, u->modul,
                                     (char *)dosya_adi,
                                     LLVMObjectFile, &hata)) {
        fprintf(stderr, "Nesne dosyası üretme hatası: %s\n", hata);
        LLVMDisposeMessage(hata);
        return 0;
    }

    return 1;
}

/*
 * llvm_asm_dosyasi_uret - Assembly dosyası (.s) üret
 */
int llvm_asm_dosyasi_uret(LLVMÜretici *u, const char *dosya_adi) {
    if (!u->hedef_makine) {
        fprintf(stderr, "Hata: Hedef makine yapılandırılmamış\n");
        return 0;
    }

    char *hata = NULL;
    if (LLVMTargetMachineEmitToFile(u->hedef_makine, u->modul,
                                     (char *)dosya_adi,
                                     LLVMAssemblyFile, &hata)) {
        fprintf(stderr, "Assembly dosyası üretme hatası: %s\n", hata);
        LLVMDisposeMessage(hata);
        return 0;
    }

    return 1;
}

/*
 * llvm_blok_olustur - Yeni temel blok oluştur
 */
LLVMBasicBlockRef llvm_blok_olustur(LLVMÜretici *u, const char *isim) {
    char etiket[64];
    snprintf(etiket, sizeof(etiket), "%s_%d", isim, u->etiket_sayaci++);
    return LLVMAppendBasicBlockInContext(u->baglam, u->mevcut_islev, etiket);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 11: IR DOĞRULAMA (VERIFICATION)
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_ir_dogrulama_ayarla - IR doğrulamayı etkinleştir/devre dışı bırak
 */
void llvm_ir_dogrulama_ayarla(LLVMÜretici *u, int etkin) {
    u->ir_dogrula = etkin;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 12: DEBUG BİLGİSİ (DWARF)
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_debug_baslat - Debug bilgisi üretimini başlat
 */
int llvm_debug_baslat(LLVMÜretici *u, const char *kaynak_dosya) {
    if (!u->hata_ayiklama) return 1;  /* Debug devre dışı */

    /* Debug bilgisi yapısını oluştur */
    u->debug_bilgi = (LLVMDebugBilgisi *)arena_ayir(u->arena, sizeof(LLVMDebugBilgisi));
    if (!u->debug_bilgi) return 0;

    /* DIBuilder oluştur */
    u->debug_bilgi->di_builder = LLVMCreateDIBuilder(u->modul);
    if (!u->debug_bilgi->di_builder) {
        fprintf(stderr, "Hata: DIBuilder oluşturulamadı\n");
        return 0;
    }

    /* Kaynak dosya adını ayır */
    const char *dosya_adi = kaynak_dosya;
    const char *son_slash = strrchr(kaynak_dosya, '/');
    if (son_slash) dosya_adi = son_slash + 1;

    /* Dizin yolunu çıkar */
    char dizin[512] = ".";
    if (son_slash) {
        int dizin_uzunluk = (int)(son_slash - kaynak_dosya);
        if (dizin_uzunluk >= (int)sizeof(dizin)) dizin_uzunluk = sizeof(dizin) - 1;
        memcpy(dizin, kaynak_dosya, dizin_uzunluk);
        dizin[dizin_uzunluk] = '\0';
    }

    /* Dosya metadata oluştur */
    u->debug_bilgi->dosya = LLVMDIBuilderCreateFile(
        u->debug_bilgi->di_builder,
        dosya_adi, strlen(dosya_adi),
        dizin, strlen(dizin)
    );

    /* Derleme birimi (compile unit) oluştur */
    u->debug_bilgi->derleme_birimi = LLVMDIBuilderCreateCompileUnit(
        u->debug_bilgi->di_builder,
        LLVMDWARFSourceLanguageC,           /* Dil (C olarak göster) */
        u->debug_bilgi->dosya,              /* Dosya */
        "Tonyukuk Derleyici", 19,           /* Üretici adı */
        0,                                   /* Optimizasyon yok (debug için) */
        "", 0,                              /* Flags */
        0,                                   /* Runtime version */
        "", 0,                              /* Split name */
        LLVMDWARFEmissionFull,              /* Emission kind */
        0,                                   /* DWO id */
        0,                                   /* Split debug inlining */
        0,                                   /* Debug info for profiling */
        "", 0,                              /* Sys root */
        "", 0                               /* SDK */
    );

    /* Kapsam yığınını başlat */
    u->debug_bilgi->kapsam_yigini = (LLVMMetadataRef *)arena_ayir(
        u->arena, sizeof(LLVMMetadataRef) * 64);
    u->debug_bilgi->kapsam_derinlik = 0;
    u->debug_bilgi->kapsam_yigini[0] = u->debug_bilgi->derleme_birimi;

    /* Debug DWARF version ayarla */
    LLVMAddModuleFlag(u->modul, LLVMModuleFlagBehaviorWarning,
                      "Dwarf Version", 13,
                      LLVMValueAsMetadata(LLVMConstInt(
                          LLVMInt32TypeInContext(u->baglam), 5, 0)));

    LLVMAddModuleFlag(u->modul, LLVMModuleFlagBehaviorWarning,
                      "Debug Info Version", 18,
                      LLVMValueAsMetadata(LLVMConstInt(
                          LLVMInt32TypeInContext(u->baglam), 3, 0)));

    return 1;
}

/*
 * llvm_debug_sonlandir - Debug bilgisi üretimini sonlandır
 */
void llvm_debug_sonlandir(LLVMÜretici *u) {
    if (!u->debug_bilgi || !u->debug_bilgi->di_builder) return;

    LLVMDIBuilderFinalize(u->debug_bilgi->di_builder);
}

/*
 * llvm_debug_satir_ayarla - Mevcut satır/sütun bilgisini ayarla
 */
void llvm_debug_satir_ayarla(LLVMÜretici *u, int satir, int sutun) {
    u->mevcut_satir = satir;
    u->mevcut_sutun = sutun;

    if (!u->debug_bilgi || !u->debug_bilgi->di_builder) return;
    if (satir <= 0) return;
    if (u->debug_bilgi->kapsam_derinlik <= 0) return;  /* Kapsam yoksa çık */

    /* Mevcut kapsam (fonksiyon) */
    LLVMMetadataRef kapsam = u->debug_bilgi->kapsam_yigini[
        u->debug_bilgi->kapsam_derinlik
    ];

    /* Debug konum oluştur */
    LLVMMetadataRef konum = LLVMDIBuilderCreateDebugLocation(
        u->baglam,
        satir,
        sutun > 0 ? sutun : 0,
        kapsam,
        NULL  /* InlinedAt */
    );

    /* Builder'a konum bilgisini ekle */
    LLVMSetCurrentDebugLocation2(u->olusturucu, konum);
}

/*
 * llvm_debug_fonksiyon_olustur - Fonksiyon için debug bilgisi
 */
LLVMMetadataRef llvm_debug_fonksiyon_olustur(LLVMÜretici *u, const char *isim,
                                              int satir, LLVMTypeRef tip) {
    if (!u->debug_bilgi || !u->debug_bilgi->di_builder) return NULL;
    (void)tip;  /* Şimdilik kullanılmıyor */

    /* Subroutine tipi oluştur */
    LLVMMetadataRef *param_tipleri = NULL;
    LLVMMetadataRef sr_tip = LLVMDIBuilderCreateSubroutineType(
        u->debug_bilgi->di_builder,
        u->debug_bilgi->dosya,
        param_tipleri, 0,
        LLVMDIFlagZero
    );

    /* Fonksiyon debug bilgisi */
    LLVMMetadataRef sp = LLVMDIBuilderCreateFunction(
        u->debug_bilgi->di_builder,
        u->debug_bilgi->dosya,              /* Scope (dosya) */
        isim, strlen(isim),                 /* İsim */
        isim, strlen(isim),                 /* Linkage name */
        u->debug_bilgi->dosya,              /* Dosya */
        satir,                              /* Satır */
        sr_tip,                             /* Tip */
        0,                                  /* IsLocalToUnit */
        1,                                  /* IsDefinition */
        satir,                              /* Scope line */
        LLVMDIFlagPrototyped,               /* Flags */
        0                                   /* IsOptimized */
    );

    /* Kapsamı yığına ekle */
    if (u->debug_bilgi->kapsam_derinlik < 63) {
        u->debug_bilgi->kapsam_yigini[++u->debug_bilgi->kapsam_derinlik] = sp;
    }

    return sp;
}

/*
 * llvm_debug_degisken_tanimla - Değişken için debug bilgisi
 */
void llvm_debug_degisken_tanimla(LLVMÜretici *u, const char *isim,
                                  LLVMValueRef alloca, int satir) {
    if (!u->debug_bilgi || !u->debug_bilgi->di_builder) return;
    if (!alloca) return;

    /* Basit i64 tipi için debug tipi */
    /* DW_ATE_signed = 5 (DWARF standardı) */
    LLVMMetadataRef debug_tip = LLVMDIBuilderCreateBasicType(
        u->debug_bilgi->di_builder,
        "tam", 3,
        64,                 /* Bit boyutu */
        5,                  /* DW_ATE_signed */
        LLVMDIFlagZero
    );

    /* Mevcut kapsam */
    if (u->debug_bilgi->kapsam_derinlik <= 0) return;
    LLVMMetadataRef kapsam = u->debug_bilgi->kapsam_yigini[
        u->debug_bilgi->kapsam_derinlik
    ];

    /* Yerel değişken oluştur */
    LLVMMetadataRef var = LLVMDIBuilderCreateAutoVariable(
        u->debug_bilgi->di_builder,
        kapsam,
        isim, strlen(isim),
        u->debug_bilgi->dosya,
        satir,
        debug_tip,
        0,              /* AlwaysPreserve */
        LLVMDIFlagZero,
        0               /* AlignInBits */
    );

    /* Debug declare ekle */
    LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(
        u->debug_bilgi->di_builder, NULL, 0);

    LLVMMetadataRef konum = LLVMDIBuilderCreateDebugLocation(
        u->baglam, satir, 0, kapsam, NULL);

    LLVMDIBuilderInsertDeclareAtEnd(
        u->debug_bilgi->di_builder,
        alloca,
        var,
        expr,
        konum,
        LLVMGetInsertBlock(u->olusturucu)
    );
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 13: JIT DERLEME
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_jit_baslat - JIT motorunu başlat
 */
int llvm_jit_baslat(LLVMÜretici *u) {
    u->jit_bilgi = (LLVMJITBilgisi *)arena_ayir(u->arena, sizeof(LLVMJITBilgisi));
    if (!u->jit_bilgi) return 0;

    /* LLJIT oluştur */
    LLVMErrorRef hata = LLVMOrcCreateLLJIT(&u->jit_bilgi->lljit, NULL);
    if (hata) {
        char *mesaj = LLVMGetErrorMessage(hata);
        fprintf(stderr, "JIT başlatma hatası: %s\n", mesaj);
        LLVMDisposeErrorMessage(mesaj);
        return 0;
    }

    u->jit_bilgi->aktif = 1;
    return 1;
}

/*
 * llvm_jit_sonlandir - JIT motorunu sonlandır
 */
void llvm_jit_sonlandir(LLVMÜretici *u) {
    if (!u->jit_bilgi || !u->jit_bilgi->aktif) return;

    if (u->jit_bilgi->lljit) {
        LLVMOrcDisposeLLJIT(u->jit_bilgi->lljit);
    }
    u->jit_bilgi->aktif = 0;
}

/*
 * llvm_jit_modul_ekle - Modülü JIT'e ekle
 */
int llvm_jit_modul_ekle(LLVMÜretici *u) {
    if (!u->jit_bilgi || !u->jit_bilgi->aktif) return 0;

    /* Thread-safe modül oluştur */
    LLVMOrcThreadSafeContextRef ts_ctx = LLVMOrcCreateNewThreadSafeContext();
    LLVMOrcThreadSafeModuleRef ts_mod = LLVMOrcCreateNewThreadSafeModule(
        LLVMCloneModule(u->modul), ts_ctx);

    /* Ana JIT dylib al */
    LLVMOrcJITDylibRef main_dylib = LLVMOrcLLJITGetMainJITDylib(u->jit_bilgi->lljit);

    /* Modülü ekle */
    LLVMErrorRef hata = LLVMOrcLLJITAddLLVMIRModule(
        u->jit_bilgi->lljit, main_dylib, ts_mod);

    if (hata) {
        char *mesaj = LLVMGetErrorMessage(hata);
        fprintf(stderr, "JIT modül ekleme hatası: %s\n", mesaj);
        LLVMDisposeErrorMessage(mesaj);
        return 0;
    }

    return 1;
}

/*
 * llvm_jit_fonksiyon_al - JIT'ten fonksiyon adresini al
 */
void *llvm_jit_fonksiyon_al(LLVMÜretici *u, const char *isim) {
    if (!u->jit_bilgi || !u->jit_bilgi->aktif) return NULL;

    LLVMOrcExecutorAddress adres;
    LLVMErrorRef hata = LLVMOrcLLJITLookup(u->jit_bilgi->lljit, &adres, isim);

    if (hata) {
        char *mesaj = LLVMGetErrorMessage(hata);
        fprintf(stderr, "JIT fonksiyon arama hatası (%s): %s\n", isim, mesaj);
        LLVMDisposeErrorMessage(mesaj);
        return NULL;
    }

    return (void *)(uintptr_t)adres;
}

/*
 * llvm_jit_calistir - JIT ile main fonksiyonunu çalıştır
 */
int llvm_jit_calistir(LLVMÜretici *u) {
    if (!llvm_jit_modul_ekle(u)) return -1;

    /* main fonksiyonunu bul */
    int (*main_fn)(void) = (int (*)(void))llvm_jit_fonksiyon_al(u, "main");
    if (!main_fn) {
        fprintf(stderr, "JIT: 'main' fonksiyonu bulunamadı\n");
        return -1;
    }

    /* Çalıştır */
    return main_fn();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 14: TİP DÖNÜŞÜM YARDIMCILARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_tipler_uyumlu_mu - İki tipin uyumlu olup olmadığını kontrol et
 */
int llvm_tipler_uyumlu_mu(LLVMTypeRef tip1, LLVMTypeRef tip2) {
    if (tip1 == tip2) return 1;

    LLVMTypeKind t1 = LLVMGetTypeKind(tip1);
    LLVMTypeKind t2 = LLVMGetTypeKind(tip2);

    /* Integer türleri arasında uyumluluk */
    if (t1 == LLVMIntegerTypeKind && t2 == LLVMIntegerTypeKind) {
        return 1;  /* Boyut farkı olabilir ama dönüşüm yapılabilir */
    }

    /* Float türleri arasında uyumluluk */
    if ((t1 == LLVMFloatTypeKind || t1 == LLVMDoubleTypeKind) &&
        (t2 == LLVMFloatTypeKind || t2 == LLVMDoubleTypeKind)) {
        return 1;
    }

    /* Integer ve Float arasında uyumluluk */
    if ((t1 == LLVMIntegerTypeKind &&
         (t2 == LLVMFloatTypeKind || t2 == LLVMDoubleTypeKind)) ||
        (t2 == LLVMIntegerTypeKind &&
         (t1 == LLVMFloatTypeKind || t1 == LLVMDoubleTypeKind))) {
        return 1;
    }

    return 0;
}

/*
 * llvm_örtük_dönüşüm - Örtük (implicit) tip dönüşümü
 */
LLVMValueRef llvm_örtük_dönüşüm(LLVMÜretici *u, LLVMValueRef deger,
                                 LLVMTypeRef hedef_tip) {
    if (!deger || !hedef_tip) return deger;

    LLVMTypeRef kaynak_tip = LLVMTypeOf(deger);
    if (kaynak_tip == hedef_tip) return deger;

    LLVMTypeKind kaynak_kind = LLVMGetTypeKind(kaynak_tip);
    LLVMTypeKind hedef_kind = LLVMGetTypeKind(hedef_tip);

    /* Integer genişletme/daraltma */
    if (kaynak_kind == LLVMIntegerTypeKind && hedef_kind == LLVMIntegerTypeKind) {
        unsigned kaynak_bits = LLVMGetIntTypeWidth(kaynak_tip);
        unsigned hedef_bits = LLVMGetIntTypeWidth(hedef_tip);

        if (kaynak_bits < hedef_bits) {
            /* Genişletme (sign extend) */
            return LLVMBuildSExt(u->olusturucu, deger, hedef_tip, "sext");
        } else if (kaynak_bits > hedef_bits) {
            /* Daraltma (truncate) */
            return LLVMBuildTrunc(u->olusturucu, deger, hedef_tip, "trunc");
        }
    }

    /* Integer -> Float */
    if (kaynak_kind == LLVMIntegerTypeKind &&
        (hedef_kind == LLVMFloatTypeKind || hedef_kind == LLVMDoubleTypeKind)) {
        return LLVMBuildSIToFP(u->olusturucu, deger, hedef_tip, "sitofp");
    }

    /* Float -> Integer */
    if ((kaynak_kind == LLVMFloatTypeKind || kaynak_kind == LLVMDoubleTypeKind) &&
        hedef_kind == LLVMIntegerTypeKind) {
        return LLVMBuildFPToSI(u->olusturucu, deger, hedef_tip, "fptosi");
    }

    /* Float genişletme/daraltma */
    if ((kaynak_kind == LLVMFloatTypeKind || kaynak_kind == LLVMDoubleTypeKind) &&
        (hedef_kind == LLVMFloatTypeKind || hedef_kind == LLVMDoubleTypeKind)) {
        if (kaynak_kind == LLVMFloatTypeKind && hedef_kind == LLVMDoubleTypeKind) {
            return LLVMBuildFPExt(u->olusturucu, deger, hedef_tip, "fpext");
        } else {
            return LLVMBuildFPTrunc(u->olusturucu, deger, hedef_tip, "fptrunc");
        }
    }

    /* Bool (i1) -> Integer */
    if (kaynak_kind == LLVMIntegerTypeKind &&
        LLVMGetIntTypeWidth(kaynak_tip) == 1 &&
        hedef_kind == LLVMIntegerTypeKind) {
        return LLVMBuildZExt(u->olusturucu, deger, hedef_tip, "zext_bool");
    }

    /* Integer -> Bool (i1) */
    if (kaynak_kind == LLVMIntegerTypeKind &&
        hedef_kind == LLVMIntegerTypeKind &&
        LLVMGetIntTypeWidth(hedef_tip) == 1) {
        /* Sıfırla karşılaştır */
        LLVMValueRef sifir = LLVMConstInt(kaynak_tip, 0, 0);
        return LLVMBuildICmp(u->olusturucu, LLVMIntNE, deger, sifir, "tobool");
    }

    return deger;  /* Dönüşüm yapılamadı */
}

/*
 * llvm_açık_dönüşüm - Açık (explicit) tip dönüşümü
 */
LLVMValueRef llvm_açık_dönüşüm(LLVMÜretici *u, LLVMValueRef deger,
                                LLVMTypeRef hedef_tip, const char *tip_adı) {
    (void)tip_adı;  /* Hata mesajları için kullanılabilir */

    /* Önce örtük dönüşümü dene */
    LLVMValueRef sonuç = llvm_örtük_dönüşüm(u, deger, hedef_tip);
    if (sonuç != deger) return sonuç;

    /* Pointer dönüşümleri */
    LLVMTypeKind kaynak_kind = LLVMGetTypeKind(LLVMTypeOf(deger));
    LLVMTypeKind hedef_kind = LLVMGetTypeKind(hedef_tip);

    /* Integer -> Pointer */
    if (kaynak_kind == LLVMIntegerTypeKind && hedef_kind == LLVMPointerTypeKind) {
        return LLVMBuildIntToPtr(u->olusturucu, deger, hedef_tip, "inttoptr");
    }

    /* Pointer -> Integer */
    if (kaynak_kind == LLVMPointerTypeKind && hedef_kind == LLVMIntegerTypeKind) {
        return LLVMBuildPtrToInt(u->olusturucu, deger, hedef_tip, "ptrtoint");
    }

    /* Bitcast (pointer türleri arası) */
    if (kaynak_kind == LLVMPointerTypeKind && hedef_kind == LLVMPointerTypeKind) {
        return LLVMBuildBitCast(u->olusturucu, deger, hedef_tip, "bitcast");
    }

    return deger;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÖLÜM 15: FRONTEND OPTİMİZASYONLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * llvm_frontend_optimizasyonlari - Frontend seviyesi optimizasyonlar
 *
 * Bu fonksiyon AST üzerinde çalışır ve LLVM'e daha temiz IR verir:
 * - Sabit katlama (constant folding)
 * - Ölü kod eleme (dead code elimination)
 * - Erişilemez blok temizleme
 */
void llvm_frontend_optimizasyonlari(LLVMÜretici *u, Düğüm *program) {
    if (!program || !u) return;
    (void)u;  /* Şimdilik kullanılmıyor */

    /* AST üzerinde optimizasyon geçişi */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *cocuk = program->çocuklar[i];
        if (!cocuk) continue;

        /* Sabit katlama: İkili işlemlerde her iki taraf da sabit ise hesapla */
        if (cocuk->tur == DÜĞÜM_İKİLİ_İŞLEM) {
            if (cocuk->çocuk_sayısı >= 2) {
                Düğüm *sol = cocuk->çocuklar[0];
                Düğüm *sag = cocuk->çocuklar[1];

                if (sol && sag &&
                    sol->tur == DÜĞÜM_TAM_SAYI &&
                    sag->tur == DÜĞÜM_TAM_SAYI) {

                    long long sol_val = sol->veri.tam_deger;
                    long long sag_val = sag->veri.tam_deger;
                    long long sonuç = 0;
                    int gecerli = 1;

                    switch (cocuk->veri.islem.islem) {
                        case TOK_ARTI:   sonuç = sol_val + sag_val; break;
                        case TOK_EKSI:   sonuç = sol_val - sag_val; break;
                        case TOK_ÇARPIM: sonuç = sol_val * sag_val; break;
                        case TOK_BÖLME:
                            if (sag_val != 0) sonuç = sol_val / sag_val;
                            else gecerli = 0;
                            break;
                        case TOK_YÜZDE:
                            if (sag_val != 0) sonuç = sol_val % sag_val;
                            else gecerli = 0;
                            break;
                        default: gecerli = 0;
                    }

                    if (gecerli) {
                        /* Düğümü sabit değere dönüştür */
                        cocuk->tur = DÜĞÜM_TAM_SAYI;
                        cocuk->veri.tam_deger = sonuç;
                        cocuk->çocuk_sayısı = 0;
                    }
                }
            }
        }

        /* Ölü kod eleme: return'den sonraki kodları işaretle */
        if (cocuk->tur == DÜĞÜM_İŞLEV && cocuk->çocuk_sayısı > 1) {
            Düğüm *govde = cocuk->çocuklar[1];
            if (govde) {
                int return_bulundu = 0;
                for (int j = 0; j < govde->çocuk_sayısı; j++) {
                    if (return_bulundu) {
                        /* Bu düğüm erişilemez, NULL yap */
                        govde->çocuklar[j] = NULL;
                    }
                    if (govde->çocuklar[j] &&
                        govde->çocuklar[j]->tur == DÜĞÜM_DÖNDÜR) {
                        return_bulundu = 1;
                    }
                }
            }
        }

        /* Alt düğümlere özyinelemeli uygula */
        if (cocuk->çocuk_sayısı > 0) {
            Düğüm temp_prog;
            temp_prog.çocuklar = cocuk->çocuklar;
            temp_prog.çocuk_sayısı = cocuk->çocuk_sayısı;
            llvm_frontend_optimizasyonlari(u, &temp_prog);
        }
    }
}
