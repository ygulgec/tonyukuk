#include "sozcuk.h"
#include "utf8.h"
#include "hata.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Anahtar kelime tablosu */
typedef struct {
    const char *kelime;
    SözcükTürü  tur;
} AnahtarKelime;

static AnahtarKelime anahtar_kelimeler[] = {
    {"tam",       TOK_TAM},
    {"ondalık",   TOK_ONDALIK},  /* UTF-8: ondalık */
    {"metin",     TOK_METİN},
    {"mantık",    TOK_MANTIK},   /* UTF-8: mantık */
    {"dizi",      TOK_DİZİ},
    {"eğer",      TOK_EĞER},     /* UTF-8: eğer */
    {"yoksa",     TOK_YOKSA},
    {"ise",       TOK_İSE},
    {"son",       TOK_SON},
    {"döngü",     TOK_DÖNGÜ},    /* UTF-8: döngü */
    {"iken",      TOK_İKEN},
    {"döndür",    TOK_DÖNDÜR},   /* UTF-8: döndür */
    {"kır",       TOK_KIR},      /* UTF-8: kır */
    {"devam",     TOK_DEVAM},
    {"işlev",     TOK_İŞLEV},    /* UTF-8: işlev */
    {"sınıf",     TOK_SINIF},    /* UTF-8: sınıf */
    {"kullan",    TOK_KULLAN},
    {"bu",        TOK_BU},
    {"genel",     TOK_GENEL},
    {"dene",      TOK_DENE},
    {"yakala",    TOK_YAKALA},
    {"fırlat",    TOK_FIRLAT},   /* UTF-8: fırlat */
    {"ve",        TOK_VE},
    {"veya",      TOK_VEYA},
    {"değil",     TOK_DEĞİL},    /* UTF-8: değil */
    {"doğru",     TOK_DOĞRU},    /* UTF-8: doğru */
    {"yanlış",    TOK_YANLIŞ},   /* UTF-8: yanlış */
    {"yazdır",    TOK_YAZDIR},
    {"bo\xc5\x9f", TOK_BOŞ},
    {"sabit",     TOK_SABIT},
    {"e\xc5\x9fle", TOK_EŞLE},
    {"durum",     TOK_DURUM},
    {"varsay\xc4\xb1lan", TOK_VARSAYILAN},
    {"her",       TOK_HER},
    {"i\xc3\xa7in", TOK_İÇİN},
    {"say\xc4\xb1m",     TOK_SAYIM},
    {"sayim",      TOK_SAYIM},
    {"aray\xc3\xbcz",    TOK_ARAYÜZ},
    {"arayuz",     TOK_ARAYÜZ},
    {"uygula",     TOK_UYGULA},
    {"e\xc5\x9fzamans\xc4\xb1z", TOK_EŞZAMANSIZ},
    {"eszamansiz",  TOK_EŞZAMANSIZ},
    {"bekle",       TOK_BEKLE},
    {"de\xc4\x9fi\xc5\x9fken", TOK_DEĞİŞKEN},   /* değişken */
    {"değişken",    TOK_DEĞİŞKEN},
    {"de\xc4\x9f",  TOK_DEĞİŞKEN},              /* değ (kısaltma) */
    {"deg",         TOK_DEĞİŞKEN},
    {"\xc3\xbcrete\xc3\xa7", TOK_ÜRETEÇ},       /* üreteç */
    {"uretec",      TOK_ÜRETEÇ},
    {"\xc3\xbcret", TOK_ÜRET},                   /* üret */
    {"uret",        TOK_ÜRET},
    {"tip",         TOK_TİP_TANIMI},
    {"soyut",       TOK_SOYUT},
    {"test",        TOK_TEST},
    {"do\xc4\x9frula",  TOK_DOĞRULA},   /* doğrula */
    {"dogrula",     TOK_DOĞRULA},
    {"sonunda",     TOK_SONUNDA},
    {"ile",         TOK_ILE},
    {"olarak",      TOK_OLARAK},
    {"k\xc3\xbcme", TOK_KÜME},         /* küme (UTF-8) */
    {"kume",         TOK_KÜME},

    /* Sonuç/Seçenek tip sistemi */
    {"Tamam",        TOK_TAMAM},
    {"Hata",         TOK_HATA_SONUÇ},
    {"Bir",          TOK_BİR},
    {"Hi\xc3\xa7",   TOK_HİÇ},          /* Hiç (UTF-8) */
    {"Hic",          TOK_HİÇ},
    {"Sonu\xc3\xa7", TOK_SONUÇ},        /* Sonuç (UTF-8) */
    {"Sonuc",        TOK_SONUÇ},
    {"Se\xc3\xa7enek", TOK_SEÇENEK},    /* Seçenek (UTF-8) */
    {"Secenek",      TOK_SEÇENEK},

    /* OOP erişim belirleyicileri */
    {"\xc3\xb6zel",  TOK_ÖZEL},         /* özel (UTF-8) */
    {"ozel",         TOK_ÖZEL},
    {"korumal\xc4\xb1", TOK_KORUMALI},  /* korumalı (UTF-8) */
    {"korumali",     TOK_KORUMALI},
    {"statik",       TOK_STATİK},
    {"\xc3\xb6zellik", TOK_ÖZELLİK},    /* özellik (UTF-8) */
    {"özellik",      TOK_ÖZELLİK},
    {"al",           TOK_AL},
    {"ayarla",       TOK_AYARLA},
    {"yok_et",       TOK_YOK_ET},

    {NULL, 0}
};

static void sozcuk_ekle(SözcükÇözümleyici *sc, Sözcük s) {
    if (sc->sozcuk_sayisi >= sc->sozcuk_kapasite) {
        sc->sozcuk_kapasite *= 2;
        Sözcük *yeni = (Sözcük *)realloc(sc->sozcukler,
            sc->sozcuk_kapasite * sizeof(Sözcük));
        if (!yeni) { fprintf(stderr, "bellek yetersiz\n"); abort(); }
        sc->sozcukler = yeni;
    }
    sc->sozcukler[sc->sozcuk_sayisi++] = s;
}

static char mevcut(SözcükÇözümleyici *sc __attribute__((unused))) {
    return sc->kaynak[sc->pos];
}

static char ilerle(SözcükÇözümleyici *sc) {
    char c = sc->kaynak[sc->pos++];
    if (c == '\n') {
        sc->satir++;
        sc->sutun = 1;
    } else {
        sc->sutun++;
    }
    return c;
}

static char bak(SözcükÇözümleyici *sc) {
    return sc->kaynak[sc->pos];
}

static char bak_sonraki(SözcükÇözümleyici *sc) {
    if (sc->kaynak[sc->pos] == '\0') return '\0';
    return sc->kaynak[sc->pos + 1];
}

static int eslesir(SözcükÇözümleyici *sc, char beklenen) {
    if (sc->kaynak[sc->pos] == beklenen) {
        ilerle(sc);
        return 1;
    }
    return 0;
}

static void bosluk_atla(SözcükÇözümleyici *sc) {
    while (1) {
        char c = bak(sc);
        if (c == ' ' || c == '\t' || c == '\r') {
            ilerle(sc);
        } else if (c == '#') {
            /* Yorum: satır sonuna kadar atla */
            while (bak(sc) != '\0' && bak(sc) != '\n') {
                ilerle(sc);
            }
        } else if (c == '/' && bak_sonraki(sc) == '*') {
            /* Blok yorum */
            int yorum_satir = sc->satir;
            int yorum_sutun = sc->sutun;
            ilerle(sc);  /* / atla */
            ilerle(sc);  /* * atla */
            int derinlik = 1;
            while (bak(sc) != '\0' && derinlik > 0) {
                if (bak(sc) == '/' && bak_sonraki(sc) == '*') {
                    ilerle(sc); ilerle(sc);
                    derinlik++;
                    if (derinlik > 64) {
                        hata_bildir(HATA_KAPANMAMIŞ_YORUM, yorum_satir, yorum_sutun);
                        break;
                    }
                } else if (bak(sc) == '*' && bak_sonraki(sc) == '/') {
                    ilerle(sc); ilerle(sc);
                    derinlik--;
                } else {
                    ilerle(sc);
                }
            }
            if (derinlik > 0) {
                hata_bildir(HATA_KAPANMAMIŞ_YORUM, yorum_satir, yorum_sutun);
            }
        } else {
            break;
        }
    }
}

static void sayi_oku(SözcükÇözümleyici *sc) {
    int baslangic_pos = sc->pos;
    int baslangic_sutun = sc->sutun;
    int ondalik = 0;

    while (bak(sc) >= '0' && bak(sc) <= '9') ilerle(sc);

    if (bak(sc) == '.' && bak_sonraki(sc) >= '0' && bak_sonraki(sc) <= '9') {
        ondalik = 1;
        ilerle(sc); /* '.' atla */
        while (bak(sc) >= '0' && bak(sc) <= '9') ilerle(sc);
    }

    int uzunluk = sc->pos - baslangic_pos;
    Sözcük s;
    s.başlangıç = sc->kaynak + baslangic_pos;
    s.uzunluk = uzunluk;
    s.satir = sc->satir;
    s.sutun = baslangic_sutun;

    if (ondalik) {
        s.tur = TOK_ONDALIK_SAYI;
        s.deger.ondalık_değer = strtod(s.başlangıç, NULL);
    } else {
        s.tur = TOK_TAM_SAYI;
        /* strtoll ile parse et (tam sayı) */
        char buf[64];
        int len = uzunluk < 63 ? uzunluk : 63;
        memcpy(buf, s.başlangıç, len);
        buf[len] = '\0';
        s.deger.tam_deger = strtoll(buf, NULL, 10);
    }

    sozcuk_ekle(sc, s);
}

static void metin_oku(SözcükÇözümleyici *sc) {
    int baslangic_sutun = sc->sutun;
    int baslangic_satir = sc->satir;

    /* Çok satırlı metin: """ ... """ */
    if (sc->kaynak[sc->pos] == '"' &&
        sc->kaynak[sc->pos + 1] == '"' &&
        sc->kaynak[sc->pos + 2] == '"') {
        ilerle(sc); ilerle(sc); ilerle(sc); /* açılış """ atla */
        int baslangic_pos3 = sc->pos;
        while (bak(sc) != '\0') {
            if (bak(sc) == '"' && bak_sonraki(sc) == '"' &&
                sc->kaynak[sc->pos + 2] == '"') {
                break;
            }
            ilerle(sc);
        }
        int uzunluk3 = sc->pos - baslangic_pos3;
        if (bak(sc) == '"') { ilerle(sc); ilerle(sc); ilerle(sc); } /* kapanış """ atla */
        Sözcük s;
        s.tur = TOK_METİN_DEĞERİ;
        s.başlangıç = sc->kaynak + baslangic_pos3;
        s.uzunluk = uzunluk3;
        s.satir = baslangic_satir;
        s.sutun = baslangic_sutun;
        s.deger.tam_deger = 0;
        sozcuk_ekle(sc, s);
        return;
    }

    ilerle(sc); /* açılış " atla */
    int baslangic_pos = sc->pos;

    /* String interpolasyonu: ${...} tespiti */
    int interpolasyon_var = 0;
    {
        int tmp = sc->pos;
        while (sc->kaynak[tmp] != '\0' && sc->kaynak[tmp] != '"') {
            if (sc->kaynak[tmp] == '\\') { tmp++; if (sc->kaynak[tmp]) tmp++; continue; }
            if (sc->kaynak[tmp] == '$' && sc->kaynak[tmp+1] == '{') {
                interpolasyon_var = 1;
                break;
            }
            tmp++;
        }
    }

    if (interpolasyon_var) {
        /* İnterpolasyonlu string: parçalara böl */
        int parca_sayisi = 0;
        while (bak(sc) != '\0' && bak(sc) != '"') {
            if (bak(sc) == '\\') {
                ilerle(sc);
                if (bak(sc) != '\0') ilerle(sc);
                continue;
            }
            if (bak(sc) == '$' && bak_sonraki(sc) == '{') {
                /* Önceki metin parçasını ekle */
                int parcauz = sc->pos - baslangic_pos;
                if (parcauz > 0 || parca_sayisi == 0) {
                    if (parca_sayisi > 0) {
                        Sözcük art; art.tur = TOK_ARTI; art.başlangıç = "+"; art.uzunluk = 1;
                        art.satir = sc->satir; art.sutun = sc->sutun;
                        sozcuk_ekle(sc, art);
                    }
                    Sözcük ms; ms.tur = TOK_METİN_DEĞERİ;
                    ms.başlangıç = sc->kaynak + baslangic_pos; ms.uzunluk = parcauz;
                    ms.satir = baslangic_satir; ms.sutun = baslangic_sutun;
                    sozcuk_ekle(sc, ms);
                    parca_sayisi++;
                }
                ilerle(sc); ilerle(sc); /* ${ atla */

                /* İfade tokenlarını oku: } ile kapatılır */
                if (parca_sayisi > 0) {
                    Sözcük art; art.tur = TOK_ARTI; art.başlangıç = "+"; art.uzunluk = 1;
                    art.satir = sc->satir; art.sutun = sc->sutun;
                    sozcuk_ekle(sc, art);
                }
                /* İfade başlangıcı */
                int ifade_bas = sc->pos;
                int ifade_sutun = sc->sutun;
                int parantez = 1;
                while (bak(sc) != '\0' && parantez > 0) {
                    if (bak(sc) == '{') parantez++;
                    else if (bak(sc) == '}') { parantez--; if (parantez == 0) break; }
                    ilerle(sc);
                }
                int ifade_uz = sc->pos - ifade_bas;
                /* İfade tokenı: basit tanımlayıcı olarak ekle */
                Sözcük is; is.tur = TOK_TANIMLAYICI;
                is.başlangıç = sc->kaynak + ifade_bas; is.uzunluk = ifade_uz;
                is.satir = baslangic_satir; is.sutun = ifade_sutun;
                sozcuk_ekle(sc, is);
                parca_sayisi++;

                if (bak(sc) == '}') ilerle(sc); /* } atla */
                baslangic_pos = sc->pos;
                continue;
            }
            if (bak(sc) == '\n') {
                hata_bildir(HATA_KAPANMAMIŞ_METİN, baslangic_satir, baslangic_sutun);
                return;
            }
            ilerle(sc);
        }
        /* Son metin parçası */
        int parcauz = sc->pos - baslangic_pos;
        if (parcauz > 0) {
            Sözcük art; art.tur = TOK_ARTI; art.başlangıç = "+"; art.uzunluk = 1;
            art.satir = sc->satir; art.sutun = sc->sutun;
            sozcuk_ekle(sc, art);
            Sözcük ms; ms.tur = TOK_METİN_DEĞERİ;
            ms.başlangıç = sc->kaynak + baslangic_pos; ms.uzunluk = parcauz;
            ms.satir = baslangic_satir; ms.sutun = baslangic_sutun;
            sozcuk_ekle(sc, ms);
        }
        if (bak(sc) == '"') ilerle(sc);
        return;
    }

    while (bak(sc) != '\0' && bak(sc) != '"') {
        if (bak(sc) == '\\') {
            ilerle(sc); /* escape karakterini atla */
        }
        if (bak(sc) == '\n') {
            hata_bildir(HATA_KAPANMAMIŞ_METİN, baslangic_satir, baslangic_sutun);
            return;
        }
        ilerle(sc);
    }

    if (bak(sc) == '\0') {
        hata_bildir(HATA_KAPANMAMIŞ_METİN, baslangic_satir, baslangic_sutun);
        return;
    }

    int uzunluk = sc->pos - baslangic_pos;
    ilerle(sc); /* kapanış " atla */

    Sözcük s;
    s.tur = TOK_METİN_DEĞERİ;
    s.başlangıç = sc->kaynak + baslangic_pos;
    s.uzunluk = uzunluk;
    s.satir = baslangic_satir;
    s.sutun = baslangic_sutun;
    s.deger.tam_deger = 0;
    sozcuk_ekle(sc, s);
}

static void tanimlayici_oku(SözcükÇözümleyici *sc) {
    int baslangic_pos = sc->pos;
    int baslangic_sutun = sc->sutun;

    /* İlk karakter: UTF-8 codepoint oku ve kontrol et */
    int kontrol_pos = sc->pos;
    uint32_t kod = utf8_codepoint_oku(sc->kaynak, &kontrol_pos);
    if (!utf8_tanimlayici_baslangic(kod)) return;

    /* Byte seviyesinde ilerle */
    int byte_len = kontrol_pos - sc->pos;
    for (int i = 0; i < byte_len; i++) ilerle(sc);

    /* Devam karakterleri */
    while (1) {
        kontrol_pos = sc->pos;
        if (sc->kaynak[kontrol_pos] == '\0') break;
        kod = utf8_codepoint_oku(sc->kaynak, &kontrol_pos);
        if (!utf8_tanimlayici_devam(kod)) break;
        byte_len = kontrol_pos - sc->pos;
        for (int i = 0; i < byte_len; i++) ilerle(sc);
    }

    int uzunluk = sc->pos - baslangic_pos;

    /* Anahtar kelime kontrolü */
    SözcükTürü tur = TOK_TANIMLAYICI;
    for (int i = 0; anahtar_kelimeler[i].kelime != NULL; i++) {
        int klen = (int)strlen(anahtar_kelimeler[i].kelime);
        if (klen == uzunluk &&
            memcmp(sc->kaynak + baslangic_pos, anahtar_kelimeler[i].kelime, uzunluk) == 0) {
            tur = anahtar_kelimeler[i].tur;
            break;
        }
    }

    Sözcük s;
    s.tur = tur;
    s.başlangıç = sc->kaynak + baslangic_pos;
    s.uzunluk = uzunluk;
    s.satir = sc->satir;
    s.sutun = baslangic_sutun;
    s.deger.tam_deger = 0;
    sozcuk_ekle(sc, s);
}

void sözcük_çözümle(SözcükÇözümleyici *sc, const char *kaynak) {
    sc->kaynak = kaynak;
    sc->pos = 0;
    sc->satir = 1;
    sc->sutun = 1;
    sc->sozcuk_kapasite = 256;
    sc->sozcuk_sayisi = 0;
    sc->sozcukler = (Sözcük *)malloc(sc->sozcuk_kapasite * sizeof(Sözcük));

    while (1) {
        bosluk_atla(sc);
        char c = bak(sc);

        if (c == '\0') break;

        int satir = sc->satir;
        int sutun = sc->sutun;

        /* Yeni satır */
        if (c == '\n') {
            ilerle(sc);
            /* Ardışık yeni satırları birleştir */
            if (sc->sozcuk_sayisi > 0 &&
                sc->sozcukler[sc->sozcuk_sayisi - 1].tur != TOK_YENİ_SATIR) {
                Sözcük s = {TOK_YENİ_SATIR, sc->kaynak + sc->pos - 1, 1, satir, sutun, {0}};
                sozcuk_ekle(sc, s);
            }
            continue;
        }

        /* Sayılar */
        if (c >= '0' && c <= '9') {
            sayi_oku(sc);
            continue;
        }

        /* Metin değeri */
        if (c == '"') {
            metin_oku(sc);
            continue;
        }

        /* Tanımlayıcı veya anahtar kelime */
        if (c == '_' || (unsigned char)c >= 0x80 ||
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            tanimlayici_oku(sc);
            continue;
        }

        /* Tek ve çift karakterli operatörler */
        Sözcük s;
        s.başlangıç = sc->kaynak + sc->pos;
        s.satir = satir;
        s.sutun = sutun;
        s.deger.tam_deger = 0;

        switch (c) {
        case '=':
            ilerle(sc);
            if (eslesir(sc, '=')) {
                s.tur = TOK_EŞİT_EŞİT; s.uzunluk = 2;
            } else {
                s.tur = TOK_EŞİTTİR; s.uzunluk = 1;
            }
            break;
        case '!':
            ilerle(sc);
            if (eslesir(sc, '=')) {
                s.tur = TOK_EŞİT_DEĞİL; s.uzunluk = 2;
            } else {
                char buf[8];
                buf[0] = '!'; buf[1] = '\0';
                hata_bildir(HATA_BEKLENMEYEN_KARAKTER, satir, sutun, buf);
                continue;
            }
            break;
        case '<':
            ilerle(sc);
            if (eslesir(sc, '=')) {
                s.tur = TOK_KÜÇÜK_EŞİT; s.uzunluk = 2;
            } else if (eslesir(sc, '<')) {
                s.tur = TOK_SOL_KAYDIR; s.uzunluk = 2;
            } else {
                s.tur = TOK_KÜÇÜK; s.uzunluk = 1;
            }
            break;
        case '>':
            ilerle(sc);
            if (eslesir(sc, '=')) {
                s.tur = TOK_BÜYÜK_EŞİT; s.uzunluk = 2;
            } else if (eslesir(sc, '>')) {
                s.tur = TOK_SAĞ_KAYDIR; s.uzunluk = 2;
            } else {
                s.tur = TOK_BÜYÜK; s.uzunluk = 1;
            }
            break;
        case '+':
            ilerle(sc);
            if (eslesir(sc, '=')) {
                s.tur = TOK_ARTI_EŞİT; s.uzunluk = 2;
            } else {
                s.tur = TOK_ARTI; s.uzunluk = 1;
            }
            break;
        case '-':
            ilerle(sc);
            if (eslesir(sc, '>')) {
                s.tur = TOK_OK; s.uzunluk = 2;
            } else if (eslesir(sc, '=')) {
                s.tur = TOK_EKSİ_EŞİT; s.uzunluk = 2;
            } else {
                s.tur = TOK_EKSI; s.uzunluk = 1;
            }
            break;
        case '*':
            ilerle(sc);
            if (eslesir(sc, '=')) {
                s.tur = TOK_ÇARPIM_EŞİT; s.uzunluk = 2;
            } else {
                s.tur = TOK_ÇARPIM; s.uzunluk = 1;
            }
            break;
        case '/':
            ilerle(sc);
            if (eslesir(sc, '=')) {
                s.tur = TOK_BÖLME_EŞİT; s.uzunluk = 2;
            } else {
                s.tur = TOK_BÖLME; s.uzunluk = 1;
            }
            break;
        case '%':
            ilerle(sc);
            if (eslesir(sc, '=')) {
                s.tur = TOK_YÜZDE_EŞİT; s.uzunluk = 2;
            } else {
                s.tur = TOK_YÜZDE; s.uzunluk = 1;
            }
            break;
        case '|':
            ilerle(sc);
            if (eslesir(sc, '>')) {
                s.tur = TOK_BORU; s.uzunluk = 2;
            } else {
                s.tur = TOK_BİT_VEYA; s.uzunluk = 1;
            }
            break;
        case '&':
            ilerle(sc);
            s.tur = TOK_BİT_VE; s.uzunluk = 1;
            break;
        case '^':
            ilerle(sc);
            s.tur = TOK_BİT_XOR; s.uzunluk = 1;
            break;
        case '~':
            ilerle(sc);
            s.tur = TOK_BİT_DEĞİL; s.uzunluk = 1;
            break;
        case '.':
            ilerle(sc);
            if (eslesir(sc, '.')) {
                if (eslesir(sc, '.')) {
                    s.tur = TOK_ÜÇ_NOKTA; s.uzunluk = 3;
                } else {
                    s.tur = TOK_ARALIK; s.uzunluk = 2;
                }
            } else {
                s.tur = TOK_NOKTA; s.uzunluk = 1;
            }
            break;
        case '@':
            ilerle(sc);
            s.tur = TOK_AT; s.uzunluk = 1;
            break;
        case ',':
            ilerle(sc);
            s.tur = TOK_VİRGÜL; s.uzunluk = 1;
            break;
        case ':':
            ilerle(sc);
            if (bak(sc) == '=') {
                ilerle(sc);
                s.tur = TOK_WALRUS; s.uzunluk = 2;
            } else {
                s.tur = TOK_İKİ_NOKTA; s.uzunluk = 1;
            }
            break;
        case '(':
            ilerle(sc);
            s.tur = TOK_PAREN_AC; s.uzunluk = 1;
            break;
        case ')':
            ilerle(sc);
            s.tur = TOK_PAREN_KAPA; s.uzunluk = 1;
            break;
        case '[':
            ilerle(sc);
            s.tur = TOK_KÖŞELİ_AÇ; s.uzunluk = 1;
            break;
        case ']':
            ilerle(sc);
            s.tur = TOK_KÖŞELİ_KAPA; s.uzunluk = 1;
            break;
        case '{': ilerle(sc); s.tur = TOK_SÜSLÜ_AÇ; s.uzunluk = 1; break;
        case '}': ilerle(sc); s.tur = TOK_SÜSLÜ_KAPA; s.uzunluk = 1; break;
        case '?':
            ilerle(sc);
            if (eslesir(sc, '?')) {
                s.tur = TOK_SORU_SORU; s.uzunluk = 2;
            } else {
                s.tur = TOK_SORU; s.uzunluk = 1;
            }
            break;
        default: {
            char buf[8];
            utf8_codepoint_yaz(buf, (unsigned char)c);
            buf[utf8_byte_uzunluk((unsigned char)c)] = '\0';
            hata_bildir(HATA_BEKLENMEYEN_KARAKTER, satir, sutun, buf);
            ilerle(sc);
            continue;
        }
        }

        sozcuk_ekle(sc, s);
    }

    /* Dosya sonu */
    Sözcük eof = {TOK_DOSYA_SONU, sc->kaynak + sc->pos, 0, sc->satir, sc->sutun, {0}};
    sozcuk_ekle(sc, eof);
}

const char *sözcük_tür_adı(SözcükTürü tur) {
    switch (tur) {
    case TOK_TAM_SAYI:     return "tam sayı";
    case TOK_ONDALIK_SAYI: return "ondalık sayı";
    case TOK_METİN_DEĞERİ: return "metin değeri";
    case TOK_DOĞRU:        return "doğru";
    case TOK_YANLIŞ:       return "yanlış";
    case TOK_TAM:          return "tam";
    case TOK_ONDALIK:      return "ondalık";
    case TOK_METİN:        return "metin";
    case TOK_MANTIK:       return "mantık";
    case TOK_DİZİ:         return "dizi";
    case TOK_EĞER:         return "eğer";
    case TOK_YOKSA:        return "yoksa";
    case TOK_İSE:          return "ise";
    case TOK_SON:          return "son";
    case TOK_DÖNGÜ:        return "döngü";
    case TOK_İKEN:         return "iken";
    case TOK_DÖNDÜR:       return "döndür";
    case TOK_KIR:          return "kır";
    case TOK_DEVAM:        return "devam";
    case TOK_İŞLEV:        return "işlev";
    case TOK_SINIF:        return "sınıf";
    case TOK_KULLAN:       return "kullan";
    case TOK_BU:           return "bu";
    case TOK_GENEL:        return "genel";
    case TOK_DENE:         return "dene";
    case TOK_YAKALA:       return "yakala";
    case TOK_FIRLAT:       return "fırlat";
    case TOK_VE:           return "ve";
    case TOK_VEYA:         return "veya";
    case TOK_DEĞİL:        return "değil";
    case TOK_YAZDIR:       return "yazdır";
    case TOK_BOŞ:          return "boş";
    case TOK_SABIT:        return "sabit";
    case TOK_EŞLE:         return "eşle";
    case TOK_DURUM:        return "durum";
    case TOK_VARSAYILAN:   return "varsayılan";
    case TOK_HER:          return "her";
    case TOK_İÇİN:         return "için";
    case TOK_EŞİTTİR:      return "=";
    case TOK_EŞİT_EŞİT:    return "==";
    case TOK_EŞİT_DEĞİL:   return "!=";
    case TOK_KÜÇÜK:        return "<";
    case TOK_BÜYÜK:        return ">";
    case TOK_KÜÇÜK_EŞİT:   return "<=";
    case TOK_BÜYÜK_EŞİT:   return ">=";
    case TOK_ARTI:         return "+";
    case TOK_EKSI:         return "-";
    case TOK_ÇARPIM:       return "*";
    case TOK_BÖLME:        return "/";
    case TOK_YÜZDE:        return "%";
    case TOK_ARTI_EŞİT:    return "+=";
    case TOK_EKSİ_EŞİT:    return "-=";
    case TOK_ÇARPIM_EŞİT:  return "*=";
    case TOK_BÖLME_EŞİT:   return "/=";
    case TOK_YÜZDE_EŞİT:   return "%=";
    case TOK_BORU:         return "|>";
    case TOK_OK:           return "->";
    case TOK_NOKTA:        return ".";
    case TOK_VİRGÜL:       return ",";
    case TOK_İKİ_NOKTA:    return ":";
    case TOK_PAREN_AC:     return "(";
    case TOK_PAREN_KAPA:   return ")";
    case TOK_KÖŞELİ_AÇ:    return "[";
    case TOK_KÖŞELİ_KAPA:  return "]";
    case TOK_SAYIM:        return "sayım";
    case TOK_ARAYÜZ:       return "arayüz";
    case TOK_UYGULA:       return "uygula";
    case TOK_SÜSLÜ_AÇ:     return "{";
    case TOK_SÜSLÜ_KAPA:   return "}";
    case TOK_SORU:         return "?";
    case TOK_SORU_SORU:    return "??";
    case TOK_AT:           return "@";
    case TOK_ARALIK:       return "..";
    case TOK_ÜÇ_NOKTA:     return "...";
    case TOK_ILE:          return "ile";
    case TOK_OLARAK:       return "olarak";
    case TOK_KÜME:         return "küme";
    case TOK_EŞZAMANSIZ:   return "eşzamansız";
    case TOK_BEKLE:        return "bekle";
    case TOK_DEĞİŞKEN:     return "değişken";
    case TOK_BİT_VE:       return "&";
    case TOK_BİT_VEYA:     return "|";
    case TOK_BİT_XOR:      return "^";
    case TOK_SOL_KAYDIR:   return "<<";
    case TOK_SAĞ_KAYDIR:   return ">>";
    case TOK_BİT_DEĞİL:    return "~";
    case TOK_ÜRETEÇ:       return "üreteç";
    case TOK_ÜRET:         return "üret";
    case TOK_TİP_TANIMI:   return "tip";
    case TOK_SOYUT:        return "soyut";
    case TOK_TEST:         return "test";
    case TOK_DOĞRULA:      return "do\xc4\x9frula";

    /* Sonuç/Seçenek tip sistemi */
    case TOK_TAMAM:        return "Tamam";
    case TOK_HATA_SONUÇ:   return "Hata";
    case TOK_BİR:          return "Bir";
    case TOK_HİÇ:          return "Hi\xc3\xa7";
    case TOK_SONUÇ:        return "Sonu\xc3\xa7";
    case TOK_SEÇENEK:      return "Se\xc3\xa7enek";

    /* OOP erişim belirleyicileri */
    case TOK_ÖZEL:         return "\xc3\xb6zel";
    case TOK_KORUMALI:     return "korumal\xc4\xb1";
    case TOK_STATİK:       return "statik";
    case TOK_ÖZELLİK:      return "\xc3\xb6zellik";
    case TOK_AL:           return "al";
    case TOK_AYARLA:       return "ayarla";
    case TOK_YOK_ET:       return "yok_et";

    case TOK_TANIMLAYICI:  return "tanımlayıcı";
    case TOK_YENİ_SATIR:   return "yeni satır";
    case TOK_DOSYA_SONU:   return "dosya sonu";
    }
    return "bilinmeyen";
}

void sözcük_serbest(SözcükÇözümleyici *sc) {
    free(sc->sozcukler);
    sc->sozcukler = NULL;
    sc->sozcuk_sayisi = 0;
}
