/*
 * Tonyukuk Belgeleme Oluşturucu (trdoc)
 * .tr dosyalarından ## belge yorumlarını çıkarır ve HTML üretir.
 *
 * Kullanım:
 *   trdoc dosya.tr              -> dosya.html
 *   trdoc dosya.tr -o cikti.html
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAKS_SATIRLAR 4096
#define MAKS_SATIR    1024
#define MAKS_BELGE    256

typedef struct {
    char yorum[MAKS_SATIR];   /* ## belge yorumu */
    char isim[512];           /* fonksiyon/sınıf adı */
    char tur[64];             /* "islev", "sinif", "değişken" */
    char parametreler[1024];  /* parametre listesi */
    char dönüş_tipi[128];     /* dönüş tipi */
    int  satir;               /* satır numarası */
} BelgeGirdi;

static BelgeGirdi belgeler[MAKS_BELGE];
static int belge_sayisi = 0;

/* Satır başındaki boşlukları atla */
static const char *bosluk_atla(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* Satırdan ## yorumunu çıkar */
static int belge_yorumu_mu(const char *satir, char *yorum, int boyut) {
    const char *p = bosluk_atla(satir);
    if (p[0] == '#' && p[1] == '#') {
        p += 2;
        if (*p == ' ') p++;
        int len = (int)strlen(p);
        while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r'))
            len--;
        if (len >= boyut) len = boyut - 1;
        memcpy(yorum, p, len);
        yorum[len] = '\0';
        return 1;
    }
    return 0;
}

/* "islev" veya "işlev" anahtar kelimesi ile başlıyor mu? */
static int islev_mi(const char *satir, char *isim, int isim_boyut,
                     char *parametreler, int param_boyut,
                     char *dönüş_tipi, int donus_boyut) {
    const char *p = bosluk_atla(satir);
    int eslesti = 0;

    /* "islev " veya "işlev " (UTF-8: \xc4\xb1\xc5\x9f) */
    if (strncmp(p, "islev ", 6) == 0) { p += 6; eslesti = 1; }
    else if (strncmp(p, "\xc4\xb1\xc5\x9flev ", 8) == 0) { p += 8; eslesti = 1; }

    if (!eslesti) return 0;

    p = bosluk_atla(p);

    /* İsim */
    int i = 0;
    while (*p && *p != '(' && *p != ' ' && *p != '\n' && i < isim_boyut - 1) {
        isim[i++] = *p++;
    }
    isim[i] = '\0';

    /* Parametreler */
    parametreler[0] = '\0';
    if (*p == '(') {
        const char *başlangıç = p;
        while (*p && *p != ')') p++;
        if (*p == ')') p++;
        int plen = (int)(p - başlangıç);
        if (plen >= param_boyut) plen = param_boyut - 1;
        memcpy(parametreler, başlangıç, plen);
        parametreler[plen] = '\0';
    }

    /* Dönüş tipi: -> veya : sonrası */
    dönüş_tipi[0] = '\0';
    p = bosluk_atla(p);
    if (p[0] == '-' && p[1] == '>') {
        p += 2;
        p = bosluk_atla(p);
        int j = 0;
        while (*p && *p != '\n' && *p != '\r' && *p != ' ' && j < donus_boyut - 1) {
            dönüş_tipi[j++] = *p++;
        }
        dönüş_tipi[j] = '\0';
    } else if (*p == ':') {
        p++;
        p = bosluk_atla(p);
        int j = 0;
        while (*p && *p != '\n' && *p != '\r' && *p != ' ' && j < donus_boyut - 1) {
            dönüş_tipi[j++] = *p++;
        }
        dönüş_tipi[j] = '\0';
    }

    return 1;
}

/* "sinif" veya "sınıf" ile başlıyor mu? */
static int sinif_mi(const char *satir, char *isim, int isim_boyut) {
    const char *p = bosluk_atla(satir);
    int eslesti = 0;

    if (strncmp(p, "sinif ", 6) == 0) { p += 6; eslesti = 1; }
    else if (strncmp(p, "s\xc4\xb1n\xc4\xb1" "f ", 8) == 0) { p += 8; eslesti = 1; }

    if (!eslesti) return 0;
    p = bosluk_atla(p);

    int i = 0;
    while (*p && *p != ' ' && *p != '\n' && *p != '(' && *p != ':' && i < isim_boyut - 1) {
        isim[i++] = *p++;
    }
    isim[i] = '\0';
    return 1;
}

/* Dosyayı oku ve belge girdilerini çıkar */
static int dosya_isle(const char *dosya_yolu) {
    FILE *f = fopen(dosya_yolu, "r");
    if (!f) {
        fprintf(stderr, "trdoc: dosya a\xc3\xa7\xc4\xb1lamad\xc4\xb1: %s\n", dosya_yolu);
        return -1;
    }

    /* Heap'te yer ayır (stack overflow önlemi) */
    char **satirlar = (char **)malloc(MAKS_SATIRLAR * sizeof(char *));
    if (!satirlar) { fclose(f); return -1; }
    for (int k = 0; k < MAKS_SATIRLAR; k++) satirlar[k] = NULL;

    int satir_sayisi = 0;
    char buf[MAKS_SATIR];
    while (satir_sayisi < MAKS_SATIRLAR && fgets(buf, MAKS_SATIR, f)) {
        satirlar[satir_sayisi] = strdup(buf);
        satir_sayisi++;
    }
    fclose(f);

    /* ## yorumlarını bul ve sonraki satırlarla ilişkilendir */
    for (int i = 0; i < satir_sayisi && belge_sayisi < MAKS_BELGE; i++) {
        char yorum[MAKS_SATIR];
        if (!belge_yorumu_mu(satirlar[i], yorum, sizeof(yorum)))
            continue;

        /* Birden fazla ## satırını birleştir */
        char tam_yorum[MAKS_SATIR * 4];
        snprintf(tam_yorum, sizeof(tam_yorum), "%s", yorum);

        int j = i + 1;
        while (j < satir_sayisi) {
            char ek_yorum[MAKS_SATIR];
            if (belge_yorumu_mu(satirlar[j], ek_yorum, sizeof(ek_yorum))) {
                int mevcut_len = (int)strlen(tam_yorum);
                if (mevcut_len + (int)strlen(ek_yorum) + 2 < (int)sizeof(tam_yorum)) {
                    tam_yorum[mevcut_len] = ' ';
                    strcpy(tam_yorum + mevcut_len + 1, ek_yorum);
                }
                j++;
            } else {
                break;
            }
        }

        /* Yorum sonrasındaki ilk kod satırını analiz et */
        if (j < satir_sayisi) {
            BelgeGirdi *bg = &belgeler[belge_sayisi];
            strncpy(bg->yorum, tam_yorum, sizeof(bg->yorum) - 1);
            bg->satir = j + 1;

            char isim[512], parametreler[1024], dönüş_tipi[128];
            if (islev_mi(satirlar[j], isim, sizeof(isim),
                         parametreler, sizeof(parametreler),
                         dönüş_tipi, sizeof(dönüş_tipi))) {
                strncpy(bg->isim, isim, sizeof(bg->isim) - 1);
                strncpy(bg->tur, "islev", sizeof(bg->tur) - 1);
                strncpy(bg->parametreler, parametreler, sizeof(bg->parametreler) - 1);
                strncpy(bg->dönüş_tipi, dönüş_tipi, sizeof(bg->dönüş_tipi) - 1);
                belge_sayisi++;
            } else if (sinif_mi(satirlar[j], isim, sizeof(isim))) {
                strncpy(bg->isim, isim, sizeof(bg->isim) - 1);
                strncpy(bg->tur, "sinif", sizeof(bg->tur) - 1);
                bg->parametreler[0] = '\0';
                bg->dönüş_tipi[0] = '\0';
                belge_sayisi++;
            }
        }

        i = j - 1;  /* İşlenen satırları atla */
    }

    /* Temizlik */
    for (int k = 0; k < satir_sayisi; k++) free(satirlar[k]);
    free(satirlar);

    return 0;
}

/* HTML çıktısı üret */
static int html_uret(const char *dosya_adi, const char *cikti_dosya) {
    FILE *f = fopen(cikti_dosya, "w");
    if (!f) {
        fprintf(stderr, "trdoc: \xc3\xa7\xc4\xb1kt\xc4\xb1 dosyas\xc4\xb1 olu\xc5\x9fturulamad\xc4\xb1: %s\n", cikti_dosya);
        return -1;
    }

    fprintf(f, "<!DOCTYPE html>\n<html lang=\"tr\">\n<head>\n");
    fprintf(f, "  <meta charset=\"UTF-8\">\n");
    fprintf(f, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(f, "  <title>%s - Tonyukuk Belgeleme</title>\n", dosya_adi);
    fprintf(f, "  <style>\n");
    fprintf(f, "    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n");
    fprintf(f, "           max-width: 900px; margin: 0 auto; padding: 20px;\n");
    fprintf(f, "           background: #fafafa; color: #333; }\n");
    fprintf(f, "    h1 { color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }\n");
    fprintf(f, "    h2 { color: #2980b9; margin-top: 30px; }\n");
    fprintf(f, "    .belge-girdi { background: #fff; border: 1px solid #ddd;\n");
    fprintf(f, "                   border-radius: 8px; padding: 16px; margin: 12px 0;\n");
    fprintf(f, "                   box-shadow: 0 1px 3px rgba(0,0,0,0.1); }\n");
    fprintf(f, "    .islev-isim { font-family: 'Courier New', monospace;\n");
    fprintf(f, "                  font-size: 1.1em; font-weight: bold;\n");
    fprintf(f, "                  color: #e74c3c; }\n");
    fprintf(f, "    .sinif-isim { font-family: 'Courier New', monospace;\n");
    fprintf(f, "                  font-size: 1.1em; font-weight: bold;\n");
    fprintf(f, "                  color: #8e44ad; }\n");
    fprintf(f, "    .parametre { color: #27ae60; font-family: monospace; }\n");
    fprintf(f, "    .donus { color: #f39c12; font-family: monospace; }\n");
    fprintf(f, "    .aciklama { margin-top: 8px; color: #555; line-height: 1.6; }\n");
    fprintf(f, "    .satir-no { color: #999; font-size: 0.85em; }\n");
    fprintf(f, "    .etiket { display: inline-block; padding: 2px 8px; border-radius: 3px;\n");
    fprintf(f, "              font-size: 0.8em; font-weight: bold; margin-right: 8px; }\n");
    fprintf(f, "    .etiket-islev { background: #fadbd8; color: #e74c3c; }\n");
    fprintf(f, "    .etiket-sinif { background: #e8daef; color: #8e44ad; }\n");
    fprintf(f, "    footer { margin-top: 40px; text-align: center; color: #999;\n");
    fprintf(f, "             font-size: 0.85em; border-top: 1px solid #ddd; padding-top: 10px; }\n");
    fprintf(f, "  </style>\n");
    fprintf(f, "</head>\n<body>\n");

    fprintf(f, "  <h1>%s</h1>\n", dosya_adi);

    /* İşlevler */
    int islev_var = 0;
    for (int i = 0; i < belge_sayisi; i++) {
        if (strcmp(belgeler[i].tur, "islev") == 0) {
            if (!islev_var) {
                fprintf(f, "  <h2>İşlevler</h2>\n");
                islev_var = 1;
            }
            fprintf(f, "  <div class=\"belge-girdi\">\n");
            fprintf(f, "    <span class=\"etiket etiket-islev\">işlev</span>\n");
            fprintf(f, "    <span class=\"islev-isim\">%s</span>", belgeler[i].isim);
            if (belgeler[i].parametreler[0]) {
                fprintf(f, " <span class=\"parametre\">%s</span>", belgeler[i].parametreler);
            }
            if (belgeler[i].dönüş_tipi[0]) {
                fprintf(f, " &rarr; <span class=\"donus\">%s</span>", belgeler[i].dönüş_tipi);
            }
            fprintf(f, "\n");
            fprintf(f, "    <div class=\"aciklama\">%s</div>\n", belgeler[i].yorum);
            fprintf(f, "    <div class=\"satir-no\">Sat\xc4\xb1r: %d</div>\n", belgeler[i].satir);
            fprintf(f, "  </div>\n");
        }
    }

    /* Sınıflar */
    int sinif_var = 0;
    for (int i = 0; i < belge_sayisi; i++) {
        if (strcmp(belgeler[i].tur, "sinif") == 0) {
            if (!sinif_var) {
                fprintf(f, "  <h2>S\xc4\xb1n\xc4\xb1" "flar</h2>\n");
                sinif_var = 1;
            }
            fprintf(f, "  <div class=\"belge-girdi\">\n");
            fprintf(f, "    <span class=\"etiket etiket-sinif\">s\xc4\xb1n\xc4\xb1" "f</span>\n");
            fprintf(f, "    <span class=\"sinif-isim\">%s</span>\n", belgeler[i].isim);
            fprintf(f, "    <div class=\"aciklama\">%s</div>\n", belgeler[i].yorum);
            fprintf(f, "    <div class=\"satir-no\">Sat\xc4\xb1r: %d</div>\n", belgeler[i].satir);
            fprintf(f, "  </div>\n");
        }
    }

    if (belge_sayisi == 0) {
        fprintf(f, "  <p><em>Belge yorumu (##) bulunamad\xc4\xb1.</em></p>\n");
    }

    fprintf(f, "  <footer>Tonyukuk Belgeleme Olu\xc5\x9fturucu (trdoc) taraf\xc4\xb1ndan \xc3\xbcretildi</footer>\n");
    fprintf(f, "</body>\n</html>\n");

    fclose(f);
    return 0;
}

static void kullanim_goster(void) {
    fprintf(stderr, "Tonyukuk Belgeleme Olu\xc5\x9fturucu (trdoc)\n\n");
    fprintf(stderr, "Kullan\xc4\xb1m:\n");
    fprintf(stderr, "  trdoc <dosya.tr>           HTML belge \xc3\xbcret\n");
    fprintf(stderr, "  trdoc <dosya.tr> -o <\xc3\xa7\xc4\xb1kt\xc4\xb1.html>\n\n");
    fprintf(stderr, "Se\xc3\xa7enekler:\n");
    fprintf(stderr, "  -o <dosya>   \xc3\x87\xc4\xb1kt\xc4\xb1 dosyas\xc4\xb1n\xc4\xb1 belirle\n");
    fprintf(stderr, "  -h, --help   Bu mesaj\xc4\xb1 g\xc3\xb6ster\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        kullanim_goster();
        return 1;
    }

    const char *girdi_dosya = NULL;
    const char *cikti_dosya = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            cikti_dosya = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            kullanim_goster();
            return 0;
        } else {
            girdi_dosya = argv[i];
        }
    }

    if (!girdi_dosya) {
        fprintf(stderr, "trdoc: giri\xc5\x9f dosyas\xc4\xb1 belirtilmedi\n");
        return 1;
    }

    /* Varsayılan çıktı dosya adı */
    char varsayilan_cikti[512];
    if (!cikti_dosya) {
        strncpy(varsayilan_cikti, girdi_dosya, sizeof(varsayilan_cikti) - 6);
        varsayilan_cikti[sizeof(varsayilan_cikti) - 6] = '\0';
        int len = (int)strlen(varsayilan_cikti);
        /* .tr uzantısını .html ile değiştir */
        if (len > 3 && strcmp(varsayilan_cikti + len - 3, ".tr") == 0) {
            strcpy(varsayilan_cikti + len - 3, ".html");
        } else {
            strcat(varsayilan_cikti, ".html");
        }
        cikti_dosya = varsayilan_cikti;
    }

    if (dosya_isle(girdi_dosya) != 0) return 1;

    if (html_uret(girdi_dosya, cikti_dosya) != 0) return 1;

    fprintf(stderr, "trdoc: %d belge girdisi bulundu -> %s\n", belge_sayisi, cikti_dosya);
    return 0;
}
