#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "sozcuk.h"
#include "cozumleyici.h"
#include "agac.h"
#include "anlam.h"
#include "uretici.h"
#include "uretici_arm64.h"
#include "uretici_wasm.h"
#include "uretici_avr.h"
#include "uretici_xtensa.h"
#include "uretici_arm_m0.h"
#include "uretici_elf.h"
#include "hata.h"
#include "bellek.h"
#include "optimize.h"
#include "kaynak_harita.h"
#include <limits.h>

/* LLVM backend (opsiyonel, derleme zamanında belirlenir) */
#ifdef LLVM_BACKEND_MEVCUT
#include "llvm_uretici.h"
#include "wasm_kopru.h"
#endif

/* ========== Dosya modülü içe aktarma ========== */

#define MAKS_ICERIK_DOSYA 64
static char *icerik_dosyalar[MAKS_ICERIK_DOSYA];
static int icerik_dosya_sayisi = 0;

static int dosya_zaten_dahil(const char *tam_yol) {
    for (int i = 0; i < icerik_dosya_sayisi; i++) {
        if (strcmp(icerik_dosyalar[i], tam_yol) == 0) return 1;
    }
    return 0;
}

static void dosya_dahil_kaydet(const char *tam_yol) {
    if (icerik_dosya_sayisi < MAKS_ICERIK_DOSYA) {
        icerik_dosyalar[icerik_dosya_sayisi++] = strdup(tam_yol);
    }
}

static int dosya_modulu_mu(const char *modul) {
    if (!modul) return 0;
    int len = (int)strlen(modul);
    return len > 3 && strcmp(modul + len - 3, ".tr") == 0;
}

static void dizin_al(const char *dosya_yolu, char *dizin, int boyut) {
    const char *son_slash = strrchr(dosya_yolu, '/');
    if (son_slash) {
        int dlen = (int)(son_slash - dosya_yolu);
        if (dlen >= boyut) dlen = boyut - 1;
        memcpy(dizin, dosya_yolu, dlen);
        dizin[dlen] = '\0';
    } else {
        snprintf(dizin, boyut, ".");
    }
}

/* Dosya modüllerini işle: kullan "dosya.tr" -> dosyayı oku, çözümle, AST'ye ekle */
static void modulleri_isle(Düğüm *program, const char *kaynak_dosya, Arena *arena) {
    char kaynak_dizin[512];
    dizin_al(kaynak_dosya, kaynak_dizin, sizeof(kaynak_dizin));

    /* Ters sırada işle: ekleme başa yapıldığından doğru sıra korunur */
    for (int i = program->çocuk_sayısı - 1; i >= 0; i--) {
        Düğüm *cocuk = program->çocuklar[i];
        if (cocuk->tur != DÜĞÜM_KULLAN) continue;
        if (!dosya_modulu_mu(cocuk->veri.kullan.modul)) continue;

        /* Tam dosya yolunu oluştur */
        char tam_yol[1024];
        if (cocuk->veri.kullan.modul[0] == '/') {
            snprintf(tam_yol, sizeof(tam_yol), "%s", cocuk->veri.kullan.modul);
        } else {
            snprintf(tam_yol, sizeof(tam_yol), "%s/%s", kaynak_dizin, cocuk->veri.kullan.modul);
        }

        /* Dairesel bağımlılık kontrolü */
        char gercek_yol[PATH_MAX];
        if (realpath(tam_yol, gercek_yol) == NULL) {
            fprintf(stderr, "%s:%d:%d: hata: dosya açılamadı: %s\n",
                    hata_dosya_adi ? hata_dosya_adi : "?",
                    cocuk->satir, cocuk->sutun, tam_yol);
            hata_sayisi++;
            continue;
        }
        if (dosya_zaten_dahil(gercek_yol)) {
            /* Zaten dahil edilmiş, düğümü kaldır */
            for (int j = i; j < program->çocuk_sayısı - 1; j++) {
                program->çocuklar[j] = program->çocuklar[j + 1];
            }
            program->çocuk_sayısı--;
            continue;
        }
        dosya_dahil_kaydet(gercek_yol);

        /* Dosyayı oku */
        FILE *f = fopen(tam_yol, "rb");
        if (!f) {
            fprintf(stderr, "%s:%d:%d: hata: dosya açılamadı: %s\n",
                    hata_dosya_adi ? hata_dosya_adi : "?",
                    cocuk->satir, cocuk->sutun, tam_yol);
            hata_sayisi++;
            continue;
        }
        fseek(f, 0, SEEK_END);
        long boyut = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *modul_kaynak = (char *)malloc(boyut + 1);
        if (fread(modul_kaynak, 1, boyut, f) != (size_t)boyut) {
            free(modul_kaynak);
            fclose(f);
            hata_sayisi++;
            continue;
        }
        modul_kaynak[boyut] = '\0';
        fclose(f);

        /* Sözcük çözümle */
        SözcükÇözümleyici modul_sc;
        sözcük_çözümle(&modul_sc, modul_kaynak);

        if (hata_sayisi > 0) {
            sözcük_serbest(&modul_sc);
            free(modul_kaynak);
            continue;
        }

        /* Çözümle (parse) */
        Cozumleyici modul_coz;
        Düğüm *modul_program = cozumle(&modul_coz, modul_sc.sozcukler, modul_sc.sozcuk_sayisi, arena);

        if (hata_sayisi > 0) {
            sözcük_serbest(&modul_sc);
            free(modul_kaynak);
            continue;
        }

        /* Modül içindeki dosya modüllerini de özyinelemeli olarak işle */
        modulleri_isle(modul_program, tam_yol, arena);

        /* DÜĞÜM_KULLAN düğümünü kaldır */
        for (int j = i; j < program->çocuk_sayısı - 1; j++) {
            program->çocuklar[j] = program->çocuklar[j + 1];
        }
        program->çocuk_sayısı--;

        /* Modülün bildirimlerini programın başına ekle */
        int eklenen = modul_program->çocuk_sayısı;
        if (eklenen > 0) {
            int yeni_toplam = program->çocuk_sayısı + eklenen;
            while (yeni_toplam > program->çocuk_kapasite) {
                int yeni_kap = program->çocuk_kapasite * 2;
                if (yeni_kap < 8) yeni_kap = 8;
                Düğüm **yeni = (Düğüm **)arena_ayir(arena, sizeof(Düğüm *) * yeni_kap);
                if (program->çocuk_sayısı > 0) {
                    memcpy(yeni, program->çocuklar, sizeof(Düğüm *) * program->çocuk_sayısı);
                }
                program->çocuklar = yeni;
                program->çocuk_kapasite = yeni_kap;
            }

            /* Mevcut düğümleri sağa kaydır */
            for (int j = program->çocuk_sayısı - 1; j >= 0; j--) {
                program->çocuklar[j + eklenen] = program->çocuklar[j];
            }
            /* Modül düğümlerini başa kopyala */
            for (int j = 0; j < eklenen; j++) {
                program->çocuklar[j] = modul_program->çocuklar[j];
            }
            program->çocuk_sayısı += eklenen;
        }

        sözcük_serbest(&modul_sc);
        free(modul_kaynak);
    }
}

static void icerik_dosyalar_serbest(void) {
    for (int i = 0; i < icerik_dosya_sayisi; i++) {
        free(icerik_dosyalar[i]);
    }
    icerik_dosya_sayisi = 0;
}

static char *dosya_oku(const char *dosya_adi) {
    FILE *f = fopen(dosya_adi, "rb");
    if (!f) {
        hata_genel("dosya açılamadı: %s", dosya_adi);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long boyut = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *icerik = (char *)malloc(boyut + 1);
    if (!icerik) { fclose(f); return NULL; }
    if (fread(icerik, 1, boyut, f) != (size_t)boyut) {
        free(icerik);
        fclose(f);
        return NULL;
    }
    icerik[boyut] = '\0';
    fclose(f);

    return icerik;
}

/* Dosya adından uzantıyı çıkar */
static void cikti_adi_olustur(const char *girdi, char *cikti, int boyut, const char *uzanti) {
    const char *son_nokta = strrchr(girdi, '.');
    const char *son_slash = strrchr(girdi, '/');
    const char *temel = son_slash ? son_slash + 1 : girdi;

    int temel_uzunluk;
    if (son_nokta && son_nokta > temel) {
        temel_uzunluk = (int)(son_nokta - temel);
    } else {
        temel_uzunluk = (int)strlen(temel);
    }

    snprintf(cikti, boyut, "%.*s%s", temel_uzunluk, temel, uzanti);
}

/* Harici araç (as, gcc/ld) çıktısını Türkçeye çevir */
static void hata_cevir(const char *satir, FILE *hedef) {
    /* Bilinen İngilizce kalıpları Türkçeye çevir */
    const char *p;

    /* "undefined reference to `xxx'" */
    if ((p = strstr(satir, "undefined reference to"))) {
        const char *isim_bas = strchr(p, '`');
        const char *isim_son = isim_bas ? strchr(isim_bas + 1, '\'') : NULL;
        if (isim_bas && isim_son) {
            char isim[256];
            int len = (int)(isim_son - isim_bas - 1);
            if (len > 0 && len < (int)sizeof(isim)) {
                snprintf(isim, sizeof(isim), "%.*s", len, isim_bas + 1);
                fprintf(hedef, "  hata: '%s' tanımsız referans\n", isim);
                return;
            }
        }
    }

    /* "in function `xxx':" */
    if (strstr(satir, "in function")) {
        return; /* Bu satırı atla, Türkçe versiyonda gereksiz */
    }

    /* "error: ld returned 1 exit status" */
    if (strstr(satir, "ld returned") || strstr(satir, "exit status")) {
        return; /* Zaten "bağlama hatası" yazıyoruz */
    }

    /* "multiple definition of" */
    if ((p = strstr(satir, "multiple definition of"))) {
        const char *isim_bas = strchr(p, '`');
        const char *isim_son = isim_bas ? strchr(isim_bas + 1, '\'') : NULL;
        if (isim_bas && isim_son) {
            char isim[256];
            int len = (int)(isim_son - isim_bas - 1);
            if (len > 0 && len < (int)sizeof(isim)) {
                snprintf(isim, sizeof(isim), "%.*s", len, isim_bas + 1);
                fprintf(hedef, "  hata: '%s' birden fazla tanımlanmış\n", isim);
                return;
            }
        }
    }

    /* "cannot find -lxxx" */
    if ((p = strstr(satir, "cannot find -l"))) {
        fprintf(hedef, "  hata: '%s' kütüphanesi bulunamadı\n", p + 14);
        return;
    }

    /* "No such file or directory" */
    if (strstr(satir, "No such file or directory")) {
        fprintf(hedef, "  hata: dosya bulunamadı\n");
        return;
    }

    /* "relocation truncated" */
    if (strstr(satir, "relocation truncated")) {
        fprintf(hedef, "  hata: adres aralığı aşıldı\n");
        return;
    }

    /* Bilinmeyen mesajlar olduğu gibi geçsin */
    fprintf(hedef, "%s", satir);
}

/* Komutu çalıştır, çıktısını yakala ve Türkçeye çevir. Başarıda 0 döner. */
static int komut_calistir_tr(const char *komut) {
    char buf[1024];
    FILE *fp = popen(komut, "r");
    if (!fp) return -1;

    char *tum_cikti = NULL;
    size_t tum_boyut = 0;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        size_t len = strlen(buf);
        char *yeni = realloc(tum_cikti, tum_boyut + len + 1);
        if (!yeni) break;
        tum_cikti = yeni;
        memcpy(tum_cikti + tum_boyut, buf, len);
        tum_boyut += len;
        tum_cikti[tum_boyut] = '\0';
    }

    int durum = pclose(fp);
    int kod = WIFEXITED(durum) ? WEXITSTATUS(durum) : -1;

    /* Hata varsa çıktıyı Türkçeye çevirip yazdır */
    if (kod != 0 && tum_cikti) {
        char *satir = tum_cikti;
        char *sonraki;
        while (satir && *satir) {
            sonraki = strchr(satir, '\n');
            if (sonraki) *sonraki = '\0';
            if (strlen(satir) > 0)
                hata_cevir(satir, stderr);
            if (sonraki) {
                satir = sonraki + 1;
            } else {
                break;
            }
        }
    }

    free(tum_cikti);
    return kod;
}

static void kullanim_goster(void) {
    fprintf(stderr, "Kullanım: tonyukuk-derle [seçenekler] <kaynak.tr>\n");
    fprintf(stderr, "\nSeçenekler:\n");
    fprintf(stderr, "  -o <dosya>    Çıktı dosya adı\n");
    fprintf(stderr, "  -s            Assembly dosyasını sakla\n");
    fprintf(stderr, "  -g            Hata ayıklama bilgisi ekle (DWARF)\n");
    fprintf(stderr, "  -hedef <tür>  Hedef platform (x86_64, arm64, wasm, windows)\n");
    fprintf(stderr, "  -O            Optimizasyon gecisi (sabit katlama, olu kod eleme)\n");
    fprintf(stderr, "  -O0/-O1/-O2/-O3  LLVM optimizasyon seviyesi (--backend=llvm ile)\n");
    fprintf(stderr, "  -art\xc4\xb1ml\xc4\xb1      Art\xc4\xb1ml\xc4\xb1 derleme (de\xc4\x9fi\xc5\x9fmemi\xc5\x9f dosyalar\xc4\xb1 atla)\n");
    fprintf(stderr, "  -harita       Kaynak harita dosyas\xc4\xb1 (.map) \xc3\xbcret\n");
    fprintf(stderr, "  -profil       Profil entegrasyonu (i\xc5\x9flev zamanlama raporu)\n");
    fprintf(stderr, "  --s\xc4\xb1na        Test modunda derle (test bloklar\xc4\xb1n\xc4\xb1 \xc3\xa7al\xc4\xb1\xc5\x9ft\xc4\xb1r)\n");
    fprintf(stderr, "  --etkile\xc5\x9fimli Etkile\xc5\x9fimli REPL modunu ba\xc5\x9flat\n");
    fprintf(stderr, "  --backend=vm   Bytecode VM backend (taşınabilir .trbc dosyası üret)\n");
#ifdef LLVM_BACKEND_MEVCUT
    fprintf(stderr, "  --backend=llvm LLVM IR backend kullan (çoklu platform desteği)\n");
    fprintf(stderr, "  --emit-llvm   LLVM IR dosyası (.ll) üret\n");
    fprintf(stderr, "  --emit-bc     LLVM bitcode dosyası (.bc) üret\n");
    fprintf(stderr, "  --emit-asm    Assembly dosyası (.s) üret\n");
    fprintf(stderr, "  --emit-obj    Nesne dosyası (.o) üret\n");
    fprintf(stderr, "  --verify-ir   IR doğrulamasını etkinleştir (hata ayıklama)\n");
    fprintf(stderr, "  --jit         JIT modunda çalıştır (derleme yapmadan)\n");
#endif
    fprintf(stderr, "  --yard\xc4\xb1m     Bu yard\xc4\xb1m mesaj\xc4\xb1n\xc4\xb1 g\xc3\xb6ster\n");
    fprintf(stderr, "\nÖrnek:\n");
    fprintf(stderr, "  tonyukuk-derle merhaba.tr\n");
    fprintf(stderr, "  tonyukuk-derle -o program merhaba.tr\n");
    fprintf(stderr, "  tonyukuk-derle testler/test.tr --s\xc4\xb1na -o test_bin\n");
    fprintf(stderr, "  tonyukuk-derle --etkile\xc5\x9fimli\n");
#ifdef LLVM_BACKEND_MEVCUT
    fprintf(stderr, "  tonyukuk-derle --backend=llvm -O2 merhaba.tr\n");
    fprintf(stderr, "  tonyukuk-derle --backend=llvm --emit-llvm merhaba.tr\n");
#endif
}

/* ========== REPL (Etkileşimli Mod) ========== */

/* Blok başlatan anahtar sözcükleri kontrol et */
static int blok_baslangici_mi(const char *satir) {
    /* Satırın başındaki boşlukları atla */
    while (*satir == ' ' || *satir == '\t') satir++;

    const char *blok_sozcukler[] = {
        "i\xc5\x9f" "lev",       /* işlev */
        "e\xc4\x9f" "er",        /* eğer */
        "iken",
        "d\xc3\xb6ng\xc3\xbc",  /* döngü */
        "her",
        "e\xc5\x9f" "le",        /* eşle */
        "s\xc4\xb1n\xc4\xb1" "f", /* sınıf */
        "say\xc4\xb1m",          /* sayım */
        "dene",
        NULL
    };

    for (int i = 0; blok_sozcukler[i]; i++) {
        size_t blen = strlen(blok_sozcukler[i]);
        if (strncmp(satir, blok_sozcukler[i], blen) == 0) {
            char sonraki = satir[blen];
            if (sonraki == '\0' || sonraki == ' ' || sonraki == '\t' ||
                sonraki == '(' || sonraki == '\n')
                return 1;
        }
    }
    return 0;
}

/* Dosyanın tüm içeriğini oku, dönen bellek free() ile serbest bırakılmalı */
static char *dosya_tamamen_oku(const char *yol) {
    FILE *f = fopen(yol, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long boyut = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(boyut + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t okunan = fread(buf, 1, boyut, f);
    buf[okunan] = '\0';
    fclose(f);
    return buf;
}

/* Güvenli komut çalıştırma: system() yerine fork/execvp kullanır */
static int guvenli_komut_calistir(char *const argv[], const char *stdout_dosya, const char *stderr_dosya) {
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        /* Çocuk process */
        if (stdout_dosya) {
            int fd = open(stdout_dosya, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) { dup2(fd, STDOUT_FILENO); close(fd); }
        }
        if (stderr_dosya) {
            int fd = open(stderr_dosya, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) { dup2(fd, STDERR_FILENO); close(fd); }
        }
        execvp(argv[0], argv);
        _exit(127);
    }

    /* Ebeveyn process */
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int repl_modunda_calistir(void) {
    /* Derleyici yolunu bul */
    char exe_yolu[512];
    ssize_t exe_len = readlink("/proc/self/exe", exe_yolu, sizeof(exe_yolu) - 1);
    if (exe_len <= 0) {
        fprintf(stderr, "tonyukuk-derle: REPL: derleyici yolu bulunamadı\n");
        return 1;
    }
    exe_yolu[exe_len] = '\0';

    const int BUF_BOYUT = 65536;
    char *kaynak_buf = (char *)malloc(BUF_BOYUT);
    if (!kaynak_buf) {
        fprintf(stderr, "tonyukuk-derle: REPL: bellek ayrılamadı\n");
        return 1;
    }
    kaynak_buf[0] = '\0';
    int kaynak_pos = 0;

    char *onceki_cikti = NULL;
    char satir[1024];

    fprintf(stderr, "\033[1;36mTonyukuk REPL v2.0\033[0m \xe2\x80\x94 \xc3\x87\xc4\xb1kmak i\xc3\xa7in ':cikis' yaz\xc4\xb1n, yardim icin ':yardim'\n");

    /* Güvenli geçici dizin oluştur (mkdtemp kullanarak) */
    char repl_dizin[] = "/tmp/tonyukuk-repl-XXXXXX";
    if (!mkdtemp(repl_dizin)) {
        fprintf(stderr, "tonyukuk-derle: REPL: güvenli geçici dizin oluşturulamadı\n");
        free(kaynak_buf);
        return 1;
    }

    char repl_kaynak[512], repl_bin[512], repl_out[512], repl_err[512];
    snprintf(repl_kaynak, sizeof(repl_kaynak), "%s/repl.tr", repl_dizin);
    snprintf(repl_bin, sizeof(repl_bin), "%s/repl_bin", repl_dizin);
    snprintf(repl_out, sizeof(repl_out), "%s/repl_out.txt", repl_dizin);
    snprintf(repl_err, sizeof(repl_err), "%s/repl_err.txt", repl_dizin);

    /* Geçmiş desteği */
    char *gecmis[256];
    int gecmis_sayisi = 0;

    /* Geçmişi dosyadan oku - kullanıcı home dizininde güvenli konum */
    char gecmis_dosya[512];
    const char *home = getenv("HOME");
    if (home) {
        snprintf(gecmis_dosya, sizeof(gecmis_dosya), "%s/.tonyukuk_repl_gecmis", home);
    } else {
        snprintf(gecmis_dosya, sizeof(gecmis_dosya), "%s/gecmis", repl_dizin);
    }
    {
        FILE *gf = fopen(gecmis_dosya, "r");
        if (gf) {
            char gbuf[1024];
            while (gecmis_sayisi < 256 && fgets(gbuf, sizeof(gbuf), gf)) {
                size_t gl = strlen(gbuf);
                if (gl > 0 && gbuf[gl-1] == '\n') gbuf[--gl] = '\0';
                if (gl > 0) gecmis[gecmis_sayisi++] = strdup(gbuf);
            }
            fclose(gf);
        }
    }

    while (1) {
        fprintf(stdout, "\033[1;32mtr>\033[0m ");
        fflush(stdout);

        if (!fgets(satir, sizeof(satir), stdin))
            break; /* EOF (Ctrl+D) */

        /* Satır sonu karakterini kaldır */
        size_t slen = strlen(satir);
        if (slen > 0 && satir[slen - 1] == '\n') satir[--slen] = '\0';
        if (slen > 0 && satir[slen - 1] == '\r') satir[--slen] = '\0';

        /* Boş satırı atla */
        if (slen == 0) continue;

        /* Geçmişe ekle */
        if (gecmis_sayisi < 256) {
            gecmis[gecmis_sayisi++] = strdup(satir);
        }

        /* REPL komutları (:ile başlayan) */
        if (satir[0] == ':') {
            if (strcmp(satir, ":cikis") == 0 ||
                strcmp(satir, ":\xc3\xa7\xc4\xb1k\xc4\xb1\xc5\x9f") == 0 ||
                strcmp(satir, ":q") == 0) {
                break;
            }
            if (strcmp(satir, ":yardim") == 0 ||
                strcmp(satir, ":yard\xc4\xb1m") == 0 ||
                strcmp(satir, ":h") == 0) {
                fprintf(stderr, "\033[1;33mREPL Komutlar\xc4\xb1:\033[0m\n");
                fprintf(stderr, "  :cikis     REPL'den \xc3\xa7\xc4\xb1k\n");
                fprintf(stderr, "  :yardim    Bu mesaj\xc4\xb1 g\xc3\xb6ster\n");
                fprintf(stderr, "  :temizle   Kaynak tamponu temizle\n");
                fprintf(stderr, "  :gecmis    Komut ge\xc3\xa7mi\xc5\x9fini g\xc3\xb6ster\n");
                fprintf(stderr, "  :kaynak    Mevcut kaynak kodunu g\xc3\xb6ster\n");
                fprintf(stderr, "\n  \xc3\x87ok sat\xc4\xb1rl\xc4\xb1 bloklar otomatik alg\xc4\xb1lan\xc4\xb1r (islev/eger/dongu/sinif)\n");
                continue;
            }
            if (strcmp(satir, ":temizle") == 0) {
                kaynak_pos = 0;
                kaynak_buf[0] = '\0';
                if (onceki_cikti) { free(onceki_cikti); onceki_cikti = NULL; }
                fprintf(stderr, "\033[33mKaynak tamponu temizlendi.\033[0m\n");
                continue;
            }
            if (strcmp(satir, ":gecmis") == 0 ||
                strcmp(satir, ":ge\xc3\xa7mi\xc5\x9f") == 0) {
                for (int gi = 0; gi < gecmis_sayisi; gi++) {
                    fprintf(stderr, "  %d: %s\n", gi + 1, gecmis[gi]);
                }
                continue;
            }
            if (strcmp(satir, ":kaynak") == 0) {
                if (kaynak_pos > 0) {
                    fprintf(stderr, "\033[90m--- Mevcut kaynak ---\033[0m\n");
                    fprintf(stderr, "%s", kaynak_buf);
                    fprintf(stderr, "\033[90m--- ---\033[0m\n");
                } else {
                    fprintf(stderr, "\033[90m(bo\xc5\x9f)\033[0m\n");
                }
                continue;
            }
            fprintf(stderr, "Bilinmeyen komut: %s (:yardim ile komutlar\xc4\xb1 g\xc3\xb6r\xc3\xbcn)\n", satir);
            continue;
        }

        /* Çıkış kontrolü: çık, cik */
        if (strcmp(satir, "\xc3\xa7\xc4\xb1k") == 0 ||
            strcmp(satir, "cik") == 0) {
            break;
        }

        /* Çok satırlı blok desteği */
        char blok_buf[8192];
        blok_buf[0] = '\0';
        int blok_pos = 0;

        blok_pos += snprintf(blok_buf + blok_pos, sizeof(blok_buf) - blok_pos, "%s\n", satir);

        if (blok_baslangici_mi(satir)) {
            int derinlik = 1;
            while (derinlik > 0) {
                fprintf(stdout, "... ");
                fflush(stdout);

                if (!fgets(satir, sizeof(satir), stdin)) break;
                slen = strlen(satir);
                if (slen > 0 && satir[slen - 1] == '\n') satir[--slen] = '\0';
                if (slen > 0 && satir[slen - 1] == '\r') satir[--slen] = '\0';

                blok_pos += snprintf(blok_buf + blok_pos,
                                     sizeof(blok_buf) - blok_pos, "%s\n", satir);

                if (blok_baslangici_mi(satir)) derinlik++;

                /* "son" satırını kontrol et */
                const char *p = satir;
                while (*p == ' ' || *p == '\t') p++;
                if (strncmp(p, "son", 3) == 0) {
                    char c = p[3];
                    if (c == '\0' || c == ' ' || c == '\t' || c == '\n')
                        derinlik--;
                }
            }
        }

        /* Eklenmeden önceki konumu kaydet (geri almak için) */
        int eski_pos = kaynak_pos;

        /* Bloğu kaynak tamponuna ekle */
        if (blok_pos > 0) {
            if (kaynak_pos + blok_pos < BUF_BOYUT) {
                memcpy(kaynak_buf + kaynak_pos, blok_buf, blok_pos);
                kaynak_pos += blok_pos;
                kaynak_buf[kaynak_pos] = '\0';
            }
        } else {
            kaynak_pos += snprintf(kaynak_buf + kaynak_pos,
                                   BUF_BOYUT - kaynak_pos, "%s\n", satir);
        }

        /* Geçici kaynak dosyasını yaz (güvenli dizinde) */
        FILE *tmp = fopen(repl_kaynak, "w");
        if (!tmp) {
            fprintf(stderr, "tonyukuk-derle: REPL: ge\xc3\xa7ici dosya olu\xc5\x9fturulamad\xc4\xb1\n");
            kaynak_pos = eski_pos;
            kaynak_buf[kaynak_pos] = '\0';
            continue;
        }
        fprintf(tmp, "%s", kaynak_buf);
        fclose(tmp);

        /* Derlemeyi dene - güvenli fork/execvp kullanarak */
        char *derleme_argv[] = {exe_yolu, repl_kaynak, "-o", repl_bin, NULL};
        int derleme_sonuc = guvenli_komut_calistir(derleme_argv, NULL, repl_err);

        if (derleme_sonuc != 0) {
            /* Derleme hatası: hatayı göster, son eklenen kodu geri al */
            char *hata_metni = dosya_tamamen_oku(repl_err);
            if (hata_metni) {
                /* Her satırı Türkçeye çevirerek yazdır */
                char *s = hata_metni;
                char *sn;
                while (s && *s) {
                    sn = strchr(s, '\n');
                    if (sn) *sn = '\0';
                    if (strlen(s) > 0) hata_cevir(s, stderr);
                    if (sn) { s = sn + 1; } else break;
                }
                free(hata_metni);
            } else {
                fprintf(stderr, "tonyukuk-derle: derleme hatas\xc4\xb1\n");
            }
            kaynak_pos = eski_pos;
            kaynak_buf[kaynak_pos] = '\0';
            continue;
        }

        /* Programı çalıştır - güvenli fork/execvp kullanarak */
        char *calistir_argv[] = {repl_bin, NULL};
        guvenli_komut_calistir(calistir_argv, repl_out, repl_out);

        /* Çıktıyı oku */
        char *yeni_cikti = dosya_tamamen_oku(repl_out);
        if (!yeni_cikti) yeni_cikti = strdup("");

        /* Yalnızca yeni çıktıyı göster (farkı bul) */
        if (onceki_cikti) {
            size_t onceki_len = strlen(onceki_cikti);
            size_t yeni_len = strlen(yeni_cikti);

            if (yeni_len > onceki_len &&
                strncmp(yeni_cikti, onceki_cikti, onceki_len) == 0) {
                /* Yeni çıktı eskinin devamı: sadece farkı göster */
                fprintf(stdout, "%s", yeni_cikti + onceki_len);
            } else if (strcmp(yeni_cikti, onceki_cikti) != 0) {
                /* Tamamen farklı çıktı: tümünü göster */
                fprintf(stdout, "%s", yeni_cikti);
            }
            /* Aynı ise hiçbir şey gösterme */
        } else {
            /* İlk çalıştırma: tüm çıktıyı göster */
            if (strlen(yeni_cikti) > 0)
                fprintf(stdout, "%s", yeni_cikti);
        }
        fflush(stdout);

        /* Önceki çıktıyı güncelle */
        if (onceki_cikti) free(onceki_cikti);
        onceki_cikti = yeni_cikti;
    }

    /* Geçmişi kaydet */
    {
        FILE *gf = fopen(gecmis_dosya, "w");
        if (gf) {
            int başlangıç = gecmis_sayisi > 100 ? gecmis_sayisi - 100 : 0;
            for (int gi = başlangıç; gi < gecmis_sayisi; gi++) {
                fprintf(gf, "%s\n", gecmis[gi]);
            }
            fclose(gf);
        }
        for (int gi = 0; gi < gecmis_sayisi; gi++) free(gecmis[gi]);
    }

    /* Temizlik */
    fprintf(stdout, "\n");
    free(kaynak_buf);
    if (onceki_cikti) free(onceki_cikti);

    /* Güvenli geçici dizini temizle */
    remove(repl_kaynak);
    remove(repl_bin);
    remove(repl_out);
    remove(repl_err);
    rmdir(repl_dizin);

    return 0;
}

int main(int argc, char **argv) {
    const char *kaynak_dosya = NULL;
    const char *cikti_dosya = NULL;
    int assembly_sakla = 0;
    int repl_modu = 0;
    int debug_modu = 0;
    int optimize_modu = 0;
    int artimli_modu = 0;
    int harita_modu = 0;
    int profil_modu = 0;
    int test_modu = 0;
    const char *hedef = "x86_64";  /* varsayılan hedef platform */

    /* VM backend seçeneği */
    int vm_backend = 0;           /* --backend=vm */

    /* LLVM backend seçenekleri */
    int llvm_backend = 0;         /* --backend=llvm */
    int llvm_emit_ir = 0;         /* --emit-llvm (.ll dosyası üret) */
    int llvm_emit_bc = 0;         /* --emit-bc (.bc dosyası üret) */
    int llvm_emit_asm = 0;        /* --emit-asm (.s dosyası üret) */
    int llvm_emit_obj = 0;        /* --emit-obj (.o dosyası üret) */
    int llvm_verify_ir = 0;       /* --verify-ir (IR doğrulama) */
    int llvm_jit_mode = 0;        /* --jit (JIT modunda çalıştır) */
    int llvm_opt_seviye = 0;      /* -O0, -O1, -O2, -O3 */

    /* Komut satırı argümanlarını işle */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            cikti_dosya = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0) {
            assembly_sakla = 1;
        } else if (strcmp(argv[i], "-g") == 0) {
            debug_modu = 1;
        } else if (strcmp(argv[i], "-O") == 0) {
            optimize_modu = 1;
        } else if (strcmp(argv[i], "-art\xc4\xb1ml\xc4\xb1") == 0 || strcmp(argv[i], "-artimli") == 0) {
            artimli_modu = 1;
        } else if (strcmp(argv[i], "-harita") == 0) {
            harita_modu = 1;
        } else if (strcmp(argv[i], "-profil") == 0) {
            profil_modu = 1;
        } else if (strcmp(argv[i], "--test") == 0 || strcmp(argv[i], "-test") == 0 ||
                   strcmp(argv[i], "--s\xc4\xb1na") == 0 || strcmp(argv[i], "-s\xc4\xb1na") == 0) {
            test_modu = 1;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--repl") == 0 ||
                   strcmp(argv[i], "--etkile\xc5\x9fimli") == 0 || strcmp(argv[i], "-etkile\xc5\x9fimli") == 0) {
            repl_modu = 1;
        } else if (strcmp(argv[i], "-hedef") == 0 || strcmp(argv[i], "--hedef") == 0) {
            if (i + 1 < argc) hedef = argv[++i];
        } else if (strcmp(argv[i], "--backend=vm") == 0 || strcmp(argv[i], "-backend=vm") == 0) {
            vm_backend = 1;
        } else if (strcmp(argv[i], "--backend=llvm") == 0 || strcmp(argv[i], "-backend=llvm") == 0) {
#ifdef LLVM_BACKEND_MEVCUT
            llvm_backend = 1;
#else
            fprintf(stderr, "tonyukuk-derle: LLVM backend derlenmedi. LLVM 17 kurulu olmal\xc4\xb1.\n");
            fprintf(stderr, "Kurulum: sudo apt install llvm-17-dev\n");
            return 1;
#endif
        } else if (strcmp(argv[i], "--emit-llvm") == 0) {
            llvm_emit_ir = 1;
            llvm_backend = 1;  /* emit-llvm otomatik olarak LLVM backend'i etkinleştirir */
        } else if (strcmp(argv[i], "--emit-bc") == 0) {
            llvm_emit_bc = 1;
            llvm_backend = 1;
        } else if (strcmp(argv[i], "--emit-asm") == 0) {
            llvm_emit_asm = 1;
            llvm_backend = 1;
        } else if (strcmp(argv[i], "--emit-obj") == 0) {
            llvm_emit_obj = 1;
            llvm_backend = 1;
        } else if (strcmp(argv[i], "--verify-ir") == 0) {
            llvm_verify_ir = 1;
        } else if (strcmp(argv[i], "--jit") == 0) {
            llvm_jit_mode = 1;
            llvm_backend = 1;
        } else if (strcmp(argv[i], "-O0") == 0) {
            llvm_opt_seviye = 0;
        } else if (strcmp(argv[i], "-O1") == 0) {
            llvm_opt_seviye = 1;
        } else if (strcmp(argv[i], "-O2") == 0) {
            llvm_opt_seviye = 2;
        } else if (strcmp(argv[i], "-O3") == 0) {
            llvm_opt_seviye = 3;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--yard\xc4\xb1m") == 0) {
            kullanim_goster();
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "tonyukuk-derle: bilinmeyen se\xc3\xa7" "enek: %s\n", argv[i]);
            kullanim_goster();
            return 1;
        } else {
            kaynak_dosya = argv[i];
        }
    }

    /* REPL modu */
    if (repl_modu) {
        return repl_modunda_calistir();
    }

    if (!kaynak_dosya) {
        fprintf(stderr, "tonyukuk-derle: kaynak dosya belirtilmedi\n");
        kullanim_goster();
        return 1;
    }

    /* .tr uzantısı kontrolü */
    const char *uzanti = strrchr(kaynak_dosya, '.');
    if (!uzanti || strcmp(uzanti, ".tr") != 0) {
        fprintf(stderr, "tonyukuk-derle: uyar\xc4\xb1: dosya uzant\xc4\xb1s\xc4\xb1 .tr de\xc4\x9fil\n");
    }

    /* Artımlı derleme: .tro dosyası kontrolü */
    if (artimli_modu) {
        char tro_dosya[512];
        snprintf(tro_dosya, sizeof(tro_dosya), "%.*so",
                 (int)(uzanti ? (uzanti - kaynak_dosya + 3) : (int)strlen(kaynak_dosya)),
                 kaynak_dosya);
        /* .tr -> .tro: kaynak_dosya uzantısını .tro yap */
        if (uzanti && strcmp(uzanti, ".tr") == 0) {
            snprintf(tro_dosya, sizeof(tro_dosya), "%.*stro",
                     (int)(uzanti - kaynak_dosya + 1), kaynak_dosya);
        }

        struct stat tro_stat;
        if (stat(tro_dosya, &tro_stat) == 0) {
            /* .tro dosyasını oku ve bağımlılıkları kontrol et */
            FILE *tro_f = fopen(tro_dosya, "r");
            if (tro_f) {
                char tro_satir[1024];
                int bagimsiz_degismis = 0;
                int gecerli_format = 0;

                /* İlk satır: versiyon kontrolü */
                if (fgets(tro_satir, sizeof(tro_satir), tro_f)) {
                    size_t sl = strlen(tro_satir);
                    if (sl > 0 && tro_satir[sl-1] == '\n') tro_satir[--sl] = '\0';
                    if (strcmp(tro_satir, "v1") == 0) {
                        gecerli_format = 1;
                    }
                }

                if (gecerli_format) {
                    /* Her satırı oku: "ana:dosya:mtime" veya "bag:dosya:mtime" */
                    while (fgets(tro_satir, sizeof(tro_satir), tro_f)) {
                        size_t sl = strlen(tro_satir);
                        if (sl > 0 && tro_satir[sl-1] == '\n') tro_satir[--sl] = '\0';
                        if (sl == 0) continue;

                        /* Tür alanını ayır (ana veya bag) */
                        char *ilk_iki_nokta = strchr(tro_satir, ':');
                        if (!ilk_iki_nokta) { bagimsiz_degismis = 1; break; }

                        /* Dosya adı alanını ayır */
                        char *ikinci_iki_nokta = strrchr(tro_satir, ':');
                        if (!ikinci_iki_nokta || ikinci_iki_nokta == ilk_iki_nokta) {
                            bagimsiz_degismis = 1; break;
                        }

                        /* Dosya adını çıkar */
                        *ikinci_iki_nokta = '\0';
                        const char *dosya_adi = ilk_iki_nokta + 1;
                        long kayitli_mtime = atol(ikinci_iki_nokta + 1);

                        /* Dosyanın mevcut mtime değerini al */
                        struct stat dosya_stat;
                        if (stat(dosya_adi, &dosya_stat) != 0) {
                            /* Dosya artık mevcut değil */
                            bagimsiz_degismis = 1;
                            break;
                        }
                        if ((long)dosya_stat.st_mtime != kayitli_mtime) {
                            bagimsiz_degismis = 1;
                            break;
                        }
                    }
                } else {
                    /* Eski format veya bozuk .tro, yeniden derle */
                    bagimsiz_degismis = 1;
                }

                fclose(tro_f);

                if (!bagimsiz_degismis) {
                    fprintf(stderr, "tonyukuk-derle: '%s' g\xc3\xbcncel, yeniden derleme atland\xc4\xb1\n",
                            kaynak_dosya);
                    return 0;
                }
            }
        }
    }

    /* Kaynak dosyayı oku */
    char *kaynak = dosya_oku(kaynak_dosya);
    if (!kaynak) return 1;

    /* Hata raporlama için dosya bilgisi */
    hata_dosya_adi = kaynak_dosya;
    hata_kaynak = kaynak;

    /* 1. Sözcük çözümleme (Lexing) */
    SözcükÇözümleyici sc;
    sözcük_çözümle(&sc, kaynak);

    if (hata_sayisi > 0) {
        fprintf(stderr, "tonyukuk-derle: %d hata bulundu, derleme durduruluyor\n", hata_sayisi);
        sözcük_serbest(&sc);
        free(kaynak);
        return 1;
    }

    /* 2. Ayrıştırma (Parsing) */
    Arena arena;
    arena_baslat(&arena);

    Cozumleyici coz;
    Düğüm *program = cozumle(&coz, sc.sozcukler, sc.sozcuk_sayisi, &arena);

    if (hata_sayisi > 0) {
        fprintf(stderr, "tonyukuk-derle: %d hata bulundu, derleme durduruluyor\n", hata_sayisi);
        arena_serbest(&arena);
        sözcük_serbest(&sc);
        free(kaynak);
        return 1;
    }

    /* 2.5. Dosya modüllerini işle (kullan "dosya.tr") */
    {
        /* Ana kaynak dosyayı dahil edilmiş olarak kaydet (dairesel bağımlılık koruması) */
        char ana_gercek_yol[PATH_MAX];
        if (realpath(kaynak_dosya, ana_gercek_yol) != NULL) {
            dosya_dahil_kaydet(ana_gercek_yol);
        }
        modulleri_isle(program, kaynak_dosya, &arena);
        if (hata_sayisi > 0) {
            fprintf(stderr, "tonyukuk-derle: %d hata bulundu, derleme durduruluyor\n", hata_sayisi);
            icerik_dosyalar_serbest();
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }
    }

    /* 3. Anlamsal çözümleme (Semantic Analysis) */
    AnlamÇözümleyici ac;
    anlam_çözümle(&ac, program, &arena, hedef);

    if (hata_sayisi > 0) {
        fprintf(stderr, "tonyukuk-derle: %d hata bulundu, derleme durduruluyor\n", hata_sayisi);
        arena_serbest(&arena);
        sözcük_serbest(&sc);
        free(kaynak);
        return 1;
    }

    /* 3.5. Optimizasyon gecisi (-O bayragi) */
    if (optimize_modu) {
        optimize_et(program);
    }

#ifdef LLVM_BACKEND_MEVCUT
    /* 4. LLVM Backend kod üretimi */
    if (llvm_backend) {
        /* Hedef üçlüsünü belirle */
        const char *hedef_uclu = NULL;
        if (strcmp(hedef, "x86_64") == 0) {
            hedef_uclu = "x86_64-pc-linux-gnu";
        } else if (strcmp(hedef, "arm64") == 0) {
            hedef_uclu = "aarch64-unknown-linux-gnu";
        } else if (strcmp(hedef, "wasm") == 0) {
            hedef_uclu = "wasm32-unknown-unknown";
            /* WASM hedef ayarları llvm_üretici_oluştur sonrasında yapılacak */
        } else if (strcmp(hedef, "windows") == 0) {
            hedef_uclu = "x86_64-pc-windows-gnu";
        } else if (strcmp(hedef, "riscv64") == 0) {
            hedef_uclu = "riscv64-unknown-linux-gnu";
        } else {
            hedef_uclu = "x86_64-pc-linux-gnu";
        }

        /* Modül adını dosya adından çıkar */
        char modül_adı[256];
        const char *dosya_adi = strrchr(kaynak_dosya, '/');
        dosya_adi = dosya_adi ? dosya_adi + 1 : kaynak_dosya;
        snprintf(modül_adı, sizeof(modül_adı), "%s", dosya_adi);
        char *son_nokta = strrchr(modül_adı, '.');
        if (son_nokta) *son_nokta = '\0';

        /* LLVM üretici oluştur */
        LLVMÜretici *llvm_u = llvm_üretici_oluştur(
            &arena,
            modül_adı,
            hedef_uclu,
            llvm_opt_seviye,
            debug_modu
        );

        if (!llvm_u) {
            fprintf(stderr, "tonyukuk-derle: LLVM üretici oluşturulamadı\n");
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        /* Test modu aktar */
        llvm_u->test_modu = test_modu;

        /* WASM hedef ve import ayarları */
        if (strcmp(hedef, "wasm") == 0) {
            wasm_hedef_ayarla(llvm_u);
            wasm_ice_aktarimlari_tanimla(llvm_u);
        }

        /* IR doğrulama ayarla */
        if (llvm_verify_ir) {
            llvm_ir_dogrulama_ayarla(llvm_u, 1);
        }

        /* Debug bilgisini başlat (eğer -g aktifse) */
        if (debug_modu) {
            if (!llvm_debug_baslat(llvm_u, kaynak_dosya)) {
                fprintf(stderr, "tonyukuk-derle: LLVM debug bilgisi başlatılamadı\n");
            }
        }

        /* Monomorphization: Generic özelleştirmeleri LLVM üreticiye aktar */
        llvm_u->generic_ozellestirilmisler = ac.ozellestirilmisler;
        llvm_u->generic_ozellestirme_sayisi = ac.ozellestirme_sayisi;

        /* Frontend seviyesi optimizasyonlar */
        if (llvm_opt_seviye > 0) {
            llvm_frontend_optimizasyonlari(llvm_u, program);
        }

        /* LLVM IR üret */
        llvm_program_üret(llvm_u, program);

        /* WASM dışa aktarımları ayarla */
        if (strcmp(hedef, "wasm") == 0) {
            wasm_disa_aktarimlari_ayarla(llvm_u);
        }

        /* Debug bilgisini sonlandır */
        if (debug_modu) {
            llvm_debug_sonlandir(llvm_u);
        }

        /* Modülü doğrula */
        if (!llvm_modul_dogrula(llvm_u)) {
            fprintf(stderr, "tonyukuk-derle: LLVM modül doğrulama hatası\n");
            llvm_uretici_yok_et(llvm_u);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        /* JIT modunda çalıştır */
        if (llvm_jit_mode) {
            if (!llvm_jit_baslat(llvm_u)) {
                fprintf(stderr, "tonyukuk-derle: JIT başlatılamadı\n");
                llvm_uretici_yok_et(llvm_u);
                arena_serbest(&arena);
                sözcük_serbest(&sc);
                free(kaynak);
                return 1;
            }
            int jit_sonuc = llvm_jit_calistir(llvm_u);
            llvm_jit_sonlandir(llvm_u);
            llvm_uretici_yok_et(llvm_u);
            icerik_dosyalar_serbest();
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return jit_sonuc;
        }

        /* Optimizasyonları uygula */
        if (llvm_opt_seviye > 0) {
            llvm_optimizasyonlari_uygula(llvm_u);
        }

        /* Çıktı dosya adını belirle */
        char cikti_yolu[512];
        if (cikti_dosya) {
            snprintf(cikti_yolu, sizeof(cikti_yolu), "%s", cikti_dosya);
        } else {
            cikti_adi_olustur(kaynak_dosya, cikti_yolu, sizeof(cikti_yolu), "");
        }

        /* Çıktı türüne göre dosya yaz */
        if (llvm_emit_ir) {
            /* LLVM IR dosyası (.ll) */
            char ll_dosya[520];
            snprintf(ll_dosya, sizeof(ll_dosya), "%s.ll", cikti_yolu);
            if (llvm_ir_dosyaya_yaz(llvm_u, ll_dosya) == 0) {
                fprintf(stderr, "tonyukuk-derle: LLVM IR dosyası yazılamadı\n");
                llvm_uretici_yok_et(llvm_u);
                arena_serbest(&arena);
                sözcük_serbest(&sc);
                free(kaynak);
                return 1;
            }
            fprintf(stderr, "tonyukuk-derle: LLVM IR üretildi -> %s\n", ll_dosya);
        }

        if (llvm_emit_bc) {
            /* LLVM Bitcode dosyası (.bc) */
            char bc_dosya[520];
            snprintf(bc_dosya, sizeof(bc_dosya), "%s.bc", cikti_yolu);
            if (llvm_bitcode_dosyaya_yaz(llvm_u, bc_dosya) == 0) {
                fprintf(stderr, "tonyukuk-derle: LLVM bitcode dosyası yazılamadı\n");
                llvm_uretici_yok_et(llvm_u);
                arena_serbest(&arena);
                sözcük_serbest(&sc);
                free(kaynak);
                return 1;
            }
            fprintf(stderr, "tonyukuk-derle: LLVM bitcode üretildi -> %s.bc\n", cikti_yolu);
        }

        if (llvm_emit_asm) {
            /* Assembly dosyası (.s) üret */
            char asm_dosya[520];
            snprintf(asm_dosya, sizeof(asm_dosya), "%s.s", cikti_yolu);
            if (llvm_asm_dosyasi_uret(llvm_u, asm_dosya) == 0) {
                fprintf(stderr, "tonyukuk-derle: Assembly dosyası üretilemedi\n");
                llvm_uretici_yok_et(llvm_u);
                arena_serbest(&arena);
                sözcük_serbest(&sc);
                free(kaynak);
                return 1;
            }
            fprintf(stderr, "tonyukuk-derle: Assembly üretildi -> %s\n", asm_dosya);
        }

        if (llvm_emit_obj) {
            /* Nesne dosyası (.o) üret ve çık */
            char obj_dosya[520];
            snprintf(obj_dosya, sizeof(obj_dosya), "%s.o", cikti_yolu);
            if (llvm_nesne_dosyasi_uret(llvm_u, obj_dosya) == 0) {
                fprintf(stderr, "tonyukuk-derle: Nesne dosyası üretilemedi\n");
                llvm_uretici_yok_et(llvm_u);
                arena_serbest(&arena);
                sözcük_serbest(&sc);
                free(kaynak);
                return 1;
            }
            fprintf(stderr, "tonyukuk-derle: Nesne dosyası üretildi -> %s\n", obj_dosya);
            llvm_uretici_yok_et(llvm_u);
            icerik_dosyalar_serbest();
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 0;
        }

        /* Sadece IR/BC/ASM istenmemişse, çalıştırılabilir üret */
        if (!llvm_emit_ir && !llvm_emit_bc && !llvm_emit_asm) {
            /* Nesne dosyası üret */
            char obj_dosya[520];
            snprintf(obj_dosya, sizeof(obj_dosya), "%s.o", cikti_yolu);
            if (llvm_nesne_dosyasi_uret(llvm_u, obj_dosya) == 0) {
                fprintf(stderr, "tonyukuk-derle: Nesne dosyası üretilemedi\n");
                llvm_uretici_yok_et(llvm_u);
                arena_serbest(&arena);
                sözcük_serbest(&sc);
                free(kaynak);
                return 1;
            }

            /* Derleyici dizinini bul */
            char derleyici_llvm_yolu[512] = "";
            {
                char exe_yolu[512];
                ssize_t len = readlink("/proc/self/exe", exe_yolu, sizeof(exe_yolu) - 1);
                if (len > 0) {
                    exe_yolu[len] = '\0';
                    char *son_slash = strrchr(exe_yolu, '/');
                    if (son_slash) {
                        *son_slash = '\0';
                        snprintf(derleyici_llvm_yolu, sizeof(derleyici_llvm_yolu), "%s", exe_yolu);
                    }
                }
            }

            /* Bağlama */
            char komut[2048];
            char libtr_yolu[512];
            snprintf(libtr_yolu, sizeof(libtr_yolu), "%s/libtr.a", derleyici_llvm_yolu);
            int sonuç;

            /* WASM hedefi için wasm_kopru ile bağla */
            if (strcmp(hedef, "wasm") == 0) {
                /* Geçici nesne dosyasını wasm_ikili_uret'e ver */
                sonuç = wasm_ikili_uret(llvm_u, cikti_yolu);
                /* wasm_ikili_uret kendi obj dosyasını üretip siler,
                   buradaki obj_dosya'yı da temizle */
                remove(obj_dosya);
            } else if (strcmp(hedef, "windows") == 0) {
                /* Windows hedefi için MinGW cross-compiler kullan */
                char libtr_win_yolu[512];
                snprintf(libtr_win_yolu, sizeof(libtr_win_yolu), "%s/libtr_win.a", derleyici_llvm_yolu);
                FILE *libtr_test = fopen(libtr_win_yolu, "r");
                if (libtr_test) {
                    fclose(libtr_test);
                    snprintf(komut, sizeof(komut),
                             "x86_64-w64-mingw32-gcc %s -o %s %s %s "
                             "-lole32 -lcomctl32 -lgdi32 -luser32 -lkernel32 -ldwmapi "
                             "-lshlwapi -luuid -lcomdlg32 -mwindows -mconsole 2>&1",
                             debug_modu ? "-g" : "", cikti_yolu, obj_dosya, libtr_win_yolu);
                } else {
                    snprintf(komut, sizeof(komut),
                             "x86_64-w64-mingw32-gcc %s -o %s %s "
                             "-lole32 -luser32 -lkernel32 -mwindows -mconsole 2>&1",
                             debug_modu ? "-g" : "", cikti_yolu, obj_dosya);
                }
                sonuç = komut_calistir_tr(komut);
            } else {
                /* Normal hedefler için gcc kullan */
                /* GTK/WebKit modülleri kullanılıyorsa bağlantı bayraklarını belirle */
                const char *gtk_bayraklar = "";
                if (strstr(kaynak, "kullan pencere") || strstr(kaynak, "kullan grafik") ||
                    strstr(kaynak, "kullan web")) {
                    if (strstr(kaynak, "kullan web")) {
                        gtk_bayraklar = " $(pkg-config --libs webkit2gtk-4.1 2>/dev/null || echo '-lwebkit2gtk-4.1 -lgtk-3 -lgobject-2.0 -lglib-2.0')";
                    } else {
                        gtk_bayraklar = " $(pkg-config --libs gtk+-3.0 2>/dev/null || echo '-lgtk-3 -lgobject-2.0 -lglib-2.0')";
                    }
                }
                FILE *libtr_test = fopen(libtr_yolu, "r");
                if (libtr_test) {
                    fclose(libtr_test);
                    snprintf(komut, sizeof(komut),
                             "gcc -no-pie %s -o %s %s %s -lm -lpthread -lsqlite3%s 2>&1",
                             debug_modu ? "-g" : "", cikti_yolu, obj_dosya, libtr_yolu, gtk_bayraklar);
                } else {
                    snprintf(komut, sizeof(komut),
                             "gcc -no-pie %s -o %s %s -lm -lpthread%s 2>&1",
                             debug_modu ? "-g" : "", cikti_yolu, obj_dosya, gtk_bayraklar);
                }
                sonuç = komut_calistir_tr(komut);
            }
            if (sonuç != 0) {
                fprintf(stderr, "tonyukuk-derle: bağlama hatası\n");
                llvm_uretici_yok_et(llvm_u);
                arena_serbest(&arena);
                sözcük_serbest(&sc);
                free(kaynak);
                return 1;
            }

            /* Geçici nesne dosyasını sil */
            remove(obj_dosya);
            fprintf(stderr, "tonyukuk-derle: '%s' LLVM backend ile derlendi -> %s\n", kaynak_dosya, cikti_yolu);
        }

        /* Temizlik */
        llvm_uretici_yok_et(llvm_u);
        icerik_dosyalar_serbest();
        arena_serbest(&arena);
        sözcük_serbest(&sc);
        free(kaynak);
        return 0;
    }
#else
    (void)llvm_backend;
    (void)llvm_emit_ir;
    (void)llvm_emit_bc;
    (void)llvm_emit_asm;
    (void)llvm_emit_obj;
    (void)llvm_verify_ir;
    (void)llvm_jit_mode;
    (void)llvm_opt_seviye;
#endif

    /* 4.5. VM Backend */
    if (vm_backend) {
        Üretici vm_uretici;
        memset(&vm_uretici, 0, sizeof(vm_uretici));

        /* Çıktı dosya adını ayarla */
        char vm_cikti[512];
        if (cikti_dosya) {
            snprintf(vm_cikti, sizeof(vm_cikti), "%s", cikti_dosya);
        } else {
            cikti_adi_olustur(kaynak_dosya, vm_cikti, sizeof(vm_cikti), ".trbc");
        }
        metin_baslat(&vm_uretici.cikti);
        metin_ekle(&vm_uretici.cikti, vm_cikti);

        kod_uret_vm(&vm_uretici, program, &arena);

        fprintf(stderr, "tonyukuk-derle: '%s' VM backend ile derlendi -> %s\n",
                kaynak_dosya, vm_cikti);

        metin_serbest(&vm_uretici.cikti);
        arena_serbest(&arena);
        sözcük_serbest(&sc);
        free(kaynak);
        return 0;
    }

    /* 4.6. Doğrudan ELF64 Backend (harici araç gerektirmez) */
    if (strcmp(hedef, "elf64") == 0) {
        ElfÜretici elf_ü;
        memset(&elf_ü, 0, sizeof(elf_ü));

        kod_üret_elf64(&elf_ü, program, &arena);

        /* Çıktı dosya adı */
        char elf_çıktı[256];
        if (cikti_dosya) {
            snprintf(elf_çıktı, sizeof(elf_çıktı), "%s", cikti_dosya);
        } else {
            cikti_adi_olustur(kaynak_dosya, elf_çıktı, sizeof(elf_çıktı), "");
        }

        if (elf64_dosya_yaz(&elf_ü, elf_çıktı) != 0) {
            fprintf(stderr, "tonyukuk-derle: ELF64 dosya yazma hatası\n");
            elf_üretici_serbest(&elf_ü);
            icerik_dosyalar_serbest();
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        /* Çalıştırılabilir izni ver */
        chmod(elf_çıktı, 0755);

        fprintf(stderr, "tonyukuk-derle: '%s' doğrudan ELF64 olarak derlendi -> %s\n",
                kaynak_dosya, elf_çıktı);

        elf_üretici_serbest(&elf_ü);
        icerik_dosyalar_serbest();
        arena_serbest(&arena);
        sözcük_serbest(&sc);
        free(kaynak);
        return 0;
    }

    /* 5. Kod uretimi (hedef platforma gore) */
    Üretici üretici;
    memset(&üretici, 0, sizeof(üretici));
    üretici.debug_modu = debug_modu;
    üretici.kaynak_dosya = kaynak_dosya;
    üretici.test_modu = test_modu;
    üretici.profil_modu = profil_modu;
    üretici.harita_modu = harita_modu;

    /* Monomorphization: Generic özelleştirmeleri kod üreticiye aktar */
    üretici.generic_ozellestirilmisler = ac.ozellestirilmisler;
    üretici.generic_ozellestirme_sayisi = ac.ozellestirme_sayisi;

    /* Kaynak harita baslat */
    KaynakHarita kaynak_harita;
    if (harita_modu) {
        kaynak_harita_baslat(&kaynak_harita, kaynak_dosya);
    }

    if (strcmp(hedef, "arm64") == 0) {
        kod_uret_arm64(&üretici, program, &arena);
    } else if (strcmp(hedef, "wasm") == 0) {
        kod_uret_wasm(&üretici, program, &arena);
    } else if (strcmp(hedef, "avr") == 0) {
        kod_uret_avr(&üretici, program, &arena);
    } else if (strcmp(hedef, "xtensa") == 0 || strcmp(hedef, "esp32") == 0) {
        kod_uret_xtensa(&üretici, program, &arena);
    } else if (strcmp(hedef, "arm-m0") == 0 || strcmp(hedef, "pico") == 0) {
        kod_uret_arm_m0(&üretici, program, &arena);
    } else {
        /* x86_64 veya windows (varsayilan) */
        kod_üret(&üretici, program, &arena);
    }

    /* Kaynak harita yaz */
    if (harita_modu) {
        char harita_dosya[512];
        if (cikti_dosya) {
            snprintf(harita_dosya, sizeof(harita_dosya), "%s.map", cikti_dosya);
        } else {
            cikti_adi_olustur(kaynak_dosya, harita_dosya, sizeof(harita_dosya), ".map");
        }
        /* Her program dugumu icin satirbilgisi eslemesi */
        if (program) {
            for (int khidx = 0; khidx < program->çocuk_sayısı; khidx++) {
                if (program->çocuklar[khidx]->satir > 0) {
                    kaynak_harita.asm_satir_sayac = (khidx + 1) * 5;
                    kaynak_harita_ekle(&kaynak_harita, program->çocuklar[khidx]->satir);
                }
            }
        }
        kaynak_harita_yaz(&kaynak_harita, harita_dosya);
        fprintf(stderr, "tonyukuk-derle: kaynak harita yazildi -> %s\n", harita_dosya);
    }

    /* Artimli derleme: .tro dosyasini olustur (yapilandirmis format) */
    if (artimli_modu && uzanti && strcmp(uzanti, ".tr") == 0) {
        char tro_dosya[512];
        snprintf(tro_dosya, sizeof(tro_dosya), "%.*stro",
                 (int)(uzanti - kaynak_dosya + 1), kaynak_dosya);
        FILE *tro_f = fopen(tro_dosya, "w");
        if (tro_f) {
            fprintf(tro_f, "v1\n");
            /* Ana kaynak dosyanın mtime değerini yaz */
            {
                struct stat ana_stat;
                if (stat(kaynak_dosya, &ana_stat) == 0) {
                    fprintf(tro_f, "ana:%s:%ld\n", kaynak_dosya, (long)ana_stat.st_mtime);
                }
            }
            /* Dahil edilen tüm bağımlılık dosyalarının mtime değerlerini yaz */
            for (int bi = 0; bi < icerik_dosya_sayisi; bi++) {
                struct stat bag_stat;
                if (stat(icerik_dosyalar[bi], &bag_stat) == 0) {
                    /* Ana dosyayı tekrar yazmayı atla (zaten yukarıda yazıldı) */
                    char ana_gercek[PATH_MAX];
                    if (realpath(kaynak_dosya, ana_gercek) != NULL &&
                        strcmp(icerik_dosyalar[bi], ana_gercek) == 0) {
                        continue;
                    }
                    fprintf(tro_f, "bag:%s:%ld\n", icerik_dosyalar[bi], (long)bag_stat.st_mtime);
                }
            }
            fclose(tro_f);
        }
    }

    /* Cikti dosya adi */
    char elf_dosya[256];
    if (cikti_dosya) {
        snprintf(elf_dosya, sizeof(elf_dosya), "%s", cikti_dosya);
    } else {
        cikti_adi_olustur(kaynak_dosya, elf_dosya, sizeof(elf_dosya), "");
    }

    /* Derleyici dizinini bul */
    char derleyici_yolu[512] = "";
    {
        char exe_yolu[512];
        ssize_t len = readlink("/proc/self/exe", exe_yolu, sizeof(exe_yolu) - 1);
        if (len > 0) {
            exe_yolu[len] = '\0';
            char *son_slash = strrchr(exe_yolu, '/');
            if (son_slash) {
                *son_slash = '\0';
                snprintf(derleyici_yolu, sizeof(derleyici_yolu), "%s", exe_yolu);
            }
        }
    }

    char komut[2048];
    int sonuç;

    if (strcmp(hedef, "wasm") == 0) {
        /* WASM: .wat dosyası yaz, assembly/linking atla */
        char wat_dosya[512];
        if (cikti_dosya) {
            snprintf(wat_dosya, sizeof(wat_dosya), "%s.wat", cikti_dosya);
        } else {
            cikti_adi_olustur(kaynak_dosya, wat_dosya, sizeof(wat_dosya), ".wat");
        }
        /* WAT: dogrudan yaz (assembly_yaz x86 basligi ekler, WAT icin istemiyoruz) */
        {
            FILE *wf = fopen(wat_dosya, "w");
            if (!wf) {
                fprintf(stderr, "tonyukuk-derle: WAT dosyası oluşturulamadı: %s\n", wat_dosya);
                üretici_serbest(&üretici);
                arena_serbest(&arena);
                sözcük_serbest(&sc);
                free(kaynak);
                return 1;
            }
            fprintf(wf, ";; Tonyukuk Derleyici tarafından üretildi (WASM)\n");
            if (üretici.cikti.veri) fprintf(wf, "%s", üretici.cikti.veri);
            fclose(wf);
        }
        fprintf(stderr, "tonyukuk-derle: '%s' WASM modülü olarak derlendi -> %s\n", kaynak_dosya, wat_dosya);
    } else if (strcmp(hedef, "arm64") == 0) {
        /* ARM64: aarch64-linux-gnu araçları ile derle */
        char asm_dosya[256];
        cikti_adi_olustur(kaynak_dosya, asm_dosya, sizeof(asm_dosya), ".s");

        if (assembly_yaz(&üretici, asm_dosya) != 0) {
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        char obj_dosya[256];
        cikti_adi_olustur(kaynak_dosya, obj_dosya, sizeof(obj_dosya), ".o");

        snprintf(komut, sizeof(komut), "aarch64-linux-gnu-as -o %s %s 2>&1", obj_dosya, asm_dosya);
        sonuç = komut_calistir_tr(komut);
        if (sonuç != 0) {
            fprintf(stderr, "tonyukuk-derle: ARM64 assembly hatası (aarch64-linux-gnu-as gerekli)\n");
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        snprintf(komut, sizeof(komut),
                 "aarch64-linux-gnu-gcc -o %s %s -lm -static 2>&1",
                 elf_dosya, obj_dosya);
        sonuç = komut_calistir_tr(komut);
        if (sonuç != 0) {
            fprintf(stderr, "tonyukuk-derle: ARM64 bağlama hatası (aarch64-linux-gnu-gcc gerekli)\n");
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        if (!assembly_sakla) remove(asm_dosya);
        remove(obj_dosya);
        fprintf(stderr, "tonyukuk-derle: '%s' ARM64 olarak derlendi -> %s\n", kaynak_dosya, elf_dosya);
    } else if (strcmp(hedef, "avr") == 0) {
        /* AVR: avr-gcc araçları ile derle (Arduino UNO/Nano/Mega) */
        char asm_dosya[256];
        cikti_adi_olustur(kaynak_dosya, asm_dosya, sizeof(asm_dosya), ".s");

        if (assembly_yaz(&üretici, asm_dosya) != 0) {
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        char obj_dosya[256];
        cikti_adi_olustur(kaynak_dosya, obj_dosya, sizeof(obj_dosya), ".o");

        /* Assembly -> Object (avr-gcc ile, C preprocessor desteği için) */
        snprintf(komut, sizeof(komut),
                 "avr-gcc -c -mmcu=atmega328p -x assembler-with-cpp -o %s %s 2>&1", obj_dosya, asm_dosya);
        sonuç = komut_calistir_tr(komut);
        if (sonuç != 0) {
            fprintf(stderr, "tonyukuk-derle: AVR assembly hatası (avr-gcc gerekli)\n");
            fprintf(stderr, "Kurulum: sudo apt install gcc-avr avr-libc\n");
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        /* AVR runtime'ı derle (donanım fonksiyonları: GPIO, PWM, UART, vb.) */
        char avr_rt_obj[256];
        snprintf(avr_rt_obj, sizeof(avr_rt_obj), "%s_rt.o", obj_dosya);
        int rt_var = 0;
        {
            char avr_cz[512];
            snprintf(avr_cz, sizeof(avr_cz),
                     "%s/gomulu/calismazamani/avr_cz.c", derleyici_yolu);
            snprintf(komut, sizeof(komut),
                     "avr-gcc -c -mmcu=atmega328p -Os -DF_CPU=16000000UL "
                     "-I%s/gomulu/calismazamani -o %s %s 2>&1",
                     derleyici_yolu, avr_rt_obj, avr_cz);
            rt_var = (komut_calistir_tr(komut) == 0) ? 1 : 0;
        }

        /* Object -> ELF (program + runtime) */
        if (rt_var) {
            snprintf(komut, sizeof(komut),
                     "avr-gcc -mmcu=atmega328p -o %s.elf %s %s 2>&1",
                     elf_dosya, obj_dosya, avr_rt_obj);
        } else {
            snprintf(komut, sizeof(komut),
                     "avr-gcc -mmcu=atmega328p -o %s.elf %s 2>&1",
                     elf_dosya, obj_dosya);
        }
        sonuç = komut_calistir_tr(komut);
        if (rt_var) remove(avr_rt_obj);
        if (sonuç != 0) {
            fprintf(stderr, "tonyukuk-derle: AVR bağlama hatası\n");
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        /* ELF -> Intel HEX (Arduino'ya yüklemek için) */
        char hex_dosya[280];
        snprintf(hex_dosya, sizeof(hex_dosya), "%s.hex", elf_dosya);
        snprintf(komut, sizeof(komut),
                 "avr-objcopy -O ihex -R .eeprom %s.elf %s 2>&1",
                 elf_dosya, hex_dosya);
        sonuç = komut_calistir_tr(komut);
        if (sonuç != 0) {
            fprintf(stderr, "tonyukuk-derle: HEX dönüştürme hatası\n");
        }

        if (!assembly_sakla) remove(asm_dosya);
        remove(obj_dosya);
        fprintf(stderr, "tonyukuk-derle: '%s' AVR olarak derlendi -> %s.elf, %s\n", kaynak_dosya, elf_dosya, hex_dosya);
        fprintf(stderr, "Arduino'ya yüklemek için: avrdude -c arduino -p m328p -P /dev/ttyUSB0 -U flash:w:%s\n", hex_dosya);
    } else if (strcmp(hedef, "xtensa") == 0 || strcmp(hedef, "esp32") == 0) {
        /* ESP32: ESP-IDF uyumlu C kodu üret */
        char c_dosya[256];
        cikti_adi_olustur(kaynak_dosya, c_dosya, sizeof(c_dosya), "_esp32.c");

        FILE *c_fp = fopen(c_dosya, "w");
        if (!c_fp) {
            fprintf(stderr, "tonyukuk-derle: %s yazılamadı\n", c_dosya);
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }
        fprintf(c_fp, "%s", üretici.cikti.veri);
        fclose(c_fp);

        fprintf(stderr, "tonyukuk-derle: '%s' ESP32 için derlendi -> %s\n", kaynak_dosya, c_dosya);
        fprintf(stderr, "\nESP-IDF ile derlemek için:\n");
        fprintf(stderr, "  1. ESP-IDF ortamını kurun: https://docs.espressif.com/projects/esp-idf/\n");
        fprintf(stderr, "  2. Yeni proje oluşturun: idf.py create-project projemiz\n");
        fprintf(stderr, "  3. %s dosyasını main/main.c olarak kopyalayın\n", c_dosya);
        fprintf(stderr, "  4. Derleyin: idf.py build\n");
        fprintf(stderr, "  5. Yükleyin: idf.py -p /dev/ttyUSB0 flash\n");
    } else if (strcmp(hedef, "arm-m0") == 0 || strcmp(hedef, "pico") == 0) {
        /* Raspberry Pi Pico: Pico SDK uyumlu C kodu üret */
        char c_dosya[256];
        cikti_adi_olustur(kaynak_dosya, c_dosya, sizeof(c_dosya), "_pico.c");

        FILE *c_fp = fopen(c_dosya, "w");
        if (!c_fp) {
            fprintf(stderr, "tonyukuk-derle: %s yazılamadı\n", c_dosya);
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }
        fprintf(c_fp, "%s", üretici.cikti.veri);
        fclose(c_fp);

        fprintf(stderr, "tonyukuk-derle: '%s' Raspberry Pi Pico için derlendi -> %s\n", kaynak_dosya, c_dosya);
        fprintf(stderr, "\nPico SDK ile derlemek için:\n");
        fprintf(stderr, "  1. Pico SDK kurun: https://github.com/raspberrypi/pico-sdk\n");
        fprintf(stderr, "  2. Proje dizini oluşturun ve CMakeLists.txt ekleyin\n");
        fprintf(stderr, "  3. %s dosyasını ana kaynak olarak kullanın\n", c_dosya);
        fprintf(stderr, "  4. mkdir build && cd build && cmake .. && make\n");
        fprintf(stderr, "  5. .uf2 dosyasını BOOTSEL modunda Pico'ya kopyalayın\n");
    } else if (strcmp(hedef, "windows") == 0) {
        /* Windows x86-64: mingw araçları ile çapraz derle */
        char asm_dosya[256];
        cikti_adi_olustur(kaynak_dosya, asm_dosya, sizeof(asm_dosya), ".s");

        if (assembly_yaz(&üretici, asm_dosya) != 0) {
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        char obj_dosya[256];
        cikti_adi_olustur(kaynak_dosya, obj_dosya, sizeof(obj_dosya), ".o");

        /* .exe uzantısı ekle */
        char win_dosya[280];
        snprintf(win_dosya, sizeof(win_dosya), "%s.exe", elf_dosya);

        snprintf(komut, sizeof(komut),
                 "x86_64-w64-mingw32-as -o %s %s 2>&1", obj_dosya, asm_dosya);
        sonuç = komut_calistir_tr(komut);
        if (sonuç != 0) {
            fprintf(stderr, "tonyukuk-derle: Windows assembly hatası (x86_64-w64-mingw32-as gerekli)\n");
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        snprintf(komut, sizeof(komut),
                 "x86_64-w64-mingw32-gcc -o %s %s -lm 2>&1",
                 win_dosya, obj_dosya);
        sonuç = komut_calistir_tr(komut);
        if (sonuç != 0) {
            fprintf(stderr, "tonyukuk-derle: Windows bağlama hatası (x86_64-w64-mingw32-gcc gerekli)\n");
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        if (!assembly_sakla) remove(asm_dosya);
        remove(obj_dosya);
        fprintf(stderr, "tonyukuk-derle: '%s' Windows için derlendi -> %s\n", kaynak_dosya, win_dosya);
    } else {
        /* Varsayılan: x86_64 Linux */
        char asm_dosya[256];
        cikti_adi_olustur(kaynak_dosya, asm_dosya, sizeof(asm_dosya), ".s");

        if (assembly_yaz(&üretici, asm_dosya) != 0) {
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        /* 5. Assembly'den object dosyasına (as) */
        char obj_dosya[256];
        cikti_adi_olustur(kaynak_dosya, obj_dosya, sizeof(obj_dosya), ".o");

        snprintf(komut, sizeof(komut), "as %s -o %s %s 2>&1",
                 debug_modu ? "--gdwarf-5" : "", obj_dosya, asm_dosya);
        sonuç = komut_calistir_tr(komut);
        if (sonuç != 0) {
            fprintf(stderr, "tonyukuk-derle: assembly hatası\n");
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        /* 6. Bağlama (gcc) */
        /* GTK/WebKit modülleri kullanılıyorsa bağlantı bayraklarını belirle */
        const char *gtk_bayraklar2 = "";
        if (strstr(kaynak, "kullan pencere") || strstr(kaynak, "kullan grafik") ||
            strstr(kaynak, "kullan web")) {
            if (strstr(kaynak, "kullan web")) {
                gtk_bayraklar2 = " $(pkg-config --libs webkit2gtk-4.1 2>/dev/null || echo '-lwebkit2gtk-4.1 -lgtk-3 -lgobject-2.0 -lglib-2.0')";
            } else {
                gtk_bayraklar2 = " $(pkg-config --libs gtk+-3.0 2>/dev/null || echo '-lgtk-3 -lgobject-2.0 -lglib-2.0')";
            }
        }
        char libtr_yolu[512];
        snprintf(libtr_yolu, sizeof(libtr_yolu), "%s/libtr.a", derleyici_yolu);
        FILE *libtr_test = fopen(libtr_yolu, "r");
        if (libtr_test) {
            fclose(libtr_test);
            snprintf(komut, sizeof(komut),
                     "gcc -no-pie %s -o %s %s %s -lm -lpthread -lsqlite3%s 2>&1",
                     debug_modu ? "-g" : "", elf_dosya, obj_dosya, libtr_yolu, gtk_bayraklar2);
        } else {
            snprintf(komut, sizeof(komut),
                     "gcc -no-pie %s -o %s %s -lm -lpthread%s 2>&1",
                     debug_modu ? "-g" : "", elf_dosya, obj_dosya, gtk_bayraklar2);
        }
        sonuç = komut_calistir_tr(komut);
        if (sonuç != 0) {
            fprintf(stderr, "tonyukuk-derle: bağlama hatası\n");
            üretici_serbest(&üretici);
            arena_serbest(&arena);
            sözcük_serbest(&sc);
            free(kaynak);
            return 1;
        }

        /* Geçici dosyaları temizle */
        if (!assembly_sakla) {
            remove(asm_dosya);
        }
        remove(obj_dosya);

        fprintf(stderr, "tonyukuk-derle: '%s' başarıyla derlendi -> %s\n", kaynak_dosya, elf_dosya);
    }

    /* Temizlik */
    icerik_dosyalar_serbest();
    üretici_serbest(&üretici);
    arena_serbest(&arena);
    sözcük_serbest(&sc);
    free(kaynak);

    return 0;
}
