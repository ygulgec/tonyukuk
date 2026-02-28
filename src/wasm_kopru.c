/*
 * wasm_kopru.c — LLVM Backend üzerinden WebAssembly binary üretimi
 *
 * Tonyukuk Türkçe programlama dili derleyicisi için WASM köprü katmanı.
 * LLVM IR üreticisini kullanarak tarayıcıda çalıştırılabilir .wasm
 * binary dosyası oluşturur.
 *
 * Derleme akışı:
 *   Tonyukuk kaynak (.tr)
 *     → LLVM IR (llvm_uretici.c)
 *       → WASM nesne dosyası (.o)
 *         → wasm-ld ile bağlama
 *           → .wasm binary
 *
 * Tarayıcı tarafında JavaScript import nesnesi (env) ile eşleşen
 * fonksiyon tanımları bu dosyada yapılır.
 */

#ifdef LLVM_BACKEND_MEVCUT

#include "wasm_kopru.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *                    WASM HEDEF AYARLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

void wasm_hedef_ayarla(LLVMÜretici *üretici) {
    /*
     * TÜRKÇE: WASM hedef doğrulaması.
     * Not: hedef_uclu ve veri düzeni llvm_üretici_oluştur() tarafından
     * zaten ayarlanıyor (arena_strdup ile). Burada sadece doğrulama yapıyoruz.
     * Gerekirse ek WASM-spesifik ayarlar buraya eklenebilir.
     */
    if (!üretici->hedef_uclu ||
        strstr(üretici->hedef_uclu, "wasm32") == NULL) {
        fprintf(stderr, "wasm_kopru: Uyarı — hedef wasm32 değil: %s\n",
                üretici->hedef_uclu ? üretici->hedef_uclu : "(boş)");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                    İÇE AKTARIM TANIMLARI (JS IMPORTS)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* TÜRKÇE: Yardımcı — tek parametreli fonksiyon tanımla */
static LLVMValueRef tanimla_tek_param(
    LLVMÜretici *u,
    const char *isim,
    LLVMTypeRef parametre_tipi,
    LLVMTypeRef dönüş_tipi
) {
    LLVMTypeRef param_tipleri[] = { parametre_tipi };
    LLVMTypeRef fonk_tipi = LLVMFunctionType(dönüş_tipi, param_tipleri, 1, 0);
    LLVMValueRef fonk = LLVMAddFunction(u->modul, isim, fonk_tipi);
    LLVMSetLinkage(fonk, LLVMExternalLinkage);
    return fonk;
}

/* TÜRKÇE: Yardımcı — çift parametreli fonksiyon tanımla */
static LLVMValueRef tanimla_cift_param(
    LLVMÜretici *u,
    const char *isim,
    LLVMTypeRef param1_tipi,
    LLVMTypeRef param2_tipi,
    LLVMTypeRef dönüş_tipi
) {
    LLVMTypeRef param_tipleri[] = { param1_tipi, param2_tipi };
    LLVMTypeRef fonk_tipi = LLVMFunctionType(dönüş_tipi, param_tipleri, 2, 0);
    LLVMValueRef fonk = LLVMAddFunction(u->modul, isim, fonk_tipi);
    LLVMSetLinkage(fonk, LLVMExternalLinkage);
    return fonk;
}

/* TÜRKÇE: Yardımcı — parametresiz fonksiyon tanımla */
static LLVMValueRef tanimla_parametresiz(
    LLVMÜretici *u,
    const char *isim,
    LLVMTypeRef dönüş_tipi
) {
    LLVMTypeRef fonk_tipi = LLVMFunctionType(dönüş_tipi, NULL, 0, 0);
    LLVMValueRef fonk = LLVMAddFunction(u->modul, isim, fonk_tipi);
    LLVMSetLinkage(fonk, LLVMExternalLinkage);
    return fonk;
}

void wasm_ice_aktarimlari_tanimla(LLVMÜretici *üretici) {
    LLVMContextRef bag = üretici->baglam;

    /* TÜRKÇE: Temel tipler */
    LLVMTypeRef i32_tipi = LLVMInt32TypeInContext(bag);
    LLVMTypeRef i64_tipi = LLVMInt64TypeInContext(bag);
    LLVMTypeRef f64_tipi = LLVMDoubleTypeInContext(bag);
    LLVMTypeRef bos_tipi = LLVMVoidTypeInContext(bag);

    /* ── Yazdırma fonksiyonları ── */

    /* TÜRKÇE: _yazdir_tam(sayi: i64) → void */
    tanimla_tek_param(üretici, "_yazdir_tam", i64_tipi, bos_tipi);

    /* TÜRKÇE: _yazdir_metin(isaretci: i32, uzunluk: i32) → void */
    tanimla_cift_param(üretici, "_yazdir_metin", i32_tipi, i32_tipi, bos_tipi);

    /* TÜRKÇE: _yazdir_ondalik(deger: f64) → void */
    tanimla_tek_param(üretici, "_yazdir_ondalik", f64_tipi, bos_tipi);

    /* ── Bellek yönetimi ── */

    /* TÜRKÇE: __bellek_buyut(sayfa_sayisi: i32) → i32 (önceki sayfa sayısı) */
    tanimla_tek_param(üretici, "__bellek_buyut", i32_tipi, i32_tipi);

    /* ── Matematik fonksiyonları ── */
    /* TÜRKÇE: Tek parametreli matematik fonksiyonları */
    const char *tek_param_mat[] = {
        "Math_sin", "Math_cos", "Math_tan",
        "Math_sqrt", "Math_abs",
        "Math_floor", "Math_ceil", "Math_round",
        "Math_log", "Math_exp"
    };
    int tek_param_mat_sayisi = (int)(sizeof(tek_param_mat) / sizeof(tek_param_mat[0]));

    for (int i = 0; i < tek_param_mat_sayisi; i++) {
        tanimla_tek_param(üretici, tek_param_mat[i], f64_tipi, f64_tipi);
    }

    /* TÜRKÇE: Çift parametreli matematik — Math_pow(taban, us) */
    tanimla_cift_param(üretici, "Math_pow", f64_tipi, f64_tipi, f64_tipi);

    /* ── Yardımcı fonksiyonlar ── */

    /* TÜRKÇE: rastgele() → f64  (0.0 ile 1.0 arası) */
    {
        LLVMTypeRef fonk_tipi = LLVMFunctionType(f64_tipi, NULL, 0, 0);
        LLVMValueRef fonk = LLVMAddFunction(üretici->modul, "rastgele", fonk_tipi);
        LLVMSetLinkage(fonk, LLVMExternalLinkage);
    }

    /* TÜRKÇE: zaman_damgasi() → i64  (milisaniye) */
    {
        LLVMTypeRef fonk_tipi = LLVMFunctionType(i64_tipi, NULL, 0, 0);
        LLVMValueRef fonk = LLVMAddFunction(üretici->modul, "zaman_damgasi", fonk_tipi);
        LLVMSetLinkage(fonk, LLVMExternalLinkage);
    }

    /* ══ GPIO fonksiyonları (donanım simülasyonu) ══ */

    /* TÜRKÇE: pin_modu(pin: i64, mod: i64) → void */
    {
        LLVMValueRef fn = tanimla_cift_param(üretici, "pin_modu",
                                              i64_tipi, i64_tipi, bos_tipi);
        LLVMTypeRef pt[] = { i64_tipi, i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(bos_tipi, pt, 2, 0);
        llvm_sembol_ekle(üretici, "pin_modu", fn, ft, 0, 1);
    }

    /* TÜRKÇE: dijital_yaz(pin: i64, deger: i64) → void */
    {
        LLVMValueRef fn = tanimla_cift_param(üretici, "dijital_yaz",
                                              i64_tipi, i64_tipi, bos_tipi);
        LLVMTypeRef pt[] = { i64_tipi, i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(bos_tipi, pt, 2, 0);
        llvm_sembol_ekle(üretici, "dijital_yaz", fn, ft, 0, 1);
    }

    /* TÜRKÇE: dijital_oku(pin: i64) → i64 */
    {
        LLVMValueRef fn = tanimla_tek_param(üretici, "dijital_oku",
                                             i64_tipi, i64_tipi);
        LLVMTypeRef pt[] = { i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(i64_tipi, pt, 1, 0);
        llvm_sembol_ekle(üretici, "dijital_oku", fn, ft, 0, 1);
    }

    /* TÜRKÇE: analog_oku(pin: i64) → i64 */
    {
        LLVMValueRef fn = tanimla_tek_param(üretici, "analog_oku",
                                             i64_tipi, i64_tipi);
        LLVMTypeRef pt[] = { i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(i64_tipi, pt, 1, 0);
        llvm_sembol_ekle(üretici, "analog_oku", fn, ft, 0, 1);
    }

    /* TÜRKÇE: pwm_yaz(pin: i64, deger: i64) → void */
    {
        LLVMValueRef fn = tanimla_cift_param(üretici, "pwm_yaz",
                                              i64_tipi, i64_tipi, bos_tipi);
        LLVMTypeRef pt[] = { i64_tipi, i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(bos_tipi, pt, 2, 0);
        llvm_sembol_ekle(üretici, "pwm_yaz", fn, ft, 0, 1);
    }

    /* ══ Zamanlama fonksiyonları ══ */

    /* TÜRKÇE: bekle_ms(ms: i64) → void */
    {
        LLVMValueRef fn = tanimla_tek_param(üretici, "bekle_ms",
                                             i64_tipi, bos_tipi);
        LLVMTypeRef pt[] = { i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(bos_tipi, pt, 1, 0);
        llvm_sembol_ekle(üretici, "bekle_ms", fn, ft, 0, 1);
    }

    /* TÜRKÇE: bekle_us(us: i64) → void */
    {
        LLVMValueRef fn = tanimla_tek_param(üretici, "bekle_us",
                                             i64_tipi, bos_tipi);
        LLVMTypeRef pt[] = { i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(bos_tipi, pt, 1, 0);
        llvm_sembol_ekle(üretici, "bekle_us", fn, ft, 0, 1);
    }

    /* TÜRKÇE: milis() → i64 */
    {
        LLVMValueRef fn = tanimla_parametresiz(üretici, "milis", i64_tipi);
        LLVMTypeRef ft = LLVMFunctionType(i64_tipi, NULL, 0, 0);
        llvm_sembol_ekle(üretici, "milis", fn, ft, 0, 1);
    }

    /* TÜRKÇE: mikros() → i64 */
    {
        LLVMValueRef fn = tanimla_parametresiz(üretici, "mikros", i64_tipi);
        LLVMTypeRef ft = LLVMFunctionType(i64_tipi, NULL, 0, 0);
        llvm_sembol_ekle(üretici, "mikros", fn, ft, 0, 1);
    }

    /* ══ Seri iletişim fonksiyonları ══ */

    /* TÜRKÇE: seri_baslat(baud: i64) → void */
    {
        LLVMValueRef fn = tanimla_tek_param(üretici, "seri_baslat",
                                             i64_tipi, bos_tipi);
        LLVMTypeRef pt[] = { i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(bos_tipi, pt, 1, 0);
        llvm_sembol_ekle(üretici, "seri_baslat", fn, ft, 0, 1);
    }

    /* TÜRKÇE: seri_yaz(isaretci: i32, uzunluk: i64) → void */
    {
        LLVMTypeRef pt[] = { i32_tipi, i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(bos_tipi, pt, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(üretici->modul, "seri_yaz", ft);
        LLVMSetLinkage(fn, LLVMExternalLinkage);
        llvm_sembol_ekle(üretici, "seri_yaz", fn, ft, 0, 1);
    }

    /* TÜRKÇE: seri_yaz_satir(isaretci: i32, uzunluk: i64) → void */
    {
        LLVMTypeRef pt[] = { i32_tipi, i64_tipi };
        LLVMTypeRef ft = LLVMFunctionType(bos_tipi, pt, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(üretici->modul, "seri_yaz_satir", ft);
        LLVMSetLinkage(fn, LLVMExternalLinkage);
        llvm_sembol_ekle(üretici, "seri_yaz_satir", fn, ft, 0, 1);
    }

    /* TÜRKÇE: seri_yazdir(isaretci: i32) → void */
    {
        LLVMValueRef fn = tanimla_tek_param(üretici, "seri_yazdir",
                                             i32_tipi, bos_tipi);
        LLVMTypeRef pt[] = { i32_tipi };
        LLVMTypeRef ft = LLVMFunctionType(bos_tipi, pt, 1, 0);
        llvm_sembol_ekle(üretici, "seri_yazdir", fn, ft, 0, 1);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                    DIŞA AKTARIM AYARLARI (EXPORTS)
 * ═══════════════════════════════════════════════════════════════════════════ */

void wasm_disa_aktarimlari_ayarla(LLVMÜretici *üretici) {
    /* TÜRKÇE: main fonksiyonunu dışa aktar */
    LLVMValueRef ana_islev = LLVMGetNamedFunction(üretici->modul, "main");
    if (ana_islev) {
        LLVMSetLinkage(ana_islev, LLVMExternalLinkage);
    }

    /* TÜRKÇE: _baslat fonksiyonunu dışa aktar (varsa) */
    LLVMValueRef baslat_islev = LLVMGetNamedFunction(üretici->modul, "_baslat");
    if (baslat_islev) {
        LLVMSetLinkage(baslat_islev, LLVMExternalLinkage);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                    WASM BİNARY ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

int wasm_ikili_uret(LLVMÜretici *üretici, const char *cikti_dosya) {
    /* TÜRKÇE: Geçici nesne dosyası adı */
    char gecici_nesne[512];
    snprintf(gecici_nesne, sizeof(gecici_nesne), "%s.o", cikti_dosya);

    /* TÜRKÇE: LLVM → nesne dosyası üret */
    if (llvm_nesne_dosyasi_uret(üretici, gecici_nesne) == 0) {
        fprintf(stderr, "tonyukuk-derle: WASM nesne dosyası üretilemedi\n");
        return 1;
    }

    /* TÜRKÇE: wasm-ld ile bağlama komutu oluştur */
    char komut[2048];
    snprintf(komut, sizeof(komut),
        "wasm-ld-17 %s -o %s "
        "--no-entry "
        "--export-all "
        "--allow-undefined "
        "--import-memory "
        "--initial-memory=1048576 "
        "--max-memory=16777216 "
        "--stack-first "
        "2>&1",
        gecici_nesne, cikti_dosya);

    /* TÜRKÇE: Bağlama komutunu çalıştır */
    int sonuç = system(komut);

    if (sonuç != 0) {
        /* TÜRKÇE: wasm-ld-17 başarısız → wasm-ld dene */
        snprintf(komut, sizeof(komut),
            "wasm-ld %s -o %s "
            "--no-entry "
            "--export-all "
            "--allow-undefined "
            "--import-memory "
            "--initial-memory=1048576 "
            "--max-memory=16777216 "
            "--stack-first "
            "2>&1",
            gecici_nesne, cikti_dosya);

        sonuç = system(komut);
    }

    /* TÜRKÇE: Geçici nesne dosyasını temizle */
    remove(gecici_nesne);

    if (sonuç != 0) {
        fprintf(stderr, "tonyukuk-derle: WASM bağlama hatası (wasm-ld gerekli)\n");
        fprintf(stderr, "Kurulum: sudo apt install lld-17\n");
        return 1;
    }

    fprintf(stderr, "tonyukuk-derle: WASM binary üretildi → %s\n", cikti_dosya);
    return 0;
}

#endif /* LLVM_BACKEND_MEVCUT */
