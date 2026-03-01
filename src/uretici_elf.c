/*
 * uretici_elf.c — Doğrudan ELF64 İkili Üretim Backend'i
 *
 * Tonyukuk derleyicisi için AST'den doğrudan x86_64 makine kodu
 * ve ELF64 çalıştırılabilir dosya üreten backend.
 *
 * Harici araç bağımlılığı yok (as, gcc, libc gereksiz).
 * Giriş noktası _start, tüm I/O Linux syscall ile yapılır.
 */

#include "uretici_elf.h"
#include "elf64.h"
#include "x86_kodlayici.h"
#include "tablo.h"
#include "sozcuk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *                     ETİKET / YAMA YÖNETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

static int yeni_etiket(ElfÜretici *eü) {
    int no = eü->etiket_sayaç++;
    if (no >= eü->etiket_kapasite) {
        int yeni_kap = eü->etiket_kapasite ? eü->etiket_kapasite * 2 : 256;
        eü->etiket_ofsetler = (int *)realloc(eü->etiket_ofsetler, yeni_kap * sizeof(int));
        for (int i = eü->etiket_kapasite; i < yeni_kap; i++)
            eü->etiket_ofsetler[i] = -1;
        eü->etiket_kapasite = yeni_kap;
    }
    eü->etiket_ofsetler[no] = -1;
    return no;
}

static void etiket_tanımla(ElfÜretici *eü, int etiket_no) {
    if (etiket_no >= 0 && etiket_no < eü->etiket_kapasite)
        eü->etiket_ofsetler[etiket_no] = eü->kod.uzunluk;
}

static void yama_ekle(ElfÜretici *eü, int kod_ofseti, int etiket_no) {
    if (eü->yama_sayısı >= eü->yama_kapasite) {
        int yeni_kap = eü->yama_kapasite ? eü->yama_kapasite * 2 : 256;
        eü->yamalar = realloc(eü->yamalar, yeni_kap * sizeof(eü->yamalar[0]));
        eü->yama_kapasite = yeni_kap;
    }
    eü->yamalar[eü->yama_sayısı].kod_ofseti = kod_ofseti;
    eü->yamalar[eü->yama_sayısı].etiket_no = etiket_no;
    eü->yama_sayısı++;
}

static void yamaları_uygula(ElfÜretici *eü) {
    for (int i = 0; i < eü->yama_sayısı; i++) {
        int etiket = eü->yamalar[i].etiket_no;
        int hedef = eü->etiket_ofsetler[etiket];
        if (hedef >= 0) {
            x86_atlama_yamala(&eü->kod, eü->yamalar[i].kod_ofseti, hedef);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     İŞLEV TABLOSU
 * ═══════════════════════════════════════════════════════════════════════════ */

static void işlev_tablosu_ekle(ElfÜretici *eü, const char *isim, int etiket) {
    if (eü->işlev_sayısı >= eü->işlev_kapasite) {
        int yeni_kap = eü->işlev_kapasite ? eü->işlev_kapasite * 2 : 64;
        eü->işlev_tablosu = realloc(eü->işlev_tablosu, yeni_kap * sizeof(eü->işlev_tablosu[0]));
        eü->işlev_kapasite = yeni_kap;
    }
    eü->işlev_tablosu[eü->işlev_sayısı].isim = arena_strdup(eü->arena, isim);
    eü->işlev_tablosu[eü->işlev_sayısı].etiket = etiket;
    eü->işlev_sayısı++;
}

static int işlev_etiket_bul(ElfÜretici *eü, const char *isim) {
    for (int i = 0; i < eü->işlev_sayısı; i++) {
        if (strcmp(eü->işlev_tablosu[i].isim, isim) == 0)
            return eü->işlev_tablosu[i].etiket;
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     METİN / ONDALIK SABİT TABLOSU
 * ═══════════════════════════════════════════════════════════════════════════ */

static int metin_literal_ekle(ElfÜretici *eü, const char *metin, int kaynak_uzunluk) {
    if (eü->metin_sayaç >= eü->metin_kapasite) {
        int yeni_kap = eü->metin_kapasite ? eü->metin_kapasite * 2 : 64;
        eü->metin_tablosu = realloc(eü->metin_tablosu, yeni_kap * sizeof(eü->metin_tablosu[0]));
        eü->metin_kapasite = yeni_kap;
    }
    int idx = eü->metin_sayaç++;
    int rodata_başlangıç = eü->salt_okunur.uzunluk;
    int gerçek_uzunluk = 0;

    for (int i = 0; i < kaynak_uzunluk; i++) {
        unsigned char c = (unsigned char)metin[i];
        if (c == '\\' && i + 1 < kaynak_uzunluk) {
            i++;
            switch (metin[i]) {
                case 'n':  ikil_byte_ekle(&eü->salt_okunur, '\n'); break;
                case 't':  ikil_byte_ekle(&eü->salt_okunur, '\t'); break;
                case 'r':  ikil_byte_ekle(&eü->salt_okunur, '\r'); break;
                case '\\': ikil_byte_ekle(&eü->salt_okunur, '\\'); break;
                case '"':  ikil_byte_ekle(&eü->salt_okunur, '"');  break;
                case '0':  ikil_byte_ekle(&eü->salt_okunur, '\0'); break;
                default:   ikil_byte_ekle(&eü->salt_okunur, (uint8_t)metin[i]); break;
            }
        } else {
            ikil_byte_ekle(&eü->salt_okunur, c);
        }
        gerçek_uzunluk++;
    }

    eü->metin_tablosu[idx].rodata_ofseti = rodata_başlangıç;
    eü->metin_tablosu[idx].uzunluk = gerçek_uzunluk;
    return idx;
}

static int ondalık_sabit_ekle(ElfÜretici *eü, double değer) {
    if (eü->ondalık_sayaç >= eü->ondalık_kapasite) {
        int yeni_kap = eü->ondalık_kapasite ? eü->ondalık_kapasite * 2 : 64;
        eü->ondalık_tablosu = realloc(eü->ondalık_tablosu, yeni_kap * sizeof(eü->ondalık_tablosu[0]));
        eü->ondalık_kapasite = yeni_kap;
    }
    int idx = eü->ondalık_sayaç++;
    /* 8-byte hizalama */
    while (eü->salt_okunur.uzunluk % 8 != 0)
        ikil_byte_ekle(&eü->salt_okunur, 0);
    eü->ondalık_tablosu[idx].rodata_ofseti = eü->salt_okunur.uzunluk;
    /* IEEE 754 double olarak yaz */
    uint64_t ham;
    memcpy(&ham, &değer, 8);
    ikil_qword_ekle(&eü->salt_okunur, ham);
    return idx;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     GLOBAL DEĞİŞKEN (BSS) YÖNETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

static int genel_değişken_bul(ElfÜretici *eü, const char *isim) {
    for (int i = 0; i < eü->genel_değişken_sayısı; i++)
        if (strcmp(eü->genel_değişkenler[i].isim, isim) == 0)
            return i;
    return -1;
}

static int genel_değişken_ekle(ElfÜretici *eü, const char *isim, int boyut) {
    int mevcut = genel_değişken_bul(eü, isim);
    if (mevcut >= 0) return mevcut;

    if (eü->genel_değişken_sayısı >= eü->genel_değişken_kapasite) {
        int yeni_kap = eü->genel_değişken_kapasite ? eü->genel_değişken_kapasite * 2 : 64;
        eü->genel_değişkenler = realloc(eü->genel_değişkenler, yeni_kap * sizeof(eü->genel_değişkenler[0]));
        eü->genel_değişken_kapasite = yeni_kap;
    }
    int idx = eü->genel_değişken_sayısı++;
    /* 8-byte hizala */
    eü->bss_boyut = (eü->bss_boyut + 7) & ~7;
    eü->genel_değişkenler[idx].isim = arena_strdup(eü->arena, isim);
    eü->genel_değişkenler[idx].bss_ofseti = eü->bss_boyut;
    eü->genel_değişkenler[idx].boyut = boyut;
    eü->bss_boyut += boyut;
    return idx;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     RIP-BAĞIL YAMA YÖNETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

static void rodata_yama_ekle(ElfÜretici *eü, int kod_ofseti, int rodata_ofseti) {
    if (eü->rodata_yama_sayısı >= eü->rodata_yama_kapasite) {
        int yeni_kap = eü->rodata_yama_kapasite ? eü->rodata_yama_kapasite * 2 : 128;
        eü->rodata_yamaları = realloc(eü->rodata_yamaları, yeni_kap * sizeof(eü->rodata_yamaları[0]));
        eü->rodata_yama_kapasite = yeni_kap;
    }
    eü->rodata_yamaları[eü->rodata_yama_sayısı].kod_ofseti = kod_ofseti;
    eü->rodata_yamaları[eü->rodata_yama_sayısı].rodata_ofseti = rodata_ofseti;
    eü->rodata_yama_sayısı++;
}

static void bss_yama_ekle(ElfÜretici *eü, int kod_ofseti, int bss_indeksi, int ek_ofset) {
    if (eü->bss_yama_sayısı >= eü->bss_yama_kapasite) {
        int yeni_kap = eü->bss_yama_kapasite ? eü->bss_yama_kapasite * 2 : 128;
        eü->bss_yamaları = realloc(eü->bss_yamaları, yeni_kap * sizeof(eü->bss_yamaları[0]));
        eü->bss_yama_kapasite = yeni_kap;
    }
    eü->bss_yamaları[eü->bss_yama_sayısı].kod_ofseti = kod_ofseti;
    eü->bss_yamaları[eü->bss_yama_sayısı].bss_indeksi = bss_indeksi;
    eü->bss_yamaları[eü->bss_yama_sayısı].ek_ofset = ek_ofset;
    eü->bss_yama_sayısı++;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     İLERİ BİLDİRİMLER
 * ═══════════════════════════════════════════════════════════════════════════ */

static void ifade_üret_elf(ElfÜretici *eü, Düğüm *d);
static void bildirim_üret_elf(ElfÜretici *eü, Düğüm *d);
static void blok_üret_elf(ElfÜretici *eü, Düğüm *d);

/* ═══════════════════════════════════════════════════════════════════════════
 *                     YARDIMCI FONKSİYON ÜRETİMİ
 *
 * Bunlar çalışma zamanında kullanılan, makine koduna gömülü fonksiyonlar.
 * libc yerine doğrudan Linux syscall kullanırlar.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* _yazdır_tam: tam sayıyı stdout'a yaz
 * Giriş: rdi = tam sayı değeri
 * Algoritmа: sayıyı 10'a bölüp rakamları yığın üzerinde oluştur, sys_write ile yaz */
static void yazdır_tam_üret(ElfÜretici *eü) {
    etiket_tanımla(eü, eü->yazdır_tam_etiket);

    /* pushq %rbp; movq %rsp, %rbp */
    x86_yığına_it(&eü->kod, YAZ_RBP);
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RBP, YAZ_RSP);
    /* sub rsp, 32 — yığın üzerinde buffer */
    x86_çıkar_yazmaç_sabit32(&eü->kod, YAZ_RSP, 32);

    /* rax = rdi (sayı) */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RDI);

    /* r10 = 0 (negatif bayrağı) */
    x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_R10, YAZ_R10);
    /* r11 = buffer sonu (rbp) */
    x86_adres_yükle(&eü->kod, YAZ_R11, YAZ_RBP, 0);

    /* Negatif kontrolü */
    x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
    int pos_atlama = x86_atla_büyük_eşitse(&eü->kod);

    /* Negatifse: r10 = 1, rax = -rax */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_R10, 1);
    x86_olumsuzla(&eü->kod, YAZ_RAX);

    int döngü_etiket = yeni_etiket(eü);
    int döngü_son_etiket = yeni_etiket(eü);

    /* Pozitif giriş noktası */
    x86_atlama_yamala(&eü->kod, pos_atlama, eü->kod.uzunluk);

    /* Sıfır kontrolü */
    x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
    int sıfır_atlama = x86_atla_eşit_değilse(&eü->kod);

    /* Sıfırsa: '0' yaz (tek byte) */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, '0');
    x86_taşı_bellek_yazmaç_byte(&eü->kod, YAZ_RBP, -1, YAZ_RAX);
    x86_adres_yükle(&eü->kod, YAZ_RSI, YAZ_RBP, -1);
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RDX, 1);
    int sıfır_yaz_atla = x86_atla(&eü->kod);

    x86_atlama_yamala(&eü->kod, sıfır_atlama, eü->kod.uzunluk);

    /* rcx = 0 (rakam sayacı) */
    x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RCX, YAZ_RCX);
    /* r9 = 10 (bölen) */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_R9, 10);

    /* Döngü: rakamları çıkar */
    etiket_tanımla(eü, döngü_etiket);
    x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
    int son_atlama = x86_atla_eşitse(&eü->kod);
    yama_ekle(eü, son_atlama, döngü_son_etiket);

    /* rdx:rax / r9 → rax=bölüm, rdx=kalan */
    x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RDX, YAZ_RDX);
    x86_işaret_genişlet(&eü->kod);
    x86_böl_yazmaç(&eü->kod, YAZ_R9);

    /* kalan + '0' → yığına it */
    x86_topla_yazmaç_sabit32(&eü->kod, YAZ_RDX, '0');
    x86_yığına_it(&eü->kod, YAZ_RDX);
    x86_topla_yazmaç_sabit32(&eü->kod, YAZ_RCX, 1);

    int geri_atlama = x86_atla(&eü->kod);
    yama_ekle(eü, geri_atlama, döngü_etiket);

    /* Döngü sonu: rakamları buffer'a yaz */
    etiket_tanımla(eü, döngü_son_etiket);

    /* rsi = r11 - rcx - r10 (negatif için -1) */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDX, YAZ_RCX);  /* rdx = rakam sayısı */
    x86_topla_yazmaç_yazmaç(&eü->kod, YAZ_RDX, YAZ_R10);  /* + negatif bayrağı */

    /* Rakamları buffer'a kopyala */
    /* rsi = rbp - rdx (buffer başlangıcı) */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RBP);
    x86_çıkar_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RDX);

    /* Negatifse '-' ekle */
    x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_R10, YAZ_R10);
    int eksi_atla = x86_atla_eşitse(&eü->kod);
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, '-');
    x86_taşı_bellek_yazmaç_byte(&eü->kod, YAZ_RSI, 0, YAZ_RAX);
    x86_topla_yazmaç_sabit32(&eü->kod, YAZ_RSI, 1);
    x86_atlama_yamala(&eü->kod, eksi_atla, eü->kod.uzunluk);

    /* Rakamları yığından çekip buffer'a yaz */
    /* r8 = rsi (yazma işaretçisi) */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_R8, YAZ_RSI);

    int kopyala_etiket = yeni_etiket(eü);
    int kopyala_son = yeni_etiket(eü);
    etiket_tanımla(eü, kopyala_etiket);
    x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RCX, YAZ_RCX);
    int k_atlama = x86_atla_eşitse(&eü->kod);
    yama_ekle(eü, k_atlama, kopyala_son);

    x86_yığından_çek(&eü->kod, YAZ_RAX);
    x86_taşı_bellek_yazmaç_byte(&eü->kod, YAZ_R8, 0, YAZ_RAX);
    x86_topla_yazmaç_sabit32(&eü->kod, YAZ_R8, 1);
    x86_çıkar_yazmaç_sabit32(&eü->kod, YAZ_RCX, 1);
    int k_geri = x86_atla(&eü->kod);
    yama_ekle(eü, k_geri, kopyala_etiket);

    etiket_tanımla(eü, kopyala_son);

    /* rsi = buffer başlangıcı (rbp - toplam uzunluk) */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RBP);
    x86_çıkar_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RDX);

    /* Sıfır yaz birleşme noktası */
    x86_atlama_yamala(&eü->kod, sıfır_yaz_atla, eü->kod.uzunluk);

    /* sys_write(1, rsi, rdx) */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, 1);   /* sys_write */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RDI, 1);   /* stdout */
    /* rsi ve rdx zaten ayarlı */
    x86_sistem_çağrısı(&eü->kod);

    /* Yeni satır yaz */
    int ys_yama = x86_adres_yükle_rip_bağıl(&eü->kod, YAZ_RSI);
    rodata_yama_ekle(eü, ys_yama, eü->yeni_satır_ofseti);
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, 1);
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RDI, 1);
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RDX, 1);
    x86_sistem_çağrısı(&eü->kod);

    /* Epilog */
    x86_ayrıl(&eü->kod);
    x86_dön(&eü->kod);
}

/* _yazdır_metin: stringi stdout'a yaz + yeni satır
 * Giriş: rdi = string işaretçisi, rsi = uzunluk */
static void yazdır_metin_üret(ElfÜretici *eü) {
    etiket_tanımla(eü, eü->yazdır_metin_etiket);

    x86_yığına_it(&eü->kod, YAZ_RBP);
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RBP, YAZ_RSP);

    /* sys_write(1, rdi, rsi) */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDX, YAZ_RSI);  /* rdx = uzunluk */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RDI);  /* rsi = buffer */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, 1);        /* sys_write */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RDI, 1);        /* stdout */
    x86_sistem_çağrısı(&eü->kod);

    /* Yeni satır yaz */
    int ys_yama = x86_adres_yükle_rip_bağıl(&eü->kod, YAZ_RSI);
    rodata_yama_ekle(eü, ys_yama, eü->yeni_satır_ofseti);
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, 1);
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RDI, 1);
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RDX, 1);
    x86_sistem_çağrısı(&eü->kod);

    x86_ayrıl(&eü->kod);
    x86_dön(&eü->kod);

}

/* _bellek_ayır: mmap syscall ile bellek ayır
 * Giriş: rdi = boyut
 * Çıkış: rax = bellek adresi */
static void bellek_ayır_üret(ElfÜretici *eü) {
    etiket_tanımla(eü, eü->bellek_ayır_etiket);

    x86_yığına_it(&eü->kod, YAZ_RBP);
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RBP, YAZ_RSP);

    /* mmap(NULL, boyut, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RDI);  /* rsi = boyut */
    x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RDI, YAZ_RDI);  /* rdi = NULL */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RDX, 3);        /* PROT_READ|PROT_WRITE */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_R10, 0x22);     /* MAP_PRIVATE|MAP_ANONYMOUS */
    x86_taşı_yazmaç_sabit64(&eü->kod, YAZ_R8, -1);        /* fd = -1 */
    x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_R9, YAZ_R9);  /* offset = 0 */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, 9);        /* sys_mmap */
    x86_sistem_çağrısı(&eü->kod);

    x86_ayrıl(&eü->kod);
    x86_dön(&eü->kod);
}

/* _metin_birleştir: iki stringi birleştir
 * Giriş: rdi=sol_ptr, rsi=sol_uzunluk, rdx=sağ_ptr, rcx=sağ_uzunluk
 * Çıkış: rax=yeni_ptr, rbx=yeni_uzunluk */
static void metin_birleştir_üret(ElfÜretici *eü) {
    /* bellek_ayır da gerekli — kod_üret_elf64 sırasında üretilecek */
    if (!eü->bellek_ayır_üretildi) {
        eü->bellek_ayır_etiket = yeni_etiket(eü);
        eü->bellek_ayır_üretildi = 1;
    }

    etiket_tanımla(eü, eü->metin_birleştir_etiket);

    x86_yığına_it(&eü->kod, YAZ_RBP);
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RBP, YAZ_RSP);
    x86_çıkar_yazmaç_sabit32(&eü->kod, YAZ_RSP, 48);

    /* Parametreleri kaydet */
    x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -8,  YAZ_RDI);  /* sol_ptr */
    x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -16, YAZ_RSI);  /* sol_uzunluk */
    x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -24, YAZ_RDX);  /* sağ_ptr */
    x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -32, YAZ_RCX);  /* sağ_uzunluk */

    /* Toplam uzunluk hesapla */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDI, YAZ_RSI);
    x86_topla_yazmaç_yazmaç(&eü->kod, YAZ_RDI, YAZ_RCX);
    x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -40, YAZ_RDI);  /* toplam */

    /* Bellek ayır */
    int çağrı_yama = x86_çağır_yakın(&eü->kod);
    yama_ekle(eü, çağrı_yama, eü->bellek_ayır_etiket);

    /* rax = yeni buffer, r12 = yeni buffer (sakla) */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_R12, YAZ_RAX);

    /* Sol stringi kopyala: rep movsb (rsi→rdi, rcx adet) */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDI, YAZ_RAX);
    x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RSI, YAZ_RBP, -8);
    x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RCX, YAZ_RBP, -16);
    x86_tekrarla_kopyala(&eü->kod);

    /* Sağ stringi kopyala (rdi zaten doğru konumda) */
    x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RSI, YAZ_RBP, -24);
    x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RCX, YAZ_RBP, -32);
    x86_tekrarla_kopyala(&eü->kod);

    /* Sonuç: rax=ptr, rbx=uzunluk */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_R12);
    x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RBX, YAZ_RBP, -40);

    x86_ayrıl(&eü->kod);
    x86_dön(&eü->kod);
}

/* _metin_karşılaştır: iki stringi karşılaştır
 * Giriş: rdi=sol_ptr, rsi=sol_uzunluk, rdx=sağ_ptr, rcx=sağ_uzunluk
 * Çıkış: rax=1 (eşit) veya 0 (farklı) */
static void metin_karşılaştır_üret(ElfÜretici *eü) {
    etiket_tanımla(eü, eü->metin_karşılaştır_etiket);

    x86_yığına_it(&eü->kod, YAZ_RBP);
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RBP, YAZ_RSP);

    /* Uzunluk kontrolü: sol_uz != sağ_uz → farklı */
    x86_karşılaştır_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RCX);
    int farklı_atla = x86_atla_eşit_değilse(&eü->kod);

    /* Byte-byte karşılaştır: repe cmpsb (rsi vs rdi, rcx adet) */
    /* rsi→rdi karşılaştırması: rsi=sol, rdi=sağ, rcx=uzunluk */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RCX, YAZ_RSI);  /* rcx = uzunluk */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RDI);  /* rsi = sol_ptr */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDI, YAZ_RDX);  /* rdi = sağ_ptr */
    /* repe cmpsb: F3 A6 */
    ikil_byte_ekle(&eü->kod, 0xF3);
    ikil_byte_ekle(&eü->kod, 0xA6);

    /* Eşitse rax=1 */
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, 0);
    x86_koşul_eşit(&eü->kod, YAZ_RAX);
    x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
    int son_atla = x86_atla(&eü->kod);

    /* Uzunluk farklı: rax=0 */
    x86_atlama_yamala(&eü->kod, farklı_atla, eü->kod.uzunluk);
    x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);

    x86_atlama_yamala(&eü->kod, son_atla, eü->kod.uzunluk);

    x86_ayrıl(&eü->kod);
    x86_dön(&eü->kod);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     İFADE ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

static void ifade_üret_elf(ElfÜretici *eü, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
    case DÜĞÜM_TAM_SAYI:
        x86_taşı_yazmaç_sabit64(&eü->kod, YAZ_RAX, d->veri.tam_deger);
        break;

    case DÜĞÜM_ONDALIK_SAYI: {
        int idx = ondalık_sabit_ekle(eü, d->veri.ondalık_değer);
        int yama = x86_ondalık_taşı_rip_bağıl(&eü->kod, YAZ_XMM0);
        rodata_yama_ekle(eü, yama, eü->ondalık_tablosu[idx].rodata_ofseti);
        break;
    }

    case DÜĞÜM_METİN_DEĞERİ: {
        char *metin = d->veri.metin_değer;
        int uz = metin ? (int)strlen(metin) : 0;
        int idx = metin_literal_ekle(eü, metin, uz);
        /* LEA rax, [RIP + metin_ofseti] */
        int yama = x86_adres_yükle_rip_bağıl(&eü->kod, YAZ_RAX);
        rodata_yama_ekle(eü, yama, eü->metin_tablosu[idx].rodata_ofseti);
        /* MOV rbx, uzunluk */
        x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RBX, eü->metin_tablosu[idx].uzunluk);
        break;
    }

    case DÜĞÜM_MANTIK_DEĞERİ:
        x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, d->veri.mantık_değer ? 1 : 0);
        break;

    case DÜĞÜM_BOŞ_DEĞER:
        x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
        x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RBX, YAZ_RBX);
        break;

    case DÜĞÜM_TANIMLAYICI: {
        char *isim = d->veri.tanimlayici.isim;
        Sembol *s = sembol_ara(eü->kapsam, isim);
        if (!s) break;

        if (s->global_mi) {
            int bss_idx = genel_değişken_bul(eü, isim);
            if (bss_idx < 0) break;
            if (s->tip == TİP_ONDALIK) {
                int yama = x86_ondalık_taşı_rip_bağıl(&eü->kod, YAZ_XMM0);
                bss_yama_ekle(eü, yama, bss_idx, 0);
            } else if (s->tip == TİP_METİN) {
                int yama1 = x86_taşı_yazmaç_rip_bağıl(&eü->kod, YAZ_RAX);
                bss_yama_ekle(eü, yama1, bss_idx, 0);
                int yama2 = x86_taşı_yazmaç_rip_bağıl(&eü->kod, YAZ_RBX);
                bss_yama_ekle(eü, yama2, bss_idx, 8);
            } else {
                int yama = x86_taşı_yazmaç_rip_bağıl(&eü->kod, YAZ_RAX);
                bss_yama_ekle(eü, yama, bss_idx, 0);
            }
        } else {
            int ofset = (s->yerel_indeks + 1) * 8;
            if (s->tip == TİP_ONDALIK) {
                x86_ondalık_taşı_yazmaç_bellek(&eü->kod, YAZ_XMM0, YAZ_RBP, -ofset);
            } else if (s->tip == TİP_METİN) {
                x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RAX, YAZ_RBP, -ofset);
                x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RBX, YAZ_RBP, -(ofset + 8));
            } else {
                x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RAX, YAZ_RBP, -ofset);
            }
        }
        break;
    }

    case DÜĞÜM_İKİLİ_İŞLEM: {
        if (d->çocuk_sayısı < 2) break;
        Düğüm *sol = d->çocuklar[0];
        Düğüm *sağ = d->çocuklar[1];
        SözcükTürü işlem = d->veri.islem.islem;

        /* Mantık kısa devre: VE ve VEYA */
        if (işlem == TOK_VE) {
            ifade_üret_elf(eü, sol);
            x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
            int kısa_devre = x86_atla_eşitse(&eü->kod);
            ifade_üret_elf(eü, sağ);
            x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
            x86_koşul_eşit_değil(&eü->kod, YAZ_RAX);
            x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
            int son = x86_atla(&eü->kod);
            x86_atlama_yamala(&eü->kod, kısa_devre, eü->kod.uzunluk);
            x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
            x86_atlama_yamala(&eü->kod, son, eü->kod.uzunluk);
            break;
        }
        if (işlem == TOK_VEYA) {
            ifade_üret_elf(eü, sol);
            x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
            int kısa_devre = x86_atla_eşit_değilse(&eü->kod);
            ifade_üret_elf(eü, sağ);
            x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
            x86_koşul_eşit_değil(&eü->kod, YAZ_RAX);
            x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
            int son = x86_atla(&eü->kod);
            x86_atlama_yamala(&eü->kod, kısa_devre, eü->kod.uzunluk);
            x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, 1);
            x86_atlama_yamala(&eü->kod, son, eü->kod.uzunluk);
            break;
        }

        /* Ondalık aritmetik mi? */
        int ondalık_mı = (sol->sonuç_tipi == TİP_ONDALIK || sağ->sonuç_tipi == TİP_ONDALIK);

        if (ondalık_mı) {
            ifade_üret_elf(eü, sol);
            /* xmm0'ı yığına kaydet */
            x86_çıkar_yazmaç_sabit32(&eü->kod, YAZ_RSP, 8);
            x86_ondalık_taşı_bellek_yazmaç(&eü->kod, YAZ_RSP, 0, YAZ_XMM0);

            ifade_üret_elf(eü, sağ);
            x86_ondalık_taşı_yazmaç_yazmaç(&eü->kod, YAZ_XMM1, YAZ_XMM0);

            /* Sol değeri geri yükle */
            x86_ondalık_taşı_yazmaç_bellek(&eü->kod, YAZ_XMM0, YAZ_RSP, 0);
            x86_topla_yazmaç_sabit32(&eü->kod, YAZ_RSP, 8);

            switch (işlem) {
            case TOK_ARTI:   x86_ondalık_topla(&eü->kod, YAZ_XMM0, YAZ_XMM1); break;
            case TOK_EKSI:   x86_ondalık_çıkar(&eü->kod, YAZ_XMM0, YAZ_XMM1); break;
            case TOK_ÇARPIM: x86_ondalık_çarp(&eü->kod, YAZ_XMM0, YAZ_XMM1); break;
            case TOK_BÖLME:  x86_ondalık_böl(&eü->kod, YAZ_XMM0, YAZ_XMM1); break;
            case TOK_EŞİT_EŞİT: case TOK_EŞİT_DEĞİL:
            case TOK_KÜÇÜK: case TOK_BÜYÜK:
            case TOK_KÜÇÜK_EŞİT: case TOK_BÜYÜK_EŞİT:
                x86_ondalık_karşılaştır(&eü->kod, YAZ_XMM0, YAZ_XMM1);
                switch (işlem) {
                case TOK_EŞİT_EŞİT:   x86_koşul_eşit(&eü->kod, YAZ_RAX); break;
                case TOK_EŞİT_DEĞİL:  x86_koşul_eşit_değil(&eü->kod, YAZ_RAX); break;
                case TOK_KÜÇÜK:        ikil_byte_ekle(&eü->kod, 0x0F); ikil_byte_ekle(&eü->kod, 0x92); ikil_byte_ekle(&eü->kod, 0xC0); break; /* SETB al */
                case TOK_BÜYÜK:        ikil_byte_ekle(&eü->kod, 0x0F); ikil_byte_ekle(&eü->kod, 0x97); ikil_byte_ekle(&eü->kod, 0xC0); break; /* SETA al */
                case TOK_KÜÇÜK_EŞİT:  ikil_byte_ekle(&eü->kod, 0x0F); ikil_byte_ekle(&eü->kod, 0x96); ikil_byte_ekle(&eü->kod, 0xC0); break; /* SETBE al */
                case TOK_BÜYÜK_EŞİT:  ikil_byte_ekle(&eü->kod, 0x0F); ikil_byte_ekle(&eü->kod, 0x93); ikil_byte_ekle(&eü->kod, 0xC0); break; /* SETAE al */
                default: break;
                }
                x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
                break;
            default: break;
            }
            break;
        }

        /* Metin birleştirme */
        if (işlem == TOK_ARTI &&
            (sol->sonuç_tipi == TİP_METİN || sağ->sonuç_tipi == TİP_METİN)) {
            if (!eü->metin_birleştir_üretildi) {
                eü->metin_birleştir_etiket = yeni_etiket(eü);
                eü->metin_birleştir_üretildi = 1;
            }
            /* Sol: rax=ptr, rbx=len → yığına kaydet */
            ifade_üret_elf(eü, sol);
            x86_yığına_it(&eü->kod, YAZ_RAX);
            x86_yığına_it(&eü->kod, YAZ_RBX);
            /* Sağ: rax=ptr, rbx=len */
            ifade_üret_elf(eü, sağ);
            x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDX, YAZ_RAX);  /* sağ_ptr */
            x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RCX, YAZ_RBX);  /* sağ_uz */
            x86_yığından_çek(&eü->kod, YAZ_RSI);  /* sol_uz */
            x86_yığından_çek(&eü->kod, YAZ_RDI);  /* sol_ptr */
            int yama = x86_çağır_yakın(&eü->kod);
            yama_ekle(eü, yama, eü->metin_birleştir_etiket);
            break;
        }

        /* Metin karşılaştırma */
        if ((işlem == TOK_EŞİT_EŞİT || işlem == TOK_EŞİT_DEĞİL) &&
            sol->sonuç_tipi == TİP_METİN) {
            if (!eü->metin_karşılaştır_üretildi) {
                eü->metin_karşılaştır_etiket = yeni_etiket(eü);
                eü->metin_karşılaştır_üretildi = 1;
            }
            ifade_üret_elf(eü, sol);
            x86_yığına_it(&eü->kod, YAZ_RAX);
            x86_yığına_it(&eü->kod, YAZ_RBX);
            ifade_üret_elf(eü, sağ);
            x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDX, YAZ_RAX);
            x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RCX, YAZ_RBX);
            x86_yığından_çek(&eü->kod, YAZ_RSI);
            x86_yığından_çek(&eü->kod, YAZ_RDI);
            int yama = x86_çağır_yakın(&eü->kod);
            yama_ekle(eü, yama, eü->metin_karşılaştır_etiket);
            if (işlem == TOK_EŞİT_DEĞİL) {
                x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
                /* Toggle: xor ile 1 */
                x86_karşılaştır_yazmaç_sabit32(&eü->kod, YAZ_RAX, 0);
                x86_koşul_eşit(&eü->kod, YAZ_RAX);
                x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
            }
            break;
        }

        /* Tam sayı aritmetik */
        ifade_üret_elf(eü, sol);
        x86_yığına_it(&eü->kod, YAZ_RAX);
        ifade_üret_elf(eü, sağ);
        x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RCX, YAZ_RAX);
        x86_yığından_çek(&eü->kod, YAZ_RAX);

        switch (işlem) {
        case TOK_ARTI:   x86_topla_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX); break;
        case TOK_EKSI:   x86_çıkar_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX); break;
        case TOK_ÇARPIM: x86_çarp_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX); break;
        case TOK_BÖLME:
            x86_işaret_genişlet(&eü->kod);
            x86_böl_yazmaç(&eü->kod, YAZ_RCX);
            break;
        case TOK_YÜZDE:
            x86_işaret_genişlet(&eü->kod);
            x86_böl_yazmaç(&eü->kod, YAZ_RCX);
            x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RDX);  /* kalan */
            break;
        case TOK_EŞİT_EŞİT:
            x86_karşılaştır_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX);
            x86_koşul_eşit(&eü->kod, YAZ_RAX);
            x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
            break;
        case TOK_EŞİT_DEĞİL:
            x86_karşılaştır_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX);
            x86_koşul_eşit_değil(&eü->kod, YAZ_RAX);
            x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
            break;
        case TOK_KÜÇÜK:
            x86_karşılaştır_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX);
            x86_koşul_küçük(&eü->kod, YAZ_RAX);
            x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
            break;
        case TOK_BÜYÜK:
            x86_karşılaştır_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX);
            x86_koşul_büyük(&eü->kod, YAZ_RAX);
            x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
            break;
        case TOK_KÜÇÜK_EŞİT:
            x86_karşılaştır_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX);
            x86_koşul_küçük_eşit(&eü->kod, YAZ_RAX);
            x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
            break;
        case TOK_BÜYÜK_EŞİT:
            x86_karşılaştır_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX);
            x86_koşul_büyük_eşit(&eü->kod, YAZ_RAX);
            x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
            break;
        /* Bit işlemleri */
        case TOK_BİT_VE:    x86_ve_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX); break;
        case TOK_BİT_VEYA:  x86_veya_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX); break;
        case TOK_BİT_XOR:   x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX); break;
        case TOK_SOL_KAYDIR:
            x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RCX, YAZ_RCX);  /* cl'de zaten */
            x86_sola_kaydır_cl(&eü->kod, YAZ_RAX);
            break;
        case TOK_SAĞ_KAYDIR:
            x86_sağa_kaydır_cl(&eü->kod, YAZ_RAX);
            break;
        default: break;
        }
        break;
    }

    case DÜĞÜM_TEKLİ_İŞLEM: {
        if (d->çocuk_sayısı < 1) break;
        ifade_üret_elf(eü, d->çocuklar[0]);
        SözcükTürü işlem = d->veri.islem.islem;
        if (işlem == TOK_EKSI) {
            x86_olumsuzla(&eü->kod, YAZ_RAX);
        } else if (işlem == TOK_DEĞİL) {
            x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
            x86_koşul_eşit(&eü->kod, YAZ_RAX);
            x86_sıfır_genişlet_byte(&eü->kod, YAZ_RAX, YAZ_RAX);
        } else if (işlem == TOK_BİT_DEĞİL) {
            x86_değillebitsel(&eü->kod, YAZ_RAX);
        }
        break;
    }

    case DÜĞÜM_ÇAĞRI: {
        if (!d->veri.tanimlayici.isim) break;
        char *isim = d->veri.tanimlayici.isim;

        /* yazdır yerleşik fonksiyonu */
        if (strcmp(isim, "yazdır") == 0 || strcmp(isim, "yazdir") == 0) {
            if (d->çocuk_sayısı < 1) break;
            Düğüm *arg = d->çocuklar[0];
            ifade_üret_elf(eü, arg);

            if (arg->sonuç_tipi == TİP_METİN || arg->tur == DÜĞÜM_METİN_DEĞERİ) {
                /* Sadece etiket oluştur, kod en sonda üretilecek */
                if (!eü->yazdır_metin_üretildi) {
                    eü->yazdır_metin_etiket = yeni_etiket(eü);
                    eü->yazdır_metin_üretildi = 1;
                }
                x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDI, YAZ_RAX);
                x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RBX);
                int yama = x86_çağır_yakın(&eü->kod);
                yama_ekle(eü, yama, eü->yazdır_metin_etiket);
            } else if (arg->sonuç_tipi == TİP_ONDALIK || arg->tur == DÜĞÜM_ONDALIK_SAYI) {
                if (!eü->yazdır_tam_üretildi) {
                    eü->yazdır_tam_etiket = yeni_etiket(eü);
                    eü->yazdır_tam_üretildi = 1;
                }
                x86_ondalık_tama_çevir(&eü->kod, YAZ_RDI, YAZ_XMM0);
                int yama = x86_çağır_yakın(&eü->kod);
                yama_ekle(eü, yama, eü->yazdır_tam_etiket);
            } else if (arg->sonuç_tipi == TİP_MANTIK) {
                /* Mantık: doğru/yanlış metin olarak yazdır */
                if (!eü->yazdır_metin_üretildi) {
                    eü->yazdır_metin_etiket = yeni_etiket(eü);
                    eü->yazdır_metin_üretildi = 1;
                }
                int doğru_idx = metin_literal_ekle(eü, "doğru", 7);  /* UTF-8: 7 byte */
                int yanlış_idx = metin_literal_ekle(eü, "yanlış", 8); /* UTF-8: 8 byte */

                x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
                int yanlış_atla = x86_atla_eşitse(&eü->kod);

                /* doğru */
                int y1 = x86_adres_yükle_rip_bağıl(&eü->kod, YAZ_RDI);
                rodata_yama_ekle(eü, y1, eü->metin_tablosu[doğru_idx].rodata_ofseti);
                x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RSI, eü->metin_tablosu[doğru_idx].uzunluk);
                int doğru_son = x86_atla(&eü->kod);

                /* yanlış */
                x86_atlama_yamala(&eü->kod, yanlış_atla, eü->kod.uzunluk);
                int y2 = x86_adres_yükle_rip_bağıl(&eü->kod, YAZ_RDI);
                rodata_yama_ekle(eü, y2, eü->metin_tablosu[yanlış_idx].rodata_ofseti);
                x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RSI, eü->metin_tablosu[yanlış_idx].uzunluk);

                x86_atlama_yamala(&eü->kod, doğru_son, eü->kod.uzunluk);
                int yama = x86_çağır_yakın(&eü->kod);
                yama_ekle(eü, yama, eü->yazdır_metin_etiket);
            } else {
                /* Tam sayı */
                if (!eü->yazdır_tam_üretildi) {
                    eü->yazdır_tam_etiket = yeni_etiket(eü);
                    eü->yazdır_tam_üretildi = 1;
                }
                x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDI, YAZ_RAX);
                int yama = x86_çağır_yakın(&eü->kod);
                yama_ekle(eü, yama, eü->yazdır_tam_etiket);
            }
            break;
        }

        /* Kullanıcı tanımlı işlev çağrısı */
        int fn_etiket = işlev_etiket_bul(eü, isim);
        if (fn_etiket < 0) break;

        /* Argüman tiplerini belirle */
        YazmaçNo arg_yazmaçlar[] = { YAZ_RDI, YAZ_RSI, YAZ_RDX, YAZ_RCX, YAZ_R8, YAZ_R9 };
        int arg_sayısı = d->çocuk_sayısı;

        /* Fonksiyon sembolünden tip bilgisi al */
        Sembol *fn_sym = sembol_ara(eü->kapsam, isim);
        TipTürü arg_tipleri[6] = {0};
        for (int i = 0; i < arg_sayısı && i < 6; i++) {
            if (fn_sym && i < fn_sym->param_sayisi)
                arg_tipleri[i] = fn_sym->param_tipleri[i];
            else if (d->çocuklar[i]->sonuç_tipi != TİP_BİLİNMİYOR)
                arg_tipleri[i] = d->çocuklar[i]->sonuç_tipi;
            else
                arg_tipleri[i] = TİP_TAM;
        }

        /* Argümanları ters sırayla hesapla ve yığına it */
        for (int i = arg_sayısı - 1; i >= 0; i--) {
            ifade_üret_elf(eü, d->çocuklar[i]);
            if (arg_tipleri[i] == TİP_METİN) {
                x86_yığına_it(&eü->kod, YAZ_RBX);  /* length */
                x86_yığına_it(&eü->kod, YAZ_RAX);  /* pointer */
            } else {
                x86_yığına_it(&eü->kod, YAZ_RAX);
            }
        }

        /* Yığından argüman yazmaçlarına yükle */
        int reg_idx = 0;
        for (int i = 0; i < arg_sayısı && reg_idx < 6; i++) {
            if (arg_tipleri[i] == TİP_METİN && reg_idx + 1 < 6) {
                x86_yığından_çek(&eü->kod, arg_yazmaçlar[reg_idx]);      /* ptr */
                reg_idx++;
                x86_yığından_çek(&eü->kod, arg_yazmaçlar[reg_idx]);      /* len */
                reg_idx++;
            } else {
                x86_yığından_çek(&eü->kod, arg_yazmaçlar[reg_idx]);
                reg_idx++;
            }
        }

        int yama = x86_çağır_yakın(&eü->kod);
        yama_ekle(eü, yama, fn_etiket);
        break;
    }

    case DÜĞÜM_ATAMA: {
        /* AST yapısı: veri.tanimlayici.isim = hedef, çocuklar[0] = değer ifadesi */
        if (d->çocuk_sayısı < 1) break;
        char *isim = d->veri.tanimlayici.isim;
        if (!isim) break;
        Sembol *s = sembol_ara(eü->kapsam, isim);
        if (!s) break;

        ifade_üret_elf(eü, d->çocuklar[0]);

        if (s->global_mi) {
            int bss_idx = genel_değişken_bul(eü, isim);
            if (bss_idx < 0) break;
            if (s->tip == TİP_ONDALIK) {
                int yama = x86_ondalık_taşı_rip_bağıl(&eü->kod, YAZ_XMM0);
                /* Bu MOV [RIP], xmm olmalı — ancak basitlik için şimdilik atla */
                (void)yama;
            } else if (s->tip == TİP_METİN) {
                int y1 = x86_taşı_rip_bağıl_yazmaç(&eü->kod, YAZ_RAX);
                bss_yama_ekle(eü, y1, bss_idx, 0);
                int y2 = x86_taşı_rip_bağıl_yazmaç(&eü->kod, YAZ_RBX);
                bss_yama_ekle(eü, y2, bss_idx, 8);
            } else {
                int yama = x86_taşı_rip_bağıl_yazmaç(&eü->kod, YAZ_RAX);
                bss_yama_ekle(eü, yama, bss_idx, 0);
            }
        } else {
            int ofset = (s->yerel_indeks + 1) * 8;
            if (s->tip == TİP_ONDALIK) {
                x86_ondalık_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, YAZ_XMM0);
            } else if (s->tip == TİP_METİN) {
                x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, YAZ_RAX);
                x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -(ofset + 8), YAZ_RBX);
            } else {
                x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, YAZ_RAX);
            }
        }
        break;
    }

    default:
        /* Desteklenmeyen düğüm türü — sessizce atla */
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     BİLDİRİM ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

static void bildirim_üret_elf(ElfÜretici *eü, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
    case DÜĞÜM_DEĞİŞKEN: {
        char *isim = d->veri.değişken.isim;
        char *tip_adi = d->veri.değişken.tip;
        TipTürü tip = tip_adi ? tip_adı_çevir(tip_adi) : TİP_TAM;
        int genel = d->veri.değişken.genel;

        if (genel) {
            int boyut = (tip == TİP_METİN) ? 16 : 8;
            int bss_idx = genel_değişken_ekle(eü, isim, boyut);

            /* Sembol tablosuna ekle */
            Sembol *s = sembol_ekle(eü->arena, eü->kapsam, isim, tip);
            s->global_mi = 1;

            if (d->çocuk_sayısı > 0) {
                ifade_üret_elf(eü, d->çocuklar[0]);
                if (tip == TİP_METİN) {
                    int y1 = x86_taşı_rip_bağıl_yazmaç(&eü->kod, YAZ_RAX);
                    bss_yama_ekle(eü, y1, bss_idx, 0);
                    int y2 = x86_taşı_rip_bağıl_yazmaç(&eü->kod, YAZ_RBX);
                    bss_yama_ekle(eü, y2, bss_idx, 8);
                } else if (tip == TİP_ONDALIK) {
                    /* Geçici: ondalık global atama */
                } else {
                    int yama = x86_taşı_rip_bağıl_yazmaç(&eü->kod, YAZ_RAX);
                    bss_yama_ekle(eü, yama, bss_idx, 0);
                }
            }
        } else {
            Sembol *s = sembol_ekle(eü->arena, eü->kapsam, isim, tip);
            /* sembol_ekle zaten yerel_indeks ayarlıyor ve yerel_sayac artırıyor */
            if (tip == TİP_METİN) eü->kapsam->yerel_sayac++;  /* ptr + len = 2 slot */

            if (d->çocuk_sayısı > 0) {
                ifade_üret_elf(eü, d->çocuklar[0]);
                int ofset = (s->yerel_indeks + 1) * 8;
                if (tip == TİP_ONDALIK) {
                    x86_ondalık_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, YAZ_XMM0);
                } else if (tip == TİP_METİN) {
                    x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, YAZ_RAX);
                    x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -(ofset + 8), YAZ_RBX);
                } else {
                    x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, YAZ_RAX);
                }
            }
        }
        break;
    }

    case DÜĞÜM_EĞER: {
        /* çocuklar[0]=koşul, çocuklar[1]=doğru blok,
           çocuklar[2..N-1] = yoksa eğer koşul/blok çiftleri veya son yoksa bloğu */
        if (d->çocuk_sayısı < 2) break;
        int yoksa_etiket = yeni_etiket(eü);
        int son_etiket = yeni_etiket(eü);

        ifade_üret_elf(eü, d->çocuklar[0]);
        x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
        int atla = x86_atla_eşitse(&eü->kod);
        yama_ekle(eü, atla, yoksa_etiket);

        /* Doğru blok */
        bildirim_üret_elf(eü, d->çocuklar[1]);
        int son_atla = x86_atla(&eü->kod);
        yama_ekle(eü, son_atla, son_etiket);

        etiket_tanımla(eü, yoksa_etiket);

        /* yoksa eğer / yoksa blokları */
        int ci = 2;
        while (ci < d->çocuk_sayısı) {
            if (d->çocuklar[ci]->tur != DÜĞÜM_BLOK) {
                /* yoksa eğer koşulu */
                int sonraki = yeni_etiket(eü);
                ifade_üret_elf(eü, d->çocuklar[ci]);
                x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
                int yatla = x86_atla_eşitse(&eü->kod);
                yama_ekle(eü, yatla, sonraki);
                ci++;
                if (ci < d->çocuk_sayısı) {
                    bildirim_üret_elf(eü, d->çocuklar[ci]);
                    ci++;
                }
                int satla = x86_atla(&eü->kod);
                yama_ekle(eü, satla, son_etiket);
                etiket_tanımla(eü, sonraki);
            } else {
                /* Son yoksa bloğu */
                bildirim_üret_elf(eü, d->çocuklar[ci]);
                ci++;
            }
        }

        etiket_tanımla(eü, son_etiket);
        break;
    }

    case DÜĞÜM_DÖNGÜ: {
        /* çocuklar[0]=başlangıç, çocuklar[1]=bitiş, çocuklar[2]=gövde (adımsız)
         * çocuklar[0]=başlangıç, çocuklar[1]=bitiş, çocuklar[2]=adım, çocuklar[3]=gövde (adımlı)
         * Adım varsa: çocuk_sayısı > 3 */
        if (d->çocuk_sayısı < 3) break;
        int adim_var = (d->çocuk_sayısı > 3);
        int gövde_idx = adim_var ? 3 : 2;

        char *deg_isim = d->veri.dongu.isim;
        Sembol *s = sembol_ekle(eü->arena, eü->kapsam, deg_isim, TİP_TAM);
        int ofset = (s->yerel_indeks + 1) * 8;

        /* Başlangıç değeri */
        ifade_üret_elf(eü, d->çocuklar[0]);
        x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, YAZ_RAX);

        int başlangıç = yeni_etiket(eü);
        int bitiş = yeni_etiket(eü);

        int eski_başlangıç = eü->döngü_başlangıç_etiket;
        int eski_bitiş = eü->döngü_bitiş_etiket;
        eü->döngü_başlangıç_etiket = başlangıç;
        eü->döngü_bitiş_etiket = bitiş;

        etiket_tanımla(eü, başlangıç);

        /* Bitiş değerini hesapla ve karşılaştır */
        ifade_üret_elf(eü, d->çocuklar[1]);
        x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RCX, YAZ_RAX);
        x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RAX, YAZ_RBP, -ofset);
        x86_karşılaştır_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX);
        int çık_atla = x86_atla_büyükse(&eü->kod);
        yama_ekle(eü, çık_atla, bitiş);

        /* Gövde */
        bildirim_üret_elf(eü, d->çocuklar[gövde_idx]);

        /* Adım */
        x86_taşı_yazmaç_bellek(&eü->kod, YAZ_RAX, YAZ_RBP, -ofset);
        if (adim_var && d->çocuklar[2]) {
            /* Özel adım değeri */
            x86_yığına_it(&eü->kod, YAZ_RAX);
            ifade_üret_elf(eü, d->çocuklar[2]);
            x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RCX, YAZ_RAX);
            x86_yığından_çek(&eü->kod, YAZ_RAX);
            x86_topla_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RCX);
        } else {
            x86_topla_yazmaç_sabit32(&eü->kod, YAZ_RAX, 1);
        }
        x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, YAZ_RAX);

        int geri = x86_atla(&eü->kod);
        yama_ekle(eü, geri, başlangıç);

        etiket_tanımla(eü, bitiş);
        eü->döngü_başlangıç_etiket = eski_başlangıç;
        eü->döngü_bitiş_etiket = eski_bitiş;
        break;
    }

    case DÜĞÜM_İKEN: {
        /* çocuklar[0]=koşul, çocuklar[1]=gövde */
        if (d->çocuk_sayısı < 2) break;

        int başlangıç = yeni_etiket(eü);
        int bitiş = yeni_etiket(eü);

        int eski_başlangıç = eü->döngü_başlangıç_etiket;
        int eski_bitiş = eü->döngü_bitiş_etiket;
        eü->döngü_başlangıç_etiket = başlangıç;
        eü->döngü_bitiş_etiket = bitiş;

        etiket_tanımla(eü, başlangıç);
        ifade_üret_elf(eü, d->çocuklar[0]);
        x86_sına_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
        int çık_atla = x86_atla_eşitse(&eü->kod);
        yama_ekle(eü, çık_atla, bitiş);

        bildirim_üret_elf(eü, d->çocuklar[1]);

        int geri = x86_atla(&eü->kod);
        yama_ekle(eü, geri, başlangıç);

        etiket_tanımla(eü, bitiş);
        eü->döngü_başlangıç_etiket = eski_başlangıç;
        eü->döngü_bitiş_etiket = eski_bitiş;
        break;
    }

    case DÜĞÜM_KIR: {
        if (eü->döngü_bitiş_etiket >= 0) {
            int yama = x86_atla(&eü->kod);
            yama_ekle(eü, yama, eü->döngü_bitiş_etiket);
        }
        break;
    }

    case DÜĞÜM_DEVAM: {
        if (eü->döngü_başlangıç_etiket >= 0) {
            int yama = x86_atla(&eü->kod);
            yama_ekle(eü, yama, eü->döngü_başlangıç_etiket);
        }
        break;
    }

    case DÜĞÜM_DÖNDÜR: {
        if (d->çocuk_sayısı > 0) {
            ifade_üret_elf(eü, d->çocuklar[0]);
        } else {
            x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
        }
        x86_ayrıl(&eü->kod);
        x86_dön(&eü->kod);
        break;
    }

    case DÜĞÜM_BLOK:
        blok_üret_elf(eü, d);
        break;

    case DÜĞÜM_İFADE_BİLDİRİMİ:
        if (d->çocuk_sayısı > 0)
            ifade_üret_elf(eü, d->çocuklar[0]);
        break;

    case DÜĞÜM_İŞLEV:
        /* İşlevler ayrıca üretilir, burada atla */
        break;

    case DÜĞÜM_KULLAN:
        /* Modül desteği henüz yok */
        if (d->veri.kullan.modul) {
            fprintf(stderr, "tonyukuk-derle: elf64 backend henüz '%s' modülünü desteklemiyor\n",
                    d->veri.kullan.modul);
        }
        break;

    default:
        /* İfade olarak dene */
        ifade_üret_elf(eü, d);
        break;
    }
}

static void blok_üret_elf(ElfÜretici *eü, Düğüm *d) {
    if (!d) return;
    for (int i = 0; i < d->çocuk_sayısı; i++) {
        bildirim_üret_elf(eü, d->çocuklar[i]);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     İŞLEV ÜRETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

static void işlev_üret_elf(ElfÜretici *eü, Düğüm *d) {
    char *isim = d->veri.islev.isim;
    int fn_etiket = işlev_etiket_bul(eü, isim);
    if (fn_etiket < 0) return;

    etiket_tanımla(eü, fn_etiket);

    /* Yeni kapsam */
    Kapsam *eski_kapsam = eü->kapsam;
    eü->kapsam = kapsam_oluştur(eü->arena, eski_kapsam);
    eü->kapsam->yerel_sayac = 0;  /* Her işlevin kendi yığın çerçevesi var */

    /* Prolog */
    x86_yığına_it(&eü->kod, YAZ_RBP);
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RBP, YAZ_RSP);

    /* Parametreleri sembol tablosuna ekle */
    YazmaçNo param_yazmaçlar[] = { YAZ_RDI, YAZ_RSI, YAZ_RDX, YAZ_RCX, YAZ_R8, YAZ_R9 };
    int param_sayısı = 0;

    /* AST yapısı: çocuklar[0] = parametre listesi düğümü, çocuklar[1] = gövde */
    Düğüm *param_listesi = (d->çocuk_sayısı > 0) ? d->çocuklar[0] : NULL;
    if (param_listesi) {
        param_sayısı = param_listesi->çocuk_sayısı;
    }

    /* Parametre tiplerini topla */
    TipTürü param_tipleri[6] = {0};
    for (int i = 0; i < param_sayısı && i < 6; i++) {
        Düğüm *param = param_listesi->çocuklar[i];
        if (!param) continue;
        char *p_isim = param->veri.değişken.isim;
        char *p_tip_adi = param->veri.değişken.tip;
        TipTürü p_tip = p_tip_adi ? tip_adı_çevir(p_tip_adi) : TİP_TAM;
        param_tipleri[i] = p_tip;

        Sembol *s = sembol_ekle(eü->arena, eü->kapsam, p_isim, p_tip);
        s->parametre_mi = 1;
        if (p_tip == TİP_METİN) eü->kapsam->yerel_sayac++;  /* ptr + len = 2 slot */
    }

    /* Yığın alanı ayır */
    int yığın_boyut = (eü->kapsam->yerel_sayac + 32) * 8;
    yığın_boyut = (yığın_boyut + 15) & ~15;  /* 16-byte hizala */
    x86_çıkar_yazmaç_sabit32(&eü->kod, YAZ_RSP, yığın_boyut);

    /* Parametreleri yığına kaydet — metin 2 yazmaç kullanır */
    int reg_idx = 0;
    for (int i = 0; i < param_sayısı && reg_idx < 6; i++) {
        Düğüm *param = param_listesi->çocuklar[i];
        if (!param) continue;
        Sembol *s = sembol_ara(eü->kapsam, param->veri.değişken.isim);
        if (!s) continue;
        int ofset = (s->yerel_indeks + 1) * 8;
        if (param_tipleri[i] == TİP_METİN && reg_idx + 1 < 6) {
            x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, param_yazmaçlar[reg_idx]);
            reg_idx++;
            x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -(ofset + 8), param_yazmaçlar[reg_idx]);
            reg_idx++;
        } else {
            x86_taşı_bellek_yazmaç(&eü->kod, YAZ_RBP, -ofset, param_yazmaçlar[reg_idx]);
            reg_idx++;
        }
    }

    /* Dönüş tipi */
    char *dönüş_tipi = d->veri.islev.dönüş_tipi;
    eü->mevcut_işlev_dönüş_tipi = dönüş_tipi ? tip_adı_çevir(dönüş_tipi) : TİP_BOŞLUK;

    /* Gövde üret — çocuklar[1] */
    if (d->çocuk_sayısı > 1 && d->çocuklar[1]) {
        bildirim_üret_elf(eü, d->çocuklar[1]);
    }

    /* Varsayılan dönüş (fonksiyon sonuna ulaşılırsa) */
    x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
    x86_ayrıl(&eü->kod);
    x86_dön(&eü->kod);

    /* Kapsamı geri yükle */
    eü->kapsam = eski_kapsam;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     ANA KOD ÜRETİM FONKSİYONU
 * ═══════════════════════════════════════════════════════════════════════════ */

void kod_üret_elf64(ElfÜretici *eü, Düğüm *program, Arena *arena) {
    eü->arena = arena;
    eü->kapsam = kapsam_oluştur(arena, NULL);
    eü->döngü_başlangıç_etiket = -1;
    eü->döngü_bitiş_etiket = -1;

    ikil_başlat(&eü->kod);
    ikil_başlat(&eü->salt_okunur);

    /* Yeni satır byte'ını rodata'ya ekle (ofset 0) */
    ikil_byte_ekle(&eü->salt_okunur, '\n');
    eü->yeni_satır_ofseti = 0;

    /* Ana gövde etiketi */
    eü->ana_gövde_etiket = yeni_etiket(eü);

    /* ===== 1. İşlev ön taraması ===== */
    if (program) {
        for (int i = 0; i < program->çocuk_sayısı; i++) {
            Düğüm *d = program->çocuklar[i];
            if (d->tur == DÜĞÜM_İŞLEV) {
                char *isim = d->veri.islev.isim;
                int etiket = yeni_etiket(eü);
                işlev_tablosu_ekle(eü, isim, etiket);

                /* Sembol tablosuna da ekle */
                char *dönüş_tipi = d->veri.islev.dönüş_tipi;
                TipTürü dt = dönüş_tipi ? tip_adı_çevir(dönüş_tipi) : TİP_BOŞLUK;
                Sembol *s = sembol_ekle(arena, eü->kapsam, isim, TİP_İŞLEV);
                s->dönüş_tipi = dt;
                s->param_sayisi = (d->çocuk_sayısı > 0) ? d->çocuklar[0]->çocuk_sayısı : 0;
                /* Parametre tiplerini kaydet */
                if (d->çocuk_sayısı > 0) {
                    Düğüm *params = d->çocuklar[0];
                    for (int j = 0; j < params->çocuk_sayısı && j < 32; j++) {
                        s->param_tipleri[j] = tip_adı_çevir(params->çocuklar[j]->veri.değişken.tip);
                    }
                }
            }
        }
    }

    /* ===== 2. _başlat giriş noktası ===== */
    /* pop rdi (argc), mov rsi rsp (argv), and rsp ~0xF, call ana, exit */
    x86_yığından_çek(&eü->kod, YAZ_RDI);
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RSI, YAZ_RSP);
    x86_ve_yazmaç_sabit32(&eü->kod, YAZ_RSP, -16);
    int ana_çağrı = x86_çağır_yakın(&eü->kod);
    yama_ekle(eü, ana_çağrı, eü->ana_gövde_etiket);
    /* exit: mov rdi rax, mov rax 231, syscall */
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RDI, YAZ_RAX);
    x86_taşı_yazmaç_sabit32(&eü->kod, YAZ_RAX, 231);
    x86_sistem_çağrısı(&eü->kod);

    /* ===== 3. Kullanıcı işlevleri ===== */
    if (program) {
        for (int i = 0; i < program->çocuk_sayısı; i++) {
            if (program->çocuklar[i]->tur == DÜĞÜM_İŞLEV)
                işlev_üret_elf(eü, program->çocuklar[i]);
        }
    }

    /* ===== 4. Ana gövde ===== */
    etiket_tanımla(eü, eü->ana_gövde_etiket);
    x86_yığına_it(&eü->kod, YAZ_RBP);
    x86_taşı_yazmaç_yazmaç(&eü->kod, YAZ_RBP, YAZ_RSP);

    /* Geniş yığın ayır */
    int yığın_tahmin = 256;
    x86_çıkar_yazmaç_sabit32(&eü->kod, YAZ_RSP, yığın_tahmin);

    if (program) {
        for (int i = 0; i < program->çocuk_sayısı; i++) {
            Düğüm *d = program->çocuklar[i];
            if (d->tur == DÜĞÜM_İŞLEV) continue;  /* zaten üretildi */
            if (d->tur == DÜĞÜM_SINIF) continue;    /* henüz desteklenmiyor */
            bildirim_üret_elf(eü, d);
        }
    }

    /* Varsayılan dönüş: 0 */
    x86_özel_veya_yazmaç_yazmaç(&eü->kod, YAZ_RAX, YAZ_RAX);
    x86_ayrıl(&eü->kod);
    x86_dön(&eü->kod);

    /* ===== 5. Yardımcı fonksiyonları üret (bayrak ayarlandıysa) ===== */
    if (eü->yazdır_tam_üretildi)
        yazdır_tam_üret(eü);
    if (eü->yazdır_metin_üretildi)
        yazdır_metin_üret(eü);
    if (eü->bellek_ayır_üretildi)
        bellek_ayır_üret(eü);
    if (eü->metin_birleştir_üretildi)
        metin_birleştir_üret(eü);
    if (eü->metin_karşılaştır_üretildi)
        metin_karşılaştır_üret(eü);

    /* ===== 6. Etiket yamalarını uygula ===== */
    yamaları_uygula(eü);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     ELF64 DOSYA YAZIMI
 * ═══════════════════════════════════════════════════════════════════════════ */

int elf64_dosya_yaz(ElfÜretici *eü, const char *dosya_adı) {
    /* Bölüm düzeni hesapla */
    /* .text dosya ofseti: sayfa hizalı */
    uint64_t metin_dosya_ofset = SAYFA_BOYUT;  /* 0x1000 */
    uint64_t metin_sanal_adres = YÜKLEME_ADRESİ + metin_dosya_ofset;
    uint64_t metin_boyut = eü->kod.uzunluk;

    /* .rodata dosya ofseti: .text'ten sonra, sayfa hizalı */
    uint64_t veri_dosya_ofset = sayfa_hizala(metin_dosya_ofset + metin_boyut);
    uint64_t veri_sanal_adres = YÜKLEME_ADRESİ + veri_dosya_ofset;
    uint64_t rodata_boyut = eü->salt_okunur.uzunluk;

    /* BSS: rodata'dan sonra, dosyada yer kaplamaz */
    uint64_t bss_sanal_adres = veri_sanal_adres + rodata_boyut;
    /* 8-byte hizala */
    bss_sanal_adres = (bss_sanal_adres + 7) & ~(uint64_t)7;
    uint64_t veri_bellek_boyut = rodata_boyut + (bss_sanal_adres - veri_sanal_adres - rodata_boyut) + eü->bss_boyut;

    /* ===== RIP-bağıl yamaları uygula ===== */

    /* Rodata yamaları */
    for (int i = 0; i < eü->rodata_yama_sayısı; i++) {
        int kod_ofs = eü->rodata_yamaları[i].kod_ofseti;
        int rodata_ofs = eü->rodata_yamaları[i].rodata_ofseti;
        /* RIP-bağıl: hedef_adres - (komut_adresi + 4) */
        int64_t hedef = (int64_t)(veri_sanal_adres + rodata_ofs);
        int64_t komut_sonu = (int64_t)(metin_sanal_adres + kod_ofs + 4);
        int32_t bağıl = (int32_t)(hedef - komut_sonu);
        ikil_yama_int32(&eü->kod, kod_ofs, bağıl);
    }

    /* BSS yamaları */
    for (int i = 0; i < eü->bss_yama_sayısı; i++) {
        int kod_ofs = eü->bss_yamaları[i].kod_ofseti;
        int bss_idx = eü->bss_yamaları[i].bss_indeksi;
        int ek_ofs = eü->bss_yamaları[i].ek_ofset;
        int bss_ofs = eü->genel_değişkenler[bss_idx].bss_ofseti + ek_ofs;
        int64_t hedef = (int64_t)(bss_sanal_adres + bss_ofs);
        int64_t komut_sonu = (int64_t)(metin_sanal_adres + kod_ofs + 4);
        int32_t bağıl = (int32_t)(hedef - komut_sonu);
        ikil_yama_int32(&eü->kod, kod_ofs, bağıl);
    }

    /* ===== ELF Başlığı ===== */
    Elf64Başlık eb;
    memset(&eb, 0, sizeof(eb));
    eb.sihir[0] = ELF_SİHİR_0;
    eb.sihir[1] = ELF_SİHİR_1;
    eb.sihir[2] = ELF_SİHİR_2;
    eb.sihir[3] = ELF_SİHİR_3;
    eb.sihir[4] = ELF_SINIF_64;
    eb.sihir[5] = ELF_VERİ_KÜÇÜK_SONCUL;
    eb.sihir[6] = ELF_SÜRÜM_MEVCUT;
    eb.sihir[7] = ELF_ABI_YOK;
    eb.tür = ELF_TÜR_ÇALIŞTIR;
    eb.makine = ELF_MAKİNE_X86_64;
    eb.sürüm = ELF_SÜRÜM_MEVCUT;
    eb.giriş_noktası = metin_sanal_adres;   /* _başlat adresi */
    eb.pb_ofset = ELF64_BAŞLIK_BOYUT;       /* 64 */
    eb.bb_ofset = 0;
    eb.bayraklar = 0;
    eb.başlık_boyut = ELF64_BAŞLIK_BOYUT;
    eb.pb_giriş_boyut = ELF64_PB_BOYUT;
    eb.pb_giriş_sayısı = 2;
    eb.bb_giriş_boyut = 0;
    eb.bb_giriş_sayısı = 0;
    eb.bb_metin_indeks = 0;

    /* ===== Program Başlıkları ===== */

    /* Segment 0: .text (Oku + Çalıştır) */
    Elf64ProgramBaşlık pb_metin;
    memset(&pb_metin, 0, sizeof(pb_metin));
    pb_metin.tür = PB_YÜKLE;
    pb_metin.bayraklar = PB_OKU | PB_ÇALIŞTIR;
    pb_metin.dosya_ofset = metin_dosya_ofset;
    pb_metin.sanal_adres = metin_sanal_adres;
    pb_metin.fiziksel_adres = metin_sanal_adres;
    pb_metin.dosya_boyut = metin_boyut;
    pb_metin.bellek_boyut = metin_boyut;
    pb_metin.hizalama = SAYFA_BOYUT;

    /* Segment 1: .rodata + .bss (Oku + Yaz) */
    Elf64ProgramBaşlık pb_veri;
    memset(&pb_veri, 0, sizeof(pb_veri));
    pb_veri.tür = PB_YÜKLE;
    pb_veri.bayraklar = PB_OKU | PB_YAZ;
    pb_veri.dosya_ofset = veri_dosya_ofset;
    pb_veri.sanal_adres = veri_sanal_adres;
    pb_veri.fiziksel_adres = veri_sanal_adres;
    pb_veri.dosya_boyut = rodata_boyut;
    pb_veri.bellek_boyut = veri_bellek_boyut;  /* bss dahil */
    pb_veri.hizalama = SAYFA_BOYUT;

    /* ===== Dosyaya yaz ===== */
    FILE *f = fopen(dosya_adı, "wb");
    if (!f) {
        fprintf(stderr, "tonyukuk-derle: ELF64 dosya oluşturulamadı: %s\n", dosya_adı);
        return 1;
    }

    /* ELF başlık */
    fwrite(&eb, 1, sizeof(eb), f);

    /* Program başlıkları */
    fwrite(&pb_metin, 1, sizeof(pb_metin), f);
    fwrite(&pb_veri, 1, sizeof(pb_veri), f);

    /* Başlıktan .text'e kadar doldurma (0x1000 - başlık boyutu) */
    {
        long mevcut = ftell(f);
        long dolgu = (long)metin_dosya_ofset - mevcut;
        if (dolgu > 0) {
            uint8_t sıfır = 0;
            for (long i = 0; i < dolgu; i++)
                fwrite(&sıfır, 1, 1, f);
        }
    }

    /* .text bölümü */
    fwrite(eü->kod.veri, 1, eü->kod.uzunluk, f);

    /* .text'ten .rodata'ya kadar doldurma */
    {
        long mevcut = ftell(f);
        long dolgu = (long)veri_dosya_ofset - mevcut;
        if (dolgu > 0) {
            uint8_t sıfır = 0;
            for (long i = 0; i < dolgu; i++)
                fwrite(&sıfır, 1, 1, f);
        }
    }

    /* .rodata bölümü */
    if (eü->salt_okunur.uzunluk > 0) {
        fwrite(eü->salt_okunur.veri, 1, eü->salt_okunur.uzunluk, f);
    }

    fclose(f);

    /* Çalıştırma izni ver */
    chmod(dosya_adı, 0755);

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     TEMİZLİK
 * ═══════════════════════════════════════════════════════════════════════════ */

void elf_üretici_serbest(ElfÜretici *eü) {
    ikil_serbest(&eü->kod);
    ikil_serbest(&eü->salt_okunur);
    if (eü->etiket_ofsetler) free(eü->etiket_ofsetler);
    if (eü->yamalar) free(eü->yamalar);
    if (eü->işlev_tablosu) free(eü->işlev_tablosu);
    if (eü->metin_tablosu) free(eü->metin_tablosu);
    if (eü->ondalık_tablosu) free(eü->ondalık_tablosu);
    if (eü->genel_değişkenler) free(eü->genel_değişkenler);
    if (eü->rodata_yamaları) free(eü->rodata_yamaları);
    if (eü->bss_yamaları) free(eü->bss_yamaları);
}
