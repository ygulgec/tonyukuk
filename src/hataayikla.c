/* Tonyukuk Hata Ayıklayıcı - ptrace tabanlı */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/personality.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>

/* DWARF satır bilgisi yapısı */
typedef struct {
    uint64_t adres;
    int satir;
    const char *dosya;
} SatirBilgisi;

/* Kesme noktası yapısı */
typedef struct {
    uint64_t adres;
    uint8_t orijinal_byte;
    int aktif;
    int satir;
} KesmeNoktasi;

/* Debugger durumu */
typedef struct {
    pid_t hedef_pid;
    int calisiyor;
    KesmeNoktasi kesme_noktalari[64];
    int kesme_sayisi;
    const char *kaynak_dosya;
    char **kaynak_satirlar;
    int satir_sayisi;
} Debugger;

/* Kaynak dosyayı yükle */
static int kaynak_yukle(Debugger *d, const char *dosya) {
    FILE *f = fopen(dosya, "r");
    if (!f) return 0;

    d->kaynak_dosya = dosya;
    d->kaynak_satirlar = malloc(sizeof(char *) * 10000);
    d->satir_sayisi = 0;

    char buf[4096];
    while (fgets(buf, sizeof(buf), f) && d->satir_sayisi < 10000) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        d->kaynak_satirlar[d->satir_sayisi] = strdup(buf);
        d->satir_sayisi++;
    }
    fclose(f);
    return 1;
}

/* Kaynak satırını göster */
static void satir_goster(Debugger *d, int satir) {
    if (satir > 0 && satir <= d->satir_sayisi) {
        printf("  %4d | %s\n", satir, d->kaynak_satirlar[satir - 1]);
    }
}

/* Register değerlerini göster */
static void registerler_goster(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace GETREGS");
        return;
    }

    printf("Registerler:\n");
    printf("  rax: 0x%016llx  rbx: 0x%016llx\n", regs.rax, regs.rbx);
    printf("  rcx: 0x%016llx  rdx: 0x%016llx\n", regs.rcx, regs.rdx);
    printf("  rsi: 0x%016llx  rdi: 0x%016llx\n", regs.rsi, regs.rdi);
    printf("  rbp: 0x%016llx  rsp: 0x%016llx\n", regs.rbp, regs.rsp);
    printf("  rip: 0x%016llx\n", regs.rip);
}

/* Kesme noktası koy */
static int kesme_noktasi_koy(Debugger *d, uint64_t adres, int satir) {
    if (d->kesme_sayisi >= 64) return 0;

    /* Orijinal byte'ı oku */
    long veri = ptrace(PTRACE_PEEKTEXT, d->hedef_pid, (void *)adres, NULL);
    if (veri == -1 && errno != 0) {
        perror("ptrace PEEKTEXT");
        return 0;
    }

    /* int3 (0xCC) yerleştir */
    long yeni_veri = (veri & ~0xFF) | 0xCC;
    if (ptrace(PTRACE_POKETEXT, d->hedef_pid, (void *)adres, (void *)yeni_veri) == -1) {
        perror("ptrace POKETEXT");
        return 0;
    }

    d->kesme_noktalari[d->kesme_sayisi].adres = adres;
    d->kesme_noktalari[d->kesme_sayisi].orijinal_byte = (uint8_t)(veri & 0xFF);
    d->kesme_noktalari[d->kesme_sayisi].aktif = 1;
    d->kesme_noktalari[d->kesme_sayisi].satir = satir;
    d->kesme_sayisi++;

    return 1;
}

/* Kesme noktasını geri al */
static void kesme_noktasi_geri_al(Debugger *d, int idx) {
    if (idx < 0 || idx >= d->kesme_sayisi) return;
    if (!d->kesme_noktalari[idx].aktif) return;

    uint64_t adres = d->kesme_noktalari[idx].adres;
    long veri = ptrace(PTRACE_PEEKTEXT, d->hedef_pid, (void *)adres, NULL);
    long yeni_veri = (veri & ~0xFF) | d->kesme_noktalari[idx].orijinal_byte;
    ptrace(PTRACE_POKETEXT, d->hedef_pid, (void *)adres, (void *)yeni_veri);
    d->kesme_noktalari[idx].aktif = 0;
}

/* Tek adım at */
static void tek_adim(pid_t pid) {
    ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
    int status;
    waitpid(pid, &status, 0);
}

/* Devam et */
static void devam_et(pid_t pid) {
    ptrace(PTRACE_CONT, pid, NULL, NULL);
}

/* Bellek oku ve göster */
static void bellek_goster(pid_t pid, uint64_t adres, int boyut) {
    printf("Bellek 0x%lx (%d byte):\n", adres, boyut);
    for (int i = 0; i < boyut; i += 8) {
        long veri = ptrace(PTRACE_PEEKDATA, pid, (void *)(adres + i), NULL);
        if (veri == -1 && errno != 0) {
            printf("  0x%lx: <okunamadı>\n", adres + i);
            break;
        }
        printf("  0x%lx: 0x%016lx  (%ld)\n", adres + i, veri, veri);
    }
}

/* Stack'i göster */
static void stack_goster(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace GETREGS");
        return;
    }

    printf("Stack (RSP=0x%llx):\n", regs.rsp);
    for (int i = 0; i < 8; i++) {
        uint64_t adres = regs.rsp + i * 8;
        long veri = ptrace(PTRACE_PEEKDATA, pid, (void *)adres, NULL);
        printf("  [rsp+%d]: 0x%016lx  (%ld)\n", i * 8, veri, veri);
    }
}

/* Yardım göster */
static void yardim_goster(void) {
    printf("\nTonyukuk Hata Ayıklayıcı Komutları:\n");
    printf("  kes <satir>  - Kesme noktası koy (satır numarasına)\n");
    printf("  d, devam     - Programı devam ettir\n");
    printf("  a, adim      - Tek adım at\n");
    printf("  r, reg       - Registerleri göster\n");
    printf("  s, stack     - Stack'i göster\n");
    printf("  b <adres>    - Bellekten oku (örn: b 0x401000)\n");
    printf("  g, goster    - Mevcut satırı göster\n");
    printf("  c, cik       - Çıkış\n");
    printf("  y, yardim    - Bu yardımı göster\n\n");
}

/* Ana debugger döngüsü */
static void debugger_dongusü(Debugger *d) {
    char komut[256];
    int status;

    printf("\nTonyukuk Hata Ayıklayıcı v1.0\n");
    printf("Hedef PID: %d\n", d->hedef_pid);
    if (d->kaynak_dosya) {
        printf("Kaynak: %s (%d satır)\n", d->kaynak_dosya, d->satir_sayisi);
    }
    yardim_goster();

    while (d->calisiyor) {
        printf("(tonyukuk-ha) ");
        fflush(stdout);

        if (!fgets(komut, sizeof(komut), stdin)) break;

        /* Yeni satırı kaldır */
        size_t len = strlen(komut);
        if (len > 0 && komut[len - 1] == '\n') komut[len - 1] = '\0';

        if (strlen(komut) == 0) continue;

        if (strcmp(komut, "c") == 0 || strcmp(komut, "cik") == 0 ||
            strcmp(komut, "q") == 0 || strcmp(komut, "quit") == 0) {
            printf("Çıkılıyor...\n");
            ptrace(PTRACE_KILL, d->hedef_pid, NULL, NULL);
            d->calisiyor = 0;
            break;
        }

        if (strcmp(komut, "d") == 0 || strcmp(komut, "devam") == 0 ||
            strcmp(komut, "c") == 0 || strcmp(komut, "continue") == 0) {
            printf("Devam ediliyor...\n");
            devam_et(d->hedef_pid);
            waitpid(d->hedef_pid, &status, 0);

            if (WIFEXITED(status)) {
                printf("Program sonlandı (çıkış kodu: %d)\n", WEXITSTATUS(status));
                d->calisiyor = 0;
            } else if (WIFSTOPPED(status)) {
                if (WSTOPSIG(status) == SIGTRAP) {
                    printf("Kesme noktasında duruldu.\n");
                    registerler_goster(d->hedef_pid);
                } else {
                    printf("Sinyal alındı: %d\n", WSTOPSIG(status));
                }
            }
            continue;
        }

        if (strcmp(komut, "a") == 0 || strcmp(komut, "adim") == 0 ||
            strcmp(komut, "s") == 0 || strcmp(komut, "step") == 0) {
            tek_adim(d->hedef_pid);
            printf("Bir adım atıldı.\n");
            registerler_goster(d->hedef_pid);
            continue;
        }

        if (strcmp(komut, "r") == 0 || strcmp(komut, "reg") == 0) {
            registerler_goster(d->hedef_pid);
            continue;
        }

        if (strcmp(komut, "st") == 0 || strcmp(komut, "stack") == 0) {
            stack_goster(d->hedef_pid);
            continue;
        }

        if (strncmp(komut, "b ", 2) == 0 || strncmp(komut, "bellek ", 7) == 0) {
            char *adres_str = strchr(komut, ' ') + 1;
            uint64_t adres = strtoull(adres_str, NULL, 0);
            bellek_goster(d->hedef_pid, adres, 64);
            continue;
        }

        if (strcmp(komut, "g") == 0 || strcmp(komut, "goster") == 0) {
            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, d->hedef_pid, NULL, &regs);
            printf("RIP: 0x%016llx\n", regs.rip);
            continue;
        }

        if (strcmp(komut, "y") == 0 || strcmp(komut, "yardim") == 0 ||
            strcmp(komut, "h") == 0 || strcmp(komut, "help") == 0) {
            yardim_goster();
            continue;
        }

        if (strncmp(komut, "kes ", 4) == 0) {
            int satir = atoi(komut + 4);
            if (satir > 0) {
                printf("Kesme noktası satır %d'e eklenecek (elle adres belirtilmeli)\n", satir);
                /* Not: Gerçek uygulamada DWARF bilgisinden adres bulunmalı */
            }
            continue;
        }

        printf("Bilinmeyen komut: %s\n", komut);
        printf("'yardim' yazarak komutları görebilirsiniz.\n");
    }
}

/* Kullanım */
static void kullanim(const char *prog) {
    fprintf(stderr, "Kullanım: %s <program> [kaynak.tr]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Tonyukuk Hata Ayıklayıcı - ptrace tabanlı debugger\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Örnek:\n");
    fprintf(stderr, "  %s ./program kaynak.tr\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        kullanim(argv[0]);
        return 1;
    }

    const char *hedef_program = argv[1];
    const char *kaynak_dosya = argc > 2 ? argv[2] : NULL;

    Debugger d = {0};
    d.calisiyor = 1;

    if (kaynak_dosya) {
        if (!kaynak_yukle(&d, kaynak_dosya)) {
            fprintf(stderr, "Uyarı: Kaynak dosya yüklenemedi: %s\n", kaynak_dosya);
        }
    }

    /* Fork ve ptrace ile hedef programı başlat */
    pid_t pid = fork();
    if (pid == 0) {
        /* Çocuk process: hedef programı çalıştır */
        personality(ADDR_NO_RANDOMIZE);  /* ASLR'yi devre dışı bırak */
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl(hedef_program, hedef_program, NULL);
        perror("execl");
        exit(1);
    } else if (pid > 0) {
        /* Ana process: debugger */
        d.hedef_pid = pid;

        /* Hedef programın başlamasını bekle */
        int status;
        waitpid(pid, &status, 0);

        if (WIFSTOPPED(status)) {
            printf("Hedef program başlatıldı ve durduruldu.\n");
            debugger_dongusü(&d);
        } else {
            fprintf(stderr, "Hedef program başlatılamadı.\n");
            return 1;
        }
    } else {
        perror("fork");
        return 1;
    }

    /* Kaynak satırlarını temizle */
    for (int i = 0; i < d.satir_sayisi; i++) {
        free(d.kaynak_satirlar[i]);
    }
    free(d.kaynak_satirlar);

    return 0;
}
