/*
 * Tonyukuk Paket Yöneticisi (ton)
 * Kullanım:
 *   ton başlat        — ton.toml şablonu oluştur
 *   ton yükle         — bağımlılıkları indir
 *   ton liste         — yüklü paketleri listele
 *   ton sil <paket>   — paketi kaldır
 *   ton derle         — projeyi derle (src/ana.tr)
 *   ton calistir      — derle ve çalıştır
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

#define MAKS_BAGIMLILIK 64
#define MAKS_SATIR_UZUNLUK 1024

typedef struct {
    char isim[256];
    char url[512];
} Bagimlilik;

typedef struct {
    char paket_isim[256];
    char paket_surum[64];
    Bagimlilik bagimliliklar[MAKS_BAGIMLILIK];
    int bagimlilik_sayisi;
} TonYapilandirma;

/* ========== ton.toml ayrıştırıcı ========== */

static void bosluk_atla(const char **s) {
    while (**s == ' ' || **s == '\t') (*s)++;
}

/* Basit key = "value" ayrıştırma */
static int anahtar_deger_ayristir(const char *satir, char *anahtar, int anahtar_boyut,
                                   char *deger, int deger_boyut) {
    const char *p = satir;
    bosluk_atla(&p);

    /* Yorum veya boş satır */
    if (*p == '#' || *p == '\0' || *p == '\n' || *p == '[') return 0;

    /* Anahtar */
    int i = 0;
    while (*p && *p != '=' && *p != ' ' && *p != '\t' && i < anahtar_boyut - 1) {
        anahtar[i++] = *p++;
    }
    anahtar[i] = '\0';
    if (i == 0) return 0;

    /* = işaretini atla */
    bosluk_atla(&p);
    if (*p != '=') return 0;
    p++;
    bosluk_atla(&p);

    /* Değer (tırnak içinde veya dışında) */
    int j = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"' && j < deger_boyut - 1) {
            deger[j++] = *p++;
        }
    } else {
        while (*p && *p != '\n' && *p != '#' && *p != ' ' && *p != '\t' && j < deger_boyut - 1) {
            deger[j++] = *p++;
        }
    }
    deger[j] = '\0';

    return 1;
}

static int ton_toml_oku(const char *dosya_yolu, TonYapilandirma *yapilandirma) {
    FILE *f = fopen(dosya_yolu, "r");
    if (!f) {
        fprintf(stderr, "ton: ton.toml bulunamadı. 'ton başlat' ile oluşturun.\n");
        return -1;
    }

    memset(yapilandirma, 0, sizeof(*yapilandirma));

    char satir[MAKS_SATIR_UZUNLUK];
    int bolum = 0;  /* 0: yok, 1: [paket], 2: [bagimliliklar] */

    while (fgets(satir, sizeof(satir), f)) {
        /* Satır sonu temizle */
        int len = (int)strlen(satir);
        while (len > 0 && (satir[len - 1] == '\n' || satir[len - 1] == '\r'))
            satir[--len] = '\0';

        const char *p = satir;
        bosluk_atla(&p);

        /* Bölüm başlığı */
        if (*p == '[') {
            if (strstr(p, "[paket]")) bolum = 1;
            else if (strstr(p, "[bagimliliklar]") || strstr(p, "[bağımlılıklar]")) bolum = 2;
            else bolum = 0;
            continue;
        }

        char anahtar[256], deger[512];
        if (!anahtar_deger_ayristir(p, anahtar, sizeof(anahtar), deger, sizeof(deger)))
            continue;

        if (bolum == 1) {
            if (strcmp(anahtar, "isim") == 0) {
                strncpy(yapilandirma->paket_isim, deger, sizeof(yapilandirma->paket_isim) - 1);
            } else if (strcmp(anahtar, "surum") == 0 || strcmp(anahtar, "sürüm") == 0) {
                strncpy(yapilandirma->paket_surum, deger, sizeof(yapilandirma->paket_surum) - 1);
            }
        } else if (bolum == 2) {
            if (yapilandirma->bagimlilik_sayisi < MAKS_BAGIMLILIK) {
                Bagimlilik *b = &yapilandirma->bagimliliklar[yapilandirma->bagimlilik_sayisi];
                strncpy(b->isim, anahtar, sizeof(b->isim) - 1);
                strncpy(b->url, deger, sizeof(b->url) - 1);
                yapilandirma->bagimlilik_sayisi++;
            }
        }
    }

    fclose(f);
    return 0;
}

/* ========== Komutlar ========== */

static int komut_baslat(void) {
    /* ton.toml zaten var mı? */
    FILE *f = fopen("ton.toml", "r");
    if (f) {
        fclose(f);
        fprintf(stderr, "ton: ton.toml zaten mevcut. Üzerine yazmak istiyor musunuz? (e/h): ");
        char cevap[8];
        if (!fgets(cevap, sizeof(cevap), stdin) || (cevap[0] != 'e' && cevap[0] != 'E')) {
            fprintf(stderr, "ton: iptal edildi\n");
            return 0;
        }
    }

    f = fopen("ton.toml", "w");
    if (!f) {
        fprintf(stderr, "ton: ton.toml oluşturulamadı\n");
        return 1;
    }

    fprintf(f, "[paket]\n");
    fprintf(f, "isim = \"projem\"\n");
    fprintf(f, "surum = \"0.1.0\"\n");
    fprintf(f, "\n");
    fprintf(f, "[bagimliliklar]\n");
    fprintf(f, "# paket_adi = \"https://ornek.com/paket.tr\"\n");

    fclose(f);
    fprintf(stderr, "ton: ton.toml oluşturuldu\n");
    return 0;
}

/* URL güvenlik doğrulaması: sadece http:// ve https:// kabul et */
static int url_dogrula(const char *url) {
    if (strncmp(url, "https://", 8) == 0) return 1;
    if (strncmp(url, "http://", 7) == 0) return 1;
    return 0;
}

/* Güvenli indirme: system() yerine fork()+execvp() kullanır */
static int dosya_indir(const char *url, const char *hedef) {
    if (!url_dogrula(url)) {
        fprintf(stderr, "ton: geçersiz URL (http:// veya https:// ile başlamalı): %s\n", url);
        return -1;
    }

    /* Önce curl ile dene */
    pid_t pid = fork();
    if (pid < 0) {
        perror("ton: fork hatası");
        return -1;
    }

    if (pid == 0) {
        /* Çocuk süreç: stderr'i /dev/null'a yönlendir */
        (void)!freopen("/dev/null", "w", stderr);
        char *args[] = {"curl", "-fsSL", "-o", (char *)hedef, (char *)url, NULL};
        execvp("curl", args);
        _exit(127); /* execvp başarısız olduysa */
    }

    int durum;
    waitpid(pid, &durum, 0);

    if (WIFEXITED(durum) && WEXITSTATUS(durum) == 0) {
        return 0;
    }

    /* curl başarısız oldu, wget ile dene */
    pid = fork();
    if (pid < 0) {
        perror("ton: fork hatası");
        return -1;
    }

    if (pid == 0) {
        (void)!freopen("/dev/null", "w", stderr);
        char *args[] = {"wget", "-q", "-O", (char *)hedef, (char *)url, NULL};
        execvp("wget", args);
        _exit(127);
    }

    waitpid(pid, &durum, 0);

    if (WIFEXITED(durum) && WEXITSTATUS(durum) == 0) {
        return 0;
    }

    return -1;
}

static int komut_yukle(void) {
    TonYapilandirma yap;
    if (ton_toml_oku("ton.toml", &yap) != 0) return 1;

    if (yap.bagimlilik_sayisi == 0) {
        fprintf(stderr, "ton: bağımlılık tanımlanmamış\n");
        return 0;
    }

    /* paketler/ dizinini oluştur */
    mkdir("paketler", 0755);

    int basarili = 0;
    int basarisiz = 0;

    for (int i = 0; i < yap.bagimlilik_sayisi; i++) {
        Bagimlilik *b = &yap.bagimliliklar[i];
        char hedef[512];
        snprintf(hedef, sizeof(hedef), "paketler/%s.tr", b->isim);

        fprintf(stderr, "ton: indiriliyor: %s -> %s\n", b->isim, b->url);

        if (dosya_indir(b->url, hedef) == 0) {
            /* İndirilen dosyanın boyutunu kontrol et */
            FILE *f = fopen(hedef, "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long boyut = ftell(f);
                fclose(f);
                if (boyut > 0) {
                    fprintf(stderr, "ton: %s başarıyla yüklendi (%ld bayt)\n", b->isim, boyut);
                    basarili++;
                } else {
                    fprintf(stderr, "ton: %s indirme başarısız (boş dosya)\n", b->isim);
                    remove(hedef);
                    basarisiz++;
                }
            } else {
                basarisiz++;
            }
        } else {
            fprintf(stderr, "ton: %s indirme başarısız\n", b->isim);
            basarisiz++;
        }
    }

    fprintf(stderr, "ton: %d paket yüklendi, %d başarısız\n", basarili, basarisiz);
    return (basarisiz > 0) ? 1 : 0;
}

static int komut_liste(void) {
    DIR *d = opendir("paketler");
    if (!d) {
        fprintf(stderr, "ton: paketler/ dizini bulunamadı. Henüz paket yüklenmemiş.\n");
        return 0;
    }

    fprintf(stdout, "Yüklü paketler:\n");
    int sayac = 0;
    struct dirent *girdisi;
    while ((girdisi = readdir(d)) != NULL) {
        /* .tr dosyalarını listele */
        const char *isim = girdisi->d_name;
        int len = (int)strlen(isim);
        if (len > 3 && strcmp(isim + len - 3, ".tr") == 0) {
            /* Dosya boyutunu al */
            char yol[512];
            snprintf(yol, sizeof(yol), "paketler/%s", isim);
            struct stat st;
            if (stat(yol, &st) == 0) {
                fprintf(stdout, "  %.*s (%ld bayt)\n", len - 3, isim, (long)st.st_size);
            } else {
                fprintf(stdout, "  %.*s\n", len - 3, isim);
            }
            sayac++;
        }
    }
    closedir(d);

    if (sayac == 0) {
        fprintf(stdout, "  (boş)\n");
    }
    fprintf(stdout, "Toplam: %d paket\n", sayac);
    return 0;
}

/* ========== Paket Silme ========== */

static int komut_sil(const char *paket_adi) {
    if (!paket_adi || strlen(paket_adi) == 0) {
        fprintf(stderr, "ton: paket adı belirtilmedi\n");
        fprintf(stderr, "Kullanım: ton sil <paket_adi>\n");
        return 1;
    }

    char yol[512];
    snprintf(yol, sizeof(yol), "paketler/%s.tr", paket_adi);

    struct stat st;
    if (stat(yol, &st) != 0) {
        fprintf(stderr, "ton: '%s' paketi bulunamadı (paketler/%s.tr mevcut değil)\n",
                paket_adi, paket_adi);
        return 1;
    }

    if (remove(yol) != 0) {
        fprintf(stderr, "ton: '%s' paketi silinemedi\n", paket_adi);
        return 1;
    }

    fprintf(stderr, "ton: '%s' paketi başarıyla silindi\n", paket_adi);
    return 0;
}

/* ========== Derleme ve Çalıştırma ========== */

/* Yardımcı: fork+exec ile bir program çalıştır, çıkış kodunu döndür */
static int program_calistir(char *const args[]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("ton: fork hatası");
        return -1;
    }

    if (pid == 0) {
        execvp(args[0], args);
        perror("ton: çalıştırma hatası");
        _exit(127);
    }

    int durum;
    waitpid(pid, &durum, 0);

    if (WIFEXITED(durum)) {
        return WEXITSTATUS(durum);
    }
    return -1;
}

static int komut_derle(void) {
    /* ton.toml'dan proje adını oku (çıktı adı için) */
    TonYapilandirma yap;
    const char *cikti_adi = "cikti";

    if (ton_toml_oku("ton.toml", &yap) == 0 && strlen(yap.paket_isim) > 0) {
        cikti_adi = yap.paket_isim;
    }

    /* src/ana.tr var mı kontrol et */
    struct stat st;
    if (stat("src/ana.tr", &st) != 0) {
        fprintf(stderr, "ton: src/ana.tr bulunamadı. Proje dizininde olduğunuzdan emin olun.\n");
        return 1;
    }

    fprintf(stderr, "ton: derleniyor: src/ana.tr -> %s\n", cikti_adi);

    char *args[] = {"tonyukuk-derle", "src/ana.tr", "-o", (char *)cikti_adi, NULL};
    int sonuc = program_calistir(args);

    if (sonuc == 0) {
        fprintf(stderr, "ton: derleme başarılı: %s\n", cikti_adi);
    } else {
        fprintf(stderr, "ton: derleme başarısız (çıkış kodu: %d)\n", sonuc);
    }

    return sonuc;
}

static int komut_calistir(void) {
    /* Önce derle */
    int sonuc = komut_derle();
    if (sonuc != 0) {
        return sonuc;
    }

    /* ton.toml'dan proje adını oku */
    TonYapilandirma yap;
    const char *cikti_adi = "cikti";

    if (ton_toml_oku("ton.toml", &yap) == 0 && strlen(yap.paket_isim) > 0) {
        cikti_adi = yap.paket_isim;
    }

    /* Çalıştırılabilir yolu oluştur */
    char calistir_yolu[512];
    snprintf(calistir_yolu, sizeof(calistir_yolu), "./%s", cikti_adi);

    fprintf(stderr, "ton: çalıştırılıyor: %s\n\n", calistir_yolu);

    char *args[] = {calistir_yolu, NULL};
    return program_calistir(args);
}

/* ========== Yeni Proje Oluştur ========== */

static void dosya_yaz_guvenli(const char *yol, const char *icerik) {
    FILE *f = fopen(yol, "w");
    if (!f) {
        fprintf(stderr, "ton: dosya olu\xc5\x9fturulamad\xc4\xb1: %s\n", yol);
        return;
    }
    fprintf(f, "%s", icerik);
    fclose(f);
    fprintf(stderr, "  + %s\n", yol);
}

static int komut_yeni(const char *proje_adi) {
    if (!proje_adi || strlen(proje_adi) == 0) {
        fprintf(stderr, "ton: proje ad\xc4\xb1 belirtilmedi\n");
        fprintf(stderr, "Kullan\xc4\xb1m: ton yeni <proje_adi>\n");
        return 1;
    }

    /* Proje dizini oluştur */
    if (mkdir(proje_adi, 0755) != 0) {
        fprintf(stderr, "ton: '%s' dizini olu\xc5\x9fturulamad\xc4\xb1 (zaten mevcut olabilir)\n", proje_adi);
        return 1;
    }

    fprintf(stderr, "ton: '%s' projesi olu\xc5\x9fturuluyor...\n", proje_adi);

    /* Alt dizinler */
    char yol[512];

    snprintf(yol, sizeof(yol), "%s/src", proje_adi);
    mkdir(yol, 0755);

    snprintf(yol, sizeof(yol), "%s/testler", proje_adi);
    mkdir(yol, 0755);

    /* ton.toml */
    snprintf(yol, sizeof(yol), "%s/ton.toml", proje_adi);
    {
        char icerik[1024];
        snprintf(icerik, sizeof(icerik),
            "[paket]\n"
            "isim = \"%s\"\n"
            "surum = \"0.1.0\"\n"
            "yazar = \"\"\n"
            "aciklama = \"\"\n"
            "\n"
            "[bagimliliklar]\n"
            "# ornek_paket = \"https://ornek.com/paket.tr\"\n",
            proje_adi);
        dosya_yaz_guvenli(yol, icerik);
    }

    /* src/ana.tr */
    snprintf(yol, sizeof(yol), "%s/src/ana.tr", proje_adi);
    {
        char icerik[512];
        snprintf(icerik, sizeof(icerik),
            "## %s ana program dosyasi\n"
            "\n"
            "yazd\xc4\xb1r(\"Merhaba, %s!\")\n",
            proje_adi, proje_adi);
        dosya_yaz_guvenli(yol, icerik);
    }

    /* testler/test_ana.tr */
    snprintf(yol, sizeof(yol), "%s/testler/test_ana.tr", proje_adi);
    dosya_yaz_guvenli(yol,
        "## Ana modul testleri\n"
        "\n"
        "test \"temel test\" ise\n"
        "    dogrula(1 == 1)\n"
        "    dogrula(2 + 2 == 4)\n"
        "son\n");

    /* README.md */
    snprintf(yol, sizeof(yol), "%s/README.md", proje_adi);
    {
        char icerik[1024];
        snprintf(icerik, sizeof(icerik),
            "# %s\n"
            "\n"
            "Tonyukuk programlama dili ile yazilmis bir proje.\n"
            "\n"
            "## Derleme\n"
            "\n"
            "```bash\n"
            "tonyukuk-derle src/ana.tr -o %s\n"
            "```\n"
            "\n"
            "## Test\n"
            "\n"
            "```bash\n"
            "tonyukuk-derle testler/test_ana.tr --test -o test_cikti\n"
            "./test_cikti\n"
            "```\n",
            proje_adi, proje_adi);
        dosya_yaz_guvenli(yol, icerik);
    }

    fprintf(stderr, "\nton: '%s' projesi ba\xc5\x9far\xc4\xb1yla olu\xc5\x9fturuldu!\n", proje_adi);
    fprintf(stderr, "\n  cd %s\n  tonyukuk-derle src/ana.tr -o %s\n  ./%s\n\n", proje_adi, proje_adi, proje_adi);
    return 0;
}

/* ========== Ana program ========== */

static void kullanim_goster(void) {
    fprintf(stderr, "Tonyukuk Paket Yöneticisi (ton)\n\n");
    fprintf(stderr, "Kullanım: ton <komut>\n\n");
    fprintf(stderr, "Komutlar:\n");
    fprintf(stderr, "  yeni <ad>     Yeni proje oluştur\n");
    fprintf(stderr, "  başlat        ton.toml şablonu oluştur\n");
    fprintf(stderr, "  yükle         bağımlılıkları indir\n");
    fprintf(stderr, "  liste         yüklü paketleri listele\n");
    fprintf(stderr, "  sil <paket>   paketi kaldır\n");
    fprintf(stderr, "  derle         projeyi derle (src/ana.tr)\n");
    fprintf(stderr, "  çalıştır      derle ve çalıştır\n");
    fprintf(stderr, "  yardım        bu mesajı göster\n");
}

/* UTF-8 uyumlu komut karşılaştırma */
static int komut_esit(const char *girdi, const char *asci, const char *turkce) {
    return strcmp(girdi, asci) == 0 || strcmp(girdi, turkce) == 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        kullanim_goster();
        return 1;
    }

    const char *komut = argv[1];

    if (strcmp(komut, "yeni") == 0 || strcmp(komut, "new") == 0) {
        const char *proje_adi = (argc >= 3) ? argv[2] : NULL;
        return komut_yeni(proje_adi);
    }

    if (komut_esit(komut, "baslat", "ba\xc5\x9flat") ||
        strcmp(komut, "init") == 0) {
        return komut_baslat();
    }

    if (komut_esit(komut, "yukle", "y\xc3\xbckle") ||
        strcmp(komut, "install") == 0) {
        return komut_yukle();
    }

    if (strcmp(komut, "liste") == 0 || strcmp(komut, "list") == 0) {
        return komut_liste();
    }

    if (strcmp(komut, "sil") == 0 || strcmp(komut, "remove") == 0) {
        const char *paket_adi = (argc >= 3) ? argv[2] : NULL;
        return komut_sil(paket_adi);
    }

    if (strcmp(komut, "derle") == 0 || strcmp(komut, "build") == 0) {
        return komut_derle();
    }

    if (komut_esit(komut, "calistir", "çalıştır") ||
        strcmp(komut, "run") == 0) {
        return komut_calistir();
    }

    if (komut_esit(komut, "yardim", "yardım") ||
        strcmp(komut, "help") == 0 ||
        strcmp(komut, "-h") == 0 || strcmp(komut, "--help") == 0) {
        kullanim_goster();
        return 0;
    }

    fprintf(stderr, "ton: bilinmeyen komut: %s\n", komut);
    kullanim_goster();
    return 1;
}
