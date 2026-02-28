#include "anlam.h"
#include "modul.h"
#include "hata.h"
#include <string.h>
#include <stdio.h>

static TipTürü dugum_analiz(AnlamÇözümleyici *ac, Düğüm *d);

/* Kapsam zincirinde en yakın sembol ismini bul (Levenshtein mesafe) */
static const char *yakin_sembol_bul(Kapsam *k, const char *isim, int esik) {
    const char *en_yakin = NULL;
    int en_kucuk = esik + 1;
    while (k) {
        for (int i = 0; i < TABLO_BOYUT; i++) {
            if (k->tablo[i] && k->tablo[i]->isim) {
                int mesafe = levenshtein_mesafe(isim, k->tablo[i]->isim, esik);
                if (mesafe < en_kucuk) {
                    en_kucuk = mesafe;
                    en_yakin = k->tablo[i]->isim;
                }
            }
        }
        k = k->ust;
    }
    return en_yakin;
}

/* Modüler kütüphane: metadata'dan sembol tablosuna kayıt */
static void modul_kaydet_generic(AnlamÇözümleyici *ac, const ModülTanım *m) {
    for (int i = 0; i < m->fonksiyon_sayisi; i++) {
        const ModülFonksiyon *f = &m->fonksiyonlar[i];
        Sembol *s = sembol_ekle(ac->arena, ac->kapsam, f->isim, f->dönüş_tipi);
        s->param_sayisi = f->param_sayisi;
        for (int j = 0; j < f->param_sayisi; j++)
            s->param_tipleri[j] = f->param_tipleri[j];
        s->dönüş_tipi = f->dönüş_tipi;
        s->runtime_isim = f->runtime_isim;
        /* ASCII alternatif */
        if (f->ascii_isim) {
            Sembol *s2 = sembol_ekle(ac->arena, ac->kapsam, f->ascii_isim, f->dönüş_tipi);
            s2->param_sayisi = f->param_sayisi;
            for (int j = 0; j < f->param_sayisi; j++)
                s2->param_tipleri[j] = f->param_tipleri[j];
            s2->dönüş_tipi = f->dönüş_tipi;
            s2->runtime_isim = f->runtime_isim;
        }
    }
}

/* ---- Modül kayıt fonksiyonları ---- */

static void matematik_modul_kaydet(AnlamÇözümleyici *ac) {
    Sembol *s;

    /* Inline asm fonksiyonları: bunlar _tanim.c'de değil, elle kaydedilir */

    /* mutlak(x: tam) -> tam */
    s = sembol_ekle(ac->arena, ac->kapsam, "mutlak", TİP_TAM);
    s->param_sayisi = 1; s->param_tipleri[0] = TİP_TAM; s->dönüş_tipi = TİP_TAM;

    /* kuvvet(x: tam, y: tam) -> tam */
    s = sembol_ekle(ac->arena, ac->kapsam, "kuvvet", TİP_TAM);
    s->param_sayisi = 2; s->param_tipleri[0] = TİP_TAM; s->param_tipleri[1] = TİP_TAM; s->dönüş_tipi = TİP_TAM;

    /* karekok(x: ondalık) -> ondalık */
    s = sembol_ekle(ac->arena, ac->kapsam, "karek\xc3\xb6" "k", TİP_ONDALIK);
    s->param_sayisi = 1; s->param_tipleri[0] = TİP_ONDALIK; s->dönüş_tipi = TİP_ONDALIK;

    /* min(x: tam, y: tam) -> tam */
    s = sembol_ekle(ac->arena, ac->kapsam, "min", TİP_TAM);
    s->param_sayisi = 2; s->param_tipleri[0] = TİP_TAM; s->param_tipleri[1] = TİP_TAM; s->dönüş_tipi = TİP_TAM;

    /* maks(x: tam, y: tam) -> tam */
    s = sembol_ekle(ac->arena, ac->kapsam, "maks", TİP_TAM);
    s->param_sayisi = 2; s->param_tipleri[0] = TİP_TAM; s->param_tipleri[1] = TİP_TAM; s->dönüş_tipi = TİP_TAM;

    /* mod(x: tam, y: tam) -> tam */
    s = sembol_ekle(ac->arena, ac->kapsam, "mod", TİP_TAM);
    s->param_sayisi = 2; s->param_tipleri[0] = TİP_TAM; s->param_tipleri[1] = TİP_TAM; s->dönüş_tipi = TİP_TAM;

    /* C-runtime fonksiyonları: metadata'dan oku */
    const ModülTanım *mt = modul_bul("matematik");
    if (mt) modul_kaydet_generic(ac, mt);
}

/* sistem_modul_kaydet — artık stdlib/sistem_tanim.c'den okunuyor */

static void dizi_modul_kaydet(AnlamÇözümleyici *ac) {
    const ModülTanım *mt = modul_bul("dizi");
    if (mt) modul_kaydet_generic(ac, mt);
}

/* Tip dönüşüm fonksiyonlarını kaydet (her zaman mevcut, modül gerektirmez) */
static void donusum_fonksiyonlari_kaydet(AnlamÇözümleyici *ac) {
    Sembol *s;

    /* metin_uzunluk(m: metin) -> tam — özel codegen (movq %rbx,%rax) */
    s = sembol_ekle(ac->arena, ac->kapsam, "metin_uzunluk", TİP_TAM);
    s->param_sayisi = 1; s->param_tipleri[0] = TİP_METİN; s->dönüş_tipi = TİP_TAM;

    /* eşlem, filtre, indirge — özel codegen (fonksiyon pointer leaq) */
    s = sembol_ekle(ac->arena, ac->kapsam, "e\xc5\x9flem", TİP_DİZİ);
    s->param_sayisi = 2; s->param_tipleri[0] = TİP_DİZİ; s->param_tipleri[1] = TİP_TAM; s->dönüş_tipi = TİP_DİZİ;

    s = sembol_ekle(ac->arena, ac->kapsam, "filtre", TİP_DİZİ);
    s->param_sayisi = 2; s->param_tipleri[0] = TİP_DİZİ; s->param_tipleri[1] = TİP_TAM; s->dönüş_tipi = TİP_DİZİ;

    s = sembol_ekle(ac->arena, ac->kapsam, "indirge", TİP_TAM);
    s->param_sayisi = 3; s->param_tipleri[0] = TİP_DİZİ; s->param_tipleri[1] = TİP_TAM; s->param_tipleri[2] = TİP_TAM; s->dönüş_tipi = TİP_TAM;

    /* biçimle — özel codegen (tip-bağımlı dispatch) */
    s = sembol_ekle(ac->arena, ac->kapsam, "bi\xc3\xa7imle", TİP_METİN);
    s->param_sayisi = 2; s->dönüş_tipi = TİP_METİN;
    s = sembol_ekle(ac->arena, ac->kapsam, "bicimle", TİP_METİN);
    s->param_sayisi = 2; s->dönüş_tipi = TİP_METİN;

    /* Modülden okunan fonksiyonlar: runtime_isim ile kaydedilir */
    const ModülTanım *metin_mt = modul_bul("metin");
    if (metin_mt) modul_kaydet_generic(ac, metin_mt);

    const ModülTanım *cekirdek_mt = modul_bul("\xc3\xa7" "ekirdek");
    if (!cekirdek_mt) cekirdek_mt = modul_bul("cekirdek");
    if (cekirdek_mt) modul_kaydet_generic(ac, cekirdek_mt);
}

/* Aşağıdaki modüller artık stdlib/*_tanim.c'den okunuyor:
 * küme, tekrarlayıcı, json, ağ, düzeni, paralel,
 * veritabanı, kripto, ortam, argüman, soket */

static void metin_modul_kaydet(AnlamÇözümleyici *ac) {
    Sembol *s;

    /* Inline asm fonksiyonları: bunlar _tanim.c'de değil, elle kaydedilir */

    /* harf_buyut(m: metin) -> metin */
    s = sembol_ekle(ac->arena, ac->kapsam, "harf_buyut", TİP_METİN);
    s->param_sayisi = 1; s->param_tipleri[0] = TİP_METİN; s->dönüş_tipi = TİP_METİN;

    /* harf_kucult(m: metin) -> metin */
    s = sembol_ekle(ac->arena, ac->kapsam, "harf_kucult", TİP_METİN);
    s->param_sayisi = 1; s->param_tipleri[0] = TİP_METİN; s->dönüş_tipi = TİP_METİN;

    /* kes(m: metin, başlangıç: tam, uzunluk: tam) -> metin */
    s = sembol_ekle(ac->arena, ac->kapsam, "kes", TİP_METİN);
    s->param_sayisi = 3; s->param_tipleri[0] = TİP_METİN; s->param_tipleri[1] = TİP_TAM; s->param_tipleri[2] = TİP_TAM; s->dönüş_tipi = TİP_METİN;

    /* bul(m: metin, aranan: metin) -> tam */
    s = sembol_ekle(ac->arena, ac->kapsam, "bul", TİP_TAM);
    s->param_sayisi = 2; s->param_tipleri[0] = TİP_METİN; s->param_tipleri[1] = TİP_METİN; s->dönüş_tipi = TİP_TAM;

    /* içerir(m: metin, aranan: metin) -> mantık */
    s = sembol_ekle(ac->arena, ac->kapsam, "i\xc3\xa7" "erir", TİP_MANTIK);
    s->param_sayisi = 2; s->param_tipleri[0] = TİP_METİN; s->param_tipleri[1] = TİP_METİN; s->dönüş_tipi = TİP_MANTIK;

    /* C-runtime fonksiyonları: metadata'dan oku */
    const ModülTanım *mt = modul_bul("metin");
    if (mt) modul_kaydet_generic(ac, mt);
}

static TipTürü ifade_analiz(AnlamÇözümleyici *ac, Düğüm *d) {
    if (!d) return TİP_BOŞLUK;

    TipTürü sonuç = TİP_BİLİNMİYOR;

    switch (d->tur) {
    case DÜĞÜM_TAM_SAYI:
        sonuç = TİP_TAM;
        d->sonuç_tipi = sonuç;
        return sonuç;

    case DÜĞÜM_ONDALIK_SAYI:
        sonuç = TİP_ONDALIK;
        d->sonuç_tipi = sonuç;
        return sonuç;

    case DÜĞÜM_METİN_DEĞERİ:
        sonuç = TİP_METİN;
        d->sonuç_tipi = sonuç;
        return sonuç;

    case DÜĞÜM_MANTIK_DEĞERİ:
        sonuç = TİP_MANTIK;
        d->sonuç_tipi = sonuç;
        return sonuç;

    case DÜĞÜM_BOŞ_DEĞER:
        sonuç = TİP_BİLİNMİYOR;
        d->sonuç_tipi = sonuç;
        return sonuç;

    case DÜĞÜM_TANIMLAYICI: {
        Sembol *s = sembol_ara(ac->kapsam, d->veri.tanimlayici.isim);
        if (!s) {
            hata_bildir(HATA_TANIMSIZ_DEĞİŞKEN, d->satir, d->sutun,
                        d->veri.tanimlayici.isim);
            const char *oneri = yakin_sembol_bul(ac->kapsam, d->veri.tanimlayici.isim, 3);
            if (oneri) hata_oneri_goster(oneri);
            d->sonuç_tipi = TİP_BİLİNMİYOR;
            return TİP_BİLİNMİYOR;
        }
        /* Başlatılmamış değişken uyarısı */
        if (!s->baslangic_var && !s->parametre_mi) {
            uyarı_bildir(UYARI_BAŞLANGIÇSIZ_DEĞİŞKEN, d->satir, d->sutun,
                         d->veri.tanimlayici.isim);
        }
        d->sonuç_tipi = s->tip;
        return s->tip;
    }

    case DÜĞÜM_ARALIK:
        /* Aralık düğümü: çocuklar[0]=alt, çocuklar[1]=üst */
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) ifade_analiz(ac, d->çocuklar[1]);
        d->sonuç_tipi = TİP_TAM;
        return TİP_TAM;

    case DÜĞÜM_BEKLE:
        /* bekle ifade - async sonucu */
        if (d->çocuk_sayısı > 0) {
            sonuç = ifade_analiz(ac, d->çocuklar[0]);
            d->sonuç_tipi = sonuç;
            return sonuç;
        }
        d->sonuç_tipi = TİP_BİLİNMİYOR;
        return TİP_BİLİNMİYOR;

    case DÜĞÜM_İKİLİ_İŞLEM: {
        TipTürü sol = ifade_analiz(ac, d->çocuklar[0]);
        TipTürü sag = ifade_analiz(ac, d->çocuklar[1]);

        SözcükTürü op = d->veri.islem.islem;

        /* Operatör yükleme: sol taraf sınıf ise metot çağrısına dönüştür */
        if (sol == TİP_SINIF) {
            if (op == TOK_ARTI || op == TOK_EKSI || op == TOK_ÇARPIM || op == TOK_EŞİT_EŞİT) {
                d->sonuç_tipi = TİP_SINIF;
                return TİP_SINIF;
            }
        }

        /* Karşılaştırma operatörleri mantık döndürür */
        if (op == TOK_EŞİT_EŞİT || op == TOK_EŞİT_DEĞİL ||
            op == TOK_KÜÇÜK || op == TOK_BÜYÜK ||
            op == TOK_KÜÇÜK_EŞİT || op == TOK_BÜYÜK_EŞİT) {
            d->sonuç_tipi = TİP_MANTIK;
            return TİP_MANTIK;
        }

        /* Mantık operatörleri */
        if (op == TOK_VE || op == TOK_VEYA) {
            d->sonuç_tipi = TİP_MANTIK;
            return TİP_MANTIK;
        }

        /* Bit işlemleri */
        if (op == TOK_BİT_VE || op == TOK_BİT_VEYA || op == TOK_BİT_XOR ||
            op == TOK_SOL_KAYDIR || op == TOK_SAĞ_KAYDIR) {
            d->sonuç_tipi = TİP_TAM;
            return TİP_TAM;
        }

        /* Boş birleştirme operatörü: sol ?? sağ */
        if (op == TOK_SORU_SORU) {
            /* Sağ tarafın tipini döndür (sol boş ise sağ kullanılır) */
            TipTürü sonuc_tip = (sag != TİP_BİLİNMİYOR) ? sag : sol;
            d->sonuç_tipi = sonuc_tip;
            return sonuc_tip;
        }

        /* Aritmetik */
        if (sol == TİP_METİN && sag == TİP_METİN && op == TOK_ARTI) {
            d->sonuç_tipi = TİP_METİN;
            return TİP_METİN;
        }

        if (sol == TİP_ONDALIK || sag == TİP_ONDALIK) {
            d->sonuç_tipi = TİP_ONDALIK;
            return TİP_ONDALIK;
        }
        if (sol == TİP_TAM && sag == TİP_TAM) {
            d->sonuç_tipi = TİP_TAM;
            return TİP_TAM;
        }

        d->sonuç_tipi = TİP_BİLİNMİYOR;
        return TİP_BİLİNMİYOR;
    }

    case DÜĞÜM_TEKLİ_İŞLEM:
        sonuç = ifade_analiz(ac, d->çocuklar[0]);
        d->sonuç_tipi = sonuç;
        return sonuç;

    case DÜĞÜM_ÇAĞRI: {
        /* Argümanları analiz et */
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_analiz(ac, d->çocuklar[i]);
        }

        /* yazdır özel durumu */
        if (d->veri.tanimlayici.isim &&
            strcmp(d->veri.tanimlayici.isim, "yazdır") == 0) {
            d->sonuç_tipi = TİP_BOŞLUK;
            return TİP_BOŞLUK;
        }

        /* doğrula özel durumu */
        if (d->veri.tanimlayici.isim &&
            strcmp(d->veri.tanimlayici.isim, "do\xc4\x9frula") == 0) {
            d->sonuç_tipi = TİP_BOŞLUK;
            return TİP_BOŞLUK;
        }

        /* doğrula_eşit özel durumu */
        if (d->veri.tanimlayici.isim &&
            (strcmp(d->veri.tanimlayici.isim, "do\xc4\x9frula_e\xc5\x9fit") == 0 ||
             strcmp(d->veri.tanimlayici.isim, "dogrula_esit") == 0)) {
            d->sonuç_tipi = TİP_BOŞLUK;
            return TİP_BOŞLUK;
        }

        /* doğrula_farklı özel durumu */
        if (d->veri.tanimlayici.isim &&
            (strcmp(d->veri.tanimlayici.isim, "do\xc4\x9frula_farkl\xc4\xb1") == 0 ||
             strcmp(d->veri.tanimlayici.isim, "dogrula_farkli") == 0)) {
            d->sonuç_tipi = TİP_BOŞLUK;
            return TİP_BOŞLUK;
        }

        /* uzunluk özel durumu */
        if (d->veri.tanimlayici.isim &&
            strcmp(d->veri.tanimlayici.isim, "uzunluk") == 0) {
            d->sonuç_tipi = TİP_TAM;
            return TİP_TAM;
        }

        /* Metin metotları: m.metot() */
        if (d->veri.tanimlayici.tip &&
            strcmp(d->veri.tanimlayici.tip, "metot") == 0 &&
            d->çocuk_sayısı > 0 &&
            d->çocuklar[0]->sonuç_tipi == TİP_METİN) {
            char *mn = d->veri.tanimlayici.isim;
            if (strcmp(mn, "uzunluk") == 0 || strcmp(mn, "byte_uzunluk") == 0 ||
                strcmp(mn, "say") == 0 ||
                strcmp(mn, "bosmu") == 0 || strcmp(mn, "bo\xc5\x9fmu") == 0 ||
                strcmp(mn, "rakammi") == 0 || strcmp(mn, "rakamm\xc4\xb1") == 0 ||
                strcmp(mn, "harfmi") == 0 || strcmp(mn, "boslukmu") == 0 ||
                strcmp(mn, "bo\xc5\x9flukmu") == 0 ||
                strcmp(mn, "baslar") == 0 || strcmp(mn, "ba\xc5\x9flar") == 0 ||
                strcmp(mn, "biter") == 0 ||
                strcmp(mn, "icerir") == 0 || strcmp(mn, "i\xc3\xa7erir") == 0) {
                d->sonuç_tipi = TİP_TAM;
                return TİP_TAM;
            }
            if (strcmp(mn, "kirp") == 0 || strcmp(mn, "k\xc4\xb1rp") == 0 ||
                strcmp(mn, "tersle") == 0 ||
                strcmp(mn, "degistir") == 0 || strcmp(mn, "de\xc4\x9fi\xc5\x9ftir") == 0 ||
                strcmp(mn, "buyuk_harf") == 0 || strcmp(mn, "b\xc3\xbcy\xc3\xbck_harf") == 0 ||
                strcmp(mn, "kucuk_harf") == 0 || strcmp(mn, "k\xc3\xbc\xc3\xa7\xc3\xbck_harf") == 0 ||
                strcmp(mn, "tekrarla") == 0) {
                d->sonuç_tipi = TİP_METİN;
                return TİP_METİN;
            }
            if (strcmp(mn, "bol") == 0 || strcmp(mn, "b\xc3\xb6l") == 0) {
                d->sonuç_tipi = TİP_DİZİ;
                return TİP_DİZİ;
            }
        }

        /* Diğer fonksiyonlar */
        if (d->veri.tanimlayici.isim) {
            Sembol *s = sembol_ara(ac->kapsam, d->veri.tanimlayici.isim);
            if (!s) {
                hata_bildir(HATA_TANIMSIZ_İŞLEV, d->satir, d->sutun,
                            d->veri.tanimlayici.isim);
                const char *oneri = yakin_sembol_bul(ac->kapsam, d->veri.tanimlayici.isim, 3);
                if (oneri) hata_oneri_goster(oneri);
                d->sonuç_tipi = TİP_BİLİNMİYOR;
                return TİP_BİLİNMİYOR;
            }

            /* Generic fonksiyon çağrısı monomorphization */
            if (s->generic_mi && d->veri.tanimlayici.cagri_tip_parametre) {
                char *somut_tip = d->veri.tanimlayici.cagri_tip_parametre;
                char *orijinal_isim = d->veri.tanimlayici.isim;

                /* Özelleştirilmiş fonksiyon adı: isim_tip (örn: degistir_tam) */
                int isim_uzun = strlen(orijinal_isim) + strlen(somut_tip) + 2;
                char *ozel_isim = arena_ayir(ac->arena, isim_uzun);
                snprintf(ozel_isim, isim_uzun, "%s_%s", orijinal_isim, somut_tip);

                /* Daha önce özelleştirilmiş mi kontrol et */
                Sembol *ozel_sem = sembol_ara(ac->kapsam, ozel_isim);
                if (!ozel_sem) {
                    /* Yeni özelleştirme oluştur */
                    TipTürü donus_tip = s->dönüş_tipi;
                    /* Dönüş tipi T ise somut tipe çevir */
                    if (s->tip_parametre &&
                        strcmp(s->tip_parametre, s->tip_parametre) == 0) {
                        donus_tip = tip_adı_çevir(somut_tip);
                    }

                    ozel_sem = sembol_ekle(ac->arena, ac->kapsam, ozel_isim, donus_tip);
                    if (ozel_sem) {
                        ozel_sem->dönüş_tipi = donus_tip;
                        ozel_sem->generic_mi = 0;
                        ozel_sem->somut_tip = somut_tip;
                        ozel_sem->generic_dugum = s->generic_dugum;
                        ozel_sem->param_sayisi = s->param_sayisi;

                        /* Parametre tiplerini güncelle (T -> somut_tip) */
                        for (int i = 0; i < s->param_sayisi; i++) {
                            if (s->param_tipleri[i] == TİP_BİLİNMİYOR &&
                                s->tip_parametre != NULL) {
                                ozel_sem->param_tipleri[i] = tip_adı_çevir(somut_tip);
                            } else {
                                ozel_sem->param_tipleri[i] = s->param_tipleri[i];
                            }
                        }
                    }

                    /* Özelleştirme listesine ekle */
                    if (ac->ozellestirme_kapasite == 0) {
                        ac->ozellestirme_kapasite = 16;
                        ac->ozellestirilmisler = arena_ayir(ac->arena,
                            sizeof(GenericÖzelleştirme) * ac->ozellestirme_kapasite);
                    } else if (ac->ozellestirme_sayisi >= ac->ozellestirme_kapasite) {
                        int yeni_kap = ac->ozellestirme_kapasite * 2;
                        GenericÖzelleştirme *yeni = arena_ayir(ac->arena,
                            sizeof(GenericÖzelleştirme) * yeni_kap);
                        for (int i = 0; i < ac->ozellestirme_sayisi; i++) {
                            yeni[i] = ac->ozellestirilmisler[i];
                        }
                        ac->ozellestirilmisler = yeni;
                        ac->ozellestirme_kapasite = yeni_kap;
                    }

                    GenericÖzelleştirme *oz = &ac->ozellestirilmisler[ac->ozellestirme_sayisi++];
                    oz->orijinal_isim = orijinal_isim;
                    oz->ozel_isim = ozel_isim;
                    oz->tip_parametre = s->tip_parametre;
                    oz->somut_tip = somut_tip;
                    oz->orijinal_dugum = (Düğüm*)s->generic_dugum;
                    oz->ozel_dugum = NULL;  /* Kod üretiminde doldurulacak */
                    oz->uretildi = 0;
                }

                /* Çağrı ismini özelleştirilmiş isme güncelle */
                d->veri.tanimlayici.isim = ozel_isim;
                d->sonuç_tipi = ozel_sem ? ozel_sem->dönüş_tipi : s->dönüş_tipi;
                return d->sonuç_tipi;
            }

            /* Sınıf yapıcı çağrısı: SinifAdi(arg1, arg2, ...) */
            if (s->sınıf_bilgi) {
                d->sonuç_tipi = TİP_SINIF;
                return TİP_SINIF;
            }
            /* Metot çağrısı: parametre kontrolünü atla (nesne implicit param) */
            if (d->veri.tanimlayici.tip &&
                strcmp(d->veri.tanimlayici.tip, "metot") == 0) {
                d->sonuç_tipi = s->tip;
                return s->tip;
            }
            /* Parametre sayısı kontrolü (varsayılan değerleri hesaba kat) */
            if (s->param_sayisi > 0 || s->dönüş_tipi != TİP_BİLİNMİYOR) {
                int min_param = s->param_sayisi - s->varsayilan_sayisi;
                if (d->çocuk_sayısı < min_param || d->çocuk_sayısı > s->param_sayisi) {
                    hata_bildir(HATA_PARAMETRE_SAYISI, d->satir, d->sutun,
                                d->veri.tanimlayici.isim, s->param_sayisi, d->çocuk_sayısı);
                }
            }
            d->sonuç_tipi = s->tip;
            return s->tip;
        }
        d->sonuç_tipi = TİP_BİLİNMİYOR;
        return TİP_BİLİNMİYOR;
    }

    case DÜĞÜM_BORU:
        ifade_analiz(ac, d->çocuklar[0]);
        sonuç = ifade_analiz(ac, d->çocuklar[1]);
        d->sonuç_tipi = sonuç;
        return sonuç;

    case DÜĞÜM_DİZİ_DEĞERİ:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_analiz(ac, d->çocuklar[i]);
        }
        d->sonuç_tipi = TİP_DİZİ;
        return TİP_DİZİ;

    case DÜĞÜM_KÜME_DEĞERİ:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_analiz(ac, d->çocuklar[i]);
        }
        d->sonuç_tipi = TİP_KÜME;
        return TİP_KÜME;

    case DÜĞÜM_LİSTE_ÜRETİMİ: {
        /* çocuklar[0]=ifade, [1]=kaynak, [2]=filtre(opsiyonel) */
        /* Kaynak diziyi analiz et */
        if (d->çocuk_sayısı > 1) ifade_analiz(ac, d->çocuklar[1]);
        /* Döngü değişkenini kaydet */
        if (d->veri.dongu.isim) {
            sembol_ekle(ac->arena, ac->kapsam, d->veri.dongu.isim, TİP_TAM);
        }
        /* İfadeyi analiz et */
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        /* Filtreyi analiz et */
        if (d->çocuk_sayısı > 2) ifade_analiz(ac, d->çocuklar[2]);
        d->sonuç_tipi = TİP_DİZİ;
        return TİP_DİZİ;
    }

    case DÜĞÜM_SÖZLÜK_ÜRETİMİ: {
        /* çocuklar[0]=key_expr, [1]=val_expr, [2]=kaynak, [3]=filtre(opsiyonel) */
        /* Önce kaynak analiz, sonra döngü değişkeni, sonra key/val/filtre */
        if (d->çocuk_sayısı > 2) ifade_analiz(ac, d->çocuklar[2]);
        if (d->veri.dongu.isim) {
            Sembol *s = sembol_ekle(ac->arena, ac->kapsam, d->veri.dongu.isim, TİP_TAM);
            s->baslangic_var = 1;
        }
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) ifade_analiz(ac, d->çocuklar[1]);
        if (d->çocuk_sayısı > 3) ifade_analiz(ac, d->çocuklar[3]);
        d->sonuç_tipi = TİP_SÖZLÜK;
        return TİP_SÖZLÜK;
    }

    case DÜĞÜM_SÖZLÜK_DEĞERİ: {
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_analiz(ac, d->çocuklar[i]);
        }
        d->sonuç_tipi = TİP_SÖZLÜK;
        return TİP_SÖZLÜK;
    }

    case DÜĞÜM_ERİŞİM:
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        d->sonuç_tipi = TİP_BİLİNMİYOR;
        return TİP_BİLİNMİYOR;

    case DÜĞÜM_DİZİ_ERİŞİM:
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) ifade_analiz(ac, d->çocuklar[1]);
        /* Metin indeksleme karakter (metin) dondurur */
        if (d->çocuk_sayısı > 0 && d->çocuklar[0]->sonuç_tipi == TİP_METİN) {
            d->sonuç_tipi = TİP_METİN;
            return TİP_METİN;
        }
        d->sonuç_tipi = TİP_TAM;
        return TİP_TAM;

    case DÜĞÜM_ERİŞİM_ATAMA:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_analiz(ac, d->çocuklar[i]);
        }
        d->sonuç_tipi = TİP_BOŞLUK;
        return TİP_BOŞLUK;

    case DÜĞÜM_ÜÇLÜ:
        /* koşul ? değer1 : değer2 */
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) {
            sonuç = ifade_analiz(ac, d->çocuklar[1]);
        }
        if (d->çocuk_sayısı > 2) ifade_analiz(ac, d->çocuklar[2]);
        d->sonuç_tipi = sonuç;
        return sonuç;

    case DÜĞÜM_DİLİM:
        /* dizi[başlangıç:bitiş] veya metin[başlangıç:bitiş] */
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) ifade_analiz(ac, d->çocuklar[1]);
        if (d->çocuk_sayısı > 2) ifade_analiz(ac, d->çocuklar[2]);
        /* Metin dilimi metin döndürür */
        if (d->çocuk_sayısı > 0 && d->çocuklar[0]->sonuç_tipi == TİP_METİN) {
            d->sonuç_tipi = TİP_METİN;
            return TİP_METİN;
        }
        d->sonuç_tipi = TİP_DİZİ;
        return TİP_DİZİ;

    case DÜĞÜM_DEMET:
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_analiz(ac, d->çocuklar[i]);
        }
        d->sonuç_tipi = TİP_TAM;
        return TİP_TAM;

    /* Sonuç/Seçenek tip sistemi */
    case DÜĞÜM_SONUÇ_OLUŞTUR:
        /* Tamam(değer) veya Hata(değer) */
        if (d->çocuk_sayısı > 0) {
            ifade_analiz(ac, d->çocuklar[0]);
        }
        d->sonuç_tipi = TİP_SONUÇ;
        return TİP_SONUÇ;

    case DÜĞÜM_SEÇENEK_OLUŞTUR:
        /* Bir(değer) veya Hiç */
        if (d->çocuk_sayısı > 0) {
            ifade_analiz(ac, d->çocuklar[0]);
        }
        d->sonuç_tipi = TİP_SEÇENEK;
        return TİP_SEÇENEK;

    case DÜĞÜM_SORU_OP: {
        /* ifade? - hata yayılımı operatörü */
        if (d->çocuk_sayısı > 0) {
            TipTürü ic_tip = ifade_analiz(ac, d->çocuklar[0]);
            /* İfade Sonuç veya Seçenek tipi olmalı */
            if (ic_tip != TİP_SONUÇ && ic_tip != TİP_SEÇENEK) {
                /* Uyarı: ? operatörü sadece Sonuç veya Seçenek tipinde kullanılmalı */
                /* Şimdilik hata vermeden devam et */
            }
            /* Sonuç iç tip (başarı durumu) */
            d->sonuç_tipi = TİP_BİLİNMİYOR;  /* İç tip bilinmiyor henüz */
            return TİP_BİLİNMİYOR;
        }
        d->sonuç_tipi = TİP_BİLİNMİYOR;
        return TİP_BİLİNMİYOR;
    }

    default:
        d->sonuç_tipi = TİP_BİLİNMİYOR;
        return TİP_BİLİNMİYOR;
    }
}

static void blok_analiz(AnlamÇözümleyici *ac, Düğüm *blok) {
    if (!blok) return;
    for (int i = 0; i < blok->çocuk_sayısı; i++) {
        dugum_analiz(ac, blok->çocuklar[i]);
    }
}

/* Arayüz uygulama kontrolü: metot isimleri ve imzaları doğrula */
static void arayuz_kontrol_et(AnlamÇözümleyici *ac, Düğüm *d,
                               SinifBilgi *sb, Sembol *ay_sem) {
    for (int i = 0; i < ay_sem->arayuz_metot_sayisi; i++) {
        ArayuzMetotImza *imza = &ay_sem->arayuz_imzalar[i];
        if (!imza->isim) continue;

        /* 1. Metot adı var mı? */
        int bulundu = 0;
        for (int j = 0; j < sb->metot_sayisi; j++) {
            if (sb->metot_isimleri[j] &&
                strcmp(sb->metot_isimleri[j], imza->isim) == 0) {
                bulundu = 1;
                break;
            }
        }
        if (!bulundu) {
            hata_bildir(HATA_ARAYÜZ_UYGULAMA, d->satir, d->sutun,
                        d->veri.sinif.isim, imza->isim);
            continue;
        }

        /* 2. İmza kontrolü (param_sayisi >= 0 ise imza bilgisi var) */
        if (imza->param_sayisi >= 0) {
            char mangled[256];
            snprintf(mangled, sizeof(mangled), "%s_%s", d->veri.sinif.isim, imza->isim);
            Sembol *metot_sem = sembol_ara(ac->kapsam, mangled);
            if (metot_sem) {
                /* Parametre sayısı (bu parametresini çıkar) */
                int beklenen = imza->param_sayisi;
                int mevcut = metot_sem->param_sayisi > 0 ? metot_sem->param_sayisi - 1 : 0;
                if (mevcut != beklenen) {
                    hata_bildir(HATA_ARAYÜZ_IMZA, d->satir, d->sutun,
                                d->veri.sinif.isim, imza->isim);
                }
                /* Dönüş tipi kontrolü */
                if (imza->dönüş_tipi != TİP_BOŞLUK &&
                    metot_sem->dönüş_tipi != imza->dönüş_tipi) {
                    hata_bildir(HATA_ARAYÜZ_IMZA, d->satir, d->sutun,
                                d->veri.sinif.isim, imza->isim);
                }
            }
        }
    }
}

static TipTürü dugum_analiz(AnlamÇözümleyici *ac, Düğüm *d) {
    if (!d) return TİP_BOŞLUK;

    switch (d->tur) {
    case DÜĞÜM_DEĞİŞKEN: {
        TipTürü tip = tip_adı_çevir(d->veri.değişken.tip);
        /* Tip çıkarımı: değişken x = ifade -> tipi sağ taraftan al */
        if (tip == TİP_BİLİNMİYOR && d->veri.değişken.tip &&
            strcmp(d->veri.değişken.tip, "bilinmiyor") == 0 && d->çocuk_sayısı > 0) {
            TipTürü cikarilan = ifade_analiz(ac, d->çocuklar[0]);
            if (cikarilan != TİP_BİLİNMİYOR) {
                tip = cikarilan;
                /* Sınıf tipinde sınıf adını koru (kod üretici metot çağrıları için) */
                if (tip == TİP_SINIF && d->çocuklar[0]->tur == DÜĞÜM_ÇAĞRI &&
                    d->çocuklar[0]->veri.tanimlayici.isim) {
                    d->veri.değişken.tip = d->çocuklar[0]->veri.tanimlayici.isim;
                } else {
                    d->veri.değişken.tip = (char *)tip_adı(tip);
                }
            }
            Sembol *s = sembol_ekle(ac->arena, ac->kapsam, d->veri.değişken.isim, tip);
            s->baslangic_var = 1;
            s->global_mi = (d->veri.değişken.genel == 1) ? 1 : 0;
            s->sabit_mi = d->veri.değişken.sabit;
            if (tip == TİP_SINIF) s->sınıf_adı = d->veri.değişken.tip;
            return tip;
        }
        /* Tip takma adı çözümleme */
        if (tip == TİP_BİLİNMİYOR && d->veri.değişken.tip) {
            /* Önce tip takma adı olarak ara */
            Sembol *alias = sembol_ara(ac->kapsam, d->veri.değişken.tip);
            if (alias && alias->tip == TİP_BİLİNMİYOR && alias->dönüş_tipi != TİP_BİLİNMİYOR) {
                /* Bu bir tip takma adı */
                tip = alias->dönüş_tipi;
                d->veri.değişken.tip = (char *)tip_adı(tip);
            }
        }
        /* Sınıf tipi kontrolü: tip_adı_çevir bilinmiyor dönerse, sınıf olabilir */
        if (tip == TİP_BİLİNMİYOR && d->veri.değişken.tip) {
            SinifBilgi *sb = sınıf_bul(ac->kapsam, d->veri.değişken.tip);
            if (sb) {
                tip = TİP_SINIF;
                Sembol *s = sembol_ekle(ac->arena, ac->kapsam, d->veri.değişken.isim, tip);
                s->sınıf_adı = d->veri.değişken.tip;
                s->baslangic_var = (d->çocuk_sayısı > 0);
                s->global_mi = d->veri.değişken.genel;
                if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
                return tip;
            }
        }
        {
            Sembol *s = sembol_ekle(ac->arena, ac->kapsam, d->veri.değişken.isim, tip);
            s->baslangic_var = (d->çocuk_sayısı > 0) || (d->veri.değişken.genel == 2);
            s->global_mi = (d->veri.değişken.genel == 1) ? 1 : 0;
            s->sabit_mi = d->veri.değişken.sabit;
        }
        if (d->çocuk_sayısı > 0) {
            ifade_analiz(ac, d->çocuklar[0]);
        }
        return tip;
    }

    case DÜĞÜM_SINIF: {
        /* Sınıf tanımını sembol tablosuna kaydet */
        SinifBilgi *sb = (SinifBilgi *)arena_ayir(ac->arena, sizeof(SinifBilgi));
        sb->isim = d->veri.sinif.isim;
        sb->alan_sayisi = 0;
        sb->metot_sayisi = 0;
        sb->boyut = 0;

        /* Kalıtım: ebeveyn alanlarını kopyala */
        if (d->veri.sinif.ebeveyn) {
            SinifBilgi *ebeveyn = sınıf_bul(ac->kapsam, d->veri.sinif.ebeveyn);
            if (ebeveyn) {
                for (int i = 0; i < ebeveyn->alan_sayisi && sb->alan_sayisi < 64; i++) {
                    sb->alanlar[sb->alan_sayisi] = ebeveyn->alanlar[i];
                    sb->alan_sayisi++;
                }
                for (int i = 0; i < ebeveyn->metot_sayisi && sb->metot_sayisi < 64; i++) {
                    sb->metot_isimleri[sb->metot_sayisi] = ebeveyn->metot_isimleri[i];
                    sb->metot_sayisi++;
                }
            } else {
                /* Arayüz mü kontrol et - arayüzse hata verme */
                Sembol *ebeveyn_sem = sembol_ara(ac->kapsam, d->veri.sinif.ebeveyn);
                if (!ebeveyn_sem || ebeveyn_sem->arayuz_metot_sayisi == 0) {
                    hata_bildir(HATA_TANIMSIZ_DEĞİŞKEN, d->satir, d->sutun,
                                d->veri.sinif.ebeveyn);
                }
            }
        }

        /* Alanları ve metotları topla */
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            Düğüm *cocuk = d->çocuklar[i];
            if (cocuk->tur == DÜĞÜM_DEĞİŞKEN) {
                /* Alan */
                if (sb->alan_sayisi < 64) {
                    sb->alanlar[sb->alan_sayisi].isim = cocuk->veri.değişken.isim;
                    sb->alanlar[sb->alan_sayisi].tip = tip_adı_çevir(cocuk->veri.değişken.tip);
                    sb->alanlar[sb->alan_sayisi].offset = sb->alan_sayisi * 8;
                    sb->alan_sayisi++;
                } else {
                    hata_bildir(HATA_SINIR_AŞIMI, cocuk->satir, cocuk->sutun,
                                "s\xc4\xb1n\xc4\xb1f en fazla 64 alan i\xc3\xa7erebilir");
                }
            } else if (cocuk->tur == DÜĞÜM_İŞLEV) {
                if (sb->metot_sayisi < 64) {
                    sb->metot_isimleri[sb->metot_sayisi] = cocuk->veri.islev.isim;
                    sb->metot_sayisi++;
                } else {
                    hata_bildir(HATA_SINIR_AŞIMI, cocuk->satir, cocuk->sutun,
                                "s\xc4\xb1n\xc4\xb1f en fazla 64 metot i\xc3\xa7erebilir");
                }
            }
        }
        sb->boyut = sb->alan_sayisi * 8;

        Sembol *s = sembol_ekle(ac->arena, ac->kapsam, d->veri.sinif.isim, TİP_SINIF);
        if (s) s->sınıf_bilgi = sb;

        /* Metotları analiz et (bu parametresi ile) */
        char *onceki_sinif = ac->mevcut_sinif;
        ac->mevcut_sinif = d->veri.sinif.isim;
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            if (d->çocuklar[i]->tur == DÜĞÜM_İŞLEV) {
                dugum_analiz(ac, d->çocuklar[i]);
            }
        }
        ac->mevcut_sinif = onceki_sinif;

        /* Arayüz uygulama kontrolü (metot analizi sonrasında) */
        /* 1. uygula ile belirtilen arayüzler */
        for (int ai = 0; ai < d->veri.sinif.arayuz_sayisi; ai++) {
            char *ay_isim = d->veri.sinif.arayuzler[ai];
            Sembol *ay_sem = sembol_ara(ac->kapsam, ay_isim);
            if (!ay_sem || ay_sem->arayuz_metot_sayisi == 0) {
                hata_bildir(HATA_TANIMSIZ_DEĞİŞKEN, d->satir, d->sutun, ay_isim);
                continue;
            }
            arayuz_kontrol_et(ac, d, sb, ay_sem);
        }
        /* 2. Geriye uyumluluk: ebeveyn arayüzse kontrol et */
        if (d->veri.sinif.ebeveyn) {
            Sembol *ebeveyn_sem = sembol_ara(ac->kapsam, d->veri.sinif.ebeveyn);
            if (ebeveyn_sem && ebeveyn_sem->arayuz_metot_sayisi > 0) {
                arayuz_kontrol_et(ac, d, sb, ebeveyn_sem);
            }
        }

        return TİP_SINIF;
    }

    case DÜĞÜM_İŞLEV: {
        TipTürü donus = d->veri.islev.dönüş_tipi ?
            tip_adı_çevir(d->veri.islev.dönüş_tipi) : TİP_BOŞLUK;

        /* Generic fonksiyon mu kontrol et */
        if (d->veri.islev.tip_parametre != NULL) {
            /* Generic fonksiyon - sadece kaydet, analizi ertele */
            Sembol *fn_sem = sembol_ekle(ac->arena, ac->kapsam, d->veri.islev.isim, donus);
            if (fn_sem) {
                fn_sem->dönüş_tipi = donus;
                fn_sem->generic_mi = 1;
                fn_sem->tip_parametre = d->veri.islev.tip_parametre;
                fn_sem->generic_dugum = d;

                /* Parametre bilgilerini kaydet */
                if (d->çocuk_sayısı > 0) {
                    Düğüm *params = d->çocuklar[0];
                    fn_sem->param_sayisi = params->çocuk_sayısı;
                    for (int i = 0; i < params->çocuk_sayısı && i < 32; i++) {
                        fn_sem->param_tipleri[i] = tip_adı_çevir(params->çocuklar[i]->veri.değişken.tip);
                    }
                }
            }

            /* Generic fonksiyonları listeye ekle */
            if (ac->generic_islev_kapasite == 0) {
                ac->generic_islev_kapasite = 16;
                ac->generic_islevler = arena_ayir(ac->arena,
                    sizeof(Düğüm*) * ac->generic_islev_kapasite);
            } else if (ac->generic_islev_sayisi >= ac->generic_islev_kapasite) {
                int yeni_kap = ac->generic_islev_kapasite * 2;
                Düğüm **yeni = arena_ayir(ac->arena, sizeof(Düğüm*) * yeni_kap);
                for (int i = 0; i < ac->generic_islev_sayisi; i++) {
                    yeni[i] = ac->generic_islevler[i];
                }
                ac->generic_islevler = yeni;
                ac->generic_islev_kapasite = yeni_kap;
            }
            ac->generic_islevler[ac->generic_islev_sayisi++] = d;

            return TİP_İŞLEV;
        }

        Sembol *fn_sem = sembol_ekle(ac->arena, ac->kapsam, d->veri.islev.isim, donus);
        if (fn_sem) {
            fn_sem->dönüş_tipi = donus;
            fn_sem->generic_mi = 0;
            /* Parametre bilgilerini kaydet */
            if (d->çocuk_sayısı > 0) {
                Düğüm *params = d->çocuklar[0];
                fn_sem->param_sayisi = params->çocuk_sayısı;
                int varsayilan = 0;
                for (int i = 0; i < params->çocuk_sayısı && i < 32; i++) {
                    fn_sem->param_tipleri[i] = tip_adı_çevir(params->çocuklar[i]->veri.değişken.tip);
                    /* Varsayılan değer varsa (çocuk düğüm olarak) */
                    if (params->çocuklar[i]->çocuk_sayısı > 0) {
                        fn_sem->varsayilan_dugumler[i] = params->çocuklar[i]->çocuklar[0];
                        varsayilan++;
                    }
                }
                fn_sem->varsayilan_sayisi = varsayilan;
            }
        }

        /* Yeni kapsam */
        Kapsam *onceki = ac->kapsam;
        ac->kapsam = kapsam_oluştur(ac->arena, onceki);
        ac->kapsam->yerel_sayac = 0;
        ac->islev_icinde++;

        /* Sınıf metodu ise "bu" ekle */
        if (ac->mevcut_sinif) {
            Sembol *bu_s = sembol_ekle(ac->arena, ac->kapsam, "bu", TİP_SINIF);
            if (bu_s) {
                bu_s->parametre_mi = 1;
                bu_s->sınıf_adı = ac->mevcut_sinif;
                bu_s->sınıf_bilgi = sınıf_bul(ac->kapsam, ac->mevcut_sinif);
            }
        }

        /* Tip parametresi bağlamını ayarla (özelleştirilmiş generic için) */
        char *onceki_tip_param = ac->mevcut_tip_parametre;
        char *onceki_somut_tip = ac->mevcut_somut_tip;

        /* Parametreleri ekle */
        if (d->çocuk_sayısı > 0) {
            Düğüm *params = d->çocuklar[0];
            for (int i = 0; i < params->çocuk_sayısı; i++) {
                Düğüm *p = params->çocuklar[i];
                TipTürü p_tip;
                /* Tip parametresi mi kontrol et */
                if (ac->mevcut_tip_parametre && p->veri.değişken.tip &&
                    strcmp(p->veri.değişken.tip, ac->mevcut_tip_parametre) == 0) {
                    /* T -> somut tipe çevir */
                    p_tip = tip_adı_çevir(ac->mevcut_somut_tip);
                } else {
                    p_tip = tip_adı_çevir(p->veri.değişken.tip);
                }
                Sembol *s = sembol_ekle(ac->arena, ac->kapsam, p->veri.değişken.isim, p_tip);
                if (s) s->parametre_mi = 1;
            }
        }

        /* Gövdeyi analiz et */
        if (d->çocuk_sayısı > 1) {
            blok_analiz(ac, d->çocuklar[1]);
        }

        ac->mevcut_tip_parametre = onceki_tip_param;
        ac->mevcut_somut_tip = onceki_somut_tip;
        ac->islev_icinde--;
        ac->kapsam = onceki;
        return TİP_İŞLEV;
    }

    case DÜĞÜM_EĞER: {
        /* İlk çocuk koşul, ikinci doğru bloğu */
        ifade_analiz(ac, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) blok_analiz(ac, d->çocuklar[1]);
        /* yoksa eğer / yoksa blokları */
        for (int i = 2; i < d->çocuk_sayısı; i++) {
            if (d->çocuklar[i]->tur == DÜĞÜM_BLOK) {
                blok_analiz(ac, d->çocuklar[i]);
            } else {
                ifade_analiz(ac, d->çocuklar[i]);
            }
        }
        return TİP_BOŞLUK;
    }

    case DÜĞÜM_DÖNGÜ: {
        Kapsam *onceki = ac->kapsam;
        ac->kapsam = kapsam_oluştur(ac->arena, onceki);
        ac->dongu_icinde++;

        /* Etiketli kır/devam için döngü değişkenini yığına it */
        if (d->veri.dongu.isim && ac->dongu_derinligi < MAKS_DONGU_DERINLIK) {
            ac->dongu_degiskenleri[ac->dongu_derinligi++] = d->veri.dongu.isim;
        }

        sembol_ekle(ac->arena, ac->kapsam, d->veri.dongu.isim, TİP_TAM);
        ifade_analiz(ac, d->çocuklar[0]);
        ifade_analiz(ac, d->çocuklar[1]);
        /* Adım değeri varsa (4 çocuk): başlangıç, bitiş, adım, gövde */
        if (d->çocuk_sayısı > 3) {
            ifade_analiz(ac, d->çocuklar[2]); /* adım */
            blok_analiz(ac, d->çocuklar[3]);  /* gövde */
        } else if (d->çocuk_sayısı > 2) {
            blok_analiz(ac, d->çocuklar[2]);  /* gövde (adımsız) */
        }

        if (d->veri.dongu.isim && ac->dongu_derinligi > 0) {
            ac->dongu_derinligi--;
        }
        ac->dongu_icinde--;
        ac->kapsam = onceki;
        return TİP_BOŞLUK;
    }

    case DÜĞÜM_İKEN: {
        ac->dongu_icinde++;
        ifade_analiz(ac, d->çocuklar[0]);
        if (d->çocuk_sayısı > 1) blok_analiz(ac, d->çocuklar[1]);
        if (d->çocuk_sayısı > 2) blok_analiz(ac, d->çocuklar[2]); /* yoksa */
        ac->dongu_icinde--;
        return TİP_BOŞLUK;
    }


    case DÜĞÜM_DÖNDÜR:
        if (!ac->islev_icinde) {
            hata_bildir(HATA_İŞLEV_DIŞI_DÖNDÜR, d->satir, d->sutun);
        }
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        return TİP_BOŞLUK;

    case DÜĞÜM_KIR:
        if (!ac->dongu_icinde) {
            hata_bildir(HATA_DÖNGÜ_DIŞI_KIR, d->satir, d->sutun);
        } else if (d->veri.tanimlayici.isim) {
            /* Etiketli kır: hedef döngü değişkenini doğrula */
            int bulundu = 0;
            for (int i = 0; i < ac->dongu_derinligi; i++) {
                if (ac->dongu_degiskenleri[i] &&
                    strcmp(ac->dongu_degiskenleri[i], d->veri.tanimlayici.isim) == 0) {
                    bulundu = 1;
                    break;
                }
            }
            if (!bulundu) {
                char hata_mesaj[256];
                snprintf(hata_mesaj, sizeof(hata_mesaj),
                         "bilinmeyen d\xc3\xb6ng\xc3\xbc etiketi: '%s'",
                         d->veri.tanimlayici.isim);
                hata_bildir(HATA_SINIR_AŞIMI, d->satir, d->sutun, hata_mesaj);
            }
        }
        return TİP_BOŞLUK;

    case DÜĞÜM_DEVAM:
        if (!ac->dongu_icinde) {
            hata_bildir(HATA_DÖNGÜ_DIŞI_KIR, d->satir, d->sutun);
        } else if (d->veri.tanimlayici.isim) {
            int bulundu = 0;
            for (int i = 0; i < ac->dongu_derinligi; i++) {
                if (ac->dongu_degiskenleri[i] &&
                    strcmp(ac->dongu_degiskenleri[i], d->veri.tanimlayici.isim) == 0) {
                    bulundu = 1;
                    break;
                }
            }
            if (!bulundu) {
                char hata_mesaj[256];
                snprintf(hata_mesaj, sizeof(hata_mesaj),
                         "bilinmeyen d\xc3\xb6ng\xc3\xbc etiketi: '%s'",
                         d->veri.tanimlayici.isim);
                hata_bildir(HATA_SINIR_AŞIMI, d->satir, d->sutun, hata_mesaj);
            }
        }
        return TİP_BOŞLUK;

    case DÜĞÜM_ATAMA: {
        Sembol *s = sembol_ara(ac->kapsam, d->veri.tanimlayici.isim);
        if (!s) {
            hata_bildir(HATA_TANIMSIZ_DEĞİŞKEN, d->satir, d->sutun,
                        d->veri.tanimlayici.isim);
        } else {
            s->baslangic_var = 1;
        }
        if (s && s->sabit_mi) {
            hata_bildir(HATA_SABİT_ATAMA, d->satir, d->sutun,
                        d->veri.tanimlayici.isim);
        }
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        return TİP_BOŞLUK;
    }

    case DÜĞÜM_DİZİ_ATAMA:
        /* çocuklar[0]=dizi, çocuklar[1]=indeks, çocuklar[2]=değer */
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_analiz(ac, d->çocuklar[i]);
        }
        return TİP_BOŞLUK;

    case DÜĞÜM_ERİŞİM_ATAMA:
        /* çocuklar[0]=nesne, çocuklar[1]=değer */
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            ifade_analiz(ac, d->çocuklar[i]);
        }
        return TİP_BOŞLUK;

    case DÜĞÜM_İFADE_BİLDİRİMİ:
        if (d->çocuk_sayısı > 0) return dugum_analiz(ac, d->çocuklar[0]);
        return TİP_BOŞLUK;

    case DÜĞÜM_KULLAN:
        if (d->veri.kullan.modul) {
            /* Dosya modülleri (.tr uzantılı) ana.c'de işlendi, burada atla */
            int modul_len = (int)strlen(d->veri.kullan.modul);
            if (modul_len > 3 && strcmp(d->veri.kullan.modul + modul_len - 3, ".tr") == 0) {
                /* Dosya modülü: zaten textual inclusion ile işlendi */
            } else if (strcmp(d->veri.kullan.modul, "matematik") == 0) {
                matematik_modul_kaydet(ac);
            } else if (strcmp(d->veri.kullan.modul, "metin") == 0) {
                metin_modul_kaydet(ac);
            } else if (strcmp(d->veri.kullan.modul, "dizi") == 0) {
                dizi_modul_kaydet(ac);
            } else {
                /* Modül registrysinden ara (sistem, zaman, vb.) */
                /* Modül registrysinden ara */
                const ModülTanım *mt = modul_bul(d->veri.kullan.modul);
                if (mt) {
                    modul_kaydet_generic(ac, mt);
                } else {
                    hata_bildir(HATA_TANIMSIZ_İŞLEV, d->satir, d->sutun,
                                d->veri.kullan.modul);
                }
            }
        }
        return TİP_BOŞLUK;

    case DÜĞÜM_BLOK:
        blok_analiz(ac, d);
        return TİP_BOŞLUK;

    case DÜĞÜM_EŞLE:
        /* çocuklar[0]=ifade, sonra (deger, blok) çiftleri, opsiyonel son varsayılan blok */
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            if (d->çocuklar[i]->tur == DÜĞÜM_BLOK)
                blok_analiz(ac, d->çocuklar[i]);
            else
                ifade_analiz(ac, d->çocuklar[i]);
        }
        return TİP_BOŞLUK;

    case DÜĞÜM_HER_İÇİN: {
        /* çocuklar[0]=dizi, çocuklar[1]=gövde, çocuklar[2]=yoksa (opsiyonel) */
        ac->dongu_icinde++;
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        /* Yeni kapsam: döngü değişkenini ekle */
        Kapsam *onceki = ac->kapsam;
        ac->kapsam = kapsam_oluştur(ac->arena, onceki);
        if (d->veri.dongu.isim) {
            sembol_ekle(ac->arena, ac->kapsam, d->veri.dongu.isim, TİP_TAM);
            if (ac->dongu_derinligi < MAKS_DONGU_DERINLIK) {
                ac->dongu_degiskenleri[ac->dongu_derinligi++] = d->veri.dongu.isim;
            }
        }
        if (d->çocuk_sayısı > 1) blok_analiz(ac, d->çocuklar[1]);
        if (d->çocuk_sayısı > 2) blok_analiz(ac, d->çocuklar[2]); /* yoksa */
        if (d->veri.dongu.isim && ac->dongu_derinligi > 0) {
            ac->dongu_derinligi--;
        }
        ac->kapsam = onceki;
        ac->dongu_icinde--;
        return TİP_BOŞLUK;
    }

    case DÜĞÜM_DENE_YAKALA: {
        /* çocuklar[0]=dene, çocuklar[1..N]=yakala blokları, son=sonunda (opsiyonel) */
        if (d->çocuk_sayısı > 0) blok_analiz(ac, d->çocuklar[0]);
        int sonunda_var = (d->veri.tanimlayici.tip &&
                           strcmp(d->veri.tanimlayici.tip, "sonunda") == 0);
        int yakala_son = sonunda_var ? d->çocuk_sayısı - 1 : d->çocuk_sayısı;
        for (int i = 1; i < yakala_son; i++) {
            blok_analiz(ac, d->çocuklar[i]);
        }
        if (sonunda_var && d->çocuk_sayısı > 1) {
            blok_analiz(ac, d->çocuklar[d->çocuk_sayısı - 1]);
        }
        return TİP_BOŞLUK;
    }

    case DÜĞÜM_FIRLAT:
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        return TİP_BOŞLUK;

    case DÜĞÜM_WALRUS: {
        /* isim := ifade → değişkeni tanımla ve ifade sonucunu döndür */
        TipTürü tip = TİP_TAM;
        if (d->çocuk_sayısı > 0) {
            tip = ifade_analiz(ac, d->çocuklar[0]);
        }
        if (d->veri.tanimlayici.isim) {
            Sembol *ws = sembol_ara(ac->kapsam, d->veri.tanimlayici.isim);
            if (!ws) {
                ws = sembol_ekle(ac->arena, ac->kapsam, d->veri.tanimlayici.isim, tip);
                ws->baslangic_var = 1;
            }
        }
        d->sonuç_tipi = tip;
        return tip;
    }

    case DÜĞÜM_PAKET_AÇ:
        /* Son çocuk = kaynak ifade, diğerleri = hedef değişkenler */
        if (d->çocuk_sayısı > 0) {
            ifade_analiz(ac, d->çocuklar[d->çocuk_sayısı - 1]);
        }
        for (int i = 0; i < d->çocuk_sayısı - 1; i++) {
            if (d->çocuklar[i]->tur == DÜĞÜM_TANIMLAYICI) {
                sembol_ekle(ac->arena, ac->kapsam, d->çocuklar[i]->veri.tanimlayici.isim, TİP_TAM);
            }
        }
        return TİP_BOŞLUK;

    case DÜĞÜM_İLE_İSE:
        /* çocuklar[0]=kaynak ifade, çocuklar[1]=gövde */
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        if (d->veri.tanimlayici.isim) {
            sembol_ekle(ac->arena, ac->kapsam, d->veri.tanimlayici.isim, TİP_TAM);
        }
        if (d->çocuk_sayısı > 1) blok_analiz(ac, d->çocuklar[1]);
        return TİP_BOŞLUK;

    case DÜĞÜM_LAMBDA: {
        TipTürü donus = d->veri.islev.dönüş_tipi ?
            tip_adı_çevir(d->veri.islev.dönüş_tipi) : TİP_BOŞLUK;
        Sembol *fn_sem = sembol_ekle(ac->arena, ac->kapsam, d->veri.islev.isim, donus);
        if (fn_sem) fn_sem->dönüş_tipi = donus;

        Kapsam *onceki = ac->kapsam;
        ac->kapsam = kapsam_oluştur(ac->arena, onceki);
        ac->kapsam->yerel_sayac = 0;
        ac->islev_icinde++;

        /* Parametreleri ekle */
        if (d->çocuk_sayısı > 0) {
            Düğüm *params = d->çocuklar[0];
            for (int i = 0; i < params->çocuk_sayısı; i++) {
                Düğüm *p = params->çocuklar[i];
                TipTürü p_tip = tip_adı_çevir(p->veri.değişken.tip);
                Sembol *s = sembol_ekle(ac->arena, ac->kapsam, p->veri.değişken.isim, p_tip);
                if (s) s->parametre_mi = 1;
            }
        }
        if (d->çocuk_sayısı > 1) blok_analiz(ac, d->çocuklar[1]);
        ac->islev_icinde--;
        ac->kapsam = onceki;
        return TİP_İŞLEV;
    }

    case DÜĞÜM_PROGRAM:
        blok_analiz(ac, d);
        return TİP_BOŞLUK;

    case DÜĞÜM_SAYIM: {
        /* Sayım (enum) tanımı: her değeri tam sayı olarak kaydet */
        Sembol *s = sembol_ekle(ac->arena, ac->kapsam, d->veri.sayim.isim, TİP_SAYIM);
        if (s) {
            for (int i = 0; i < d->çocuk_sayısı && i < 64; i++) {
                Düğüm *deger = d->çocuklar[i];
                if (deger->veri.tanimlayici.isim) {
                    s->sayim_degerler[i] = deger->veri.tanimlayici.isim;
                    /* Her enum değerini global sabit olarak kaydet */
                    Sembol *es = sembol_ekle(ac->arena, ac->kapsam, deger->veri.tanimlayici.isim, TİP_TAM);
                    if (es) {
                        es->sabit_mi = 1;
                        es->baslangic_var = 1;
                    }
                }
            }
            s->sayim_deger_sayisi = d->çocuk_sayısı;
        }
        return TİP_SAYIM;
    }

    case DÜĞÜM_ARAYÜZ: {
        /* Arayüz (interface) tanımı: metot imzalarını kaydet */
        Sembol *s = sembol_ekle(ac->arena, ac->kapsam, d->veri.sayim.isim, TİP_BİLİNMİYOR);
        if (s) {
            for (int i = 0; i < d->çocuk_sayısı && i < 32; i++) {
                Düğüm *metot = d->çocuklar[i];
                if (metot->tur == DÜĞÜM_İŞLEV) {
                    /* Tam imza: işlev isim(param: tip) -> dönüş_tipi */
                    s->arayuz_imzalar[i].isim = metot->veri.islev.isim;
                    s->arayuz_imzalar[i].dönüş_tipi = metot->veri.islev.dönüş_tipi
                        ? tip_adı_çevir(metot->veri.islev.dönüş_tipi) : TİP_BOŞLUK;
                    if (metot->çocuk_sayısı > 0) {
                        Düğüm *params = metot->çocuklar[0];
                        s->arayuz_imzalar[i].param_sayisi = params->çocuk_sayısı;
                        for (int j = 0; j < params->çocuk_sayısı && j < 32; j++)
                            s->arayuz_imzalar[i].param_tipleri[j] =
                                tip_adı_çevir(params->çocuklar[j]->veri.değişken.tip);
                    } else {
                        s->arayuz_imzalar[i].param_sayisi = 0;
                    }
                } else {
                    /* Geriye uyumluluk: sadece isim (DÜĞÜM_TANIMLAYICI) */
                    s->arayuz_imzalar[i].isim = metot->veri.tanimlayici.isim;
                    s->arayuz_imzalar[i].param_sayisi = -1; /* imza yok */
                }
            }
            s->arayuz_metot_sayisi = d->çocuk_sayısı;
        }
        return TİP_BİLİNMİYOR;
    }

    case DÜĞÜM_TEST:
        /* Test bloğu: gövdeyi analiz et */
        if (d->çocuk_sayısı > 0) blok_analiz(ac, d->çocuklar[0]);
        return TİP_BOŞLUK;

    case DÜĞÜM_TİP_TANIMI: {
        /* Tip takma adı: tip Sayı = tam */
        TipTürü hedef = tip_adı_çevir(d->veri.tanimlayici.tip);
        Sembol *s = sembol_ekle(ac->arena, ac->kapsam, d->veri.tanimlayici.isim, TİP_BİLİNMİYOR);
        if (s) s->dönüş_tipi = hedef;  /* dönüş_tipi'de hedef tipi sakla */
        return TİP_BOŞLUK;
    }

    case DÜĞÜM_ÜRET:
        /* üreteç yield */
        if (d->çocuk_sayısı > 0) ifade_analiz(ac, d->çocuklar[0]);
        return TİP_BOŞLUK;

    default:
        return ifade_analiz(ac, d);
    }
}

int anlam_çözümle(AnlamÇözümleyici *ac, Düğüm *program, Arena *arena, const char *hedef) {
    ac->arena = arena;
    ac->kapsam = kapsam_oluştur(arena, NULL);
    ac->islev_icinde = 0;
    ac->dongu_icinde = 0;
    ac->mevcut_sinif = NULL;

    /* Monomorphization başlatma */
    ac->generic_islevler = NULL;
    ac->generic_islev_sayisi = 0;
    ac->generic_islev_kapasite = 0;
    ac->ozellestirilmisler = NULL;
    ac->ozellestirme_sayisi = 0;
    ac->ozellestirme_kapasite = 0;
    ac->mevcut_tip_parametre = NULL;
    ac->mevcut_somut_tip = NULL;

    donusum_fonksiyonlari_kaydet(ac);

    /* Gömülü hedeflerde donanım fonksiyonlarını otomatik kaydet
     * (kullan "donanim" yazmaya gerek kalmaz) */
    if (hedef && (strcmp(hedef, "wasm") == 0 ||
                  strcmp(hedef, "avr") == 0 ||
                  strcmp(hedef, "xtensa") == 0 ||
                  strcmp(hedef, "esp32") == 0 ||
                  strcmp(hedef, "arm-m0") == 0 ||
                  strcmp(hedef, "pico") == 0)) {
        const ModülTanım *mt = modul_bul("donan\xc4\xb1m");
        if (!mt) mt = modul_bul("donanim");
        if (mt) modul_kaydet_generic(ac, mt);
    }

    dugum_analiz(ac, program);

    return hata_sayisi;
}
