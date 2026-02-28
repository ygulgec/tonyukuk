#include "cozumleyici.h"
#include "hata.h"
#include <string.h>
#include <stdio.h>

/* Azami hata sayısı — bu sınıra ulaşılınca ayrıştırma durur */
#define AZAMI_HATA 20

/* İleri bildirimler */
static char *tip_oku(Cozumleyici *c);
static Düğüm *bildirim_cozumle(Cozumleyici *c);
static Düğüm *boru_ifade(Cozumleyici *c);

/* Yardımcı fonksiyonlar */

static Sözcük *mevcut_sozcuk(Cozumleyici *c) {
    if (c->pos >= c->sozcuk_sayisi) return &c->sozcukler[c->sozcuk_sayisi - 1];
    return &c->sozcukler[c->pos];
}

static Sözcük *onceki_sozcuk(Cozumleyici *c) __attribute__((unused));
static Sözcük *onceki_sozcuk(Cozumleyici *c) {
    if (c->pos > 0) return &c->sozcukler[c->pos - 1];
    return &c->sozcukler[0];
}

static int kontrol(Cozumleyici *c, SözcükTürü tur) {
    return mevcut_sozcuk(c)->tur == tur;
}

/* Lookahead: belirtilen offset'teki sözcüğün türüne bak */
static Sözcük *peek(Cozumleyici *c, int offset) {
    int target = c->pos + offset;
    if (target >= c->sozcuk_sayisi) return &c->sozcukler[c->sozcuk_sayisi - 1];
    if (target < 0) return &c->sozcukler[0];
    return &c->sozcukler[target];
}

/* Generic fonksiyon çağrısı mı kontrol et: f<tip>( pattern'i mi? */
static int generic_cagri_mi(Cozumleyici *c) {
    /* Mevcut token: < (TOK_KÜÇÜK)
     * Beklenen pattern: < TANIMLAYICI > (
     */
    if (!kontrol(c, TOK_KÜÇÜK)) return 0;

    /* Sonraki token bir tanımlayıcı (tip adı) olmalı */
    Sözcük *s1 = peek(c, 1);
    if (s1->tur != TOK_TANIMLAYICI &&
        s1->tur != TOK_TAM &&
        s1->tur != TOK_ONDALIK &&
        s1->tur != TOK_METİN &&
        s1->tur != TOK_MANTIK &&
        s1->tur != TOK_DİZİ) return 0;

    /* Ondan sonra > olmalı */
    Sözcük *s2 = peek(c, 2);
    if (s2->tur != TOK_BÜYÜK) return 0;

    /* Ondan sonra ( olmalı */
    Sözcük *s3 = peek(c, 3);
    if (s3->tur != TOK_PAREN_AC) return 0;

    return 1;
}

static Sözcük *ilerle(Cozumleyici *c) {
    Sözcük *s = mevcut_sozcuk(c);
    if (s->tur != TOK_DOSYA_SONU) c->pos++;
    return s;
}

static int esle_ve_ilerle(Cozumleyici *c, SözcükTürü tur) {
    if (kontrol(c, tur)) {
        ilerle(c);
        return 1;
    }
    return 0;
}

static Sözcük *bekle(Cozumleyici *c, SözcükTürü tur) {
    if (kontrol(c, tur)) {
        return ilerle(c);
    }
    Sözcük *m = mevcut_sozcuk(c);
    hata_bildir(HATA_BEKLENEN_SÖZCÜK, m->satir, m->sutun,
                sözcük_tür_adı(tur), sözcük_tür_adı(m->tur));
    c->panik_modu = 1;
    return NULL;
}

static void yeni_satir_atla(Cozumleyici *c) {
    while (kontrol(c, TOK_YENİ_SATIR)) ilerle(c);
}

static void yeni_satir_bekle(Cozumleyici *c) {
    if (kontrol(c, TOK_YENİ_SATIR) || kontrol(c, TOK_DOSYA_SONU)) {
        yeni_satir_atla(c);
    }
}

/* Panik modu kurtarma: eşzamanlama noktasına kadar sözcükleri atla */
static void senkronize(Cozumleyici *c) {
    c->panik_modu = 0;
    while (!kontrol(c, TOK_DOSYA_SONU)) {
        /* Yeni satır sonrası bildirim başlangıcı mı? */
        if (c->pos > 0 && c->sozcukler[c->pos - 1].tur == TOK_YENİ_SATIR) {
            SözcükTürü t = mevcut_sozcuk(c)->tur;
            if (t == TOK_İŞLEV || t == TOK_SINIF || t == TOK_EĞER ||
                t == TOK_DÖNGÜ || t == TOK_İKEN || t == TOK_HER ||
                t == TOK_DÖNDÜR || t == TOK_KULLAN || t == TOK_SAYIM ||
                t == TOK_ARAYÜZ || t == TOK_DENE || t == TOK_TEST ||
                t == TOK_SABIT || t == TOK_SOYUT || t == TOK_EŞZAMANSIZ ||
                t == TOK_AT || t == TOK_TAM || t == TOK_ONDALIK ||
                t == TOK_METİN || t == TOK_MANTIK || t == TOK_DİZİ ||
                t == TOK_DEĞİŞKEN || t == TOK_TİP_TANIMI || t == TOK_FIRLAT ||
                t == TOK_YAZDIR || t == TOK_TANIMLAYICI) {
                return;
            }
        }
        if (kontrol(c, TOK_SON)) return;
        ilerle(c);
    }
}

static char *sozcuk_metni(Cozumleyici *c, Sözcük *s) {
    return arena_strndup(c->arena, s->başlangıç, s->uzunluk);
}

/* İleri bildirimler */
static Düğüm *ifade_cozumle(Cozumleyici *c);
static Düğüm *bildirim_cozumle(Cozumleyici *c);
static Düğüm *boru_ifade(Cozumleyici *c);

/* ---- İfade ayrıştırma (öncelik sırasıyla) ---- */

static Düğüm *birincil_ifade(Cozumleyici *c) {
    Sözcük *s = mevcut_sozcuk(c);

    /* Tam sayı */
    if (kontrol(c, TOK_TAM_SAYI)) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_TAM_SAYI, s->satir, s->sutun);
        d->veri.tam_deger = s->deger.tam_deger;
        return d;
    }

    /* Ondalık sayı */
    if (kontrol(c, TOK_ONDALIK_SAYI)) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_ONDALIK_SAYI, s->satir, s->sutun);
        d->veri.ondalık_değer = s->deger.ondalık_değer;
        return d;
    }

    /* Metin değeri */
    if (kontrol(c, TOK_METİN_DEĞERİ)) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_METİN_DEĞERİ, s->satir, s->sutun);
        d->veri.metin_değer = arena_strndup(c->arena, s->başlangıç, s->uzunluk);
        return d;
    }

    /* Boolean */
    if (kontrol(c, TOK_DOĞRU) || kontrol(c, TOK_YANLIŞ)) {
        int deger = s->tur == TOK_DOĞRU ? 1 : 0;
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_MANTIK_DEĞERİ, s->satir, s->sutun);
        d->veri.mantık_değer = deger;
        return d;
    }

    /* boş (null) literal */
    if (kontrol(c, TOK_BOŞ)) {
        ilerle(c);
        return düğüm_oluştur(c->arena, DÜĞÜM_BOŞ_DEĞER, s->satir, s->sutun);
    }

    /* Tamam(değer) - Sonuç::Tamam yapıcısı */
    if (kontrol(c, TOK_TAMAM)) {
        ilerle(c);  /* Tamam */
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_SONUÇ_OLUŞTUR, s->satir, s->sutun);
        d->veri.sonuç_seçenek.varyant = 0;  /* Tamam = 0 */
        d->veri.sonuç_seçenek.hata_tipi = NULL;
        bekle(c, TOK_PAREN_AC);
        düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));  /* değer */
        bekle(c, TOK_PAREN_KAPA);
        return d;
    }

    /* Hata(değer) veya Hata(HataTipi(değer)) - Sonuç::Hata yapıcısı */
    if (kontrol(c, TOK_HATA_SONUÇ)) {
        ilerle(c);  /* Hata */
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_SONUÇ_OLUŞTUR, s->satir, s->sutun);
        d->veri.sonuç_seçenek.varyant = 1;  /* Hata = 1 */
        d->veri.sonuç_seçenek.hata_tipi = NULL;
        bekle(c, TOK_PAREN_AC);
        /* İç ifadeyi çözümle - eğer tanımlayıcı ise hata tipi olabilir */
        Düğüm *ic_ifade = boru_ifade(c);
        /* Hata tipi adı belirleme (örn: Hata(Bulunamadı(yol))) */
        if (ic_ifade->tur == DÜĞÜM_ÇAĞRI && ic_ifade->veri.tanimlayici.isim) {
            d->veri.sonuç_seçenek.hata_tipi = ic_ifade->veri.tanimlayici.isim;
        }
        düğüm_çocuk_ekle(c->arena, d, ic_ifade);
        bekle(c, TOK_PAREN_KAPA);
        return d;
    }

    /* Bir(değer) - Seçenek::Bir yapıcısı */
    if (kontrol(c, TOK_BİR)) {
        ilerle(c);  /* Bir */
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_SEÇENEK_OLUŞTUR, s->satir, s->sutun);
        d->veri.sonuç_seçenek.varyant = 0;  /* Bir = 0 */
        d->veri.sonuç_seçenek.hata_tipi = NULL;
        bekle(c, TOK_PAREN_AC);
        düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));  /* değer */
        bekle(c, TOK_PAREN_KAPA);
        return d;
    }

    /* Hiç - Seçenek::Hiç yapıcısı (değersiz) */
    if (kontrol(c, TOK_HİÇ)) {
        ilerle(c);  /* Hiç */
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_SEÇENEK_OLUŞTUR, s->satir, s->sutun);
        d->veri.sonuç_seçenek.varyant = 1;  /* Hiç = 1 */
        d->veri.sonuç_seçenek.hata_tipi = NULL;
        return d;
    }

    /* bekle ifade() - async/await */
    if (kontrol(c, TOK_BEKLE)) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_BEKLE, s->satir, s->sutun);
        düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
        return d;
    }

    /* yazdır(...) - yerleşik fonksiyon çağrısı */
    if (kontrol(c, TOK_YAZDIR)) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_ÇAĞRI, s->satir, s->sutun);
        d->veri.tanimlayici.isim = arena_strdup(c->arena, "yazdır");
        bekle(c, TOK_PAREN_AC);
        if (!kontrol(c, TOK_PAREN_KAPA)) {
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
            while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
                düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
            }
        }
        bekle(c, TOK_PAREN_KAPA);
        return d;
    }

    /* doğrula(...) - test assertion */
    if (kontrol(c, TOK_DOĞRULA)) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_ÇAĞRI, s->satir, s->sutun);
        d->veri.tanimlayici.isim = arena_strdup(c->arena, "do\xc4\x9frula");
        bekle(c, TOK_PAREN_AC);
        if (!kontrol(c, TOK_PAREN_KAPA)) {
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
        }
        bekle(c, TOK_PAREN_KAPA);
        return d;
    }

    /* bu.alan */
    if (kontrol(c, TOK_BU)) {
        ilerle(c);
        bekle(c, TOK_NOKTA);
        Sözcük *alan = bekle(c, TOK_TANIMLAYICI);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_ERİŞİM, s->satir, s->sutun);
        Düğüm *bu = düğüm_oluştur(c->arena, DÜĞÜM_TANIMLAYICI, s->satir, s->sutun);
        bu->veri.tanimlayici.isim = arena_strdup(c->arena, "bu");
        düğüm_çocuk_ekle(c->arena, d, bu);
        if (alan) {
            d->veri.tanimlayici.isim = sozcuk_metni(c, alan);
        }
        return d;
    }

    /* Tanımlayıcı veya walrus (:=) */
    if (kontrol(c, TOK_TANIMLAYICI)) {
        /* Walrus: isim := ifade */
        if (c->pos + 1 < c->sozcuk_sayisi &&
            c->sozcukler[c->pos + 1].tur == TOK_WALRUS) {
            Sözcük *isim_s = ilerle(c);  /* tanımlayıcı */
            ilerle(c);                    /* := */
            Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_WALRUS, s->satir, s->sutun);
            d->veri.tanimlayici.isim = sozcuk_metni(c, isim_s);
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));  /* değer ifadesi */
            return d;
        }
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_TANIMLAYICI, s->satir, s->sutun);
        d->veri.tanimlayici.isim = sozcuk_metni(c, s);
        return d;
    }

    /* Parantezli ifade */
    if (kontrol(c, TOK_PAREN_AC)) {
        ilerle(c);
        Düğüm *d = boru_ifade(c);
        bekle(c, TOK_PAREN_KAPA);
        return d;
    }

    /* Lambda: işlev(params) -> tip ... son */
    if (kontrol(c, TOK_İŞLEV)) {
        /* İşlev sonrası ( geliyorsa lambda, tanımlayıcı geliyorsa normal fonksiyon */
        if (c->pos + 1 < c->sozcuk_sayisi &&
            c->sozcukler[c->pos + 1].tur == TOK_PAREN_AC) {
            static int lambda_sayac = 0;
            Sözcük *başlangıç = ilerle(c);  /* işlev */
            Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_LAMBDA, başlangıç->satir, başlangıç->sutun);
            char isim_buf[64];
            snprintf(isim_buf, sizeof(isim_buf), "__lambda_%d", lambda_sayac++);
            d->veri.islev.isim = arena_strdup(c->arena, isim_buf);
            d->veri.islev.dönüş_tipi = NULL;

            /* Parametreler */
            bekle(c, TOK_PAREN_AC);
            Düğüm *params = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, başlangıç->satir, başlangıç->sutun);
            if (!kontrol(c, TOK_PAREN_KAPA)) {
                do {
                    Sözcük *p_isim = bekle(c, TOK_TANIMLAYICI);
                    bekle(c, TOK_İKİ_NOKTA);
                    char *p_tip = tip_oku(c);
                    Düğüm *param = düğüm_oluştur(c->arena, DÜĞÜM_DEĞİŞKEN, p_isim ? p_isim->satir : 0, 0);
                    param->veri.değişken.isim = p_isim ? sozcuk_metni(c, p_isim) : arena_strdup(c->arena, "?");
                    param->veri.değişken.tip = p_tip;
                    düğüm_çocuk_ekle(c->arena, params, param);
                } while (esle_ve_ilerle(c, TOK_VİRGÜL));
            }
            bekle(c, TOK_PAREN_KAPA);
            düğüm_çocuk_ekle(c->arena, d, params);

            /* Dönüş tipi */
            if (esle_ve_ilerle(c, TOK_OK)) {
                d->veri.islev.dönüş_tipi = tip_oku(c);
            }

            /* Gövde */
            yeni_satir_atla(c);
            esle_ve_ilerle(c, TOK_İSE);
            yeni_satir_atla(c);

            Düğüm *govde = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, mevcut_sozcuk(c)->satir, mevcut_sozcuk(c)->sutun);
            while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
                yeni_satir_atla(c);
                if (kontrol(c, TOK_SON) || kontrol(c, TOK_DOSYA_SONU)) break;
                düğüm_çocuk_ekle(c->arena, govde, bildirim_cozumle(c));
                yeni_satir_bekle(c);
            }
            bekle(c, TOK_SON);
            düğüm_çocuk_ekle(c->arena, d, govde);
            return d;
        }
    }

    /* Küme değeri küme{1, 2, 3} */
    if (kontrol(c, TOK_KÜME)) {
        ilerle(c);
        bekle(c, TOK_SÜSLÜ_AÇ);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_KÜME_DEĞERİ, s->satir, s->sutun);
        if (!kontrol(c, TOK_SÜSLÜ_KAPA)) {
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
            while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
                düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
            }
        }
        bekle(c, TOK_SÜSLÜ_KAPA);
        return d;
    }

    /* Sözlük değeri {k: v, ...} veya sözlük üretimi {k: v her x için kaynak} */
    if (kontrol(c, TOK_SÜSLÜ_AÇ)) {
        ilerle(c);
        if (kontrol(c, TOK_SÜSLÜ_KAPA)) {
            /* Boş sözlük {} */
            Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_SÖZLÜK_DEĞERİ, s->satir, s->sutun);
            bekle(c, TOK_SÜSLÜ_KAPA);
            return d;
        }
        /* İlk anahtar ifadesi */
        Düğüm *ilk_anahtar = boru_ifade(c);
        bekle(c, TOK_İKİ_NOKTA);
        Düğüm *ilk_deger = boru_ifade(c);
        /* Sözlük üretimi: {k: v her x için kaynak eğer koşul} */
        if (kontrol(c, TOK_HER)) {
            ilerle(c);  /* her */
            Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_SÖZLÜK_ÜRETİMİ, s->satir, s->sutun);
            düğüm_çocuk_ekle(c->arena, d, ilk_anahtar);   /* çocuklar[0] = key expr */
            düğüm_çocuk_ekle(c->arena, d, ilk_deger);     /* çocuklar[1] = val expr */
            /* Döngü değişkeni */
            Sözcük *deg = bekle(c, TOK_TANIMLAYICI);
            d->veri.dongu.isim = deg ? sozcuk_metni(c, deg) : arena_strdup(c->arena, "?");
            bekle(c, TOK_İÇİN);
            /* Kaynak dizi ifadesi */
            Düğüm *kaynak = boru_ifade(c);
            düğüm_çocuk_ekle(c->arena, d, kaynak);        /* çocuklar[2] = kaynak */
            /* Opsiyonel filtre: eğer koşul */
            if (kontrol(c, TOK_EĞER)) {
                ilerle(c);
                Düğüm *filtre = boru_ifade(c);
                düğüm_çocuk_ekle(c->arena, d, filtre);    /* çocuklar[3] = filtre */
            }
            bekle(c, TOK_SÜSLÜ_KAPA);
            return d;
        }
        /* Normal sözlük literali {k1: v1, k2: v2, ...} */
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_SÖZLÜK_DEĞERİ, s->satir, s->sutun);
        düğüm_çocuk_ekle(c->arena, d, ilk_anahtar);
        düğüm_çocuk_ekle(c->arena, d, ilk_deger);
        while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));  /* anahtar */
            bekle(c, TOK_İKİ_NOKTA);
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));  /* değer */
        }
        bekle(c, TOK_SÜSLÜ_KAPA);
        return d;
    }

    /* Dizi değeri [1, 2, 3] veya liste üretimi [ifade her x için kaynak] */
    if (kontrol(c, TOK_KÖŞELİ_AÇ)) {
        ilerle(c);
        if (kontrol(c, TOK_KÖŞELİ_KAPA)) {
            /* Boş dizi [] */
            Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_DİZİ_DEĞERİ, s->satir, s->sutun);
            bekle(c, TOK_KÖŞELİ_KAPA);
            return d;
        }
        /* İlk ifadeyi oku (... rest desteği) */
        Düğüm *ilk;
        if (kontrol(c, TOK_ÜÇ_NOKTA)) {
            Sözcük *rs = mevcut_sozcuk(c);
            ilerle(c); /* ... */
            Sözcük *rest_isim = bekle(c, TOK_TANIMLAYICI);
            ilk = düğüm_oluştur(c->arena, DÜĞÜM_TANIMLAYICI, rs->satir, rs->sutun);
            char *isim_str = rest_isim ? sozcuk_metni(c, rest_isim) : arena_strdup(c->arena, "?");
            char *rest_str = arena_ayir(c->arena, strlen(isim_str) + 4);
            sprintf(rest_str, "...%s", isim_str);
            ilk->veri.tanimlayici.isim = rest_str;
        } else {
            ilk = boru_ifade(c);
        }
        /* Liste üretimi: [ifade her x için kaynak eğer koşul] */
        if (kontrol(c, TOK_HER)) {
            ilerle(c);  /* her */
            Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_LİSTE_ÜRETİMİ, s->satir, s->sutun);
            düğüm_çocuk_ekle(c->arena, d, ilk);  /* çocuklar[0] = ifade */
            /* Döngü değişkeni */
            Sözcük *deg = bekle(c, TOK_TANIMLAYICI);
            d->veri.dongu.isim = deg ? sozcuk_metni(c, deg) : arena_strdup(c->arena, "?");
            bekle(c, TOK_İÇİN);
            /* Kaynak dizi ifadesi */
            Düğüm *kaynak = boru_ifade(c);
            düğüm_çocuk_ekle(c->arena, d, kaynak);  /* çocuklar[1] = kaynak */
            /* Opsiyonel filtre: eğer koşul */
            if (kontrol(c, TOK_EĞER)) {
                ilerle(c);
                Düğüm *filtre = boru_ifade(c);
                düğüm_çocuk_ekle(c->arena, d, filtre);  /* çocuklar[2] = filtre */
            }
            bekle(c, TOK_KÖŞELİ_KAPA);
            return d;
        }
        /* Normal dizi literali */
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_DİZİ_DEĞERİ, s->satir, s->sutun);
        düğüm_çocuk_ekle(c->arena, d, ilk);
        while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
            if (kontrol(c, TOK_ÜÇ_NOKTA)) {
                Sözcük *rs = mevcut_sozcuk(c);
                ilerle(c); /* ... */
                Sözcük *rest_isim = bekle(c, TOK_TANIMLAYICI);
                Düğüm *rest = düğüm_oluştur(c->arena, DÜĞÜM_TANIMLAYICI, rs->satir, rs->sutun);
                char *isim_str = rest_isim ? sozcuk_metni(c, rest_isim) : arena_strdup(c->arena, "?");
                char *rest_str = arena_ayir(c->arena, strlen(isim_str) + 4);
                sprintf(rest_str, "...%s", isim_str);
                rest->veri.tanimlayici.isim = rest_str;
                düğüm_çocuk_ekle(c->arena, d, rest);
            } else {
                düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
            }
        }
        bekle(c, TOK_KÖŞELİ_KAPA);
        return d;
    }

    hata_bildir(HATA_BEKLENEN_İFADE, s->satir, s->sutun);
    c->panik_modu = 1;
    ilerle(c);
    return düğüm_oluştur(c->arena, DÜĞÜM_TAM_SAYI, s->satir, s->sutun);
}

/* Çağrı, erişim, indeksleme */
static Düğüm *cagri_ifade(Cozumleyici *c) {
    Düğüm *sol = birincil_ifade(c);

    while (1) {
        /* Generic fonksiyon çağrısı: f<tip>(...) */
        if (sol->tur == DÜĞÜM_TANIMLAYICI && generic_cagri_mi(c)) {
            /* < işareti - generic tip parametresi */
            ilerle(c);  /* < atla */
            char *tip_param = tip_oku(c);  /* tip ismini oku */
            bekle(c, TOK_BÜYÜK);  /* > atla */
            ilerle(c);  /* ( atla */

            Düğüm *cagri = düğüm_oluştur(c->arena, DÜĞÜM_ÇAĞRI, sol->satir, sol->sutun);
            cagri->veri.tanimlayici.isim = sol->veri.tanimlayici.isim;
            cagri->veri.tanimlayici.cagri_tip_parametre = tip_param;

            if (!kontrol(c, TOK_PAREN_KAPA)) {
                düğüm_çocuk_ekle(c->arena, cagri, boru_ifade(c));
                while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
                    düğüm_çocuk_ekle(c->arena, cagri, boru_ifade(c));
                }
            }
            bekle(c, TOK_PAREN_KAPA);
            sol = cagri;
        } else if (kontrol(c, TOK_PAREN_AC)) {
            /* fonksiyon çağrısı */
            ilerle(c);
            Düğüm *cagri = düğüm_oluştur(c->arena, DÜĞÜM_ÇAĞRI, sol->satir, sol->sutun);
            cagri->veri.tanimlayici.cagri_tip_parametre = NULL;
            if (sol->tur == DÜĞÜM_ERİŞİM) {
                /* Metot çağrısı: nesne.metot(args...) */
                cagri->veri.tanimlayici.isim = sol->veri.tanimlayici.isim;
                cagri->veri.tanimlayici.tip = arena_strdup(c->arena, "metot");
                /* Nesne referansını ilk argüman olarak ekle */
                if (sol->çocuk_sayısı > 0) {
                    düğüm_çocuk_ekle(c->arena, cagri, sol->çocuklar[0]);
                }
            } else {
                cagri->veri.tanimlayici.isim = sol->veri.tanimlayici.isim;
            }
            if (!kontrol(c, TOK_PAREN_KAPA)) {
                düğüm_çocuk_ekle(c->arena, cagri, boru_ifade(c));
                while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
                    düğüm_çocuk_ekle(c->arena, cagri, boru_ifade(c));
                }
            }
            bekle(c, TOK_PAREN_KAPA);
            sol = cagri;
        } else if (kontrol(c, TOK_NOKTA)) {
            /* alan erişimi */
            ilerle(c);
            Sözcük *alan = bekle(c, TOK_TANIMLAYICI);
            Düğüm *erisim = düğüm_oluştur(c->arena, DÜĞÜM_ERİŞİM, sol->satir, sol->sutun);
            if (alan) erisim->veri.tanimlayici.isim = sozcuk_metni(c, alan);
            düğüm_çocuk_ekle(c->arena, erisim, sol);
            sol = erisim;
        } else if (kontrol(c, TOK_KÖŞELİ_AÇ)) {
            /* dizi indeksleme veya dilim */
            ilerle(c);
            Düğüm *ilk = boru_ifade(c);
            if (kontrol(c, TOK_İKİ_NOKTA)) {
                /* Dilim: dizi[başlangıç:bitiş] */
                ilerle(c);  /* : atla */
                Düğüm *ikinci = boru_ifade(c);
                Düğüm *dilim = düğüm_oluştur(c->arena, DÜĞÜM_DİLİM, sol->satir, sol->sutun);
                düğüm_çocuk_ekle(c->arena, dilim, sol);     /* çocuklar[0] = dizi */
                düğüm_çocuk_ekle(c->arena, dilim, ilk);     /* çocuklar[1] = başlangıç */
                düğüm_çocuk_ekle(c->arena, dilim, ikinci);  /* çocuklar[2] = bitiş */
                bekle(c, TOK_KÖŞELİ_KAPA);
                sol = dilim;
            } else {
                /* Normal indeksleme */
                Düğüm *indeks = düğüm_oluştur(c->arena, DÜĞÜM_DİZİ_ERİŞİM, sol->satir, sol->sutun);
                düğüm_çocuk_ekle(c->arena, indeks, sol);
                düğüm_çocuk_ekle(c->arena, indeks, ilk);
                bekle(c, TOK_KÖŞELİ_KAPA);
                sol = indeks;
            }
        } else if (kontrol(c, TOK_SORU)) {
            /* ? operatörü - Sonuç/Seçenek hata yayılımı */
            Sözcük *soru = ilerle(c);  /* ? */
            Düğüm *soru_op = düğüm_oluştur(c->arena, DÜĞÜM_SORU_OP, soru->satir, soru->sutun);
            düğüm_çocuk_ekle(c->arena, soru_op, sol);  /* ifade? */
            sol = soru_op;
        } else {
            break;
        }
    }

    return sol;
}

/* Tekli operatör: değil, -, ~ */
static Düğüm *tekli_ifade(Cozumleyici *c) {
    if (kontrol(c, TOK_DEĞİL) || kontrol(c, TOK_EKSI) || kontrol(c, TOK_BİT_DEĞİL)) {
        Sözcük *op = ilerle(c);

        /* Constant folding: -sayi -> negatif literal */
        if (op->tur == TOK_EKSI) {
            if (kontrol(c, TOK_TAM_SAYI)) {
                Sözcük *sayi = ilerle(c);
                Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_TAM_SAYI, op->satir, op->sutun);
                d->veri.tam_deger = -(sayi->deger.tam_deger);
                return d;
            }
            if (kontrol(c, TOK_ONDALIK_SAYI)) {
                Sözcük *sayi = ilerle(c);
                Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_ONDALIK_SAYI, op->satir, op->sutun);
                d->veri.ondalık_değer = -(sayi->deger.ondalık_değer);
                return d;
            }
        }

        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_TEKLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, tekli_ifade(c));
        return d;
    }
    return cagri_ifade(c);
}

/* Çarpma, bölme, mod */
static Düğüm *carpma_ifade(Cozumleyici *c) {
    Düğüm *sol = tekli_ifade(c);
    while (kontrol(c, TOK_ÇARPIM) || kontrol(c, TOK_BÖLME) || kontrol(c, TOK_YÜZDE)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, tekli_ifade(c));
        sol = d;
    }
    return sol;
}

/* Toplama, çıkarma */
static Düğüm *toplama_ifade(Cozumleyici *c) {
    Düğüm *sol = carpma_ifade(c);
    while (kontrol(c, TOK_ARTI) || kontrol(c, TOK_EKSI)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, carpma_ifade(c));
        sol = d;
    }
    return sol;
}

/* Bit kaydırma: <<, >> */
static Düğüm *bit_kaydir_ifade(Cozumleyici *c) {
    Düğüm *sol = toplama_ifade(c);
    while (kontrol(c, TOK_SOL_KAYDIR) || kontrol(c, TOK_SAĞ_KAYDIR)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, toplama_ifade(c));
        sol = d;
    }
    return sol;
}

/* Bit VE: & */
static Düğüm *bit_ve_ifade(Cozumleyici *c) {
    Düğüm *sol = bit_kaydir_ifade(c);
    while (kontrol(c, TOK_BİT_VE)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, bit_kaydir_ifade(c));
        sol = d;
    }
    return sol;
}

/* Bit XOR: ^ */
static Düğüm *bit_xor_ifade(Cozumleyici *c) {
    Düğüm *sol = bit_ve_ifade(c);
    while (kontrol(c, TOK_BİT_XOR)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, bit_ve_ifade(c));
        sol = d;
    }
    return sol;
}

/* Bit VEYA: | */
static Düğüm *bit_veya_ifade(Cozumleyici *c) {
    Düğüm *sol = bit_xor_ifade(c);
    while (kontrol(c, TOK_BİT_VEYA)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, bit_xor_ifade(c));
        sol = d;
    }
    return sol;
}

/* Karşılaştırma */
static Düğüm *karsilastirma_ifade(Cozumleyici *c) {
    Düğüm *sol = bit_veya_ifade(c);
    while (kontrol(c, TOK_KÜÇÜK) || kontrol(c, TOK_BÜYÜK) ||
           kontrol(c, TOK_KÜÇÜK_EŞİT) || kontrol(c, TOK_BÜYÜK_EŞİT)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, bit_veya_ifade(c));
        sol = d;
    }
    return sol;
}

/* Eşitlik */
static Düğüm *esitlik_ifade(Cozumleyici *c) {
    Düğüm *sol = karsilastirma_ifade(c);
    while (kontrol(c, TOK_EŞİT_EŞİT) || kontrol(c, TOK_EŞİT_DEĞİL)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, karsilastirma_ifade(c));
        sol = d;
    }
    return sol;
}

/* Mantıksal VE */
static Düğüm *ve_ifade(Cozumleyici *c) {
    Düğüm *sol = esitlik_ifade(c);
    while (kontrol(c, TOK_VE)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, esitlik_ifade(c));
        sol = d;
    }
    return sol;
}

/* Mantıksal VEYA */
static Düğüm *veya_ifade(Cozumleyici *c) {
    Düğüm *sol = ve_ifade(c);
    while (kontrol(c, TOK_VEYA)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, ve_ifade(c));
        sol = d;
    }
    return sol;
}

/* Üçlü ifade: koşul ? değer1 : değer2 */
static Düğüm *uclu_ifade(Cozumleyici *c) {
    Düğüm *sol = veya_ifade(c);
    if (kontrol(c, TOK_SORU)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_ÜÇLÜ, op->satir, op->sutun);
        düğüm_çocuk_ekle(c->arena, d, sol);           /* çocuklar[0] = koşul */
        düğüm_çocuk_ekle(c->arena, d, veya_ifade(c)); /* çocuklar[1] = doğru değer */
        bekle(c, TOK_İKİ_NOKTA);
        düğüm_çocuk_ekle(c->arena, d, veya_ifade(c)); /* çocuklar[2] = yanlış değer */
        return d;
    }
    return sol;
}

/* Boş birleştirme operatörü: a ?? b */
static Düğüm *bos_birlestir_ifade(Cozumleyici *c) {
    Düğüm *sol = uclu_ifade(c);
    while (kontrol(c, TOK_SORU_SORU)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, op->satir, op->sutun);
        d->veri.islem.islem = op->tur;
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, uclu_ifade(c));
        sol = d;
    }
    return sol;
}

/* Boru operatörü: a |> b |> c */
static Düğüm *boru_ifade(Cozumleyici *c) {
    Düğüm *sol = bos_birlestir_ifade(c);
    while (kontrol(c, TOK_BORU)) {
        Sözcük *op = ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_BORU, op->satir, op->sutun);
        düğüm_çocuk_ekle(c->arena, d, sol);
        düğüm_çocuk_ekle(c->arena, d, bos_birlestir_ifade(c));
        sol = d;
    }
    return sol;
}

/* Atama veya ifade */
static Düğüm *ifade_cozumle(Cozumleyici *c) {
    Düğüm *sol = boru_ifade(c);

    /* Bilesik atama: x += expr -> x = x + expr */
    if (sol->tur == DÜĞÜM_TANIMLAYICI) {
        SözcükTürü bilesik = mevcut_sozcuk(c)->tur;
        SözcükTürü karsilik = 0;
        if (bilesik == TOK_ARTI_EŞİT)   karsilik = TOK_ARTI;
        else if (bilesik == TOK_EKSİ_EŞİT)   karsilik = TOK_EKSI;
        else if (bilesik == TOK_ÇARPIM_EŞİT) karsilik = TOK_ÇARPIM;
        else if (bilesik == TOK_BÖLME_EŞİT)  karsilik = TOK_BÖLME;
        else if (bilesik == TOK_YÜZDE_EŞİT)  karsilik = TOK_YÜZDE;
        if (karsilik) {
            ilerle(c);
            Düğüm *sag = boru_ifade(c);
            Düğüm *sol_kopya = düğüm_oluştur(c->arena, DÜĞÜM_TANIMLAYICI, sol->satir, sol->sutun);
            sol_kopya->veri.tanimlayici.isim = sol->veri.tanimlayici.isim;
            Düğüm *ikili = düğüm_oluştur(c->arena, DÜĞÜM_İKİLİ_İŞLEM, sol->satir, sol->sutun);
            ikili->veri.islem.islem = karsilik;
            düğüm_çocuk_ekle(c->arena, ikili, sol_kopya);
            düğüm_çocuk_ekle(c->arena, ikili, sag);
            Düğüm *atama = düğüm_oluştur(c->arena, DÜĞÜM_ATAMA, sol->satir, sol->sutun);
            atama->veri.tanimlayici.isim = sol->veri.tanimlayici.isim;
            düğüm_çocuk_ekle(c->arena, atama, ikili);
            return atama;
        }
    }

    /* Çoklu atama (tuple unpacking): x, y = ifade */
    if (kontrol(c, TOK_VİRGÜL) && sol->tur == DÜĞÜM_TANIMLAYICI) {
        /* İleriye bak: virgüllü tanımlayıcı listesi + = mi? */
        int kayit = c->pos;
        int coklu = 1;
        while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
            if (!kontrol(c, TOK_TANIMLAYICI)) { coklu = 0; break; }
            ilerle(c);
        }
        if (coklu && kontrol(c, TOK_EŞİTTİR)) {
            /* Çoklu atama tespit edildi, yeniden parse */
            c->pos = kayit;
            /* Sol'u zaten DÜĞÜM_TANIMLAYICI olarak aldık */
            Düğüm *paket = düğüm_oluştur(c->arena, DÜĞÜM_PAKET_AÇ, sol->satir, sol->sutun);
            düğüm_çocuk_ekle(c->arena, paket, sol);
            while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
                Sözcük *id = bekle(c, TOK_TANIMLAYICI);
                Düğüm *td = düğüm_oluştur(c->arena, DÜĞÜM_TANIMLAYICI, id->satir, id->sutun);
                td->veri.tanimlayici.isim = sozcuk_metni(c, id);
                düğüm_çocuk_ekle(c->arena, paket, td);
            }
            bekle(c, TOK_EŞİTTİR);
            düğüm_çocuk_ekle(c->arena, paket, boru_ifade(c));
            return paket;
        }
        c->pos = kayit;  /* Çoklu atama değilse geri al */
    }

    /* Atama: tanımlayıcı = ifade */
    if (kontrol(c, TOK_EŞİTTİR) && sol->tur == DÜĞÜM_TANIMLAYICI) {
        ilerle(c);
        Düğüm *atama = düğüm_oluştur(c->arena, DÜĞÜM_ATAMA, sol->satir, sol->sutun);
        atama->veri.tanimlayici.isim = sol->veri.tanimlayici.isim;
        düğüm_çocuk_ekle(c->arena, atama, boru_ifade(c));
        return atama;
    }

    /* Dizi elemanı atama: a[i] = ifade */
    if (kontrol(c, TOK_EŞİTTİR) && sol->tur == DÜĞÜM_DİZİ_ERİŞİM) {
        ilerle(c);
        Düğüm *atama = düğüm_oluştur(c->arena, DÜĞÜM_DİZİ_ATAMA, sol->satir, sol->sutun);
        /* çocuklar[0] = dizi, çocuklar[1] = indeks, çocuklar[2] = değer */
        düğüm_çocuk_ekle(c->arena, atama, sol->çocuklar[0]);
        düğüm_çocuk_ekle(c->arena, atama, sol->çocuklar[1]);
        düğüm_çocuk_ekle(c->arena, atama, boru_ifade(c));
        return atama;
    }

    /* Yapı bozma atama: [a, b, c] = ifade */
    if (kontrol(c, TOK_EŞİTTİR) && sol->tur == DÜĞÜM_DİZİ_DEĞERİ) {
        ilerle(c);
        Düğüm *paket = düğüm_oluştur(c->arena, DÜĞÜM_PAKET_AÇ, sol->satir, sol->sutun);
        /* Hedef değişkenler: sol'un çocukları (DÜĞÜM_TANIMLAYICI olmalı) */
        for (int i = 0; i < sol->çocuk_sayısı; i++) {
            düğüm_çocuk_ekle(c->arena, paket, sol->çocuklar[i]);
        }
        /* Kaynak ifade: son çocuk olarak ekle */
        düğüm_çocuk_ekle(c->arena, paket, boru_ifade(c));
        return paket;
    }

    /* Alan atama: nesne.alan = ifade */
    if (kontrol(c, TOK_EŞİTTİR) && sol->tur == DÜĞÜM_ERİŞİM) {
        ilerle(c);
        Düğüm *atama = düğüm_oluştur(c->arena, DÜĞÜM_ERİŞİM_ATAMA, sol->satir, sol->sutun);
        atama->veri.tanimlayici.isim = sol->veri.tanimlayici.isim; /* alan adı */
        /* çocuklar[0] = nesne, çocuklar[1] = değer */
        if (sol->çocuk_sayısı > 0) {
            düğüm_çocuk_ekle(c->arena, atama, sol->çocuklar[0]); /* nesne */
        }
        düğüm_çocuk_ekle(c->arena, atama, boru_ifade(c)); /* değer */
        return atama;
    }

    return sol;
}

/* ---- Bildirimler ---- */

/* Tip ismi oku (tam, ondalık, metin, mantık, dizi, tanımlayıcı, Sonuç<T,H>, Seçenek<T>) */
static char *tip_oku(Cozumleyici *c) {
    Sözcük *s = mevcut_sozcuk(c);

    /* Sonuç<T, H> veya Seçenek<T> parametreli tip */
    if (s->tur == TOK_SONUÇ || s->tur == TOK_SEÇENEK) {
        char *temel_tip = arena_strndup(c->arena, s->başlangıç, s->uzunluk);
        ilerle(c);  /* Sonuç veya Seçenek */

        /* < var mı kontrol et */
        if (kontrol(c, TOK_KÜÇÜK)) {
            ilerle(c);  /* < */
            /* Tip parametrelerini oku */
            char tip_buf[256];
            int len = snprintf(tip_buf, sizeof(tip_buf), "%s<", temel_tip);

            /* İlk tip parametresi */
            char *ilk_param = tip_oku(c);
            len += snprintf(tip_buf + len, sizeof(tip_buf) - len, "%s", ilk_param);

            /* Virgülle ayrılmış diğer parametreler */
            while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
                char *param = tip_oku(c);
                len += snprintf(tip_buf + len, sizeof(tip_buf) - len, ",%s", param);
            }

            /* > bekle */
            bekle(c, TOK_BÜYÜK);
            len += snprintf(tip_buf + len, sizeof(tip_buf) - len, ">");

            return arena_strdup(c->arena, tip_buf);
        }
        return temel_tip;
    }

    if (s->tur == TOK_TAM || s->tur == TOK_ONDALIK ||
        s->tur == TOK_METİN || s->tur == TOK_MANTIK ||
        s->tur == TOK_DİZİ || s->tur == TOK_TANIMLAYICI) {
        ilerle(c);
        return arena_strndup(c->arena, s->başlangıç, s->uzunluk);
    }
    hata_bildir(HATA_BEKLENEN_TİP, s->satir, s->sutun);
    return arena_strdup(c->arena, "tam");
}

/* Değişken tanımı: tam x = 10 */
static Düğüm *degisken_cozumle(Cozumleyici *c) {
    Sözcük *tip_sozcuk = ilerle(c); /* tip */
    char *tip = arena_strndup(c->arena, tip_sozcuk->başlangıç, tip_sozcuk->uzunluk);

    Sözcük *isim = bekle(c, TOK_TANIMLAYICI);
    if (!isim) return düğüm_oluştur(c->arena, DÜĞÜM_DEĞİŞKEN, tip_sozcuk->satir, tip_sozcuk->sutun);

    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_DEĞİŞKEN, tip_sozcuk->satir, tip_sozcuk->sutun);
    d->veri.değişken.isim = sozcuk_metni(c, isim);
    d->veri.değişken.tip = tip;
    d->veri.değişken.erisim = 0;  /* varsayılan: genel (public) */
    d->veri.değişken.statik = 0;

    /* Çoklu dönüş: tam x, tam y = fonk() */
    if (kontrol(c, TOK_VİRGÜL)) {
        Sözcük *sonraki = &c->sozcukler[c->pos + 1];
        if (sonraki->tur == TOK_TAM || sonraki->tur == TOK_ONDALIK ||
            sonraki->tur == TOK_METİN || sonraki->tur == TOK_MANTIK) {
            ilerle(c); /* virgülü atla */
            Sözcük *tip2_sozcuk = ilerle(c); /* ikinci tip */
            char *tip2 = arena_strndup(c->arena, tip2_sozcuk->başlangıç, tip2_sozcuk->uzunluk);
            Sözcük *isim2 = bekle(c, TOK_TANIMLAYICI);

            Düğüm *d2 = düğüm_oluştur(c->arena, DÜĞÜM_DEĞİŞKEN, tip2_sozcuk->satir, tip2_sozcuk->sutun);
            d2->veri.değişken.isim = isim2 ? sozcuk_metni(c, isim2) : arena_strdup(c->arena, "?");
            d2->veri.değişken.tip = tip2;
            d2->veri.değişken.genel = 2; /* özel: ikinci dönüş değeri (rdx) */

            /* = ifade */
            Düğüm *blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, tip_sozcuk->satir, tip_sozcuk->sutun);
            if (esle_ve_ilerle(c, TOK_EŞİTTİR)) {
                Düğüm *ifade = boru_ifade(c);
                düğüm_çocuk_ekle(c->arena, d, ifade);
            }
            düğüm_çocuk_ekle(c->arena, blok, d);
            düğüm_çocuk_ekle(c->arena, blok, d2);
            return blok;
        }
    }

    if (esle_ve_ilerle(c, TOK_EŞİTTİR)) {
        düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
    }

    return d;
}

/* İşlev tanımı */
static Düğüm *islev_cozumle(Cozumleyici *c) {
    Sözcük *başlangıç = ilerle(c); /* işlev */
    Sözcük *isim = bekle(c, TOK_TANIMLAYICI);

    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İŞLEV, başlangıç->satir, başlangıç->sutun);
    d->veri.islev.isim = isim ? sozcuk_metni(c, isim) : arena_strdup(c->arena, "?");
    d->veri.islev.dönüş_tipi = NULL;
    d->veri.islev.dekorator = NULL;
    d->veri.islev.tip_parametre = NULL;
    d->veri.islev.eszamansiz = 0;
    d->veri.islev.variadic = 0;
    d->veri.islev.erisim = 0;  /* varsayılan: genel (public) */
    d->veri.islev.statik = 0;

    /* Generic tip parametresi: işlev f<T>(...) */
    if (kontrol(c, TOK_KÜÇÜK)) {
        ilerle(c);  /* < atla */
        Sözcük *tip_param = bekle(c, TOK_TANIMLAYICI);
        if (tip_param) {
            d->veri.islev.tip_parametre = sozcuk_metni(c, tip_param);
        }
        bekle(c, TOK_BÜYÜK);  /* > atla */
    }

    /* Parametreler */
    bekle(c, TOK_PAREN_AC);
    Düğüm *params = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, başlangıç->satir, başlangıç->sutun);
    if (!kontrol(c, TOK_PAREN_KAPA)) {
        do {
            /* Variadic: ...isim */
            int bu_variadic = 0;
            if (kontrol(c, TOK_ÜÇ_NOKTA)) {
                ilerle(c);  /* ... atla */
                bu_variadic = 1;
                d->veri.islev.variadic = 1;
            }
            Sözcük *p_isim = bekle(c, TOK_TANIMLAYICI);
            char *p_tip = NULL;
            if (bu_variadic) {
                p_tip = arena_strdup(c->arena, "dizi");
            } else {
                bekle(c, TOK_İKİ_NOKTA);
                p_tip = tip_oku(c);
            }
            Düğüm *param = düğüm_oluştur(c->arena, DÜĞÜM_DEĞİŞKEN, p_isim ? p_isim->satir : 0, 0);
            param->veri.değişken.isim = p_isim ? sozcuk_metni(c, p_isim) : arena_strdup(c->arena, "?");
            param->veri.değişken.tip = p_tip;
            /* Varsayılan değer: = ifade */
            if (esle_ve_ilerle(c, TOK_EŞİTTİR)) {
                düğüm_çocuk_ekle(c->arena, param, boru_ifade(c));
            }
            düğüm_çocuk_ekle(c->arena, params, param);
        } while (esle_ve_ilerle(c, TOK_VİRGÜL));
    }
    bekle(c, TOK_PAREN_KAPA);
    düğüm_çocuk_ekle(c->arena, d, params);

    /* Dönüş tipi */
    if (esle_ve_ilerle(c, TOK_OK)) {
        d->veri.islev.dönüş_tipi = tip_oku(c);
    }

    /* Gövde: ise ... son veya yeni satırdan sonra ... son */
    yeni_satir_atla(c);
    /* 'ise' opsiyonel - varsa atla */
    esle_ve_ilerle(c, TOK_İSE);
    yeni_satir_atla(c);

    Düğüm *govde = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, mevcut_sozcuk(c)->satir, mevcut_sozcuk(c)->sutun);
    while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
        yeni_satir_atla(c);
        if (kontrol(c, TOK_SON) || kontrol(c, TOK_DOSYA_SONU)) break;
        düğüm_çocuk_ekle(c->arena, govde, bildirim_cozumle(c));
        yeni_satir_bekle(c);
    }
    bekle(c, TOK_SON);
    düğüm_çocuk_ekle(c->arena, d, govde);

    return d;
}

/* eğer / yoksa eğer / yoksa */
static Düğüm *eger_cozumle(Cozumleyici *c) {
    Sözcük *başlangıç = ilerle(c); /* eğer */
    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_EĞER, başlangıç->satir, başlangıç->sutun);

    /* Koşul */
    düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
    esle_ve_ilerle(c, TOK_İSE);
    yeni_satir_atla(c);

    /* Doğru bloğu */
    Düğüm *dogru_blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, mevcut_sozcuk(c)->satir, mevcut_sozcuk(c)->sutun);
    while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_YOKSA) && !kontrol(c, TOK_DOSYA_SONU)) {
        yeni_satir_atla(c);
        if (kontrol(c, TOK_SON) || kontrol(c, TOK_YOKSA) || kontrol(c, TOK_DOSYA_SONU)) break;
        düğüm_çocuk_ekle(c->arena, dogru_blok, bildirim_cozumle(c));
        yeni_satir_bekle(c);
    }
    düğüm_çocuk_ekle(c->arena, d, dogru_blok);

    /* yoksa eğer / yoksa */
    while (kontrol(c, TOK_YOKSA)) {
        ilerle(c);
        if (kontrol(c, TOK_EĞER)) {
            ilerle(c);
            /* yoksa eğer koşulu */
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
            esle_ve_ilerle(c, TOK_İSE);
            yeni_satir_atla(c);

            Düğüm *blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, mevcut_sozcuk(c)->satir, mevcut_sozcuk(c)->sutun);
            while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_YOKSA) && !kontrol(c, TOK_DOSYA_SONU)) {
                yeni_satir_atla(c);
                if (kontrol(c, TOK_SON) || kontrol(c, TOK_YOKSA) || kontrol(c, TOK_DOSYA_SONU)) break;
                düğüm_çocuk_ekle(c->arena, blok, bildirim_cozumle(c));
                yeni_satir_bekle(c);
            }
            düğüm_çocuk_ekle(c->arena, d, blok);
        } else {
            /* yoksa bloğu (son blok) */
            yeni_satir_atla(c);
            Düğüm *yoksa_blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, mevcut_sozcuk(c)->satir, mevcut_sozcuk(c)->sutun);
            while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
                yeni_satir_atla(c);
                if (kontrol(c, TOK_SON) || kontrol(c, TOK_DOSYA_SONU)) break;
                düğüm_çocuk_ekle(c->arena, yoksa_blok, bildirim_cozumle(c));
                yeni_satir_bekle(c);
            }
            düğüm_çocuk_ekle(c->arena, d, yoksa_blok);
            break;
        }
    }

    bekle(c, TOK_SON);
    return d;
}

/* döngü i = 0, 10 ise ... son */
static Düğüm *dongu_cozumle(Cozumleyici *c) {
    Sözcük *başlangıç = ilerle(c); /* döngü */
    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_DÖNGÜ, başlangıç->satir, başlangıç->sutun);

    Sözcük *değişken = bekle(c, TOK_TANIMLAYICI);
    d->veri.dongu.isim = değişken ? sozcuk_metni(c, değişken) : arena_strdup(c->arena, "i");

    bekle(c, TOK_EŞİTTİR);
    düğüm_çocuk_ekle(c->arena, d, boru_ifade(c)); /* başlangıç değeri */
    bekle(c, TOK_VİRGÜL);
    düğüm_çocuk_ekle(c->arena, d, boru_ifade(c)); /* bitiş değeri */

    /* Opsiyonel adım değeri: döngü i = 0, 10, 2 ise */
    if (esle_ve_ilerle(c, TOK_VİRGÜL)) {
        düğüm_çocuk_ekle(c->arena, d, boru_ifade(c)); /* adım değeri */
    }

    esle_ve_ilerle(c, TOK_İSE);
    yeni_satir_atla(c);

    /* Gövde */
    Düğüm *govde = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, mevcut_sozcuk(c)->satir, mevcut_sozcuk(c)->sutun);
    while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
        yeni_satir_atla(c);
        if (kontrol(c, TOK_SON) || kontrol(c, TOK_DOSYA_SONU)) break;
        düğüm_çocuk_ekle(c->arena, govde, bildirim_cozumle(c));
        yeni_satir_bekle(c);
    }
    bekle(c, TOK_SON);
    düğüm_çocuk_ekle(c->arena, d, govde);

    return d;
}

/* iken koşul ise ... son */
static Düğüm *iken_cozumle(Cozumleyici *c) {
    Sözcük *başlangıç = ilerle(c); /* iken */
    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İKEN, başlangıç->satir, başlangıç->sutun);

    düğüm_çocuk_ekle(c->arena, d, boru_ifade(c)); /* koşul */

    esle_ve_ilerle(c, TOK_İSE);
    yeni_satir_atla(c);

    /* Gövde */
    Düğüm *govde = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, mevcut_sozcuk(c)->satir, mevcut_sozcuk(c)->sutun);
    while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_YOKSA) && !kontrol(c, TOK_DOSYA_SONU)) {
        yeni_satir_atla(c);
        if (kontrol(c, TOK_SON) || kontrol(c, TOK_YOKSA) || kontrol(c, TOK_DOSYA_SONU)) break;
        düğüm_çocuk_ekle(c->arena, govde, bildirim_cozumle(c));
        yeni_satir_bekle(c);
    }
    düğüm_çocuk_ekle(c->arena, d, govde);

    /* Yoksa bloğu (opsiyonel) — kır olmadan tamamlanırsa çalışır */
    if (kontrol(c, TOK_YOKSA)) {
        ilerle(c);  /* yoksa */
        yeni_satir_atla(c);
        Düğüm *yoksa_blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, başlangıç->satir, başlangıç->sutun);
        while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
            yeni_satir_atla(c);
            if (kontrol(c, TOK_SON)) break;
            düğüm_çocuk_ekle(c->arena, yoksa_blok, bildirim_cozumle(c));
            yeni_satir_bekle(c);
        }
        düğüm_çocuk_ekle(c->arena, d, yoksa_blok);  /* çocuklar[2] = yoksa bloğu */
    }

    bekle(c, TOK_SON);

    return d;
}

/* sınıf İsim ... son */
static Düğüm *sinif_cozumle(Cozumleyici *c) {
    Sözcük *başlangıç = ilerle(c); /* sınıf */
    Sözcük *isim = bekle(c, TOK_TANIMLAYICI);

    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_SINIF, başlangıç->satir, başlangıç->sutun);
    d->veri.sinif.isim = isim ? sozcuk_metni(c, isim) : arena_strdup(c->arena, "?");
    d->veri.sinif.ebeveyn = NULL;
    d->veri.sinif.tip_parametre = NULL;
    d->veri.sinif.soyut = 0;

    /* Generic tip parametresi: sınıf Liste<T> */
    if (kontrol(c, TOK_KÜÇÜK)) {
        ilerle(c);  /* < atla */
        Sözcük *tip_param = bekle(c, TOK_TANIMLAYICI);
        if (tip_param) {
            d->veri.sinif.tip_parametre = sozcuk_metni(c, tip_param);
        }
        bekle(c, TOK_BÜYÜK);  /* > atla */
    }

    /* Kalıtım: sınıf Kedi : Hayvan */
    if (esle_ve_ilerle(c, TOK_İKİ_NOKTA)) {
        Sözcük *ebeveyn = bekle(c, TOK_TANIMLAYICI);
        if (ebeveyn) {
            d->veri.sinif.ebeveyn = sozcuk_metni(c, ebeveyn);
        }
    }

    /* Arayüz uygulama: sınıf Kedi uygula Yazdırılabilir, Karşılaştırılabilir */
    d->veri.sinif.arayuz_sayisi = 0;
    if (esle_ve_ilerle(c, TOK_UYGULA)) {
        do {
            Sözcük *ay = bekle(c, TOK_TANIMLAYICI);
            if (ay && d->veri.sinif.arayuz_sayisi < 8) {
                d->veri.sinif.arayuzler[d->veri.sinif.arayuz_sayisi++] = sozcuk_metni(c, ay);
            }
        } while (esle_ve_ilerle(c, TOK_VİRGÜL));
    }

    yeni_satir_atla(c);

    /* Alanlar ve metotlar */
    while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
        yeni_satir_atla(c);
        if (kontrol(c, TOK_SON) || kontrol(c, TOK_DOSYA_SONU)) break;

        /* Erişim belirleyicileri ve statik: özel, korumalı, statik */
        int erisim = 0;  /* 0=genel, 1=özel, 2=korumalı */
        int statik = 0;

        while (kontrol(c, TOK_ÖZEL) || kontrol(c, TOK_KORUMALI) || kontrol(c, TOK_STATİK)) {
            if (kontrol(c, TOK_ÖZEL)) {
                erisim = 1;
                ilerle(c);
            } else if (kontrol(c, TOK_KORUMALI)) {
                erisim = 2;
                ilerle(c);
            } else if (kontrol(c, TOK_STATİK)) {
                statik = 1;
                ilerle(c);
            }
        }

        if (kontrol(c, TOK_İŞLEV)) {
            /* Metot */
            Düğüm *metot = islev_cozumle(c);
            metot->veri.islev.erisim = erisim;
            metot->veri.islev.statik = statik;
            düğüm_çocuk_ekle(c->arena, d, metot);
        } else if (kontrol(c, TOK_TAM) || kontrol(c, TOK_ONDALIK) ||
                   kontrol(c, TOK_METİN) || kontrol(c, TOK_MANTIK) ||
                   kontrol(c, TOK_DİZİ)) {
            /* Alan tanımı: tip isim */
            Sözcük *tip_sozcuk = ilerle(c);
            char *tip = arena_strndup(c->arena, tip_sozcuk->başlangıç, tip_sozcuk->uzunluk);
            Sözcük *alan_isim = bekle(c, TOK_TANIMLAYICI);

            Düğüm *alan = düğüm_oluştur(c->arena, DÜĞÜM_DEĞİŞKEN, tip_sozcuk->satir, tip_sozcuk->sutun);
            alan->veri.değişken.isim = alan_isim ? sozcuk_metni(c, alan_isim) : arena_strdup(c->arena, "?");
            alan->veri.değişken.tip = tip;
            alan->veri.değişken.erisim = erisim;
            alan->veri.değişken.statik = statik;
            düğüm_çocuk_ekle(c->arena, d, alan);
        } else {
            /* Bilinmeyen - atla */
            ilerle(c);
        }
        yeni_satir_bekle(c);
    }
    bekle(c, TOK_SON);

    return d;
}

/* eşle/durum/varsayılan çözümleme */
static Düğüm *esle_cozumle(Cozumleyici *c) {
    Sözcük *s = ilerle(c);  /* eşle */
    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_EŞLE, s->satir, s->sutun);

    /* Eşlenen ifade */
    Düğüm *ifade = boru_ifade(c);
    düğüm_çocuk_ekle(c->arena, d, ifade);  /* çocuklar[0] = eşlenen ifade */

    bekle(c, TOK_İSE);
    yeni_satir_atla(c);

    /* Durum blokları */
    while (kontrol(c, TOK_DURUM)) {
        ilerle(c);  /* durum */
        Düğüm *deger = boru_ifade(c);

        /* Aralık deseni: durum 1..10: */
        if (kontrol(c, TOK_ARALIK)) {
            ilerle(c);  /* .. atla */
            Düğüm *ust = boru_ifade(c);
            Düğüm *aralik = düğüm_oluştur(c->arena, DÜĞÜM_ARALIK, deger->satir, deger->sutun);
            düğüm_çocuk_ekle(c->arena, aralik, deger);
            düğüm_çocuk_ekle(c->arena, aralik, ust);
            deger = aralik;
        }

        /* Koruma koşulu: durum x eğer x > 50: */
        /* "eğer" TOK_EĞER olarak gelir */
        Düğüm *koruma = NULL;
        if (kontrol(c, TOK_EĞER)) {
            ilerle(c);  /* eğer atla */
            koruma = boru_ifade(c);
        }

        düğüm_çocuk_ekle(c->arena, d, deger);  /* durum değeri */

        /* Koruma koşulunu ekle (varsa) - bloktan sonra eklenir */

        bekle(c, TOK_İKİ_NOKTA);
        yeni_satir_atla(c);

        /* Durum bloğu */
        Düğüm *blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, deger->satir, deger->sutun);
        while (!kontrol(c, TOK_DURUM) && !kontrol(c, TOK_VARSAYILAN) &&
               !kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
            düğüm_çocuk_ekle(c->arena, blok, bildirim_cozumle(c));
            yeni_satir_bekle(c);
            yeni_satir_atla(c);
        }
        /* Koruma koşulu varsa: bloğu eğer koşulu ile sar */
        if (koruma) {
            Düğüm *eger = düğüm_oluştur(c->arena, DÜĞÜM_EĞER, koruma->satir, koruma->sutun);
            düğüm_çocuk_ekle(c->arena, eger, koruma);  /* koşul */
            düğüm_çocuk_ekle(c->arena, eger, blok);    /* doğru bloğu */
            Düğüm *sar_blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, blok->satir, blok->sutun);
            düğüm_çocuk_ekle(c->arena, sar_blok, eger);
            düğüm_çocuk_ekle(c->arena, d, sar_blok);
        } else {
            düğüm_çocuk_ekle(c->arena, d, blok);  /* durum bloğu */
        }
    }

    /* Varsayılan blok (opsiyonel) */
    if (kontrol(c, TOK_VARSAYILAN)) {
        ilerle(c);  /* varsayılan */
        bekle(c, TOK_İKİ_NOKTA);
        yeni_satir_atla(c);

        Düğüm *vars_blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, d->satir, d->sutun);
        while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
            düğüm_çocuk_ekle(c->arena, vars_blok, bildirim_cozumle(c));
            yeni_satir_bekle(c);
            yeni_satir_atla(c);
        }
        düğüm_çocuk_ekle(c->arena, d, vars_blok);
    }

    bekle(c, TOK_SON);
    return d;
}

/* her...için döngüsü çözümleme */
static Düğüm *her_icin_cozumle(Cozumleyici *c) {
    Sözcük *s = ilerle(c);  /* her */
    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_HER_İÇİN, s->satir, s->sutun);

    /* Döngü değişkeni */
    Sözcük *değişken = bekle(c, TOK_TANIMLAYICI);
    d->veri.dongu.isim = değişken ? sozcuk_metni(c, değişken) : arena_strdup(c->arena, "?");

    /* için */
    bekle(c, TOK_İÇİN);

    /* Dizi ifadesi */
    Düğüm *dizi = boru_ifade(c);
    düğüm_çocuk_ekle(c->arena, d, dizi);  /* çocuklar[0] = dizi ifadesi */

    bekle(c, TOK_İSE);
    yeni_satir_atla(c);

    /* Gövde */
    Düğüm *govde = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, s->satir, s->sutun);
    while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_YOKSA) && !kontrol(c, TOK_DOSYA_SONU)) {
        düğüm_çocuk_ekle(c->arena, govde, bildirim_cozumle(c));
        yeni_satir_bekle(c);
        yeni_satir_atla(c);
    }
    düğüm_çocuk_ekle(c->arena, d, govde);  /* çocuklar[1] = gövde */

    /* Yoksa bloğu (opsiyonel) — kır olmadan tamamlanırsa çalışır */
    if (kontrol(c, TOK_YOKSA)) {
        ilerle(c);  /* yoksa */
        yeni_satir_atla(c);
        Düğüm *yoksa_blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, s->satir, s->sutun);
        while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
            yeni_satir_atla(c);
            if (kontrol(c, TOK_SON)) break;
            düğüm_çocuk_ekle(c->arena, yoksa_blok, bildirim_cozumle(c));
            yeni_satir_bekle(c);
        }
        düğüm_çocuk_ekle(c->arena, d, yoksa_blok);  /* çocuklar[2] = yoksa bloğu */
    }

    bekle(c, TOK_SON);
    return d;
}

/* Arayüz metot imzası çözümleme (gövdesiz işlev) */
static Düğüm *islev_imza_cozumle(Cozumleyici *c) {
    Sözcük *başlangıç = ilerle(c); /* işlev */
    Sözcük *isim = bekle(c, TOK_TANIMLAYICI);

    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İŞLEV, başlangıç->satir, başlangıç->sutun);
    d->veri.islev.isim = isim ? sozcuk_metni(c, isim) : arena_strdup(c->arena, "?");
    d->veri.islev.dönüş_tipi = NULL;
    d->veri.islev.dekorator = NULL;
    d->veri.islev.tip_parametre = NULL;
    d->veri.islev.eszamansiz = 0;
    d->veri.islev.variadic = 0;
    d->veri.islev.erisim = 0;
    d->veri.islev.statik = 0;
    d->veri.islev.soyut = 1;  /* gövdesiz */

    /* Parametreler */
    bekle(c, TOK_PAREN_AC);
    Düğüm *params = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, başlangıç->satir, başlangıç->sutun);
    if (!kontrol(c, TOK_PAREN_KAPA)) {
        do {
            Sözcük *p_isim = bekle(c, TOK_TANIMLAYICI);
            bekle(c, TOK_İKİ_NOKTA);
            char *p_tip = tip_oku(c);
            Düğüm *param = düğüm_oluştur(c->arena, DÜĞÜM_DEĞİŞKEN,
                p_isim ? p_isim->satir : 0, 0);
            param->veri.değişken.isim = p_isim ? sozcuk_metni(c, p_isim) : arena_strdup(c->arena, "?");
            param->veri.değişken.tip = p_tip;
            düğüm_çocuk_ekle(c->arena, params, param);
        } while (esle_ve_ilerle(c, TOK_VİRGÜL));
    }
    bekle(c, TOK_PAREN_KAPA);
    düğüm_çocuk_ekle(c->arena, d, params);

    /* Dönüş tipi */
    if (esle_ve_ilerle(c, TOK_OK)) {
        d->veri.islev.dönüş_tipi = tip_oku(c);
    }

    /* Gövde yok — sadece imza */
    return d;
}

/* arayüz (interface) çözümleme */
static Düğüm *arayuz_cozumle(Cozumleyici *c) {
    Sözcük *başlangıç = ilerle(c); /* arayüz */
    Sözcük *isim = bekle(c, TOK_TANIMLAYICI);

    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_ARAYÜZ, başlangıç->satir, başlangıç->sutun);
    d->veri.sayim.isim = isim ? sozcuk_metni(c, isim) : arena_strdup(c->arena, "?");

    bekle(c, TOK_İSE);
    yeni_satir_atla(c);

    /* Metot imzaları veya isimleri */
    while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
        yeni_satir_atla(c);
        if (kontrol(c, TOK_SON)) break;

        if (kontrol(c, TOK_İŞLEV)) {
            /* Tam imza: işlev isim(param: tip, ...) -> dönüş_tipi */
            Düğüm *metot = islev_imza_cozumle(c);
            düğüm_çocuk_ekle(c->arena, d, metot);
        } else {
            /* Geriye uyumluluk: sadece metot ismi */
            Sözcük *metot = bekle(c, TOK_TANIMLAYICI);
            if (metot) {
                Düğüm *dd = düğüm_oluştur(c->arena, DÜĞÜM_TANIMLAYICI, metot->satir, metot->sutun);
                dd->veri.tanimlayici.isim = sozcuk_metni(c, metot);
                düğüm_çocuk_ekle(c->arena, d, dd);
            }
        }

        yeni_satir_atla(c);
    }

    bekle(c, TOK_SON);
    return d;
}

/* sayım (enum) çözümleme */
static Düğüm *sayim_cozumle(Cozumleyici *c) {
    Sözcük *s = ilerle(c);  /* sayım */
    Sözcük *isim = bekle(c, TOK_TANIMLAYICI);

    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_SAYIM, s->satir, s->sutun);
    d->veri.sayim.isim = isim ? sozcuk_metni(c, isim) : arena_strdup(c->arena, "?");

    bekle(c, TOK_İSE);
    yeni_satir_atla(c);

    /* Değerler: tanımlayıcı, virgül ile ayrılmış */
    while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
        yeni_satir_atla(c);
        if (kontrol(c, TOK_SON)) break;

        Sözcük *deger = bekle(c, TOK_TANIMLAYICI);
        if (deger) {
            Düğüm *dd = düğüm_oluştur(c->arena, DÜĞÜM_TANIMLAYICI, deger->satir, deger->sutun);
            dd->veri.tanimlayici.isim = sozcuk_metni(c, deger);
            düğüm_çocuk_ekle(c->arena, d, dd);
        }

        /* Opsiyonel virgül */
        esle_ve_ilerle(c, TOK_VİRGÜL);
        yeni_satir_atla(c);
    }

    bekle(c, TOK_SON);
    return d;
}

/* Bildirim (statement) çözümleme */
static Düğüm *bildirim_cozumle(Cozumleyici *c) {
    Sözcük *s = mevcut_sozcuk(c);

    /* Global değişken tanımı: genel tam x = 5 */
    if (s->tur == TOK_GENEL) {
        ilerle(c);  /* genel'i atla */
        Düğüm *d = degisken_cozumle(c);
        d->veri.değişken.genel = 1;
        return d;
    }

    /* Sabit değişken tanımı: sabit tam X = 42 */
    if (s->tur == TOK_SABIT) {
        ilerle(c);  /* sabit'i atla */
        Düğüm *d = degisken_cozumle(c);
        d->veri.değişken.sabit = 1;
        return d;
    }

    /* Değişken tanımı */
    if (s->tur == TOK_TAM || s->tur == TOK_ONDALIK ||
        s->tur == TOK_METİN || s->tur == TOK_MANTIK || s->tur == TOK_DİZİ) {
        return degisken_cozumle(c);
    }

    /* Tip çıkarımı: değişken x = ifade */
    if (s->tur == TOK_DEĞİŞKEN) {
        ilerle(c);
        Sözcük *isim = bekle(c, TOK_TANIMLAYICI);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_DEĞİŞKEN, s->satir, s->sutun);
        d->veri.değişken.isim = isim ? sozcuk_metni(c, isim) : arena_strdup(c->arena, "?");
        d->veri.değişken.tip = arena_strdup(c->arena, "bilinmiyor");
        if (esle_ve_ilerle(c, TOK_EŞİTTİR)) {
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
        }
        return d;
    }

    /* Tip takma adı: tip Sayı = tam */
    if (s->tur == TOK_TİP_TANIMI) {
        ilerle(c);
        Sözcük *takma = bekle(c, TOK_TANIMLAYICI);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_TİP_TANIMI, s->satir, s->sutun);
        d->veri.tanimlayici.isim = takma ? sozcuk_metni(c, takma) : arena_strdup(c->arena, "?");
        bekle(c, TOK_EŞİTTİR);
        d->veri.tanimlayici.tip = tip_oku(c);
        return d;
    }

    /* Soyut sınıf/işlev */
    if (s->tur == TOK_SOYUT) {
        ilerle(c);
        if (kontrol(c, TOK_SINIF)) {
            Düğüm *d = sinif_cozumle(c);
            d->veri.sinif.soyut = 1;
            return d;
        }
        if (kontrol(c, TOK_İŞLEV)) {
            Düğüm *d = islev_cozumle(c);
            d->veri.islev.soyut = 1;
            return d;
        }
        return bildirim_cozumle(c);
    }

    /* Üreteç üret ifadesi */
    if (s->tur == TOK_ÜRET) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_ÜRET, s->satir, s->sutun);
        if (!kontrol(c, TOK_YENİ_SATIR) && !kontrol(c, TOK_DOSYA_SONU)) {
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
        }
        return d;
    }

    /* Dekoratör: @isim sonrası işlev */
    if (s->tur == TOK_AT) {
        ilerle(c);  /* @ atla */
        Sözcük *dek_isim = bekle(c, TOK_TANIMLAYICI);
        char *dekorator = dek_isim ? sozcuk_metni(c, dek_isim) : arena_strdup(c->arena, "?");
        yeni_satir_atla(c);
        /* Sonraki bildirim işlev olmalı */
        if (kontrol(c, TOK_İŞLEV) || kontrol(c, TOK_EŞZAMANSIZ)) {
            Düğüm *fn = bildirim_cozumle(c);
            if (fn && fn->tur == DÜĞÜM_İŞLEV) {
                fn->veri.islev.dekorator = dekorator;
            }
            return fn;
        }
        /* İşlev değilse normal bildirim */
        return bildirim_cozumle(c);
    }

    /* Test bloğu: test "isim" ise ... son */
    if (s->tur == TOK_TEST) {
        ilerle(c);  /* test atla */
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_TEST, s->satir, s->sutun);
        /* Test adı (metin değeri) */
        if (kontrol(c, TOK_METİN_DEĞERİ)) {
            Sözcük *isim_sozcuk = ilerle(c);
            d->veri.test.isim = arena_strndup(c->arena, isim_sozcuk->başlangıç, isim_sozcuk->uzunluk);
        } else {
            d->veri.test.isim = arena_strdup(c->arena, "isimsiz test");
        }
        esle_ve_ilerle(c, TOK_İSE);
        yeni_satir_atla(c);
        /* Gövde */
        Düğüm *govde = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, mevcut_sozcuk(c)->satir, mevcut_sozcuk(c)->sutun);
        while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
            yeni_satir_atla(c);
            if (kontrol(c, TOK_SON) || kontrol(c, TOK_DOSYA_SONU)) break;
            düğüm_çocuk_ekle(c->arena, govde, bildirim_cozumle(c));
            yeni_satir_bekle(c);
        }
        bekle(c, TOK_SON);
        düğüm_çocuk_ekle(c->arena, d, govde);  /* çocuklar[0] = gövde */
        return d;
    }

    /* Eşzamansız işlev tanımı */
    if (s->tur == TOK_EŞZAMANSIZ) {
        ilerle(c);  /* eşzamansız atla */
        if (kontrol(c, TOK_İŞLEV)) {
            Düğüm *fn = islev_cozumle(c);
            fn->veri.islev.eszamansiz = 1;
            return fn;
        }
        return bildirim_cozumle(c);
    }

    /* İşlev tanımı */
    if (s->tur == TOK_İŞLEV) return islev_cozumle(c);

    /* Sınıf tanımı */
    if (s->tur == TOK_SINIF) return sinif_cozumle(c);

    /* Sayım (enum) tanımı */
    if (s->tur == TOK_SAYIM) return sayim_cozumle(c);

    /* Arayüz (interface) tanımı */
    if (s->tur == TOK_ARAYÜZ) return arayuz_cozumle(c);

    /* Sınıf değişkeni: SinifAdi değişken = ... */
    if (s->tur == TOK_TANIMLAYICI && c->pos + 1 < c->sozcuk_sayisi &&
        c->sozcukler[c->pos + 1].tur == TOK_TANIMLAYICI) {
        return degisken_cozumle(c);
    }

    /* Koşul */
    if (s->tur == TOK_EĞER) return eger_cozumle(c);

    /* Döngüler */
    if (s->tur == TOK_DÖNGÜ) return dongu_cozumle(c);
    if (s->tur == TOK_İKEN) return iken_cozumle(c);
    if (s->tur == TOK_EŞLE) return esle_cozumle(c);
    if (s->tur == TOK_HER) return her_icin_cozumle(c);

    /* Bağlam yöneticisi: ile ifade olarak d ise ... son */
    if (s->tur == TOK_ILE) {
        ilerle(c);  /* ile */
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İLE_İSE, s->satir, s->sutun);
        /* Kaynak ifade */
        Düğüm *kaynak = boru_ifade(c);
        düğüm_çocuk_ekle(c->arena, d, kaynak);  /* çocuklar[0] = kaynak */
        /* olarak değişken */
        if (kontrol(c, TOK_OLARAK)) {
            ilerle(c);
            Sözcük *deg = bekle(c, TOK_TANIMLAYICI);
            d->veri.tanimlayici.isim = deg ? sozcuk_metni(c, deg) : arena_strdup(c->arena, "?");
        } else {
            d->veri.tanimlayici.isim = NULL;
        }
        bekle(c, TOK_İSE);
        yeni_satir_atla(c);
        /* Gövde */
        Düğüm *govde = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, s->satir, s->sutun);
        while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
            yeni_satir_atla(c);
            if (kontrol(c, TOK_SON) || kontrol(c, TOK_DOSYA_SONU)) break;
            düğüm_çocuk_ekle(c->arena, govde, bildirim_cozumle(c));
            yeni_satir_bekle(c);
        }
        bekle(c, TOK_SON);
        düğüm_çocuk_ekle(c->arena, d, govde);  /* çocuklar[1] = gövde */
        return d;
    }

    /* Döndür (çoklu dönüş: döndür a, b) */
    if (s->tur == TOK_DÖNDÜR) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_DÖNDÜR, s->satir, s->sutun);
        if (!kontrol(c, TOK_YENİ_SATIR) && !kontrol(c, TOK_DOSYA_SONU)) {
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
            while (esle_ve_ilerle(c, TOK_VİRGÜL)) {
                düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
            }
        }
        return d;
    }

    /* Kır (opsiyonel etiket: kır döngü_değişkeni) */
    if (s->tur == TOK_KIR) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_KIR, s->satir, s->sutun);
        if (kontrol(c, TOK_TANIMLAYICI)) {
            Sözcük *etiket = ilerle(c);
            d->veri.tanimlayici.isim = arena_strndup(c->arena, etiket->başlangıç, etiket->uzunluk);
        }
        return d;
    }

    /* Devam (opsiyonel etiket: devam döngü_değişkeni) */
    if (s->tur == TOK_DEVAM) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_DEVAM, s->satir, s->sutun);
        if (kontrol(c, TOK_TANIMLAYICI)) {
            Sözcük *etiket = ilerle(c);
            d->veri.tanimlayici.isim = arena_strndup(c->arena, etiket->başlangıç, etiket->uzunluk);
        }
        return d;
    }

    /* Kullan */
    if (s->tur == TOK_KULLAN) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_KULLAN, s->satir, s->sutun);
        /* Dosya içe aktarma: kullan "dosya.tr" */
        if (kontrol(c, TOK_METİN_DEĞERİ)) {
            Sözcük *dosya = ilerle(c);
            d->veri.kullan.modul = arena_strndup(c->arena, dosya->başlangıç, dosya->uzunluk);
            return d;
        }
        /* Modül adı: tanımlayıcı veya anahtar kelime (metin, matematik vb.) */
        Sözcük *modul = NULL;
        if (kontrol(c, TOK_TANIMLAYICI) || kontrol(c, TOK_METİN) ||
            kontrol(c, TOK_TAM) || kontrol(c, TOK_ONDALIK) ||
            kontrol(c, TOK_MANTIK) || kontrol(c, TOK_DİZİ) ||
            kontrol(c, TOK_KÜME)) {
            modul = ilerle(c);
        } else {
            modul = bekle(c, TOK_TANIMLAYICI);
        }
        d->veri.kullan.modul = modul ? sozcuk_metni(c, modul) : arena_strdup(c->arena, "?");
        return d;
    }

    /* Dene/yakala blokları */
    if (s->tur == TOK_DENE) {
        ilerle(c);  /* dene */
        yeni_satir_atla(c);

        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_DENE_YAKALA, s->satir, s->sutun);
        d->veri.tanimlayici.isim = NULL;
        d->veri.tanimlayici.tip = NULL;

        /* Dene bloğu */
        Düğüm *dene_blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, s->satir, s->sutun);
        while (!kontrol(c, TOK_YAKALA) && !kontrol(c, TOK_DOSYA_SONU)) {
            yeni_satir_atla(c);
            if (kontrol(c, TOK_YAKALA)) break;
            düğüm_çocuk_ekle(c->arena, dene_blok, bildirim_cozumle(c));
            yeni_satir_bekle(c);
        }
        düğüm_çocuk_ekle(c->arena, d, dene_blok);  /* çocuklar[0] = dene bloğu */

        /* Çoklu yakala blokları */
        while (kontrol(c, TOK_YAKALA)) {
            ilerle(c);  /* yakala */
            Düğüm *yakala_blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, s->satir, s->sutun);
            yakala_blok->veri.tanimlayici.tip = NULL;   /* istisna tipi (NULL = hepsini yakala) */
            yakala_blok->veri.tanimlayici.isim = NULL;  /* hata değişkeni */

            /* yakala [TipAdi] [değişken] */
            if (kontrol(c, TOK_TANIMLAYICI)) {
                Sözcük *ilk = ilerle(c);
                char *ilk_isim = sozcuk_metni(c, ilk);
                if (kontrol(c, TOK_TANIMLAYICI)) {
                    /* İki tanımlayıcı: yakala TipAdi değişken */
                    yakala_blok->veri.tanimlayici.tip = ilk_isim;
                    Sözcük *ikinci = ilerle(c);
                    yakala_blok->veri.tanimlayici.isim = sozcuk_metni(c, ikinci);
                } else {
                    /* Tek tanımlayıcı: yakala değişken (hepsini yakala) */
                    yakala_blok->veri.tanimlayici.isim = ilk_isim;
                }
            }
            yeni_satir_atla(c);

            /* Yakala bloğu gövdesi */
            while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_SONUNDA) &&
                   !kontrol(c, TOK_YAKALA) && !kontrol(c, TOK_DOSYA_SONU)) {
                yeni_satir_atla(c);
                if (kontrol(c, TOK_SON) || kontrol(c, TOK_SONUNDA) || kontrol(c, TOK_YAKALA)) break;
                düğüm_çocuk_ekle(c->arena, yakala_blok, bildirim_cozumle(c));
                yeni_satir_bekle(c);
            }
            düğüm_çocuk_ekle(c->arena, d, yakala_blok);
        }

        /* Sonunda bloğu (opsiyonel) — flag ile işaretle */
        if (kontrol(c, TOK_SONUNDA)) {
            ilerle(c);  /* sonunda */
            d->veri.tanimlayici.tip = arena_strdup(c->arena, "sonunda");
            yeni_satir_atla(c);
            Düğüm *sonunda_blok = düğüm_oluştur(c->arena, DÜĞÜM_BLOK, s->satir, s->sutun);
            while (!kontrol(c, TOK_SON) && !kontrol(c, TOK_DOSYA_SONU)) {
                yeni_satir_atla(c);
                if (kontrol(c, TOK_SON)) break;
                düğüm_çocuk_ekle(c->arena, sonunda_blok, bildirim_cozumle(c));
                yeni_satir_bekle(c);
            }
            düğüm_çocuk_ekle(c->arena, d, sonunda_blok);
        }
        bekle(c, TOK_SON);

        return d;
    }

    /* Fırlat */
    if (s->tur == TOK_FIRLAT) {
        ilerle(c);
        Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_FIRLAT, s->satir, s->sutun);
        if (!kontrol(c, TOK_YENİ_SATIR) && !kontrol(c, TOK_DOSYA_SONU)) {
            düğüm_çocuk_ekle(c->arena, d, boru_ifade(c));
        }
        return d;
    }

    /* İfade bildirimi */
    Düğüm *ifade = ifade_cozumle(c);
    Düğüm *d = düğüm_oluştur(c->arena, DÜĞÜM_İFADE_BİLDİRİMİ, ifade->satir, ifade->sutun);
    düğüm_çocuk_ekle(c->arena, d, ifade);
    return d;
}

/* Ana çözümleme fonksiyonu */
Düğüm *cozumle(Cozumleyici *c, Sözcük *sozcukler, int sozcuk_sayisi, Arena *arena) {
    c->sozcukler = sozcukler;
    c->sozcuk_sayisi = sozcuk_sayisi;
    c->pos = 0;
    c->arena = arena;
    c->panik_modu = 0;

    Düğüm *program = düğüm_oluştur(arena, DÜĞÜM_PROGRAM, 1, 1);

    yeni_satir_atla(c);

    while (!kontrol(c, TOK_DOSYA_SONU)) {
        if (hata_sayisi >= AZAMI_HATA) {
            hata_genel("çok fazla hata (%d), derleme durduruluyor", hata_sayisi);
            break;
        }
        düğüm_çocuk_ekle(arena, program, bildirim_cozumle(c));
        if (c->panik_modu) senkronize(c);
        yeni_satir_bekle(c);
    }

    return program;
}
