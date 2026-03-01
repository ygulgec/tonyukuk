/*
 * elf64.h — ELF64 İkili Format Tanımları
 *
 * Tonyukuk derleyicisi doğrudan ELF64 çalıştırılabilir dosya üretimi için
 * gerekli yapı ve sabit tanımları. Tamamen Türkçe isimlendirilmiştir.
 *
 * Harici bağımlılık yok — <elf.h> kullanılmaz.
 */

#ifndef ELF64_H
#define ELF64_H

#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *                         ELF SABİTLERİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Sihirli byte'lar (e_ident[0..3]) */
#define ELF_SİHİR_0            0x7F
#define ELF_SİHİR_1            'E'
#define ELF_SİHİR_2            'L'
#define ELF_SİHİR_3            'F'

/* ELF sınıfı (e_ident[4]) */
#define ELF_SINIF_32            1
#define ELF_SINIF_64            2

/* Veri kodlaması (e_ident[5]) */
#define ELF_VERİ_KÜÇÜK_SONCUL  1   /* Little-endian */
#define ELF_VERİ_BÜYÜK_SONCUL  2   /* Big-endian */

/* ELF sürümü (e_ident[6]) */
#define ELF_SÜRÜM_MEVCUT       1

/* İşletim sistemi ABI (e_ident[7]) */
#define ELF_ABI_YOK             0   /* ELFOSABI_NONE / System V */
#define ELF_ABI_LINUX           3

/* ELF dosya türü (e_type) */
#define ELF_TÜR_YOK             0   /* ET_NONE */
#define ELF_TÜR_YENİDEN_KONUMLANIR 1   /* ET_REL */
#define ELF_TÜR_ÇALIŞTIR        2   /* ET_EXEC */
#define ELF_TÜR_PAYLAŞIMLI      3   /* ET_DYN */

/* Makine mimarisi (e_machine) */
#define ELF_MAKİNE_X86_64       62  /* EM_X86_64 */

/* Program başlığı türleri (p_type) */
#define PB_BOŞ                  0   /* PT_NULL */
#define PB_YÜKLE                1   /* PT_LOAD */
#define PB_DİNAMİK             2   /* PT_DYNAMIC */

/* Program başlığı bayrakları (p_flags) */
#define PB_ÇALIŞTIR             0x1 /* PF_X — çalıştırılabilir */
#define PB_YAZ                  0x2 /* PF_W — yazılabilir */
#define PB_OKU                  0x4 /* PF_R — okunabilir */

/* Bellek sayfa boyutu */
#define SAYFA_BOYUT             0x1000  /* 4096 byte */

/* Varsayılan yükleme adresi (x86_64 non-PIE standart) */
#define YÜKLEME_ADRESİ          0x400000

/* Başlık boyutları */
#define ELF64_BAŞLIK_BOYUT      64  /* sizeof(Elf64Başlık) */
#define ELF64_PB_BOYUT          56  /* sizeof(Elf64ProgramBaşlık) */

/* ═══════════════════════════════════════════════════════════════════════════
 *                         ELF64 YAPILAR
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ELF64 dosya başlığı — 64 byte */
typedef struct {
    uint8_t  sihir[16];          /* e_ident: ELF tanımlama byte'ları */
    uint16_t tür;                 /* e_type: dosya türü */
    uint16_t makine;              /* e_machine: mimari */
    uint32_t sürüm;               /* e_version */
    uint64_t giriş_noktası;       /* e_entry: giriş noktası sanal adresi */
    uint64_t pb_ofset;            /* e_phoff: program başlık tablosu dosya ofseti */
    uint64_t bb_ofset;            /* e_shoff: bölüm başlık tablosu dosya ofseti */
    uint32_t bayraklar;           /* e_flags: işlemciye özgü bayraklar */
    uint16_t başlık_boyut;        /* e_ehsize: ELF başlık boyutu (64) */
    uint16_t pb_giriş_boyut;      /* e_phentsize: program başlık giriş boyutu (56) */
    uint16_t pb_giriş_sayısı;     /* e_phnum: program başlık giriş sayısı */
    uint16_t bb_giriş_boyut;      /* e_shentsize: bölüm başlık giriş boyutu */
    uint16_t bb_giriş_sayısı;     /* e_shnum: bölüm başlık giriş sayısı */
    uint16_t bb_metin_indeks;     /* e_shstrndx: bölüm isim tablosu indeksi */
} Elf64Başlık;

/* ELF64 program başlığı — 56 byte */
typedef struct {
    uint32_t tür;                 /* p_type: segment türü */
    uint32_t bayraklar;           /* p_flags: segment bayrakları */
    uint64_t dosya_ofset;         /* p_offset: dosyadaki ofset */
    uint64_t sanal_adres;         /* p_vaddr: bellekteki sanal adres */
    uint64_t fiziksel_adres;      /* p_paddr: fiziksel adres (genellikle vaddr ile aynı) */
    uint64_t dosya_boyut;         /* p_filesz: dosyadaki boyut */
    uint64_t bellek_boyut;        /* p_memsz: bellekteki boyut (bss için filesz'den büyük) */
    uint64_t hizalama;            /* p_align: hizalama (SAYFA_BOYUT) */
} Elf64ProgramBaşlık;

/* ═══════════════════════════════════════════════════════════════════════════
 *                       YARDIMCI FONKSİYONLAR
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Sayfa hizalama: değeri yukarı doğru sayfa sınırına hizala */
static inline uint64_t sayfa_hizala(uint64_t değer) {
    return (değer + SAYFA_BOYUT - 1) & ~((uint64_t)SAYFA_BOYUT - 1);
}

#endif /* ELF64_H */
