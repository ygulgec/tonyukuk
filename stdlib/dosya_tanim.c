/* Dosya modülü — derleme zamanı tanımları
 * Tonyukuk Programlama Dili
 * Kapsamlı dosya sistemi işlemleri
 */
#include "../src/modul.h"

static const ModülFonksiyon dosya_fonksiyonlar[] = {
    /* ========== OKUMA/YAZMA ========== */

    /* dosya.oku(yol: metin) -> metin
     * Dosyanın tüm içeriğini okur */
    {"oku", NULL, "_tr_dosya_oku", {TİP_METİN}, 1, TİP_METİN},

    /* dosya.yaz(yol: metin, içerik: metin) -> tam
     * Dosyaya yazar (üzerine yazar), başarıda 0 döndürür */
    {"yaz", NULL, "_tr_dosya_yaz", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* dosya.ekle(yol: metin, içerik: metin) -> tam
     * Dosya sonuna ekler */
    {"ekle", NULL, "_tr_dosya_ekle", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* dosya.satirlar(yol: metin) -> dizi<metin>
     * Dosyayı satır satır okur */
    {"satirlar", "satirlar", "_tr_dosya_satirlar", {TİP_METİN}, 1, TİP_DİZİ},

    /* ========== VARLIK KONTROLLERİ ========== */

    /* dosya.var_mi(yol: metin) -> mantık
     * Dosya/dizin var mı kontrol eder */
    {"var_m\xc4\xb1", "var_mi", "_tr_dosya_var_mi", {TİP_METİN}, 1, TİP_MANTIK},

    /* dosya.dizin_mi(yol: metin) -> mantık
     * Yolun bir dizin olup olmadığını kontrol eder */
    {"dizin_mi", NULL, "_tr_dosya_dizin_mi", {TİP_METİN}, 1, TİP_MANTIK},

    /* dosya.dosya_mi(yol: metin) -> mantık
     * Yolun bir dosya olup olmadığını kontrol eder */
    {"dosya_mi", NULL, "_tr_dosya_dosya_mi", {TİP_METİN}, 1, TİP_MANTIK},

    /* dosya.okunabilir_mi(yol: metin) -> mantık */
    {"okunabilir_mi", NULL, "_tr_dosya_okunabilir_mi", {TİP_METİN}, 1, TİP_MANTIK},

    /* dosya.yazilabilir_mi(yol: metin) -> mantık */
    {"yazilabilir_mi", "yazilabilir_mi", "_tr_dosya_yazilabilir_mi", {TİP_METİN}, 1, TİP_MANTIK},

    /* ========== DİZİN İŞLEMLERİ ========== */

    /* dosya.klasor_olustur(yol: metin) -> tam
     * Tek seviye dizin oluşturur */
    {"klas\xc3\xb6r_olu\xc5\x9ftur", "klasor_olustur", "_tr_dosya_klasor_olustur", {TİP_METİN}, 1, TİP_TAM},

    /* dosya.klasor_olustur_hepsi(yol: metin) -> tam
     * Tüm ara dizinleri oluşturur (mkdir -p) */
    {"klas\xc3\xb6r_olu\xc5\x9ftur_hepsi", "klasor_olustur_hepsi", "_tr_dosya_klasor_olustur_hepsi", {TİP_METİN}, 1, TİP_TAM},

    /* dosya.listele(yol: metin) -> dizi<metin>
     * Dizindeki dosya ve klasörleri listeler */
    {"listele", NULL, "_tr_dosya_listele", {TİP_METİN}, 1, TİP_DİZİ},

    /* dosya.alt_dizinler(yol: metin) -> dizi<metin>
     * Sadece alt dizinleri listeler */
    {"alt_dizinler", NULL, "_tr_dosya_alt_dizinler", {TİP_METİN}, 1, TİP_DİZİ},

    /* dosya.dosyalar(yol: metin) -> dizi<metin>
     * Sadece dosyaları listeler */
    {"dosyalar", NULL, "_tr_dosya_dosyalar", {TİP_METİN}, 1, TİP_DİZİ},

    /* ========== SİLME/TAŞIMA/KOPYALAMA ========== */

    /* dosya.sil(yol: metin) -> tam
     * Dosya veya boş dizin siler */
    {"sil", NULL, "_tr_dosya_sil", {TİP_METİN}, 1, TİP_TAM},

    /* dosya.sil_ozyineli(yol: metin) -> tam
     * Dizini içeriğiyle birlikte siler (rm -rf) */
    {"sil_\xc3\xb6zyineli", "sil_ozyineli", "_tr_dosya_sil_ozyineli", {TİP_METİN}, 1, TİP_TAM},

    /* dosya.tasi(kaynak: metin, hedef: metin) -> tam */
    {"ta\xc5\x9f\xc4\xb1", "tasi", "_tr_dosya_tasi", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* dosya.kopyala(kaynak: metin, hedef: metin) -> tam */
    {"kopyala", NULL, "_tr_dosya_kopyala", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* dosya.yeniden_adlandir(eski: metin, yeni: metin) -> tam */
    {"yeniden_adland\xc4\xb1r", "yeniden_adlandir", "_tr_dosya_yeniden_adlandir", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* ========== META VERİ ========== */

    /* dosya.boyut(yol: metin) -> tam
     * Dosya boyutunu bayt olarak döndürür */
    {"boyut", NULL, "_tr_dosya_boyut", {TİP_METİN}, 1, TİP_TAM},

    /* dosya.degistirilme_zamani(yol: metin) -> tam
     * Unix timestamp olarak değiştirilme zamanı */
    {"de\xc4\x9fi\xc5\x9ftirilme_zaman\xc4\xb1", "degistirilme_zamani", "_tr_dosya_degistirilme_zamani", {TİP_METİN}, 1, TİP_TAM},

    /* dosya.olusturulma_zamani(yol: metin) -> tam */
    {"olu\xc5\x9fturulma_zaman\xc4\xb1", "olusturulma_zamani", "_tr_dosya_olusturulma_zamani", {TİP_METİN}, 1, TİP_TAM},

    /* dosya.izinler(yol: metin) -> tam
     * Unix izin bitlerini döndürür (0644 gibi) */
    {"izinler", NULL, "_tr_dosya_izinler", {TİP_METİN}, 1, TİP_TAM},

    /* dosya.izin_ayarla(yol: metin, mod: tam) -> tam */
    {"izin_ayarla", NULL, "_tr_dosya_izin_ayarla", {TİP_METİN, TİP_TAM}, 2, TİP_TAM},

    /* ========== YOL İŞLEMLERİ ========== */

    /* dosya.mutlak_yol(yol: metin) -> metin
     * Göreceli yolu mutlak yola çevirir */
    {"mutlak_yol", NULL, "_tr_dosya_mutlak_yol", {TİP_METİN}, 1, TİP_METİN},

    /* dosya.ust_dizin(yol: metin) -> metin
     * Üst dizini döndürür: /a/b/c -> /a/b */
    {"\xc3\xbcst_dizin", "ust_dizin", "_tr_dosya_ust_dizin", {TİP_METİN}, 1, TİP_METİN},

    /* dosya.temel_ad(yol: metin) -> metin
     * Dosya adını döndürür: /a/b/dosya.txt -> dosya.txt */
    {"temel_ad", NULL, "_tr_dosya_temel_ad", {TİP_METİN}, 1, TİP_METİN},

    /* dosya.uzanti(yol: metin) -> metin
     * Uzantıyı döndürür: dosya.txt -> .txt */
    {"uzant\xc4\xb1", "uzanti", "_tr_dosya_uzanti", {TİP_METİN}, 1, TİP_METİN},

    /* dosya.adsiz(yol: metin) -> metin
     * Uzantısız dosya adı: dosya.txt -> dosya */
    {"ads\xc4\xb1z", "adsiz", "_tr_dosya_adsiz", {TİP_METİN}, 1, TİP_METİN},

    /* dosya.yol_birlestir(yol1: metin, yol2: metin) -> metin
     * İki yolu birleştirir: /a + b/c -> /a/b/c */
    {"yol_birle\xc5\x9ftir", "yol_birlestir", "_tr_dosya_yol_birlestir", {TİP_METİN, TİP_METİN}, 2, TİP_METİN},

    /* ========== GLOB DESENLERI ========== */

    /* dosya.glob(desen: metin) -> dizi<metin>
     * *.txt, *.tr gibi desenleri arar */
    {"glob", NULL, "_tr_dosya_glob", {TİP_METİN}, 1, TİP_DİZİ},

    /* dosya.glob_ozyineli(desen: metin) -> dizi<metin>
     * Özyinelemeli glob: **\/*.tr */
    {"glob_\xc3\xb6zyineli", "glob_ozyineli", "_tr_dosya_glob_ozyineli", {TİP_METİN}, 1, TİP_DİZİ},

    /* ========== GEÇİCİ DOSYALAR ========== */

    /* dosya.gecici_dosya() -> metin
     * Benzersiz geçici dosya yolu oluşturur */
    {"ge\xc3\xa7ici_dosya", "gecici_dosya", "_tr_dosya_gecici_dosya", {0}, 0, TİP_METİN},

    /* dosya.gecici_dizin() -> metin
     * Benzersiz geçici dizin oluşturur */
    {"ge\xc3\xa7ici_dizin", "gecici_dizin", "_tr_dosya_gecici_dizin", {0}, 0, TİP_METİN},

    /* ========== SEMBOLİK BAĞLANTILAR ========== */

    /* dosya.sembolik_bag_olustur(hedef: metin, bag: metin) -> tam */
    {"sembolik_ba\xc4\x9f_olu\xc5\x9ftur", "sembolik_bag_olustur", "_tr_dosya_sembolik_bag_olustur", {TİP_METİN, TİP_METİN}, 2, TİP_TAM},

    /* dosya.sembolik_bag_mi(yol: metin) -> mantık */
    {"sembolik_ba\xc4\x9f_m\xc4\xb1", "sembolik_bag_mi", "_tr_dosya_sembolik_bag_mi", {TİP_METİN}, 1, TİP_MANTIK},

    /* dosya.sembolik_bag_oku(yol: metin) -> metin
     * Bağlantının hedefini döndürür */
    {"sembolik_ba\xc4\x9f_oku", "sembolik_bag_oku", "_tr_dosya_sembolik_bag_oku", {TİP_METİN}, 1, TİP_METİN},

    /* ========== MEVCUT DİZİN ========== */

    /* dosya.mevcut_dizin() -> metin
     * Çalışma dizinini döndürür */
    {"mevcut_dizin", NULL, "_tr_dosya_mevcut_dizin", {0}, 0, TİP_METİN},

    /* dosya.dizin_degistir(yol: metin) -> tam
     * Çalışma dizinini değiştirir */
    {"dizin_de\xc4\x9fi\xc5\x9ftir", "dizin_degistir", "_tr_dosya_dizin_degistir", {TİP_METİN}, 1, TİP_TAM},

    /* dosya.ev_dizini() -> metin
     * Kullanıcı ev dizinini döndürür */
    {"ev_dizini", NULL, "_tr_dosya_ev_dizini", {0}, 0, TİP_METİN},
};

const ModülTanım dosya_modul = {
    "dosya", NULL,
    dosya_fonksiyonlar,
    sizeof(dosya_fonksiyonlar) / sizeof(dosya_fonksiyonlar[0])
};
