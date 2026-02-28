/*
 * wasm_kopru.h — LLVM Backend üzerinden WebAssembly binary üretimi
 *
 * Tonyukuk Türkçe programlama dili derleyicisi için WASM köprü katmanı.
 * Mevcut LLVM IR üreticisini kullanarak .wasm binary dosyası oluşturur.
 *
 * Mevcut uretici_wasm.c (WAT metin üretici) dokunulmaz.
 * Bu dosya sadece LLVM → WASM binary yolunu sağlar.
 */

#ifndef WASM_KÖPRÜ_H
#define WASM_KÖPRÜ_H

#ifdef LLVM_BACKEND_MEVCUT

#include "llvm_uretici.h"

/* TÜRKÇE: WASM hedef üçlüsü ve veri düzenini ayarla */
void wasm_hedef_ayarla(LLVMÜretici *üretici);

/* TÜRKÇE: JavaScript tarafından sağlanacak import fonksiyonlarını tanımla
 * (print_i64, print_str, Math_*, bellek_büyüt, rastgele, zaman_damgası) */
void wasm_ice_aktarimlari_tanimla(LLVMÜretici *üretici);

/* TÜRKÇE: Dışa aktarılacak fonksiyonları işaretle (main, _baslat vb.) */
void wasm_disa_aktarimlari_ayarla(LLVMÜretici *üretici);

/* TÜRKÇE: LLVM nesne dosyasından wasm-ld ile .wasm binary üret
 * Dönüş: 0 başarılı, 1 hata */
int wasm_ikili_uret(LLVMÜretici *üretici, const char *cikti_dosya);

#endif /* LLVM_BACKEND_MEVCUT */

#endif /* WASM_KÖPRÜ_H */
