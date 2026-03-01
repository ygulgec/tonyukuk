/*
 * x86_kodlayici.c — x86_64 Makine Kodu Kodlayıcı
 *
 * Tonyukuk derleyicisi için doğrudan x86_64 makine kodu üretimi.
 * Her fonksiyon, İkilKod buffer'ına ham byte'lar ekler.
 *
 * x86_64 kodlama kuralları:
 * - REX prefix (0x40-0x4F): 64-bit operand boyutu ve R8-R15 erişimi
 * - ModRM byte: (mod<<6) | (reg<<3) | rm
 * - SIB byte: RSP(4) taban yazmacı olduğunda gerekli
 * - RBP(5) + mod=00 → [RIP+disp32] anlamına gelir
 */

#include "x86_kodlayici.h"
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *                     İKİL KOD BUFFER YÖNETİMİ
 * ═══════════════════════════════════════════════════════════════════════════ */

#define İKİL_BAŞLANGIÇ_KAPASİTE 4096

void ikil_başlat(İkilKod *ik) {
    ik->veri = (uint8_t *)malloc(İKİL_BAŞLANGIÇ_KAPASİTE);
    ik->uzunluk = 0;
    ik->kapasite = İKİL_BAŞLANGIÇ_KAPASİTE;
}

void ikil_serbest(İkilKod *ik) {
    if (ik->veri) free(ik->veri);
    ik->veri = NULL;
    ik->uzunluk = 0;
    ik->kapasite = 0;
}

static void ikil_büyüt(İkilKod *ik, int gereken) {
    if (ik->uzunluk + gereken <= ik->kapasite) return;
    int yeni_kap = ik->kapasite * 2;
    while (yeni_kap < ik->uzunluk + gereken) yeni_kap *= 2;
    ik->veri = (uint8_t *)realloc(ik->veri, yeni_kap);
    ik->kapasite = yeni_kap;
}

void ikil_byte_ekle(İkilKod *ik, uint8_t b) {
    ikil_büyüt(ik, 1);
    ik->veri[ik->uzunluk++] = b;
}

void ikil_word_ekle(İkilKod *ik, uint16_t w) {
    ikil_büyüt(ik, 2);
    ik->veri[ik->uzunluk++] = (uint8_t)(w & 0xFF);
    ik->veri[ik->uzunluk++] = (uint8_t)((w >> 8) & 0xFF);
}

void ikil_dword_ekle(İkilKod *ik, uint32_t dw) {
    ikil_büyüt(ik, 4);
    ik->veri[ik->uzunluk++] = (uint8_t)(dw & 0xFF);
    ik->veri[ik->uzunluk++] = (uint8_t)((dw >> 8) & 0xFF);
    ik->veri[ik->uzunluk++] = (uint8_t)((dw >> 16) & 0xFF);
    ik->veri[ik->uzunluk++] = (uint8_t)((dw >> 24) & 0xFF);
}

void ikil_qword_ekle(İkilKod *ik, uint64_t qw) {
    ikil_büyüt(ik, 8);
    for (int i = 0; i < 8; i++) {
        ik->veri[ik->uzunluk++] = (uint8_t)((qw >> (i * 8)) & 0xFF);
    }
}

void ikil_veri_ekle(İkilKod *ik, const uint8_t *veri, int boyut) {
    ikil_büyüt(ik, boyut);
    memcpy(ik->veri + ik->uzunluk, veri, boyut);
    ik->uzunluk += boyut;
}

void ikil_yama_int32(İkilKod *ik, int ofset, int32_t değer) {
    if (ofset + 4 > ik->uzunluk) return;
    ik->veri[ofset]     = (uint8_t)(değer & 0xFF);
    ik->veri[ofset + 1] = (uint8_t)((değer >> 8) & 0xFF);
    ik->veri[ofset + 2] = (uint8_t)((değer >> 16) & 0xFF);
    ik->veri[ofset + 3] = (uint8_t)((değer >> 24) & 0xFF);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     REX / ModRM YARDIMCILARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* REX prefix hesapla:
 * W=1: 64-bit operand boyutu
 * R: ModRM.reg alanı uzantısı (reg >= 8)
 * B: ModRM.rm alanı uzantısı (rm >= 8) */
static uint8_t rex_hesapla(int w, int reg, int rm) {
    uint8_t rex = 0x40;
    if (w)       rex |= 0x08;
    if (reg >= 8) rex |= 0x04;  /* REX.R */
    if (rm >= 8)  rex |= 0x01;  /* REX.B */
    return rex;
}

/* ModRM byte oluştur */
static uint8_t modrm_hesapla(int mod, int reg, int rm) {
    return (uint8_t)(((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7));
}

/* Bellek adresleme için ModRM + opsiyonel SIB + displacement kodla */
static void bellek_kodla(İkilKod *ik, int reg, int taban, int32_t ofset) {
    int taban3 = taban & 7;

    /* mod alanını belirle */
    int mod;
    if (ofset == 0 && taban3 != 5) {
        mod = 0;    /* [taban] — disp yok (RBP hariç) */
    } else if (ofset >= -128 && ofset <= 127) {
        mod = 1;    /* [taban+disp8] */
    } else {
        mod = 2;    /* [taban+disp32] */
    }

    ikil_byte_ekle(ik, modrm_hesapla(mod, reg & 7, taban3));

    /* RSP (4) veya R12 taban yazmacı → SIB byte gerekli */
    if (taban3 == 4) {
        ikil_byte_ekle(ik, 0x24);   /* SIB: scale=0, index=RSP(no index), base=RSP */
    }

    /* Displacement yaz */
    if (mod == 1) {
        ikil_byte_ekle(ik, (uint8_t)(ofset & 0xFF));
    } else if (mod == 2) {
        ikil_dword_ekle(ik, (uint32_t)ofset);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     VERİ TAŞIMA KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* MOV yazmaç, imm64 — movabs (REX.W + B8+rd + imm64) — 10 byte */
void x86_taşı_yazmaç_sabit64(İkilKod *ik, YazmaçNo hedef, int64_t değer) {
    /* Optimizasyon: int32 aralığındaysa kısa form kullan */
    if (değer >= INT32_MIN && değer <= INT32_MAX) {
        x86_taşı_yazmaç_sabit32(ik, hedef, (int32_t)değer);
        return;
    }
    ikil_byte_ekle(ik, rex_hesapla(1, 0, hedef));
    ikil_byte_ekle(ik, 0xB8 + (hedef & 7));
    ikil_qword_ekle(ik, (uint64_t)değer);
}

/* MOV yazmaç, imm32 — sign-extend (REX.W + C7 /0 + imm32) — 7 byte */
void x86_taşı_yazmaç_sabit32(İkilKod *ik, YazmaçNo hedef, int32_t değer) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, hedef));
    ikil_byte_ekle(ik, 0xC7);
    ikil_byte_ekle(ik, modrm_hesapla(3, 0, hedef & 7));
    ikil_dword_ekle(ik, (uint32_t)değer);
}

/* MOV yazmaç, yazmaç (REX.W + 8B /r + ModRM) */
void x86_taşı_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak) {
    ikil_byte_ekle(ik, rex_hesapla(1, hedef, kaynak));
    ikil_byte_ekle(ik, 0x8B);
    ikil_byte_ekle(ik, modrm_hesapla(3, hedef & 7, kaynak & 7));
}

/* MOV yazmaç, [taban+ofset] (REX.W + 8B /r + ModRM + disp) */
void x86_taşı_yazmaç_bellek(İkilKod *ik, YazmaçNo hedef, YazmaçNo taban, int32_t ofset) {
    ikil_byte_ekle(ik, rex_hesapla(1, hedef, taban));
    ikil_byte_ekle(ik, 0x8B);
    bellek_kodla(ik, hedef, taban, ofset);
}

/* MOV [taban+ofset], yazmaç (REX.W + 89 /r + ModRM + disp) */
void x86_taşı_bellek_yazmaç(İkilKod *ik, YazmaçNo taban, int32_t ofset, YazmaçNo kaynak) {
    ikil_byte_ekle(ik, rex_hesapla(1, kaynak, taban));
    ikil_byte_ekle(ik, 0x89);
    bellek_kodla(ik, kaynak, taban, ofset);
}

/* MOV byte [taban+ofset], kaynak_low8 (88 /r + ModRM + disp) — tek byte yaz */
void x86_taşı_bellek_yazmaç_byte(İkilKod *ik, YazmaçNo taban, int32_t ofset, YazmaçNo kaynak) {
    /* REX prefix sadece R8-R15 veya SPL/BPL/SIL/DIL için gerekli */
    uint8_t rex = 0;
    if (kaynak >= 8 || taban >= 8) {
        rex = 0x40;
        if (kaynak >= 8) rex |= 0x04;  /* REX.R */
        if (taban >= 8)  rex |= 0x01;  /* REX.B */
        ikil_byte_ekle(ik, rex);
    } else if (kaynak >= 4) {
        /* RSP, RBP, RSI, RDI low bytes (SPL, BPL, SIL, DIL) need REX prefix */
        ikil_byte_ekle(ik, 0x40);
    }
    ikil_byte_ekle(ik, 0x88);
    bellek_kodla(ik, kaynak, taban, ofset);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     ARİTMETİK KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Genel ALU reg,reg kodlama: REX.W + opcode + ModRM(11, src, dst) */
static void alu_yazmaç_yazmaç(İkilKod *ik, uint8_t opcode, YazmaçNo hedef, YazmaçNo kaynak) {
    ikil_byte_ekle(ik, rex_hesapla(1, kaynak, hedef));
    ikil_byte_ekle(ik, opcode);
    ikil_byte_ekle(ik, modrm_hesapla(3, kaynak & 7, hedef & 7));
}

/* ADD hedef, kaynak */
void x86_topla_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak) {
    alu_yazmaç_yazmaç(ik, 0x01, hedef, kaynak);
}

/* SUB hedef, kaynak */
void x86_çıkar_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak) {
    alu_yazmaç_yazmaç(ik, 0x29, hedef, kaynak);
}

/* IMUL hedef, kaynak (REX.W + 0F AF /r) */
void x86_çarp_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak) {
    ikil_byte_ekle(ik, rex_hesapla(1, hedef, kaynak));
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0xAF);
    ikil_byte_ekle(ik, modrm_hesapla(3, hedef & 7, kaynak & 7));
}

/* IDIV bölen — rdx:rax / bölen → rax=bölüm, rdx=kalan */
void x86_böl_yazmaç(İkilKod *ik, YazmaçNo bölen) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, bölen));
    ikil_byte_ekle(ik, 0xF7);
    ikil_byte_ekle(ik, modrm_hesapla(3, 7, bölen & 7));   /* /7 = IDIV */
}

/* CQO — rax sign-extend → rdx:rax (REX.W + 99) */
void x86_işaret_genişlet(İkilKod *ik) {
    ikil_byte_ekle(ik, 0x48);  /* REX.W */
    ikil_byte_ekle(ik, 0x99);
}

/* ADD yazmaç, imm32 */
void x86_topla_yazmaç_sabit32(İkilKod *ik, YazmaçNo hedef, int32_t değer) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, hedef));
    if (değer >= -128 && değer <= 127) {
        ikil_byte_ekle(ik, 0x83);   /* ALU r/m64, imm8 */
        ikil_byte_ekle(ik, modrm_hesapla(3, 0, hedef & 7));  /* /0 = ADD */
        ikil_byte_ekle(ik, (uint8_t)(değer & 0xFF));
    } else {
        ikil_byte_ekle(ik, 0x81);   /* ALU r/m64, imm32 */
        ikil_byte_ekle(ik, modrm_hesapla(3, 0, hedef & 7));  /* /0 = ADD */
        ikil_dword_ekle(ik, (uint32_t)değer);
    }
}

/* SUB yazmaç, imm32 */
void x86_çıkar_yazmaç_sabit32(İkilKod *ik, YazmaçNo hedef, int32_t değer) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, hedef));
    if (değer >= -128 && değer <= 127) {
        ikil_byte_ekle(ik, 0x83);
        ikil_byte_ekle(ik, modrm_hesapla(3, 5, hedef & 7));  /* /5 = SUB */
        ikil_byte_ekle(ik, (uint8_t)(değer & 0xFF));
    } else {
        ikil_byte_ekle(ik, 0x81);
        ikil_byte_ekle(ik, modrm_hesapla(3, 5, hedef & 7));  /* /5 = SUB */
        ikil_dword_ekle(ik, (uint32_t)değer);
    }
}

/* NEG yazmaç (REX.W + F7 /3) */
void x86_olumsuzla(İkilKod *ik, YazmaçNo yaz) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, yaz));
    ikil_byte_ekle(ik, 0xF7);
    ikil_byte_ekle(ik, modrm_hesapla(3, 3, yaz & 7));
}

/* NOT yazmaç (REX.W + F7 /2) */
void x86_değillebitsel(İkilKod *ik, YazmaçNo yaz) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, yaz));
    ikil_byte_ekle(ik, 0xF7);
    ikil_byte_ekle(ik, modrm_hesapla(3, 2, yaz & 7));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     KARŞILAŞTIRMA KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* CMP sol, sağ (REX.W + 39 /r) */
void x86_karşılaştır_yazmaç_yazmaç(İkilKod *ik, YazmaçNo sol, YazmaçNo sağ) {
    alu_yazmaç_yazmaç(ik, 0x39, sol, sağ);
}

/* CMP yazmaç, imm32 */
void x86_karşılaştır_yazmaç_sabit32(İkilKod *ik, YazmaçNo yaz, int32_t değer) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, yaz));
    if (değer >= -128 && değer <= 127) {
        ikil_byte_ekle(ik, 0x83);
        ikil_byte_ekle(ik, modrm_hesapla(3, 7, yaz & 7));  /* /7 = CMP */
        ikil_byte_ekle(ik, (uint8_t)(değer & 0xFF));
    } else {
        ikil_byte_ekle(ik, 0x81);
        ikil_byte_ekle(ik, modrm_hesapla(3, 7, yaz & 7));
        ikil_dword_ekle(ik, (uint32_t)değer);
    }
}

/* TEST sol, sağ (REX.W + 85 /r) */
void x86_sına_yazmaç_yazmaç(İkilKod *ik, YazmaçNo sol, YazmaçNo sağ) {
    alu_yazmaç_yazmaç(ik, 0x85, sol, sağ);
}

/* SETcc yardımcısı */
static void setcc_kodla(İkilKod *ik, uint8_t cc_kodu, YazmaçNo yaz) {
    /* SETcc r/m8: REX? + 0F + (90+cc) + ModRM */
    if (yaz >= 8) {
        ikil_byte_ekle(ik, 0x41);  /* REX.B */
    }
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x90 + cc_kodu);
    ikil_byte_ekle(ik, modrm_hesapla(3, 0, yaz & 7));
}

void x86_koşul_eşit(İkilKod *ik, YazmaçNo yaz)         { setcc_kodla(ik, 0x04, yaz); } /* SETE: cc=4 */
void x86_koşul_eşit_değil(İkilKod *ik, YazmaçNo yaz)   { setcc_kodla(ik, 0x05, yaz); } /* SETNE: cc=5 */
void x86_koşul_küçük(İkilKod *ik, YazmaçNo yaz)         { setcc_kodla(ik, 0x0C, yaz); } /* SETL: cc=C */
void x86_koşul_büyük(İkilKod *ik, YazmaçNo yaz)         { setcc_kodla(ik, 0x0F, yaz); } /* SETG: cc=F */
void x86_koşul_küçük_eşit(İkilKod *ik, YazmaçNo yaz)   { setcc_kodla(ik, 0x0E, yaz); } /* SETLE: cc=E */
void x86_koşul_büyük_eşit(İkilKod *ik, YazmaçNo yaz)   { setcc_kodla(ik, 0x0D, yaz); } /* SETGE: cc=D */

/* MOVZX r64, r8 — sıfır genişlet (REX.W + 0F B6 /r) */
void x86_sıfır_genişlet_byte(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak) {
    ikil_byte_ekle(ik, rex_hesapla(1, hedef, kaynak));
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0xB6);
    ikil_byte_ekle(ik, modrm_hesapla(3, hedef & 7, kaynak & 7));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     DALLANMA KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* JMP rel32: E9 + imm32 */
int x86_atla(İkilKod *ik) {
    ikil_byte_ekle(ik, 0xE9);
    int yama_ofseti = ik->uzunluk;
    ikil_dword_ekle(ik, 0);  /* yer tutucu, sonra yamalanacak */
    return yama_ofseti;
}

/* Jcc rel32 yardımcısı: 0F + (80+cc) + imm32 */
static int jcc_kodla(İkilKod *ik, uint8_t cc_kodu) {
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x80 + cc_kodu);
    int yama_ofseti = ik->uzunluk;
    ikil_dword_ekle(ik, 0);
    return yama_ofseti;
}

int x86_atla_eşitse(İkilKod *ik)          { return jcc_kodla(ik, 0x04); } /* JE/JZ */
int x86_atla_eşit_değilse(İkilKod *ik)    { return jcc_kodla(ik, 0x05); } /* JNE/JNZ */
int x86_atla_küçükse(İkilKod *ik)          { return jcc_kodla(ik, 0x0C); } /* JL */
int x86_atla_büyükse(İkilKod *ik)          { return jcc_kodla(ik, 0x0F); } /* JG */
int x86_atla_küçük_eşitse(İkilKod *ik)    { return jcc_kodla(ik, 0x0E); } /* JLE */
int x86_atla_büyük_eşitse(İkilKod *ik)    { return jcc_kodla(ik, 0x0D); } /* JGE */

/* Atlama yaması: rel32 = hedef - (yama_ofseti + 4) */
void x86_atlama_yamala(İkilKod *ik, int yama_ofseti, int hedef_ofseti) {
    int32_t bağıl = hedef_ofseti - (yama_ofseti + 4);
    ikil_yama_int32(ik, yama_ofseti, bağıl);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     ÇAĞRI / DÖNÜŞ
 * ═══════════════════════════════════════════════════════════════════════════ */

/* CALL rel32: E8 + imm32 */
int x86_çağır_yakın(İkilKod *ik) {
    ikil_byte_ekle(ik, 0xE8);
    int yama_ofseti = ik->uzunluk;
    ikil_dword_ekle(ik, 0);
    return yama_ofseti;
}

/* CALL *yazmaç (FF /2) */
void x86_çağır_dolaylı(İkilKod *ik, YazmaçNo yaz) {
    if (yaz >= 8) ikil_byte_ekle(ik, 0x41);  /* REX.B */
    ikil_byte_ekle(ik, 0xFF);
    ikil_byte_ekle(ik, modrm_hesapla(3, 2, yaz & 7));
}

/* RET: C3 */
void x86_dön(İkilKod *ik) {
    ikil_byte_ekle(ik, 0xC3);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     YIĞIN İŞLEMLERİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/* PUSH yazmaç: 50+rd (veya REX.B + 50+rd) */
void x86_yığına_it(İkilKod *ik, YazmaçNo yaz) {
    if (yaz >= 8) ikil_byte_ekle(ik, 0x41);  /* REX.B */
    ikil_byte_ekle(ik, 0x50 + (yaz & 7));
}

/* POP yazmaç: 58+rd (veya REX.B + 58+rd) */
void x86_yığından_çek(İkilKod *ik, YazmaçNo yaz) {
    if (yaz >= 8) ikil_byte_ekle(ik, 0x41);  /* REX.B */
    ikil_byte_ekle(ik, 0x58 + (yaz & 7));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     SİSTEM ÇAĞRISI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* SYSCALL: 0F 05 */
void x86_sistem_çağrısı(İkilKod *ik) {
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x05);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     ADRES KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* LEA yazmaç, [taban+ofset] (REX.W + 8D /r + ModRM + disp) */
void x86_adres_yükle(İkilKod *ik, YazmaçNo hedef, YazmaçNo taban, int32_t ofset) {
    ikil_byte_ekle(ik, rex_hesapla(1, hedef, taban));
    ikil_byte_ekle(ik, 0x8D);
    bellek_kodla(ik, hedef, taban, ofset);
}

/* LEA yazmaç, [RIP+disp32]
 * Kodlama: REX.W + 8D + ModRM(00, reg, 101=RIP) + disp32
 * Dönüş: disp32 alanının buffer ofseti */
int x86_adres_yükle_rip_bağıl(İkilKod *ik, YazmaçNo hedef) {
    ikil_byte_ekle(ik, rex_hesapla(1, hedef, 0));
    ikil_byte_ekle(ik, 0x8D);
    ikil_byte_ekle(ik, modrm_hesapla(0, hedef & 7, 5));  /* mod=00, rm=101 → [RIP+disp32] */
    int yama_ofseti = ik->uzunluk;
    ikil_dword_ekle(ik, 0);   /* yer tutucu */
    return yama_ofseti;
}

/* MOV yazmaç, [RIP+disp32]
 * Kodlama: REX.W + 8B + ModRM(00, reg, 101) + disp32 */
int x86_taşı_yazmaç_rip_bağıl(İkilKod *ik, YazmaçNo hedef) {
    ikil_byte_ekle(ik, rex_hesapla(1, hedef, 0));
    ikil_byte_ekle(ik, 0x8B);
    ikil_byte_ekle(ik, modrm_hesapla(0, hedef & 7, 5));
    int yama_ofseti = ik->uzunluk;
    ikil_dword_ekle(ik, 0);
    return yama_ofseti;
}

/* MOV [RIP+disp32], yazmaç
 * Kodlama: REX.W + 89 + ModRM(00, reg, 101) + disp32 */
int x86_taşı_rip_bağıl_yazmaç(İkilKod *ik, YazmaçNo kaynak) {
    ikil_byte_ekle(ik, rex_hesapla(1, kaynak, 0));
    ikil_byte_ekle(ik, 0x89);
    ikil_byte_ekle(ik, modrm_hesapla(0, kaynak & 7, 5));
    int yama_ofseti = ik->uzunluk;
    ikil_dword_ekle(ik, 0);
    return yama_ofseti;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     MANTIK / BİT İŞLEMLERİ
 * ═══════════════════════════════════════════════════════════════════════════ */

/* XOR sol, sağ (REX.W + 31 /r) */
void x86_özel_veya_yazmaç_yazmaç(İkilKod *ik, YazmaçNo sol, YazmaçNo sağ) {
    alu_yazmaç_yazmaç(ik, 0x31, sol, sağ);
}

/* AND hedef, kaynak (REX.W + 21 /r) */
void x86_ve_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak) {
    alu_yazmaç_yazmaç(ik, 0x21, hedef, kaynak);
}

/* OR hedef, kaynak (REX.W + 09 /r) */
void x86_veya_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak) {
    alu_yazmaç_yazmaç(ik, 0x09, hedef, kaynak);
}

/* AND yazmaç, imm32 */
void x86_ve_yazmaç_sabit32(İkilKod *ik, YazmaçNo hedef, int32_t değer) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, hedef));
    if (değer >= -128 && değer <= 127) {
        ikil_byte_ekle(ik, 0x83);
        ikil_byte_ekle(ik, modrm_hesapla(3, 4, hedef & 7));  /* /4 = AND */
        ikil_byte_ekle(ik, (uint8_t)(değer & 0xFF));
    } else {
        ikil_byte_ekle(ik, 0x81);
        ikil_byte_ekle(ik, modrm_hesapla(3, 4, hedef & 7));
        ikil_dword_ekle(ik, (uint32_t)değer);
    }
}

/* SHL hedef, CL (REX.W + D3 /4) */
void x86_sola_kaydır_cl(İkilKod *ik, YazmaçNo hedef) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, hedef));
    ikil_byte_ekle(ik, 0xD3);
    ikil_byte_ekle(ik, modrm_hesapla(3, 4, hedef & 7));
}

/* SHR hedef, CL (REX.W + D3 /5) */
void x86_sağa_kaydır_cl(İkilKod *ik, YazmaçNo hedef) {
    ikil_byte_ekle(ik, rex_hesapla(1, 0, hedef));
    ikil_byte_ekle(ik, 0xD3);
    ikil_byte_ekle(ik, modrm_hesapla(3, 5, hedef & 7));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     SSE2 ONDALIK (double) KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* SSE2 prefix + opcode yardımcısı */
static void sse2_kodla(İkilKod *ik, uint8_t önek, uint8_t op1, uint8_t op2,
                        int xmm_reg, int rm, int rm_tür) {
    /* Prefix */
    ikil_byte_ekle(ik, önek);    /* F2=MOVSD/ADDSD/etc, 66=MOVAPD/etc */
    /* REX (sadece xmm >= 8 veya rm >= 8 ise, şimdilik yok) */
    if (xmm_reg >= 8 || rm >= 8) {
        ikil_byte_ekle(ik, rex_hesapla(0, xmm_reg, rm));
    }
    ikil_byte_ekle(ik, op1);
    ikil_byte_ekle(ik, op2);
    if (rm_tür == 3) {
        /* yazmaç-yazmaç */
        ikil_byte_ekle(ik, modrm_hesapla(3, xmm_reg & 7, rm & 7));
    }
}

/* MOVSD xmm, [taban+ofset]
 * F2 0F 10 /r + ModRM + disp */
void x86_ondalık_taşı_yazmaç_bellek(İkilKod *ik, int xmm_hedef, YazmaçNo taban, int32_t ofset) {
    ikil_byte_ekle(ik, 0xF2);
    if (xmm_hedef >= 8 || taban >= 8) {
        ikil_byte_ekle(ik, rex_hesapla(0, xmm_hedef, taban));
    }
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x10);
    bellek_kodla(ik, xmm_hedef, taban, ofset);
}

/* MOVSD [taban+ofset], xmm
 * F2 0F 11 /r + ModRM + disp */
void x86_ondalık_taşı_bellek_yazmaç(İkilKod *ik, YazmaçNo taban, int32_t ofset, int xmm_kaynak) {
    ikil_byte_ekle(ik, 0xF2);
    if (xmm_kaynak >= 8 || taban >= 8) {
        ikil_byte_ekle(ik, rex_hesapla(0, xmm_kaynak, taban));
    }
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x11);
    bellek_kodla(ik, xmm_kaynak, taban, ofset);
}

/* MOVSD xmm_hedef, xmm_kaynak
 * F2 0F 10 /r ModRM(11, dst, src) */
void x86_ondalık_taşı_yazmaç_yazmaç(İkilKod *ik, int xmm_hedef, int xmm_kaynak) {
    ikil_byte_ekle(ik, 0xF2);
    if (xmm_hedef >= 8 || xmm_kaynak >= 8) {
        ikil_byte_ekle(ik, rex_hesapla(0, xmm_hedef, xmm_kaynak));
    }
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x10);
    ikil_byte_ekle(ik, modrm_hesapla(3, xmm_hedef & 7, xmm_kaynak & 7));
}

/* MOVSD xmm, [RIP+disp32]
 * F2 (REX?) 0F 10 ModRM(00, xmm, 101) + disp32 */
int x86_ondalık_taşı_rip_bağıl(İkilKod *ik, int xmm_hedef) {
    ikil_byte_ekle(ik, 0xF2);
    if (xmm_hedef >= 8) {
        ikil_byte_ekle(ik, rex_hesapla(0, xmm_hedef, 0));
    }
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x10);
    ikil_byte_ekle(ik, modrm_hesapla(0, xmm_hedef & 7, 5));
    int yama_ofseti = ik->uzunluk;
    ikil_dword_ekle(ik, 0);
    return yama_ofseti;
}

/* SSE2 aritmetik yardımcısı: F2 0F opcode ModRM(11,dst,src) */
static void sse2_aritmetik(İkilKod *ik, uint8_t opcode, int xmm_hedef, int xmm_kaynak) {
    ikil_byte_ekle(ik, 0xF2);
    if (xmm_hedef >= 8 || xmm_kaynak >= 8) {
        ikil_byte_ekle(ik, rex_hesapla(0, xmm_hedef, xmm_kaynak));
    }
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, opcode);
    ikil_byte_ekle(ik, modrm_hesapla(3, xmm_hedef & 7, xmm_kaynak & 7));
}

void x86_ondalık_topla(İkilKod *ik, int xmm_hedef, int xmm_kaynak)  { sse2_aritmetik(ik, 0x58, xmm_hedef, xmm_kaynak); }
void x86_ondalık_çıkar(İkilKod *ik, int xmm_hedef, int xmm_kaynak) { sse2_aritmetik(ik, 0x5C, xmm_hedef, xmm_kaynak); }
void x86_ondalık_çarp(İkilKod *ik, int xmm_hedef, int xmm_kaynak)  { sse2_aritmetik(ik, 0x59, xmm_hedef, xmm_kaynak); }
void x86_ondalık_böl(İkilKod *ik, int xmm_hedef, int xmm_kaynak)   { sse2_aritmetik(ik, 0x5E, xmm_hedef, xmm_kaynak); }

/* UCOMISD xmm, xmm — sırasız karşılaştırma
 * 66 0F 2E /r */
void x86_ondalık_karşılaştır(İkilKod *ik, int xmm_sol, int xmm_sağ) {
    ikil_byte_ekle(ik, 0x66);
    if (xmm_sol >= 8 || xmm_sağ >= 8) {
        ikil_byte_ekle(ik, rex_hesapla(0, xmm_sol, xmm_sağ));
    }
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x2E);
    ikil_byte_ekle(ik, modrm_hesapla(3, xmm_sol & 7, xmm_sağ & 7));
}

/* CVTSI2SD xmm, r64 — tam sayıdan ondalığa çevir
 * F2 REX.W 0F 2A /r */
void x86_tam_ondalığa_çevir(İkilKod *ik, int xmm_hedef, YazmaçNo kaynak) {
    ikil_byte_ekle(ik, 0xF2);
    ikil_byte_ekle(ik, rex_hesapla(1, xmm_hedef, kaynak));
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x2A);
    ikil_byte_ekle(ik, modrm_hesapla(3, xmm_hedef & 7, kaynak & 7));
}

/* CVTTSD2SI r64, xmm — ondalıktan tam sayıya çevir (kesme)
 * F2 REX.W 0F 2C /r */
void x86_ondalık_tama_çevir(İkilKod *ik, YazmaçNo hedef, int xmm_kaynak) {
    ikil_byte_ekle(ik, 0xF2);
    ikil_byte_ekle(ik, rex_hesapla(1, hedef, xmm_kaynak));
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x2C);
    ikil_byte_ekle(ik, modrm_hesapla(3, hedef & 7, xmm_kaynak & 7));
}

/* XORPD xmm, xmm — ondalık yazmaç sıfırlama
 * 66 0F 57 /r */
void x86_ondalık_sıfırla(İkilKod *ik, int xmm) {
    ikil_byte_ekle(ik, 0x66);
    if (xmm >= 8) {
        ikil_byte_ekle(ik, rex_hesapla(0, xmm, xmm));
    }
    ikil_byte_ekle(ik, 0x0F);
    ikil_byte_ekle(ik, 0x57);
    ikil_byte_ekle(ik, modrm_hesapla(3, xmm & 7, xmm & 7));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *                     DİĞER KOMUTLAR
 * ═══════════════════════════════════════════════════════════════════════════ */

/* NOP: 90 */
void x86_nop(İkilKod *ik) {
    ikil_byte_ekle(ik, 0x90);
}

/* REP MOVSB — rsi→rdi kopyalama, rcx adet: F3 A4 */
void x86_tekrarla_kopyala(İkilKod *ik) {
    ikil_byte_ekle(ik, 0xF3);
    ikil_byte_ekle(ik, 0xA4);
}

/* REP STOSB — al→rdi doldurma, rcx adet: F3 AA */
void x86_tekrarla_doldur(İkilKod *ik) {
    ikil_byte_ekle(ik, 0xF3);
    ikil_byte_ekle(ik, 0xAA);
}

/* LEAVE: C9 (mov rsp,rbp; pop rbp) */
void x86_ayrıl(İkilKod *ik) {
    ikil_byte_ekle(ik, 0xC9);
}

/* CDQE: REX.W + 98 (eax sign-extend → rax) */
void x86_işaret_genişlet_eax(İkilKod *ik) {
    ikil_byte_ekle(ik, 0x48);
    ikil_byte_ekle(ik, 0x98);
}
