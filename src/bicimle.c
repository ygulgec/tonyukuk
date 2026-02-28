/*
 * Tonyukuk Biçimleyici (Formatter)
 * Kullanım: ./bicimle dosya.tr         (stdout'a yaz)
 *           ./bicimle -w dosya.tr      (dosyayı yerinde düzenle)
 */

#include "sozcuk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Girinti seviyesini artıran anahtar sözcükler */
static int girinti_arttiran_mi(SözcükTürü tur) {
    return tur == TOK_İSE || tur == TOK_YOKSA;
}

/* Girinti seviyesini azaltan anahtar sözcükler */
static int girinti_azaltan_mi(SözcükTürü tur) {
    return tur == TOK_SON || tur == TOK_YOKSA;
}

/* Operatör mü? (etrafına boşluk koyulacak) */
static int operator_mu(SözcükTürü tur) {
    switch (tur) {
    case TOK_EŞİTTİR:
    case TOK_EŞİT_EŞİT:
    case TOK_EŞİT_DEĞİL:
    case TOK_KÜÇÜK:
    case TOK_BÜYÜK:
    case TOK_KÜÇÜK_EŞİT:
    case TOK_BÜYÜK_EŞİT:
    case TOK_ARTI:
    case TOK_EKSI:
    case TOK_ÇARPIM:
    case TOK_BÖLME:
    case TOK_YÜZDE:
    case TOK_ARTI_EŞİT:
    case TOK_EKSİ_EŞİT:
    case TOK_ÇARPIM_EŞİT:
    case TOK_BÖLME_EŞİT:
    case TOK_YÜZDE_EŞİT:
    case TOK_OK:
    case TOK_BORU:
        return 1;
    default:
        return 0;
    }
}

/* Sözcüğün metin değerini döndür */
static void sozcuk_yazdir(FILE *cikti, const Sözcük *s) {
    fwrite(s->başlangıç, 1, s->uzunluk, cikti);
}

/* Girinti yaz */
static void girinti_yazdir(FILE *cikti, int seviye) {
    for (int i = 0; i < seviye; i++) {
        fprintf(cikti, "    ");
    }
}

/* Dosya oku */
static char *dosya_oku(const char *dosya_adi) {
    FILE *f = fopen(dosya_adi, "rb");
    if (!f) {
        fprintf(stderr, "bicimle: dosya açılamadı: %s\n", dosya_adi);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long boyut = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *icerik = (char *)malloc(boyut + 1);
    if (!icerik) {
        fclose(f);
        return NULL;
    }
    fread(icerik, 1, boyut, f);
    icerik[boyut] = '\0';
    fclose(f);
    return icerik;
}

/* Yeni satır mı kontrol et - birden fazla boş satır birleştirilir */
static int yeni_satir_mi(SözcükTürü tur) {
    return tur == TOK_YENİ_SATIR;
}

static void bicimle(const char *kaynak, FILE *cikti) {
    SözcükÇözümleyici sc;
    sözcük_çözümle(&sc, kaynak);

    int girinti = 0;
    int satir_basi = 1;          /* Satırın başında mıyız? */
    int bos_satir_sayisi = 0;    /* Ardışık boş satır sayısı */

    for (int i = 0; i < sc.sozcuk_sayisi; i++) {
        Sözcük *s = &sc.sozcukler[i];

        /* Dosya sonu */
        if (s->tur == TOK_DOSYA_SONU) break;

        /* Yeni satır işleme */
        if (yeni_satir_mi(s->tur)) {
            if (!satir_basi) {
                fprintf(cikti, "\n");
                satir_basi = 1;
                bos_satir_sayisi = 0;
            } else {
                /* Birden fazla boş satır: en fazla bir boş satır bırak */
                if (bos_satir_sayisi < 1) {
                    fprintf(cikti, "\n");
                    bos_satir_sayisi++;
                }
            }
            continue;
        }

        bos_satir_sayisi = 0;

        /* "son" veya "yoksa" girintisini azalt */
        if (satir_basi && girinti_azaltan_mi(s->tur)) {
            if (girinti > 0) girinti--;
        }

        /* Satır başında girinti yaz */
        if (satir_basi) {
            girinti_yazdir(cikti, girinti);
            satir_basi = 0;
        }

        /* Operatör öncesi boşluk */
        if (operator_mu(s->tur) && i > 0 && !yeni_satir_mi(sc.sozcukler[i - 1].tur)) {
            fprintf(cikti, " ");
        }

        /* Sözcüğü yaz */
        sozcuk_yazdir(cikti, s);

        /* Operatör sonrası boşluk */
        if (operator_mu(s->tur)) {
            fprintf(cikti, " ");
        }
        /* Virgül sonrası boşluk */
        else if (s->tur == TOK_VİRGÜL) {
            fprintf(cikti, " ");
        }
        /* İki nokta sonrası boşluk (tip belirteci) */
        else if (s->tur == TOK_İKİ_NOKTA) {
            fprintf(cikti, " ");
        }
        /* Parantez/köşeli açma sonrası boşluk yok */
        else if (s->tur == TOK_PAREN_AC || s->tur == TOK_KÖŞELİ_AÇ || s->tur == TOK_SÜSLÜ_AÇ) {
            /* boşluk ekleme */
        }
        /* Kapanış parantezinden sonra (bir sonraki token parantez değilse) boşluk */
        else if (s->tur == TOK_PAREN_KAPA || s->tur == TOK_KÖŞELİ_KAPA || s->tur == TOK_SÜSLÜ_KAPA) {
            /* boşluk ekleme */
        }
        /* Anahtar sözcük ve tanımlayıcılardan sonra boşluk */
        else if (s->tur == TOK_TANIMLAYICI ||
                 s->tur == TOK_İŞLEV || s->tur == TOK_SINIF || s->tur == TOK_EĞER ||
                 s->tur == TOK_YOKSA || s->tur == TOK_İSE || s->tur == TOK_SON ||
                 s->tur == TOK_DÖNGÜ || s->tur == TOK_İKEN || s->tur == TOK_DÖNDÜR ||
                 s->tur == TOK_KIR || s->tur == TOK_DEVAM || s->tur == TOK_EŞLE ||
                 s->tur == TOK_DURUM || s->tur == TOK_VARSAYILAN ||
                 s->tur == TOK_HER || s->tur == TOK_İÇİN ||
                 s->tur == TOK_KULLAN || s->tur == TOK_BU || s->tur == TOK_GENEL ||
                 s->tur == TOK_SABIT || s->tur == TOK_DENE || s->tur == TOK_YAKALA ||
                 s->tur == TOK_FIRLAT || s->tur == TOK_VE || s->tur == TOK_VEYA ||
                 s->tur == TOK_DEĞİL || s->tur == TOK_DOĞRU || s->tur == TOK_YANLIŞ ||
                 s->tur == TOK_BOŞ || s->tur == TOK_SAYIM || s->tur == TOK_ARAYÜZ ||
                 s->tur == TOK_UYGULA || s->tur == TOK_YAZDIR ||
                 s->tur == TOK_TAM || s->tur == TOK_ONDALIK || s->tur == TOK_METİN ||
                 s->tur == TOK_MANTIK || s->tur == TOK_DİZİ ||
                 s->tur == TOK_TAM_SAYI || s->tur == TOK_ONDALIK_SAYI ||
                 s->tur == TOK_METİN_DEĞERİ) {
            /* Bir sonraki token yeni satır veya kapanış değilse boşluk ekle */
            if (i + 1 < sc.sozcuk_sayisi) {
                SözcükTürü sonraki = sc.sozcukler[i + 1].tur;
                if (sonraki != TOK_YENİ_SATIR && sonraki != TOK_DOSYA_SONU &&
                    sonraki != TOK_PAREN_KAPA && sonraki != TOK_KÖŞELİ_KAPA &&
                    sonraki != TOK_SÜSLÜ_KAPA && sonraki != TOK_VİRGÜL &&
                    sonraki != TOK_İKİ_NOKTA && sonraki != TOK_NOKTA &&
                    s->tur != TOK_PAREN_AC && s->tur != TOK_KÖŞELİ_AÇ) {
                    /* Önceki token parantez açma ise veya sonraki token parantez açma ise
                       bazı özel durumlar */
                    if (sonraki == TOK_PAREN_AC) {
                        /* fonksiyon çağrısı: isim( boşluksuz */
                        if (s->tur == TOK_TANIMLAYICI || s->tur == TOK_YAZDIR) {
                            /* boşluk ekleme */
                        } else {
                            fprintf(cikti, " ");
                        }
                    } else {
                        fprintf(cikti, " ");
                    }
                }
            }
        }

        /* "ise" veya "yoksa" girintisini artır */
        if (girinti_arttiran_mi(s->tur)) {
            /* yoksa ise tekrar artır (yoksa zaten azalttı) */
            girinti++;
        }
    }

    /* Son satır sonunda yeni satır ekle */
    if (!satir_basi) {
        fprintf(cikti, "\n");
    }

    sözcük_serbest(&sc);
}

static void kullanim_goster(void) {
    fprintf(stderr, "Kullanım: bicimle [seçenekler] <dosya.tr>\n");
    fprintf(stderr, "\nSeçenekler:\n");
    fprintf(stderr, "  -w    Dosyayı yerinde düzenle (in-place)\n");
    fprintf(stderr, "  -h    Bu yardım mesajını göster\n");
}

int main(int argc, char **argv) {
    const char *dosya_adi = NULL;
    int yerinde = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0) {
            yerinde = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--yardım") == 0) {
            kullanim_goster();
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "bicimle: bilinmeyen seçenek: %s\n", argv[i]);
            kullanim_goster();
            return 1;
        } else {
            dosya_adi = argv[i];
        }
    }

    if (!dosya_adi) {
        fprintf(stderr, "bicimle: dosya adı belirtilmedi\n");
        kullanim_goster();
        return 1;
    }

    char *kaynak = dosya_oku(dosya_adi);
    if (!kaynak) return 1;

    if (yerinde) {
        /* Geçici dosyaya yaz, sonra taşı */
        char gecici[512];
        snprintf(gecici, sizeof(gecici), "%s.bicimle.tmp", dosya_adi);
        FILE *f = fopen(gecici, "w");
        if (!f) {
            fprintf(stderr, "bicimle: geçici dosya oluşturulamadı: %s\n", gecici);
            free(kaynak);
            return 1;
        }
        bicimle(kaynak, f);
        fclose(f);
        /* Orijinal dosyayı geçici dosyayla değiştir */
        if (rename(gecici, dosya_adi) != 0) {
            fprintf(stderr, "bicimle: dosya taşınamadı\n");
            remove(gecici);
            free(kaynak);
            return 1;
        }
        fprintf(stderr, "bicimle: %s biçimlendirildi\n", dosya_adi);
    } else {
        bicimle(kaynak, stdout);
    }

    free(kaynak);
    return 0;
}
