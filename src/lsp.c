/*
 * Tonyukuk Dil Sunucusu Protokolü (LSP)
 * JSON-RPC 2.0 over stdin/stdout
 *
 * Desteklenen yöntemler:
 *   - initialize
 *   - initialized
 *   - textDocument/didOpen
 *   - textDocument/didChange
 *   - textDocument/hover
 *   - textDocument/completion
 *   - shutdown
 *   - exit
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "sozcuk.h"
#include "cozumleyici.h"
#include "agac.h"
#include "anlam.h"
#include "tablo.h"
#include "bellek.h"
#include "metin.h"
#include "hata.h"

/* ========== Basit JSON Yardımcıları ========== */

/* JSON string'ten değer çıkar (basit, iç içe desteklemez) */
static const char *json_str_bul(const char *json, const char *anahtar, char *buf, int boyut) {
    char arama[256];
    snprintf(arama, sizeof(arama), "\"%s\"", anahtar);
    const char *p = strstr(json, arama);
    if (!p) return NULL;
    p += strlen(arama);

    /* : atla */
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;

    if (*p == '"') {
        p++;
        int i = 0;
        while (*p && *p != '"' && i < boyut - 1) {
            if (*p == '\\' && *(p+1)) {
                p++;
                switch (*p) {
                    case 'n': buf[i++] = '\n'; break;
                    case 't': buf[i++] = '\t'; break;
                    case '\\': buf[i++] = '\\'; break;
                    case '"': buf[i++] = '"'; break;
                    default: buf[i++] = *p; break;
                }
            } else {
                buf[i++] = *p;
            }
            p++;
        }
        buf[i] = '\0';
        return buf;
    }
    return NULL;
}

/* JSON'dan tamsayı değer çıkar */
static int json_int_bul(const char *json, const char *anahtar) {
    char arama[256];
    snprintf(arama, sizeof(arama), "\"%s\"", anahtar);
    const char *p = strstr(json, arama);
    if (!p) return -1;
    p += strlen(arama);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    return atoi(p);
}

/* ========== JSON Escape Yardımcısı ========== */

/* JSON string'te özel karakterleri kaçış sekanslarıyla değiştir */
static void json_string_yaz(Metin *m, const char *s) {
    while (*s) {
        switch (*s) {
            case '"':  metin_ekle(m, "\\\""); break;
            case '\\': metin_ekle(m, "\\\\"); break;
            case '\n': metin_ekle(m, "\\n"); break;
            case '\r': metin_ekle(m, "\\r"); break;
            case '\t': metin_ekle(m, "\\t"); break;
            default:   metin_ekle_karakter(m, *s); break;
        }
        s++;
    }
}

/* ========== LSP Mesaj I/O ========== */

static void lsp_gonder(const char *icerik) {
    int uzunluk = (int)strlen(icerik);
    fprintf(stdout, "Content-Length: %d\r\n\r\n%s", uzunluk, icerik);
    fflush(stdout);
}

static char *lsp_oku(void) {
    /* Header: Content-Length: N\r\n\r\n */
    char baslik[256];
    int icerik_uzunluk = 0;

    while (fgets(baslik, sizeof(baslik), stdin)) {
        if (baslik[0] == '\r' || baslik[0] == '\n') break;
        if (strncmp(baslik, "Content-Length:", 15) == 0) {
            icerik_uzunluk = atoi(baslik + 15);
        }
    }

    if (icerik_uzunluk <= 0) return NULL;

    char *buf = (char *)malloc(icerik_uzunluk + 1);
    if (!buf) return NULL;

    size_t okunan = fread(buf, 1, icerik_uzunluk, stdin);
    buf[okunan] = '\0';

    return buf;
}

/* ========== Doküman Deposu ========== */

#define MAKS_DOKUMAN 64

typedef struct {
    char uri[512];
    char *icerik;
    int icerik_uzunluk;
} Dokuman;

static Dokuman dokumanlar[MAKS_DOKUMAN];
static int dokuman_sayisi = 0;

static Dokuman *dokuman_bul(const char *uri) {
    for (int i = 0; i < dokuman_sayisi; i++) {
        if (strcmp(dokumanlar[i].uri, uri) == 0)
            return &dokumanlar[i];
    }
    return NULL;
}

static Dokuman *dokuman_ekle(const char *uri) {
    Dokuman *d = dokuman_bul(uri);
    if (d) return d;
    if (dokuman_sayisi >= MAKS_DOKUMAN) return NULL;
    d = &dokumanlar[dokuman_sayisi++];
    strncpy(d->uri, uri, sizeof(d->uri) - 1);
    d->icerik = NULL;
    d->icerik_uzunluk = 0;
    return d;
}

static void dokuman_icerik_ayarla(Dokuman *d, const char *icerik) {
    if (d->icerik) free(d->icerik);
    d->icerik = strdup(icerik);
    d->icerik_uzunluk = (int)strlen(icerik);
}

/* ========== Hata Konumu Yakalama ========== */

/* İlk hatanın satır/sütun bilgisini saklamak için */
typedef struct {
    int satir;
    int sutun;
    char mesaj[512];
    int gecerli;
} HataKonumu;

/* Sözcüklerden ilk hata konumunu bul */
static HataKonumu ilk_hata_konumu_bul(SözcükÇözümleyici *sc) {
    HataKonumu konum = {0, 0, "", 0};

    /* Son sözcüğün konumunu kullan (hata genellikle son işlenen sözcükte olur) */
    /* Eğer sözcük listesinde hata varsa, son sözcük EOF veya hatalı konumdadır */
    if (sc->sozcuk_sayisi > 1) {
        /* Son sözcük EOF, ondan öncekini kullan */
        Sözcük *son = &sc->sozcukler[sc->sozcuk_sayisi - 2];
        konum.satir = son->satir > 0 ? son->satir - 1 : 0; /* LSP 0-indexed */
        konum.sutun = son->sutun > 0 ? son->sutun - 1 : 0;
        konum.gecerli = 1;
    }

    return konum;
}

/* stderr çıktısını yakalayarak hata konumunu parse et */
static HataKonumu stderr_hata_konumu_yakala(const char *icerik) {
    HataKonumu konum = {0, 0, "", 0};

    /* stderr'i geçici dosyaya yönlendir */
    FILE *eski_stderr = stderr;
    char gecici_yol[] = "/tmp/tonyukuk-lsp-XXXXXX";
    int fd = mkstemp(gecici_yol);
    if (fd < 0) return konum;

    FILE *gecici = fdopen(fd, "w+");
    if (!gecici) { close(fd); unlink(gecici_yol); return konum; }

    stderr = gecici;

    /* Ayrıştırma yap */
    hata_sayisi = 0;
    hata_dosya_adi = "kaynak";
    hata_kaynak = icerik;

    Arena arena;
    arena_baslat(&arena);

    SözcükÇözümleyici sc;
    sözcük_çözümle(&sc, icerik);

    int sozdizimi_hatasi = hata_sayisi;
    Cozumleyici coz;
    Düğüm *program = NULL;
    int anlam_hatasi = 0;

    if (sozdizimi_hatasi == 0) {
        program = cozumle(&coz, sc.sozcukler, sc.sozcuk_sayisi, &arena);
        sozdizimi_hatasi = hata_sayisi;
    }

    if (sozdizimi_hatasi == 0 && program) {
        hata_sayisi = 0;
        AnlamÇözümleyici ac;
        anlam_çözümle(&ac, program, &arena, NULL);
        anlam_hatasi = hata_sayisi;
    }

    int toplam_hata = sozdizimi_hatasi + anlam_hatasi;

    /* stderr çıktısını oku */
    fflush(gecici);
    fseek(gecici, 0, SEEK_SET);

    char satir_buf[1024];
    while (fgets(satir_buf, sizeof(satir_buf), gecici)) {
        /* Format: hata: dosya:satir:sutun: mesaj
         * veya renkli: \033[...hata:\033[... dosya:satir:sutun:\033[... mesaj */

        /* "kaynak:" kelimesini ara */
        char *dosya_pos = strstr(satir_buf, "kaynak:");
        if (!dosya_pos) continue;

        dosya_pos += strlen("kaynak:");

        int s = 0, su = 0;
        char *endp1 = NULL;
        s = (int)strtol(dosya_pos, &endp1, 10);
        if (endp1 && *endp1 == ':') {
            su = (int)strtol(endp1 + 1, NULL, 10);
        }

        if (s > 0) {
            konum.satir = s - 1;  /* LSP 0-indexed */
            konum.sutun = su > 0 ? su - 1 : 0;
            konum.gecerli = 1;

            /* Mesajı çıkar: ':' sonrası boşlukları atla */
            char *mesaj_bas = endp1 ? endp1 + 1 : NULL;
            if (mesaj_bas) {
                /* İkinci ':' atla */
                char *msg = strchr(mesaj_bas, ':');
                if (!msg) msg = mesaj_bas;
                /* ANSI escape kodlarını atla - basit temizleme */
                char *hedef = konum.mesaj;
                int kalan = (int)sizeof(konum.mesaj) - 1;
                const char *kaynak = msg;
                while (*kaynak && kalan > 0) {
                    if (*kaynak == '\033') {
                        /* ESC sekansını atla */
                        while (*kaynak && *kaynak != 'm') kaynak++;
                        if (*kaynak == 'm') kaynak++;
                        continue;
                    }
                    if (*kaynak == '\n' || *kaynak == '\r') { kaynak++; continue; }
                    *hedef++ = *kaynak++;
                    kalan--;
                }
                *hedef = '\0';

                /* Baştaki boşlukları temizle */
                char *temiz = konum.mesaj;
                while (*temiz == ' ' || *temiz == ':') temiz++;
                if (temiz != konum.mesaj) {
                    memmove(konum.mesaj, temiz, strlen(temiz) + 1);
                }
            }
            break; /* İlk hatayı bulduk */
        }
    }

    /* Eğer satır bulunamadıysa ama hata varsa, sözcüklerden bul */
    if (!konum.gecerli && toplam_hata > 0) {
        konum = ilk_hata_konumu_bul(&sc);
    }

    konum.gecerli = toplam_hata > 0 ? 1 : 0;

    /* Temizle */
    stderr = eski_stderr;
    fclose(gecici);
    unlink(gecici_yol);

    sözcük_serbest(&sc);
    arena_serbest(&arena);
    hata_sayisi = 0;
    hata_kaynak = NULL;

    /* Hata tipini kaydet */
    if (toplam_hata > 0 && !konum.gecerli) {
        konum.gecerli = 1;
    }

    /* Hata bilgisini sakla: sozdizimi_hatasi vs anlam_hatasi */
    if (sozdizimi_hatasi > 0) {
        if (konum.mesaj[0] == '\0')
            snprintf(konum.mesaj, sizeof(konum.mesaj),
                     "S\xc3\xb6zdizimi hatas\xc4\xb1");
        /* severity 1 = error, mark with negative satir trick - no, use a flag */
    } else if (anlam_hatasi > 0) {
        if (konum.mesaj[0] == '\0')
            snprintf(konum.mesaj, sizeof(konum.mesaj),
                     "Anlam hatas\xc4\xb1");
    }

    return konum;
}

/* ========== Tanılama (Diagnostics) ========== */

static void tanilamalar_gonder(const char *uri, const char *icerik) {
    /* Hata konumunu yakala */
    HataKonumu konum = stderr_hata_konumu_yakala(icerik);

    Metin json;
    metin_baslat(&json);
    metin_ekle(&json, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{");
    metin_ekle(&json, "\"uri\":\"");
    metin_ekle(&json, uri);
    metin_ekle(&json, "\",\"diagnostics\":[");

    if (konum.gecerli) {
        char konum_buf[128];
        int bitis_sutun = konum.sutun + 20;

        metin_ekle(&json, "{\"range\":{\"start\":{\"line\":");
        snprintf(konum_buf, sizeof(konum_buf), "%d", konum.satir);
        metin_ekle(&json, konum_buf);
        metin_ekle(&json, ",\"character\":");
        snprintf(konum_buf, sizeof(konum_buf), "%d", konum.sutun);
        metin_ekle(&json, konum_buf);
        metin_ekle(&json, "},\"end\":{\"line\":");
        snprintf(konum_buf, sizeof(konum_buf), "%d", konum.satir);
        metin_ekle(&json, konum_buf);
        metin_ekle(&json, ",\"character\":");
        snprintf(konum_buf, sizeof(konum_buf), "%d", bitis_sutun);
        metin_ekle(&json, konum_buf);
        metin_ekle(&json, "}},\"severity\":1,\"source\":\"tonyukuk\",\"message\":\"");

        /* Mesajı JSON-safe olarak yaz */
        if (konum.mesaj[0] != '\0') {
            json_string_yaz(&json, konum.mesaj);
        } else {
            metin_ekle(&json, "Derleme hatas\xc4\xb1");
        }
        metin_ekle(&json, "\"}");
    }

    metin_ekle(&json, "]}}");

    lsp_gonder(json.veri);
    metin_serbest(&json);
}

/* ========== Tanıma Git (Go-to-Definition) ========== */

#define MAKS_TANIM 256

typedef struct {
    char isim[128];
    int satir;
    int sutun;
} TanimKonumu;

static TanimKonumu tanimlar_listesi[MAKS_TANIM];
static int tanim_sayisi_toplam = 0;

static void tanim_ekle(const char *isim, int satir, int sutun) {
    if (tanim_sayisi_toplam >= MAKS_TANIM || !isim) return;
    TanimKonumu *t = &tanimlar_listesi[tanim_sayisi_toplam++];
    strncpy(t->isim, isim, sizeof(t->isim) - 1);
    t->isim[sizeof(t->isim) - 1] = '\0';
    t->satir = satir > 0 ? satir - 1 : 0;  /* LSP 0-indexed */
    t->sutun = sutun > 0 ? sutun - 1 : 0;
}

static void tanimlari_topla(Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
    case DÜĞÜM_İŞLEV:
        tanim_ekle(d->veri.islev.isim, d->satir, d->sutun);
        break;
    case DÜĞÜM_DEĞİŞKEN:
        tanim_ekle(d->veri.değişken.isim, d->satir, d->sutun);
        break;
    case DÜĞÜM_SINIF:
        tanim_ekle(d->veri.sinif.isim, d->satir, d->sutun);
        break;
    case DÜĞÜM_SAYIM:
        tanim_ekle(d->veri.sayim.isim, d->satir, d->sutun);
        break;
    default:
        break;
    }

    for (int i = 0; i < d->çocuk_sayısı; i++) {
        tanimlari_topla(d->çocuklar[i]);
    }
}

static void tanim_gonder(int id, const char *uri, int satir, int karakter) {
    Dokuman *d = dokuman_bul(uri);
    if (!d || !d->icerik) {
        char yanit[256];
        snprintf(yanit, sizeof(yanit),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", id);
        lsp_gonder(yanit);
        return;
    }

    /* Cursor'daki kelimeyi bul */
    const char *p = d->icerik;
    int mevcut_satir = 0;
    while (*p && mevcut_satir < satir) {
        if (*p == '\n') mevcut_satir++;
        p++;
    }
    const char *satir_baslangic = p;
    int mevcut_kar = 0;
    while (*p && *p != '\n' && mevcut_kar < karakter) {
        mevcut_kar++;
        p++;
    }
    const char *kelime_bas = p;
    while (kelime_bas > satir_baslangic &&
           (isalnum((unsigned char)kelime_bas[-1]) || kelime_bas[-1] == '_'
            || (unsigned char)kelime_bas[-1] >= 0x80))
        kelime_bas--;
    const char *kelime_son = p;
    while (*kelime_son && (isalnum((unsigned char)*kelime_son) || *kelime_son == '_'
           || (unsigned char)*kelime_son >= 0x80))
        kelime_son++;

    int kelime_len = (int)(kelime_son - kelime_bas);
    if (kelime_len <= 0 || kelime_len > 127) {
        char yanit[256];
        snprintf(yanit, sizeof(yanit),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", id);
        lsp_gonder(yanit);
        return;
    }

    char kelime[128];
    memcpy(kelime, kelime_bas, kelime_len);
    kelime[kelime_len] = '\0';

    /* AST'den tanımları topla */
    int onceki_hata_sayisi = hata_sayisi;
    hata_sayisi = 0;
    const char *onceki_dosya = hata_dosya_adi;
    const char *onceki_kaynak = hata_kaynak;
    hata_dosya_adi = "kaynak";
    hata_kaynak = d->icerik;

    /* stderr'i bastır */
    FILE *eski_stderr = stderr;
    stderr = fopen("/dev/null", "w");

    Arena arena;
    arena_baslat(&arena);

    SözcükÇözümleyici sc;
    sözcük_çözümle(&sc, d->icerik);

    Düğüm *program = NULL;
    if (hata_sayisi == 0) {
        Cozumleyici coz;
        program = cozumle(&coz, sc.sozcukler, sc.sozcuk_sayisi, &arena);
    }

    fclose(stderr);
    stderr = eski_stderr;

    tanim_sayisi_toplam = 0;
    if (program) {
        tanimlari_topla(program);
    }

    /* Kelimeyi tanımlarda ara */
    TanimKonumu *bulunan = NULL;
    for (int i = 0; i < tanim_sayisi_toplam; i++) {
        if (strcmp(tanimlar_listesi[i].isim, kelime) == 0) {
            bulunan = &tanimlar_listesi[i];
            break;
        }
    }

    sözcük_serbest(&sc);
    arena_serbest(&arena);
    hata_sayisi = onceki_hata_sayisi;
    hata_dosya_adi = onceki_dosya;
    hata_kaynak = onceki_kaynak;

    if (bulunan) {
        Metin json;
        metin_baslat(&json);
        metin_ekle(&json, "{\"jsonrpc\":\"2.0\",\"id\":");
        metin_ekle_sayi(&json, id);
        metin_ekle(&json, ",\"result\":{\"uri\":\"");
        json_string_yaz(&json, uri);
        metin_ekle(&json, "\",\"range\":{\"start\":{\"line\":");
        char num[32];
        snprintf(num, sizeof(num), "%d", bulunan->satir);
        metin_ekle(&json, num);
        metin_ekle(&json, ",\"character\":");
        snprintf(num, sizeof(num), "%d", bulunan->sutun);
        metin_ekle(&json, num);
        metin_ekle(&json, "},\"end\":{\"line\":");
        snprintf(num, sizeof(num), "%d", bulunan->satir);
        metin_ekle(&json, num);
        metin_ekle(&json, ",\"character\":");
        snprintf(num, sizeof(num), "%d", bulunan->sutun + kelime_len);
        metin_ekle(&json, num);
        metin_ekle(&json, "}}}}");
        lsp_gonder(json.veri);
        metin_serbest(&json);
    } else {
        char yanit[256];
        snprintf(yanit, sizeof(yanit),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", id);
        lsp_gonder(yanit);
    }
}

/* ========== Tamamlama (Completion) ========== */

typedef struct {
    const char *etiket;
    int tur;          /* LSP CompletionItemKind: 14=Keyword, 3=Function */
    const char *detay;
} TamamlamaOgesi;

static void tamamlama_gonder(int id) {
    /* Anahtar kelimeler (kind=14) ve yerleşik fonksiyonlar (kind=3) */
    static const TamamlamaOgesi ogeler[] = {
        /* === Anahtar kelimeler (kind=14) === */
        {"islev",       14, "anahtar kelime"},
        {"değişken",    14, "anahtar kelime"},
        {"eger",        14, "anahtar kelime"},
        {"degilse",     14, "anahtar kelime"},
        {"yoksa",       14, "anahtar kelime"},
        {"ise",         14, "anahtar kelime"},
        {"son",         14, "anahtar kelime"},
        {"dongu",       14, "anahtar kelime"},
        {"iken",        14, "anahtar kelime"},
        {"dondur",      14, "anahtar kelime"},
        {"sinif",       14, "anahtar kelime"},
        {"yeni",        14, "anahtar kelime"},
        {"bu",          14, "anahtar kelime"},
        {"taban",       14, "anahtar kelime"},
        {"her",         14, "anahtar kelime"},
        {"icin",        14, "anahtar kelime"},
        {"kullan",      14, "anahtar kelime"},
        {"esle",        14, "anahtar kelime"},
        {"durum",       14, "anahtar kelime"},
        {"varsayilan",  14, "anahtar kelime"},
        {"kir",         14, "anahtar kelime"},
        {"devam",       14, "anahtar kelime"},
        {"dene",        14, "anahtar kelime"},
        {"yakala",      14, "anahtar kelime"},
        {"firlat",      14, "anahtar kelime"},
        {"sayim",       14, "anahtar kelime"},
        {"arayuz",      14, "anahtar kelime"},
        {"test",        14, "anahtar kelime"},
        {"dogrula",     14, "anahtar kelime"},
        {"tam",         14, "anahtar kelime"},
        {"ondalik",     14, "anahtar kelime"},
        {"metin",       14, "anahtar kelime"},
        {"mantik",      14, "anahtar kelime"},
        {"bosluk",      14, "anahtar kelime"},
        {"dogru",       14, "anahtar kelime"},
        {"yanlis",      14, "anahtar kelime"},
        {"ve",          14, "anahtar kelime"},
        {"veya",        14, "anahtar kelime"},
        {"degil",       14, "anahtar kelime"},
        {"sabit",       14, "anahtar kelime"},
        {"genel",       14, "anahtar kelime"},
        {"soyut",       14, "anahtar kelime"},
        {"uygula",      14, "anahtar kelime"},
        {"eszamansiz",  14, "anahtar kelime"},
        {"bekle",       14, "anahtar kelime"},
        {"uretec",      14, "anahtar kelime"},
        {"uret",        14, "anahtar kelime"},

        /* === G/Ç Fonksiyonları (kind=3) === */
        {"yazd\xc4\xb1r",       3, "islev(deger: herhangi) -> bosluk"},
        {"satiroku",             3, "islev() -> metin"},

        /* === Dizi Fonksiyonları (kind=3) === */
        {"uzunluk",              3, "islev(dizi: dizi) -> tam"},
        {"ekle",                 3, "islev(dizi: dizi, deger: herhangi) -> bosluk"},
        {"\xc3\xa7\xc4\xb1kar", 3, "islev(dizi: dizi, indeks: tam) -> herhangi"},
        {"s\xc4\xb1rala",       3, "islev(dizi: dizi) -> dizi"},
        {"birle\xc5\x9ftir_dizi", 3, "islev(dizi1: dizi, dizi2: dizi) -> dizi"},
        {"e\xc5\x9fle",         3, "islev(dizi: dizi, f: islev) -> dizi"},
        {"filtre",               3, "islev(dizi: dizi, f: islev) -> dizi"},
        {"indirge",              3, "islev(dizi: dizi, f: islev, başlangıç: herhangi) -> herhangi"},

        /* === Metin Fonksiyonları (kind=3) === */
        {"k\xc4\xb1rp",         3, "islev(metin: metin) -> metin"},
        {"tersle",               3, "islev(metin: metin) -> metin"},
        {"tekrarla",             3, "islev(metin: metin, sayi: tam) -> metin"},
        {"ba\xc5\x9flar_m\xc4\xb1", 3, "islev(metin: metin, onek: metin) -> mantik"},
        {"biter_mi",             3, "islev(metin: metin, sonek: metin) -> mantik"},
        {"de\xc4\x9fi\xc5\x9ftir", 3, "islev(metin: metin, eski: metin, yeni: metin) -> metin"},
        {"b\xc3\xbcy\xc3\xbck_harf", 3, "islev(metin: metin) -> metin"},
        {"k\xc3\xbc\xc3\xa7\xc3\xbck_harf", 3, "islev(metin: metin) -> metin"},
        {"b\xc3\xb6l",          3, "islev(metin: metin, ayrac: metin) -> dizi"},
        {"birle\xc5\x9ftir_metin", 3, "islev(dizi: dizi, ayrac: metin) -> metin"},
        {"metin_uzunluk",        3, "islev(metin: metin) -> tam"},

        /* === Matematik Fonksiyonları (kind=3) === */
        {"sin",                  3, "islev(x: ondalik) -> ondalik"},
        {"cos",                  3, "islev(x: ondalik) -> ondalik"},
        {"tan",                  3, "islev(x: ondalik) -> ondalik"},
        {"log",                  3, "islev(x: ondalik) -> ondalik"},
        {"log10",                3, "islev(x: ondalik) -> ondalik"},
        {"\xc3\xbcst",          3, "islev(taban: ondalik, us: ondalik) -> ondalik"},
        {"karek\xc3\xb6k",      3, "islev(x: ondalik) -> ondalik"},
        {"mutlak",               3, "islev(x: ondalik) -> ondalik"},
        {"pi",                   3, "islev() -> ondalik"},
        {"rastgele",             3, "islev() -> ondalik"},
        {"min",                  3, "islev(a: ondalik, b: ondalik) -> ondalik"},
        {"maks",                 3, "islev(a: ondalik, b: ondalik) -> ondalik"},
        {"taban",                3, "islev(x: ondalik) -> tam"},
        {"tavan",                3, "islev(x: ondalik) -> tam"},
        {"yuvarla",              3, "islev(x: ondalik) -> tam"},
        {"mod",                  3, "islev(a: tam, b: tam) -> tam"},

        /* === Zaman Fonksiyonları (kind=3) === */
        {"\xc5\x9fimdi",        3, "islev() -> tam"},
        {"saat",                 3, "islev() -> tam"},
        {"dakika",               3, "islev() -> tam"},
        {"saniye",               3, "islev() -> tam"},
        {"g\xc3\xbcn",          3, "islev() -> tam"},
        {"ay",                   3, "islev() -> tam"},
        {"y\xc4\xb1l",          3, "islev() -> tam"},
        {"tarih_metin",          3, "islev() -> metin"},

        /* === Dosya Fonksiyonları (kind=3) === */
        {"dosya_oku",            3, "islev(yol: metin) -> metin"},
        {"dosya_yaz",            3, "islev(yol: metin, icerik: metin) -> bosluk"},
        {"dosya_ekle",           3, "islev(yol: metin, icerik: metin) -> bosluk"},
        {"dosya_sat\xc4\xb1rlar", 3, "islev(yol: metin) -> dizi"},

        /* === JSON / HTTP Fonksiyonları (kind=3) === */
        {"json_\xc3\xa7\xc3\xb6z\xc3\xbcmle", 3, "islev(metin: metin) -> herhangi"},
        {"http_al",              3, "islev(url: metin) -> metin"},

        /* === Kriptografi Fonksiyonları (kind=3) === */
        {"md5",                  3, "islev(metin: metin) -> metin"},
        {"sha256",               3, "islev(metin: metin) -> metin"},
        {"base64_kodla",         3, "islev(metin: metin) -> metin"},
        {"base64_\xc3\xa7\xc3\xb6z", 3, "islev(metin: metin) -> metin"},

        /* === Regex Fonksiyonları (kind=3) === */
        {"e\xc5\x9fle\xc5\x9fir_mi", 3, "islev(metin: metin, desen: metin) -> mantik"},
        {"e\xc5\x9fle\xc5\x9fme_bul", 3, "islev(metin: metin, desen: metin) -> dizi"},

        /* === Sözlük Fonksiyonları (kind=3) === */
        {"s\xc3\xb6zl\xc3\xbck_yeni",       3, "islev() -> sozluk"},
        {"s\xc3\xb6zl\xc3\xbck_ekle",       3, "islev(s: sozluk, anahtar: metin, deger: herhangi) -> bosluk"},
        {"s\xc3\xb6zl\xc3\xbck_oku",        3, "islev(s: sozluk, anahtar: metin) -> herhangi"},
        {"s\xc3\xb6zl\xc3\xbck_var_m\xc4\xb1", 3, "islev(s: sozluk, anahtar: metin) -> mantik"},
        {"s\xc3\xb6zl\xc3\xbck_sil",        3, "islev(s: sozluk, anahtar: metin) -> bosluk"},
        {"s\xc3\xb6zl\xc3\xbck_anahtarlar", 3, "islev(s: sozluk) -> dizi"},
        {"s\xc3\xb6zl\xc3\xbck_uzunluk",    3, "islev(s: sozluk) -> tam"},

        /* === Tip Dönüşüm Fonksiyonları (kind=3) === */
        {"tam_metin",            3, "islev(deger: tam) -> metin"},
        {"metin_tam",            3, "islev(deger: metin) -> tam"},
        {"ondal\xc4\xb1k_metin", 3, "islev(deger: ondalik) -> metin"},
        {"metin_ondal\xc4\xb1k", 3, "islev(deger: metin) -> ondalik"},

        {NULL, 0, NULL}
    };

    Metin json;
    metin_baslat(&json);

    char başlangıç[256];
    snprintf(başlangıç, sizeof(başlangıç),
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"isIncomplete\":false,\"items\":[", id);
    metin_ekle(&json, başlangıç);

    for (int i = 0; ogeler[i].etiket; i++) {
        if (i > 0) metin_ekle(&json, ",");
        metin_ekle(&json, "{\"label\":\"");
        json_string_yaz(&json, ogeler[i].etiket);
        metin_ekle(&json, "\",\"kind\":");
        char tur_buf[8];
        snprintf(tur_buf, sizeof(tur_buf), "%d", ogeler[i].tur);
        metin_ekle(&json, tur_buf);
        metin_ekle(&json, ",\"detail\":\"");
        json_string_yaz(&json, ogeler[i].detay);
        metin_ekle(&json, "\"}");
    }

    metin_ekle(&json, "]}}");
    lsp_gonder(json.veri);
    metin_serbest(&json);
}

/* ========== Üzerine Gelme (Hover) ========== */

static void hover_gonder(int id, const char *uri, int satir, int karakter) {
    Dokuman *d = dokuman_bul(uri);
    if (!d || !d->icerik) {
        char yanit[256];
        snprintf(yanit, sizeof(yanit),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", id);
        lsp_gonder(yanit);
        return;
    }

    /* Satırdaki kelimeyi bul */
    const char *p = d->icerik;
    int mevcut_satir = 0;
    while (*p && mevcut_satir < satir) {
        if (*p == '\n') mevcut_satir++;
        p++;
    }

    /* Satır başından karaktere ilerle */
    const char *satir_baslangic = p;
    int mevcut_kar = 0;
    while (*p && *p != '\n' && mevcut_kar < karakter) {
        mevcut_kar++;
        p++;
    }

    /* Kelime sınırlarını bul (UTF-8 uyumlu: yüksek byte'lar da kelime parçası) */
    const char *kelime_bas = p;
    while (kelime_bas > satir_baslangic &&
           (isalnum((unsigned char)kelime_bas[-1]) || kelime_bas[-1] == '_'
            || (unsigned char)kelime_bas[-1] >= 0x80))
        kelime_bas--;

    const char *kelime_son = p;
    while (*kelime_son && (isalnum((unsigned char)*kelime_son) || *kelime_son == '_'
           || (unsigned char)*kelime_son >= 0x80))
        kelime_son++;

    int kelime_len = (int)(kelime_son - kelime_bas);
    if (kelime_len <= 0 || kelime_len > 128) {
        char yanit[256];
        snprintf(yanit, sizeof(yanit),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", id);
        lsp_gonder(yanit);
        return;
    }

    char kelime[129];
    memcpy(kelime, kelime_bas, kelime_len);
    kelime[kelime_len] = '\0';

    /* Anahtar kelime ve fonksiyon bilgileri */
    static const struct { const char *isim; const char *aciklama; } bilgiler[] = {
        /* --- Anahtar kelimeler --- */
        {"islev",     "Fonksiyon tan\xc4\xb1m\xc4\xb1"},
        {"değişken",  "De\xc4\x9fi\xc5\x9fken tan\xc4\xb1m\xc4\xb1"},
        {"eger",      "Ko\xc5\x9ful ifadesi"},
        {"dongu",     "D\xc3\xb6ng\xc3\xbc"},
        {"iken",      "While d\xc3\xb6ng\xc3\xbcs\xc3\xbc"},
        {"sinif",     "S\xc4\xb1n\xc4\xb1" "f tan\xc4\xb1m\xc4\xb1"},
        {"dondur",    "Fonksiyondan de\xc4\x9f" "er d\xc3\xb6nd\xc3\xbcr"},
        {"kullan",    "Mod\xc3\xbcl i\xc3\xa7" "e aktar"},
        {"test",      "Test blo\xc4\x9fu tan\xc4\xb1m\xc4\xb1"},
        {"dogrula",   "Test do\xc4\x9frulama (assertion)"},
        {"tam",       "Tam say\xc4\xb1 tipi (64-bit)"},
        {"ondalik",   "Ondal\xc4\xb1k say\xc4\xb1 tipi (double)"},
        {"metin",     "Metin tipi (string)"},
        {"mantik",    "Mant\xc4\xb1ksal tip (bool)"},
        {"bosluk",    "Bo\xc5\x9f d\xc3\xb6n\xc3\xbc\xc5\x9f tipi (void)"},
        {"sabit",     "Sabit de\xc4\x9fi\xc5\x9fken tan\xc4\xb1m\xc4\xb1"},
        {"genel",     "Genel (public) eri\xc5\x9fim belirteci"},
        {"soyut",     "Soyut s\xc4\xb1n\xc4\xb1" "f/metot tan\xc4\xb1m\xc4\xb1"},
        {"arayuz",    "Aray\xc3\xbcz tan\xc4\xb1m\xc4\xb1 (interface)"},
        {"uygula",    "Aray\xc3\xbcz uygulama (implements)"},
        {"esle",      "E\xc5\x9fle\xc5\x9ftirme (match/switch)"},
        {"durum",     "E\xc5\x9fle\xc5\x9ftirme durumu (case)"},
        {"varsayilan", "Varsay\xc4\xb1lan durum (default)"},
        {"kir",       "D\xc3\xb6ng\xc3\xbc" "den \xc3\xa7\xc4\xb1" "k (break)"},
        {"devam",     "Sonraki iterasyona ge\xc3\xa7 (continue)"},
        {"dene",      "Hata yakalama blo\xc4\x9fu (try)"},
        {"yakala",    "Hata i\xc5\x9fleme blo\xc4\x9fu (catch)"},
        {"firlat",    "Hata f\xc4\xb1rlat (throw)"},
        {"sayim",     "Say\xc4\xb1m tipi (enum)"},
        {"dogru",     "Mant\xc4\xb1ksal do\xc4\x9fru de\xc4\x9f" "eri (true)"},
        {"yanlis",    "Mant\xc4\xb1ksal yanl\xc4\xb1\xc5\x9f de\xc4\x9f" "eri (false)"},
        {"ve",        "Mant\xc4\xb1ksal VE operat\xc3\xb6r\xc3\xbc (&&)"},
        {"veya",      "Mant\xc4\xb1ksal VEYA operat\xc3\xb6r\xc3\xbc (||)"},
        {"degil",     "Mant\xc4\xb1ksal DE\xc4\x9e\xc4\xb0L operat\xc3\xb6r\xc3\xbc (!)"},
        {"eszamansiz", "E\xc5\x9fzamans\xc4\xb1z fonksiyon (async)"},
        {"bekle",     "E\xc5\x9fzamans\xc4\xb1z bekleme (await)"},
        {"uretec",    "\xc3\x9crete\xc3\xa7 fonksiyon (generator)"},
        {"uret",      "\xc3\x9crete\xc3\xa7" "ten de\xc4\x9f" "er \xc3\xbcret (yield)"},
        {"her",       "For-each d\xc3\xb6ng\xc3\xbcs\xc3\xbc"},
        {"icin",      "D\xc3\xb6ng\xc3\xbc y\xc4\xb1neleyici (in)"},
        {"yoksa",     "Alternatif ko\xc5\x9ful dal\xc4\xb1 (else if)"},
        {"ise",       "Ko\xc5\x9ful blo\xc4\x9fu ba\xc5\x9flang\xc4\xb1" "c\xc4\xb1 (then)"},
        {"son",       "Blok sonu"},
        {"yeni",      "Nesne olu\xc5\x9fturma (new)"},
        {"bu",        "Mevcut nesne referans\xc4\xb1 (this)"},

        /* --- G/Ç Fonksiyonları --- */
        {"yazd\xc4\xb1r",
            "`yazd\xc4\xb1r(de\xc4\x9f" "er: herhangi) -> bosluk`\\n\\n"
            "Standart \xc3\xa7\xc4\xb1kt\xc4\xb1ya de\xc4\x9f" "er yazar."},
        {"yazdir",
            "`yazdir(de\xc4\x9f" "er: herhangi) -> bosluk`\\n\\n"
            "Standart \xc3\xa7\xc4\xb1kt\xc4\xb1ya de\xc4\x9f" "er yazar."},
        {"satiroku",
            "`satiroku() -> metin`\\n\\n"
            "Standart girdiden bir sat\xc4\xb1r okur ve metin olarak d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},

        /* --- Dizi Fonksiyonları --- */
        {"uzunluk",
            "`uzunluk(dizi: dizi) -> tam`\\n\\n"
            "Dizi veya metin uzunlu\xc4\x9funu d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"ekle",
            "`ekle(dizi: dizi, deger: herhangi) -> bosluk`\\n\\n"
            "Dizinin sonuna yeni eleman ekler."},
        {"\xc3\xa7\xc4\xb1kar",
            "`\xc3\xa7\xc4\xb1kar(dizi: dizi, indeks: tam) -> herhangi`\\n\\n"
            "Belirtilen indeksteki eleman\xc4\xb1 \xc3\xa7\xc4\xb1kar\xc4\xb1p d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"s\xc4\xb1rala",
            "`s\xc4\xb1rala(dizi: dizi) -> dizi`\\n\\n"
            "Diziyi s\xc4\xb1ralayarak yeni dizi d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"birle\xc5\x9ftir_dizi",
            "`birle\xc5\x9ftir_dizi(dizi1: dizi, dizi2: dizi) -> dizi`\\n\\n"
            "\xc4\xb0ki diziyi birle\xc5\x9ftirip yeni dizi d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"e\xc5\x9fle",
            "`e\xc5\x9fle(dizi: dizi, f: islev) -> dizi`\\n\\n"
            "Her elemana fonksiyon uygulayarak yeni dizi olu\xc5\x9fturur (map)."},
        {"filtre",
            "`filtre(dizi: dizi, f: islev) -> dizi`\\n\\n"
            "Ko\xc5\x9fula uyan elemanlardan yeni dizi olu\xc5\x9fturur (filter)."},
        {"indirge",
            "`indirge(dizi: dizi, f: islev, başlangıç: herhangi) -> herhangi`\\n\\n"
            "Diziyi tek de\xc4\x9f" "ere indirger (reduce)."},

        /* --- Metin Fonksiyonları --- */
        {"k\xc4\xb1rp",
            "`k\xc4\xb1rp(metin: metin) -> metin`\\n\\n"
            "Metnin ba\xc5\x9f\xc4\xb1ndaki ve sonundaki bo\xc5\x9fluklar\xc4\xb1 siler."},
        {"tersle",
            "`tersle(metin: metin) -> metin`\\n\\n"
            "Metni tersine \xc3\xa7" "evirir."},
        {"tekrarla",
            "`tekrarla(metin: metin, sayi: tam) -> metin`\\n\\n"
            "Metni belirtilen say\xc4\xb1" "da tekrarlar."},
        {"ba\xc5\x9flar_m\xc4\xb1",
            "`ba\xc5\x9flar_m\xc4\xb1(metin: metin, onek: metin) -> mantik`\\n\\n"
            "Metin belirtilen \xc3\xb6nekle ba\xc5\x9fl\xc4\xb1yor mu kontrol eder."},
        {"biter_mi",
            "`biter_mi(metin: metin, sonek: metin) -> mantik`\\n\\n"
            "Metin belirtilen sonekle bitiyor mu kontrol eder."},
        {"de\xc4\x9fi\xc5\x9ftir",
            "`de\xc4\x9fi\xc5\x9ftir(metin: metin, eski: metin, yeni: metin) -> metin`\\n\\n"
            "Metindeki eski k\xc4\xb1sm\xc4\xb1 yenisiyle de\xc4\x9fi\xc5\x9ftirir."},
        {"b\xc3\xbcy\xc3\xbck_harf",
            "`b\xc3\xbcy\xc3\xbck_harf(metin: metin) -> metin`\\n\\n"
            "Metni b\xc3\xbcy\xc3\xbck harfe \xc3\xa7" "evirir."},
        {"k\xc3\xbc\xc3\xa7\xc3\xbck_harf",
            "`k\xc3\xbc\xc3\xa7\xc3\xbck_harf(metin: metin) -> metin`\\n\\n"
            "Metni k\xc3\xbc\xc3\xa7\xc3\xbck harfe \xc3\xa7" "evirir."},
        {"b\xc3\xb6l",
            "`b\xc3\xb6l(metin: metin, ayrac: metin) -> dizi`\\n\\n"
            "Metni ayrac\xc4\xb1na g\xc3\xb6re b\xc3\xb6ler ve dizi d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"birle\xc5\x9ftir_metin",
            "`birle\xc5\x9ftir_metin(dizi: dizi, ayrac: metin) -> metin`\\n\\n"
            "Dizi elemanlar\xc4\xb1n\xc4\xb1 ayrac\xc4\xb1yla birle\xc5\x9ftirip metin d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"metin_uzunluk",
            "`metin_uzunluk(metin: metin) -> tam`\\n\\n"
            "Metnin karakter say\xc4\xb1s\xc4\xb1n\xc4\xb1 d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},

        /* --- Matematik Fonksiyonları --- */
        {"sin",
            "`sin(x: ondalik) -> ondalik`\\n\\n"
            "Sin\xc3\xbcs de\xc4\x9f" "eri hesaplar (radyan)."},
        {"cos",
            "`cos(x: ondalik) -> ondalik`\\n\\n"
            "Kosin\xc3\xbcs de\xc4\x9f" "eri hesaplar (radyan)."},
        {"tan",
            "`tan(x: ondalik) -> ondalik`\\n\\n"
            "Tanj\xc3\xa4nt de\xc4\x9f" "eri hesaplar (radyan)."},
        {"log",
            "`log(x: ondalik) -> ondalik`\\n\\n"
            "Do\xc4\x9f" "al logaritma hesaplar (ln)."},
        {"log10",
            "`log10(x: ondalik) -> ondalik`\\n\\n"
            "10 tabanl\xc4\xb1 logaritma hesaplar."},
        {"\xc3\xbcst",
            "`\xc3\xbcst(taban: ondalik, us: ondalik) -> ondalik`\\n\\n"
            "\xc3\x9cs alma i\xc5\x9flemi yapar (pow)."},
        {"karek\xc3\xb6k",
            "`karek\xc3\xb6k(x: ondalik) -> ondalik`\\n\\n"
            "Karek\xc3\xb6k hesaplar (sqrt)."},
        {"mutlak",
            "`mutlak(x: ondalik) -> ondalik`\\n\\n"
            "Mutlak de\xc4\x9f" "er d\xc3\xb6nd\xc3\xbcr\xc3\xbcr (abs)."},
        {"pi",
            "`pi() -> ondalik`\\n\\n"
            "Pi say\xc4\xb1s\xc4\xb1n\xc4\xb1 d\xc3\xb6nd\xc3\xbcr\xc3\xbcr (3.14159...)."},
        {"rastgele",
            "`rastgele() -> ondalik`\\n\\n"
            "0 ile 1 aras\xc4\xb1nda rastgele say\xc4\xb1 \xc3\xbcretir."},
        {"min",
            "`min(a: ondalik, b: ondalik) -> ondalik`\\n\\n"
            "\xc4\xb0ki de\xc4\x9f" "erden k\xc3\xbc\xc3\xa7\xc3\xbc\xc4\x9f\xc3\xbc d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"maks",
            "`maks(a: ondalik, b: ondalik) -> ondalik`\\n\\n"
            "\xc4\xb0ki de\xc4\x9f" "erden b\xc3\xbcy\xc3\xbc\xc4\x9f\xc3\xbc d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"tavan",
            "`tavan(x: ondalik) -> tam`\\n\\n"
            "Yukar\xc4\xb1 yuvarlama yapar (ceil)."},
        {"yuvarla",
            "`yuvarla(x: ondalik) -> tam`\\n\\n"
            "En yak\xc4\xb1n tam say\xc4\xb1ya yuvarlar (round)."},
        {"mod",
            "`mod(a: tam, b: tam) -> tam`\\n\\n"
            "B\xc3\xb6lme kalan\xc4\xb1n\xc4\xb1 d\xc3\xb6nd\xc3\xbcr\xc3\xbcr (modulo)."},

        /* --- Zaman Fonksiyonları --- */
        {"\xc5\x9fimdi",
            "`\xc5\x9fimdi() -> tam`\\n\\n"
            "Unix zaman damgas\xc4\xb1n\xc4\xb1 d\xc3\xb6nd\xc3\xbcr\xc3\xbcr (epoch saniye)."},
        {"saat",
            "`saat() -> tam`\\n\\n"
            "Mevcut saati d\xc3\xb6nd\xc3\xbcr\xc3\xbcr (0-23)."},
        {"dakika",
            "`dakika() -> tam`\\n\\n"
            "Mevcut dakikay\xc4\xb1 d\xc3\xb6nd\xc3\xbcr\xc3\xbcr (0-59)."},
        {"saniye",
            "`saniye() -> tam`\\n\\n"
            "Mevcut saniyeyi d\xc3\xb6nd\xc3\xbcr\xc3\xbcr (0-59)."},
        {"g\xc3\xbcn",
            "`g\xc3\xbcn() -> tam`\\n\\n"
            "Ay\xc4\xb1n g\xc3\xbcn\xc3\xbcn\xc3\xbc d\xc3\xb6nd\xc3\xbcr\xc3\xbcr (1-31)."},
        {"ay",
            "`ay() -> tam`\\n\\n"
            "Mevcut ay\xc4\xb1 d\xc3\xb6nd\xc3\xbcr\xc3\xbcr (1-12)."},
        {"y\xc4\xb1l",
            "`y\xc4\xb1l() -> tam`\\n\\n"
            "Mevcut y\xc4\xb1l\xc4\xb1 d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"tarih_metin",
            "`tarih_metin() -> metin`\\n\\n"
            "Mevcut tarih ve saati metin olarak d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},

        /* --- Dosya Fonksiyonları --- */
        {"dosya_oku",
            "`dosya_oku(yol: metin) -> metin`\\n\\n"
            "Dosyan\xc4\xb1n t\xc3\xbcm i\xc3\xa7" "eri\xc4\x9fini metin olarak okur."},
        {"dosya_yaz",
            "`dosya_yaz(yol: metin, icerik: metin) -> bosluk`\\n\\n"
            "Dosyaya i\xc3\xa7" "erik yazar (\xc3\xbcstne yazar)."},
        {"dosya_ekle",
            "`dosya_ekle(yol: metin, icerik: metin) -> bosluk`\\n\\n"
            "Dosyan\xc4\xb1n sonuna i\xc3\xa7" "erik ekler."},
        {"dosya_sat\xc4\xb1rlar",
            "`dosya_sat\xc4\xb1rlar(yol: metin) -> dizi`\\n\\n"
            "Dosyay\xc4\xb1 sat\xc4\xb1r sat\xc4\xb1r okuyup dizi olarak d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},

        /* --- JSON / HTTP --- */
        {"json_\xc3\xa7\xc3\xb6z\xc3\xbcmle",
            "`json_\xc3\xa7\xc3\xb6z\xc3\xbcmle(metin: metin) -> herhangi`\\n\\n"
            "JSON metnini ayr\xc4\xb1\xc5\x9ft\xc4\xb1r\xc4\xb1p de\xc4\x9f" "er d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"http_al",
            "`http_al(url: metin) -> metin`\\n\\n"
            "HTTP GET iste\xc4\x9fi g\xc3\xb6nderir ve yan\xc4\xb1t\xc4\xb1 d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},

        /* --- Kriptografi --- */
        {"md5",
            "`md5(metin: metin) -> metin`\\n\\n"
            "MD5 \xc3\xb6zet de\xc4\x9f" "eri hesaplar."},
        {"sha256",
            "`sha256(metin: metin) -> metin`\\n\\n"
            "SHA-256 \xc3\xb6zet de\xc4\x9f" "eri hesaplar."},
        {"base64_kodla",
            "`base64_kodla(metin: metin) -> metin`\\n\\n"
            "Metni Base64 format\xc4\xb1na kodlar."},
        {"base64_\xc3\xa7\xc3\xb6z",
            "`base64_\xc3\xa7\xc3\xb6z(metin: metin) -> metin`\\n\\n"
            "Base64 kodlanm\xc4\xb1\xc5\x9f metni \xc3\xa7\xc3\xb6zer."},

        /* --- Regex --- */
        {"e\xc5\x9fle\xc5\x9fir_mi",
            "`e\xc5\x9fle\xc5\x9fir_mi(metin: metin, desen: metin) -> mantik`\\n\\n"
            "Metin d\xc3\xbczenli ifadeyle e\xc5\x9fle\xc5\x9fiyor mu kontrol eder."},
        {"e\xc5\x9fle\xc5\x9fme_bul",
            "`e\xc5\x9fle\xc5\x9fme_bul(metin: metin, desen: metin) -> dizi`\\n\\n"
            "D\xc3\xbczenli ifade e\xc5\x9fle\xc5\x9fmelerini dizi olarak d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},

        /* --- Sözlük Fonksiyonları --- */
        {"s\xc3\xb6zl\xc3\xbck_yeni",
            "`s\xc3\xb6zl\xc3\xbck_yeni() -> sozluk`\\n\\n"
            "Yeni bo\xc5\x9f s\xc3\xb6zl\xc3\xbck olu\xc5\x9fturur."},
        {"s\xc3\xb6zl\xc3\xbck_ekle",
            "`s\xc3\xb6zl\xc3\xbck_ekle(s: sozluk, anahtar: metin, deger: herhangi) -> bosluk`\\n\\n"
            "S\xc3\xb6zl\xc3\xbc\xc4\x9f" "e anahtar-de\xc4\x9f" "er \xc3\xa7ifti ekler."},
        {"s\xc3\xb6zl\xc3\xbck_oku",
            "`s\xc3\xb6zl\xc3\xbck_oku(s: sozluk, anahtar: metin) -> herhangi`\\n\\n"
            "Anahtara kar\xc5\x9f\xc4\xb1l\xc4\xb1k gelen de\xc4\x9f" "eri d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"s\xc3\xb6zl\xc3\xbck_var_m\xc4\xb1",
            "`s\xc3\xb6zl\xc3\xbck_var_m\xc4\xb1(s: sozluk, anahtar: metin) -> mantik`\\n\\n"
            "Anahtar s\xc3\xb6zl\xc3\xbckte var m\xc4\xb1 kontrol eder."},
        {"s\xc3\xb6zl\xc3\xbck_sil",
            "`s\xc3\xb6zl\xc3\xbck_sil(s: sozluk, anahtar: metin) -> bosluk`\\n\\n"
            "S\xc3\xb6zl\xc3\xbckten anahtar\xc4\xb1 siler."},
        {"s\xc3\xb6zl\xc3\xbck_anahtarlar",
            "`s\xc3\xb6zl\xc3\xbck_anahtarlar(s: sozluk) -> dizi`\\n\\n"
            "S\xc3\xb6zl\xc3\xbc\xc4\x9f\xc3\xbcn t\xc3\xbcm anahtarlar\xc4\xb1n\xc4\xb1 dizi olarak d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},
        {"s\xc3\xb6zl\xc3\xbck_uzunluk",
            "`s\xc3\xb6zl\xc3\xbck_uzunluk(s: sozluk) -> tam`\\n\\n"
            "S\xc3\xb6zl\xc3\xbckteki eleman say\xc4\xb1s\xc4\xb1n\xc4\xb1 d\xc3\xb6nd\xc3\xbcr\xc3\xbcr."},

        /* --- Tip Dönüşümleri --- */
        {"tam_metin",
            "`tam_metin(deger: tam) -> metin`\\n\\n"
            "Tam say\xc4\xb1y\xc4\xb1 metne d\xc3\xb6n\xc3\xbc\xc5\x9ft\xc3\xbcr\xc3\xbcr."},
        {"metin_tam",
            "`metin_tam(deger: metin) -> tam`\\n\\n"
            "Metni tam say\xc4\xb1ya d\xc3\xb6n\xc3\xbc\xc5\x9ft\xc3\xbcr\xc3\xbcr."},
        {"ondal\xc4\xb1k_metin",
            "`ondal\xc4\xb1k_metin(deger: ondalik) -> metin`\\n\\n"
            "Ondal\xc4\xb1k say\xc4\xb1y\xc4\xb1 metne d\xc3\xb6n\xc3\xbc\xc5\x9ft\xc3\xbcr\xc3\xbcr."},
        {"metin_ondal\xc4\xb1k",
            "`metin_ondal\xc4\xb1k(deger: metin) -> ondalik`\\n\\n"
            "Metni ondal\xc4\xb1k say\xc4\xb1ya d\xc3\xb6n\xc3\xbc\xc5\x9ft\xc3\xbcr\xc3\xbcr."},

        {NULL, NULL}
    };

    const char *aciklama = NULL;
    for (int i = 0; bilgiler[i].isim; i++) {
        if (strcmp(kelime, bilgiler[i].isim) == 0) {
            aciklama = bilgiler[i].aciklama;
            break;
        }
    }

    if (aciklama) {
        Metin json;
        metin_baslat(&json);
        metin_ekle(&json, "{\"jsonrpc\":\"2.0\",\"id\":");
        metin_ekle_sayi(&json, id);
        metin_ekle(&json, ",\"result\":{\"contents\":{\"kind\":\"markdown\",\"value\":\"**");
        json_string_yaz(&json, kelime);
        metin_ekle(&json, "** - ");
        json_string_yaz(&json, aciklama);
        metin_ekle(&json, "\"}}}");
        lsp_gonder(json.veri);
        metin_serbest(&json);
    } else {
        char yanit[256];
        snprintf(yanit, sizeof(yanit),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", id);
        lsp_gonder(yanit);
    }
}

/* ========== Ana LSP Döngüsü ========== */

int lsp_calistir(void) {
    int kapanacak = 0;

    /* stderr'e log */
    fprintf(stderr, "tonyukuk-lsp: ba\xc5\x9flat\xc4\xb1ld\xc4\xb1\n");

    while (!kapanacak) {
        char *mesaj = lsp_oku();
        if (!mesaj) break;

        /* method alanını bul */
        char method[128];
        method[0] = '\0';
        json_str_bul(mesaj, "method", method, sizeof(method));

        int id = json_int_bul(mesaj, "id");

        if (strcmp(method, "initialize") == 0) {
            char yanit[2048];
            snprintf(yanit, sizeof(yanit),
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{"
                "\"capabilities\":{"
                "\"textDocumentSync\":1,"
                "\"completionProvider\":{\"triggerCharacters\":[\".\"]},"
                "\"hoverProvider\":true,"
                "\"definitionProvider\":true"
                "},"
                "\"serverInfo\":{\"name\":\"tonyukuk-lsp\",\"version\":\"1.0.0\"}"
                "}}", id);
            lsp_gonder(yanit);
            fprintf(stderr, "tonyukuk-lsp: initialize tamamland\xc4\xb1\n");
        }
        else if (strcmp(method, "initialized") == 0) {
            /* Bildirim, yanıt gerekmez */
            fprintf(stderr, "tonyukuk-lsp: initialized al\xc4\xb1nd\xc4\xb1\n");
        }
        else if (strcmp(method, "textDocument/didOpen") == 0) {
            char uri[512], text[65536];
            /* params.textDocument.uri ve params.textDocument.text */
            json_str_bul(mesaj, "uri", uri, sizeof(uri));
            json_str_bul(mesaj, "text", text, sizeof(text));

            Dokuman *d = dokuman_ekle(uri);
            if (d) {
                dokuman_icerik_ayarla(d, text);
                tanilamalar_gonder(uri, text);
            }
            fprintf(stderr, "tonyukuk-lsp: didOpen %s\n", uri);
        }
        else if (strcmp(method, "textDocument/didChange") == 0) {
            char uri[512];
            json_str_bul(mesaj, "uri", uri, sizeof(uri));

            /* contentChanges dizisinden text al (full sync mode) */
            char text[65536];
            json_str_bul(mesaj, "text", text, sizeof(text));

            Dokuman *d = dokuman_bul(uri);
            if (d) {
                dokuman_icerik_ayarla(d, text);
                tanilamalar_gonder(uri, text);
            }
            fprintf(stderr, "tonyukuk-lsp: didChange %s\n", uri);
        }
        else if (strcmp(method, "textDocument/completion") == 0) {
            tamamlama_gonder(id);
        }
        else if (strcmp(method, "textDocument/definition") == 0) {
            char uri[512];
            json_str_bul(mesaj, "uri", uri, sizeof(uri));
            int satir = json_int_bul(mesaj, "line");
            int karakter = json_int_bul(mesaj, "character");
            tanim_gonder(id, uri, satir, karakter);
        }
        else if (strcmp(method, "textDocument/hover") == 0) {
            char uri[512];
            json_str_bul(mesaj, "uri", uri, sizeof(uri));
            int satir = json_int_bul(mesaj, "line");
            int karakter = json_int_bul(mesaj, "character");
            hover_gonder(id, uri, satir, karakter);
        }
        else if (strcmp(method, "shutdown") == 0) {
            char yanit[128];
            snprintf(yanit, sizeof(yanit),
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", id);
            lsp_gonder(yanit);
            fprintf(stderr, "tonyukuk-lsp: shutdown\n");
        }
        else if (strcmp(method, "exit") == 0) {
            kapanacak = 1;
            fprintf(stderr, "tonyukuk-lsp: exit\n");
        }

        free(mesaj);
    }

    /* Doküman belleğini serbest bırak */
    for (int i = 0; i < dokuman_sayisi; i++) {
        if (dokumanlar[i].icerik) free(dokumanlar[i].icerik);
    }

    return 0;
}

/* ========== Ana Program ========== */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return lsp_calistir();
}
