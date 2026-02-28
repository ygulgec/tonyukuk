#!/usr/bin/env python3
"""
Tonyukuk derleyici kaynak kodundaki ASCII-yaklaşımlı Türkçe identifier'ları
gerçek Türkçe karakterlerle değiştirir.

Kullanım:
    python3 turkce_yeniden_adlandir.py --faz 1   # Enum sabitleri
    python3 turkce_yeniden_adlandir.py --faz 2   # Tip/struct adları
    python3 turkce_yeniden_adlandir.py --faz 3   # Struct alan adları
    python3 turkce_yeniden_adlandir.py --faz 4   # Fonksiyon adları
    python3 turkce_yeniden_adlandir.py --faz 5   # Include guard/makro
    python3 turkce_yeniden_adlandir.py --faz 6   # Yerel değişkenler
    python3 turkce_yeniden_adlandir.py --faz tumu # Hepsi
"""

import re
import os
import sys
import glob
import argparse

# ============================================================
# FAZ 1: Enum Sabitleri
# ============================================================

FAZ1_TIP = {
    "TIP_TAM":        "TİP_TAM",
    "TIP_ONDALIK":    "TİP_ONDALIK",
    "TIP_METIN":      "TİP_METİN",
    "TIP_MANTIK":     "TİP_MANTIK",
    "TIP_DIZI":       "TİP_DİZİ",
    "TIP_SINIF":      "TİP_SINIF",
    "TIP_BOSLUK":     "TİP_BOŞLUK",
    "TIP_ISLEV":      "TİP_İŞLEV",
    "TIP_SOZLUK":     "TİP_SÖZLÜK",
    "TIP_SAYIM":      "TİP_SAYIM",
    "TIP_KUME":       "TİP_KÜME",
    "TIP_SONUC":      "TİP_SONUÇ",
    "TIP_SECENEK":    "TİP_SEÇENEK",
    "TIP_BILINMIYOR": "TİP_BİLİNMİYOR",
}

FAZ1_DUGUM = {
    "DUGUM_PROGRAM":          "DÜĞÜM_PROGRAM",
    "DUGUM_DEGISKEN":         "DÜĞÜM_DEĞİŞKEN",
    "DUGUM_ISLEV":            "DÜĞÜM_İŞLEV",
    "DUGUM_SINIF":            "DÜĞÜM_SINIF",
    "DUGUM_KULLAN":           "DÜĞÜM_KULLAN",
    "DUGUM_EGER":             "DÜĞÜM_EĞER",
    "DUGUM_DONGU":            "DÜĞÜM_DÖNGÜ",
    "DUGUM_IKEN":             "DÜĞÜM_İKEN",
    "DUGUM_DONDUR":           "DÜĞÜM_DÖNDÜR",
    "DUGUM_KIR":              "DÜĞÜM_KIR",
    "DUGUM_DEVAM":            "DÜĞÜM_DEVAM",
    "DUGUM_IFADE_BILDIRIMI":  "DÜĞÜM_İFADE_BİLDİRİMİ",
    "DUGUM_BLOK":             "DÜĞÜM_BLOK",
    "DUGUM_TAM_SAYI":         "DÜĞÜM_TAM_SAYI",
    "DUGUM_ONDALIK_SAYI":     "DÜĞÜM_ONDALIK_SAYI",
    "DUGUM_METIN_DEGERI":     "DÜĞÜM_METİN_DEĞERİ",
    "DUGUM_MANTIK_DEGERI":    "DÜĞÜM_MANTIK_DEĞERİ",
    "DUGUM_TANIMLAYICI":      "DÜĞÜM_TANIMLAYICI",
    "DUGUM_IKILI_ISLEM":      "DÜĞÜM_İKİLİ_İŞLEM",
    "DUGUM_TEKLI_ISLEM":      "DÜĞÜM_TEKLİ_İŞLEM",
    "DUGUM_CAGRI":            "DÜĞÜM_ÇAĞRI",
    "DUGUM_ERISIM":           "DÜĞÜM_ERİŞİM",
    "DUGUM_DIZI_DEGERI":      "DÜĞÜM_DİZİ_DEĞERİ",
    "DUGUM_DIZI_ERISIM":      "DÜĞÜM_DİZİ_ERİŞİM",
    "DUGUM_BORU":             "DÜĞÜM_BORU",
    "DUGUM_ATAMA":            "DÜĞÜM_ATAMA",
    "DUGUM_DIZI_ATAMA":       "DÜĞÜM_DİZİ_ATAMA",
    "DUGUM_ERISIM_ATAMA":     "DÜĞÜM_ERİŞİM_ATAMA",
    "DUGUM_DENE_YAKALA":      "DÜĞÜM_DENE_YAKALA",
    "DUGUM_FIRLAT":           "DÜĞÜM_FIRLAT",
    "DUGUM_LAMBDA":           "DÜĞÜM_LAMBDA",
    "DUGUM_BOS_DEGER":        "DÜĞÜM_BOŞ_DEĞER",
    "DUGUM_ESLE":             "DÜĞÜM_EŞLE",
    "DUGUM_HER_ICIN":         "DÜĞÜM_HER_İÇİN",
    "DUGUM_SAYIM":            "DÜĞÜM_SAYIM",
    "DUGUM_ARAYUZ":           "DÜĞÜM_ARAYÜZ",
    "DUGUM_SOZLUK_DEGERI":    "DÜĞÜM_SÖZLÜK_DEĞERİ",
    "DUGUM_SOZLUK_ERISIM":    "DÜĞÜM_SÖZLÜK_ERİŞİM",
    "DUGUM_ARALIK":           "DÜĞÜM_ARALIK",
    "DUGUM_BEKLE":            "DÜĞÜM_BEKLE",
    "DUGUM_DEMET":            "DÜĞÜM_DEMET",
    "DUGUM_DILIM":            "DÜĞÜM_DİLİM",
    "DUGUM_UCLU":             "DÜĞÜM_ÜÇLÜ",
    "DUGUM_URET":             "DÜĞÜM_ÜRET",
    "DUGUM_TIP_TANIMI":       "DÜĞÜM_TİP_TANIMI",
    "DUGUM_TEST":             "DÜĞÜM_TEST",
    "DUGUM_KUME_DEGERI":      "DÜĞÜM_KÜME_DEĞERİ",
    "DUGUM_LISTE_URETIMI":    "DÜĞÜM_LİSTE_ÜRETİMİ",
    "DUGUM_ILE_ISE":          "DÜĞÜM_İLE_İSE",
    "DUGUM_PAKET_AC":         "DÜĞÜM_PAKET_AÇ",
    "DUGUM_WALRUS":           "DÜĞÜM_WALRUS",
    "DUGUM_SOZLUK_URETIMI":   "DÜĞÜM_SÖZLÜK_ÜRETİMİ",
    "DUGUM_SONUC_OLUSTUR":    "DÜĞÜM_SONUÇ_OLUŞTUR",
    "DUGUM_SECENEK_OLUSTUR":  "DÜĞÜM_SEÇENEK_OLUŞTUR",
    "DUGUM_SORU_OP":          "DÜĞÜM_SORU_OP",
    "DUGUM_TIP_PARAMETRELI":  "DÜĞÜM_TİP_PARAMETRELİ",
    "DUGUM_OZELLIK":          "DÜĞÜM_ÖZELLİK",
    "DUGUM_STATIK_ERISIM":    "DÜĞÜM_STATİK_ERİŞİM",
}

FAZ1_TOK = {
    "TOK_METIN_DEGERI": "TOK_METİN_DEĞERİ",
    "TOK_DOGRU":        "TOK_DOĞRU",
    "TOK_YANLIS":       "TOK_YANLIŞ",
    "TOK_BOS":          "TOK_BOŞ",
    "TOK_METIN":        "TOK_METİN",
    "TOK_DIZI":         "TOK_DİZİ",
    "TOK_EGER":         "TOK_EĞER",
    "TOK_ISE":          "TOK_İSE",
    "TOK_DONGU":        "TOK_DÖNGÜ",
    "TOK_IKEN":         "TOK_İKEN",
    "TOK_DONDUR":       "TOK_DÖNDÜR",
    "TOK_ICIN":         "TOK_İÇİN",
    "TOK_ESLE":         "TOK_EŞLE",
    "TOK_ISLEV":        "TOK_İŞLEV",
    "TOK_DEGIL":        "TOK_DEĞİL",
    "TOK_DEGISKEN":     "TOK_DEĞİŞKEN",
    "TOK_ESITTIR":      "TOK_EŞİTTİR",
    "TOK_ESIT_ESIT":    "TOK_EŞİT_EŞİT",
    "TOK_ESIT_DEGIL":   "TOK_EŞİT_DEĞİL",
    "TOK_KUCUK":        "TOK_KÜÇÜK",
    "TOK_BUYUK":        "TOK_BÜYÜK",
    "TOK_KUCUK_ESIT":   "TOK_KÜÇÜK_EŞİT",
    "TOK_BUYUK_ESIT":   "TOK_BÜYÜK_EŞİT",
    "TOK_CARPIM":       "TOK_ÇARPIM",
    "TOK_BOLME":        "TOK_BÖLME",
    "TOK_YUZDE":        "TOK_YÜZDE",
    "TOK_ARTI_ESIT":    "TOK_ARTI_EŞİT",
    "TOK_EKSI_ESIT":    "TOK_EKSİ_EŞİT",
    "TOK_CARPIM_ESIT":  "TOK_ÇARPIM_EŞİT",
    "TOK_BOLME_ESIT":   "TOK_BÖLME_EŞİT",
    "TOK_YUZDE_ESIT":   "TOK_YÜZDE_EŞİT",
    "TOK_VIRGUL":       "TOK_VİRGÜL",
    "TOK_IKI_NOKTA":    "TOK_İKİ_NOKTA",
    "TOK_KOSELI_AC":    "TOK_KÖŞELİ_AÇ",
    "TOK_KOSELI_KAPA":  "TOK_KÖŞELİ_KAPA",
    "TOK_SUSLU_AC":     "TOK_SÜSLÜ_AÇ",
    "TOK_SUSLU_KAPA":   "TOK_SÜSLÜ_KAPA",
    "TOK_KUME":         "TOK_KÜME",
    "TOK_ESZAMANSIZ":   "TOK_EŞZAMANSIZ",
    "TOK_BIT_VE":       "TOK_BİT_VE",
    "TOK_BIT_VEYA":     "TOK_BİT_VEYA",
    "TOK_BIT_XOR":      "TOK_BİT_XOR",
    "TOK_BIT_DEGIL":    "TOK_BİT_DEĞİL",
    "TOK_URETEC":       "TOK_ÜRETEÇ",
    "TOK_URET":         "TOK_ÜRET",
    "TOK_TIP_TANIMI":   "TOK_TİP_TANIMI",
    "TOK_HATA_SONUC":   "TOK_HATA_SONUÇ",
    "TOK_BIR":          "TOK_BİR",
    "TOK_HIC":          "TOK_HİÇ",
    "TOK_SECENEK":      "TOK_SEÇENEK",
    "TOK_ARAYUZ":       "TOK_ARAYÜZ",
    "TOK_OZEL":         "TOK_ÖZEL",
    "TOK_OZELLIK":      "TOK_ÖZELLİK",
    "TOK_STATIK":       "TOK_STATİK",
    "TOK_DOGRULA":      "TOK_DOĞRULA",
    "TOK_SONUC":        "TOK_SONUÇ",
    "TOK_UC_NOKTA":     "TOK_ÜÇ_NOKTA",
    "TOK_YENI_SATIR":   "TOK_YENİ_SATIR",
    "TOK_SAG_KAYDIR":   "TOK_SAĞ_KAYDIR",
}

FAZ1_HATA = {
    "HATA_BEKLENEN_SOZCUK":        "HATA_BEKLENEN_SÖZCÜK",
    "HATA_TANIMSIZ_DEGISKEN":      "HATA_TANIMSIZ_DEĞİŞKEN",
    "HATA_TANIMSIZ_ISLEV":         "HATA_TANIMSIZ_İŞLEV",
    "HATA_TIP_UYUMSUZLUGU":        "HATA_TİP_UYUMSUZLUĞU",
    "HATA_DOSYA_ACILAMADI":        "HATA_DOSYA_AÇILAMADI",
    "HATA_SOZDIZIMI":              "HATA_SÖZDİZİMİ",
    "HATA_KAPANMAMIS_METIN":       "HATA_KAPANMAMIŞ_METİN",
    "HATA_BEKLENEN_IFADE":         "HATA_BEKLENEN_İFADE",
    "HATA_BEKLENEN_TIP":           "HATA_BEKLENEN_TİP",
    "HATA_DONGU_DISI_KIR":         "HATA_DÖNGÜ_DIŞI_KIR",
    "HATA_ISLEV_DISI_DONDUR":      "HATA_İŞLEV_DIŞI_DÖNDÜR",
    "UYARI_BASLANGICSIZ_DEGISKEN": "UYARI_BAŞLANGIÇSIZ_DEĞİŞKEN",
    "HATA_KAPANMAMIS_YORUM":       "HATA_KAPANMAMIŞ_YORUM",
    "HATA_SABIT_ATAMA":            "HATA_SABİT_ATAMA",
    "HATA_ARAYUZ_UYGULAMA":        "HATA_ARAYÜZ_UYGULAMA",
}

# ============================================================
# FAZ 2: Tip/Struct Adları
# ============================================================

FAZ2_TIPLER = {
    "DugumTuru":          "DüğümTürü",
    "Dugum":              "Düğüm",
    "SozcukTuru":         "SözcükTürü",
    "SozcukCozumleyici":  "SözcükÇözümleyici",
    "Sozcuk":             "Sözcük",
    "TipTuru":            "TipTürü",
    "SinifAlani":         "SınıfAlanı",
    "AnlamCozumleyici":   "AnlamÇözümleyici",
    "Uretici":            "Üretici",
    "LLVMUretici":        "LLVMÜretici",
    "ModulFonksiyon":     "ModülFonksiyon",
    "ModulTanim":         "ModülTanım",
    "LLVMSembolGirisi":   "LLVMSembolGirişi",
    "LLVMSinifBilgisi":   "LLVMSınıfBilgisi",
    "GenericOzellestirme": "GenericÖzelleştirme",
}

# ============================================================
# FAZ 3: Struct Alan Adları
# ============================================================

FAZ3_ALANLAR = {
    "cocuklar":        "çocuklar",
    "cocuk_sayisi":    "çocuk_sayısı",
    "cocuk_kapasite":  "çocuk_kapasite",
    "metin_deger":     "metin_değer",
    "ondalik_deger":   "ondalık_değer",
    "mantik_deger":    "mantık_değer",
    "donus_tipi":      "dönüş_tipi",
    "sonuc_tipi":      "sonuç_tipi",
    "sonuc_secenek":   "sonuç_seçenek",
    "ozellik":         "özellik",
}

# ============================================================
# FAZ 4: Fonksiyon Adları
# ============================================================

FAZ4_FONKSIYONLAR = {
    "dugum_olustur":             "düğüm_oluştur",
    "dugum_cocuk_ekle":          "düğüm_çocuk_ekle",
    "sozcuk_cozumle":            "sözcük_çözümle",
    "sozcuk_tur_adi":            "sözcük_tür_adı",
    "sozcuk_serbest":            "sözcük_serbest",
    "anlam_cozumle":             "anlam_çözümle",
    "tip_adi_cevir":             "tip_adı_çevir",
    "tip_adi":                   "tip_adı",
    "kapsam_olustur":            "kapsam_oluştur",
    "sinif_bul":                 "sınıf_bul",
    "kod_uret":                  "kod_üret",
    "uretici_olustur":           "üretici_oluştur",
    "uretici_serbest":           "üretici_serbest",
    "uyari_bildir":              "uyarı_bildir",
    "ifade_uret":                "ifade_üret",
    "llvm_uretici_olustur":      "llvm_üretici_oluştur",
    "llvm_program_uret":         "llvm_program_üret",
    "llvm_islev_uret":           "llvm_işlev_üret",
    "llvm_sinif_uret":           "llvm_sınıf_üret",
    "llvm_tip_donustur":         "llvm_tip_dönüştür",
    "llvm_ortuk_donusum":        "llvm_örtük_dönüşüm",
    "llvm_acik_donusum":         "llvm_açık_dönüşüm",
    "llvm_sinif_bilgisi_ekle":   "llvm_sınıf_bilgisi_ekle",
    "llvm_metin_sabiti_olustur": "llvm_metin_sabiti_oluştur",
}

# ============================================================
# FAZ 5: Include Guard'lar ve Makrolar
# ============================================================

FAZ5_GUARDLAR = {
    "AGAC_H":          "AĞAÇ_H",
    "SOZCUK_H":        "SÖZCÜK_H",
    "URETICI_H":       "ÜRETİCİ_H",
    "COZUMLEYICI_H":   "ÇÖZÜMLEYİCİ_H",
    "METIN_H":         "METİN_H",
    "MODUL_H":         "MODÜL_H",
    "LLVM_URETICI_H":  "LLVM_ÜRETİCİ_H",
    "WASM_KOPRU_H":    "WASM_KÖPRÜ_H",
    "KAYNAK_HARITA_H": "KAYNAK_HARİTA_H",
}

# ============================================================
# FAZ 6: Yerel Değişkenler (en yaygın olanlar)
# ============================================================

FAZ6_YEREL = {
    "baslangic":    "başlangıç",
    "sonuc":        "sonuç",
    "cozumleyici":  "çözümleyici",
    "islev_adi":    "işlev_adı",
    "degisken":     "değişken",
    "sinif_adi":    "sınıf_adı",
    "sinif_bilgi":  "sınıf_bilgi",
    "sinif_bilgisi": "sınıf_bilgisi",
    "modul_adi":    "modül_adı",
    "uretici":      "üretici",
}


# ============================================================
# Yardımcı fonksiyonlar
# ============================================================

def dosyalari_bul(kök_dizin):
    """src/ ve stdlib/ altındaki tüm .c ve .h dosyalarını + Makefile'ı döndür."""
    dosyalar = []
    for uzanti in ("*.c", "*.h"):
        dosyalar.extend(glob.glob(os.path.join(kök_dizin, "src", uzanti)))
        dosyalar.extend(glob.glob(os.path.join(kök_dizin, "stdlib", uzanti)))
        dosyalar.extend(glob.glob(os.path.join(kök_dizin, "stdlib", "win", uzanti)))
    # Makefile
    makefile = os.path.join(kök_dizin, "Makefile")
    if os.path.exists(makefile):
        dosyalar.append(makefile)
    return sorted(dosyalar)


def esleme_uygula(dosyalar, esleme, kuru_calistir=False):
    """Eşleme sözlüğünü verilen dosyalara uygula."""
    # Uzunluğa göre sırala (uzundan kısaya) - kısmi eşleşmeyi önlemek için
    sirali = sorted(esleme.items(), key=lambda x: -len(x[0]))

    # Regex pattern'leri önceden derle
    patternler = []
    for eski, yeni in sirali:
        # \b sözcük sınırı — ASCII identifier'lar için çalışır
        pattern = re.compile(r'\b' + re.escape(eski) + r'\b')
        patternler.append((pattern, yeni, eski))

    toplam_degisiklik = 0
    degisen_dosyalar = []

    for dosya_yolu in dosyalar:
        try:
            with open(dosya_yolu, 'r', encoding='utf-8') as f:
                icerik = f.read()
        except (UnicodeDecodeError, FileNotFoundError):
            continue

        orijinal = icerik
        dosya_degisiklik = 0

        for pattern, yeni, eski in patternler:
            yeni_icerik = pattern.sub(yeni, icerik)
            if yeni_icerik != icerik:
                fark = icerik.count(eski) - yeni_icerik.count(eski)
                dosya_degisiklik += max(fark, 0)
                # Daha doğru sayım
                dosya_degisiklik = len(pattern.findall(orijinal)) if dosya_degisiklik == 0 else dosya_degisiklik
                icerik = yeni_icerik

        if icerik != orijinal:
            # Basit değişiklik sayımı
            degisiklik_sayisi = sum(len(p.findall(orijinal)) for p, _, _ in patternler)
            toplam_degisiklik += degisiklik_sayisi
            kisa_yol = os.path.relpath(dosya_yolu, os.path.dirname(dosyalar[0]) if dosyalar else ".")
            degisen_dosyalar.append((kisa_yol, degisiklik_sayisi))

            if not kuru_calistir:
                with open(dosya_yolu, 'w', encoding='utf-8') as f:
                    f.write(icerik)

    return toplam_degisiklik, degisen_dosyalar


def faz_calistir(faz_no, kök_dizin, kuru_calistir=False):
    """Belirtilen fazı çalıştır."""
    dosyalar = dosyalari_bul(kök_dizin)

    faz_eslemeleri = {
        1: {**FAZ1_TIP, **FAZ1_DUGUM, **FAZ1_TOK, **FAZ1_HATA},
        2: FAZ2_TIPLER,
        3: FAZ3_ALANLAR,
        4: FAZ4_FONKSIYONLAR,
        5: FAZ5_GUARDLAR,
        6: FAZ6_YEREL,
    }

    if faz_no not in faz_eslemeleri:
        print(f"Hata: Geçersiz faz numarası: {faz_no}")
        sys.exit(1)

    esleme = faz_eslemeleri[faz_no]
    faz_adlari = {
        1: "Enum Sabitleri (TİP_, DÜĞÜM_, TOK_, HATA_)",
        2: "Tip/Struct Adları",
        3: "Struct Alan Adları",
        4: "Fonksiyon Adları",
        5: "Include Guard'lar ve Makrolar",
        6: "Yerel Değişkenler",
    }

    mod = "KURU ÇALIŞTIRMA" if kuru_calistir else "UYGULAMA"
    print(f"\n{'='*60}")
    print(f"Faz {faz_no}: {faz_adlari[faz_no]} [{mod}]")
    print(f"{'='*60}")
    print(f"Eşleme sayısı: {len(esleme)}")
    print(f"Dosya sayısı: {len(dosyalar)}")

    toplam, degisen = esleme_uygula(dosyalar, esleme, kuru_calistir)

    print(f"\nToplam değişiklik: {toplam}")
    print(f"Değişen dosya: {len(degisen)}")
    for dosya, sayi in sorted(degisen):
        print(f"  {dosya}: {sayi} değişiklik")

    return toplam, degisen


def main():
    parser = argparse.ArgumentParser(description="Tonyukuk derleyici Türkçe karakter dönüştürücü")
    parser.add_argument("--faz", required=True,
                       help="Faz numarası (1-6) veya 'tumu'")
    parser.add_argument("--kuru", action="store_true",
                       help="Kuru çalıştırma (değişiklik yapmaz)")
    parser.add_argument("--dizin", default="/var/www/tonyukuktr.com/derleyici",
                       help="Derleyici kök dizini")
    args = parser.parse_args()

    if args.faz == "tumu":
        for faz in range(1, 7):
            faz_calistir(faz, args.dizin, args.kuru)
    else:
        try:
            faz_no = int(args.faz)
        except ValueError:
            print(f"Hata: Geçersiz faz: {args.faz}")
            sys.exit(1)
        faz_calistir(faz_no, args.dizin, args.kuru)


if __name__ == "__main__":
    main()
