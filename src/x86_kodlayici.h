/*
 * x86_kodlayici.h — x86_64 Makine Kodu Kodlayıcı Başlığı
 *
 * Tonyukuk derleyicisi için doğrudan x86_64 makine kodu üretimi.
 * AT&T assembly metin üretmek yerine ham byte dizileri oluşturur.
 */

#ifndef X86_KODLAYICI_H
#define X86_KODLAYICI_H

#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *                      İKİL KOD BUFFER'I
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t *veri;          /* byte dizisi */
    int      uzunluk;       /* mevcut uzunluk */
    int      kapasite;      /* tahsis edilen kapasite */
} İkilKod;

void ikil_başlat(İkilKod *ik);
void ikil_serbest(İkilKod *ik);
void ikil_byte_ekle(İkilKod *ik, uint8_t b);
void ikil_word_ekle(İkilKod *ik, uint16_t w);       /* 2 byte, little-endian */
void ikil_dword_ekle(İkilKod *ik, uint32_t dw);     /* 4 byte, little-endian */
void ikil_qword_ekle(İkilKod *ik, uint64_t qw);     /* 8 byte, little-endian */
void ikil_veri_ekle(İkilKod *ik, const uint8_t *veri, int boyut);

/* Belirli ofset'teki int32 değerini yaz (yeniden konumlama yaması için) */
void ikil_yama_int32(İkilKod *ik, int ofset, int32_t değer);

/* ═══════════════════════════════════════════════════════════════════════════
 *                      YAZMAÇ NUMARALARI
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    YAZ_RAX = 0,  YAZ_RCX = 1,  YAZ_RDX = 2,  YAZ_RBX = 3,
    YAZ_RSP = 4,  YAZ_RBP = 5,  YAZ_RSI = 6,  YAZ_RDI = 7,
    YAZ_R8  = 8,  YAZ_R9  = 9,  YAZ_R10 = 10, YAZ_R11 = 11,
    YAZ_R12 = 12, YAZ_R13 = 13, YAZ_R14 = 14, YAZ_R15 = 15
} YazmaçNo;

/* SSE yazmaçları (aynı numaralama, bağlamda ayırt edilir) */
#define YAZ_XMM0  0
#define YAZ_XMM1  1
#define YAZ_XMM2  2
#define YAZ_XMM3  3
#define YAZ_XMM4  4
#define YAZ_XMM5  5
#define YAZ_XMM6  6
#define YAZ_XMM7  7

/* ═══════════════════════════════════════════════════════════════════════════
 *                      VERİ TAŞIMA KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* MOV yazmaç, anlık64 — movabs (REX.W + B8+rd + imm64) */
void x86_taşı_yazmaç_sabit64(İkilKod *ik, YazmaçNo hedef, int64_t değer);

/* MOV yazmaç, anlık32 — sign-extend (REX.W + C7 /0 + imm32) */
void x86_taşı_yazmaç_sabit32(İkilKod *ik, YazmaçNo hedef, int32_t değer);

/* MOV yazmaç, yazmaç (REX.W + 89 /r veya 8B /r) */
void x86_taşı_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak);

/* MOV yazmaç, [taban+ofset] (REX.W + 8B /r + ModRM + disp) */
void x86_taşı_yazmaç_bellek(İkilKod *ik, YazmaçNo hedef, YazmaçNo taban, int32_t ofset);

/* MOV [taban+ofset], yazmaç (REX.W + 89 /r + ModRM + disp) */
void x86_taşı_bellek_yazmaç(İkilKod *ik, YazmaçNo taban, int32_t ofset, YazmaçNo kaynak);

/* MOV byte [taban+ofset], kaynak_low8 (88 /r + ModRM + disp) — tek byte yaz */
void x86_taşı_bellek_yazmaç_byte(İkilKod *ik, YazmaçNo taban, int32_t ofset, YazmaçNo kaynak);

/* ═══════════════════════════════════════════════════════════════════════════
 *                      ARİTMETİK KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

void x86_topla_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak);
void x86_çıkar_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak);
void x86_çarp_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak);
void x86_böl_yazmaç(İkilKod *ik, YazmaçNo bölen);     /* IDIV: rdx:rax / bölen */
void x86_işaret_genişlet(İkilKod *ik);                  /* CQO: rax → rdx:rax */

void x86_topla_yazmaç_sabit32(İkilKod *ik, YazmaçNo hedef, int32_t değer);
void x86_çıkar_yazmaç_sabit32(İkilKod *ik, YazmaçNo hedef, int32_t değer);

void x86_olumsuzla(İkilKod *ik, YazmaçNo yaz);         /* NEG yazmaç */
void x86_değillebitsel(İkilKod *ik, YazmaçNo yaz);     /* NOT yazmaç */

/* ═══════════════════════════════════════════════════════════════════════════
 *                      KARŞILAŞTIRMA KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

void x86_karşılaştır_yazmaç_yazmaç(İkilKod *ik, YazmaçNo sol, YazmaçNo sağ);
void x86_karşılaştır_yazmaç_sabit32(İkilKod *ik, YazmaçNo yaz, int32_t değer);
void x86_sına_yazmaç_yazmaç(İkilKod *ik, YazmaçNo sol, YazmaçNo sağ);

/* SETcc — koşullu byte ayarla (yazmaç'ın düşük byte'ını 0 veya 1 yapar) */
void x86_koşul_eşit(İkilKod *ik, YazmaçNo yaz);           /* SETE */
void x86_koşul_eşit_değil(İkilKod *ik, YazmaçNo yaz);     /* SETNE */
void x86_koşul_küçük(İkilKod *ik, YazmaçNo yaz);           /* SETL */
void x86_koşul_büyük(İkilKod *ik, YazmaçNo yaz);           /* SETG */
void x86_koşul_küçük_eşit(İkilKod *ik, YazmaçNo yaz);     /* SETLE */
void x86_koşul_büyük_eşit(İkilKod *ik, YazmaçNo yaz);     /* SETGE */

/* MOVZX — byte'ı qword'e sıfır genişlet */
void x86_sıfır_genişlet_byte(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak);

/* ═══════════════════════════════════════════════════════════════════════════
 *                      DALLANMA KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Tüm atlama fonksiyonları rel32 ofseti için yama noktasını döndürür.
 * x86_atlama_yamala() ile hedef ofset hesaplanıp yazılır.
 */

int x86_atla(İkilKod *ik);                     /* JMP rel32 */
int x86_atla_eşitse(İkilKod *ik);              /* JE/JZ rel32 */
int x86_atla_eşit_değilse(İkilKod *ik);        /* JNE/JNZ rel32 */
int x86_atla_küçükse(İkilKod *ik);             /* JL rel32 */
int x86_atla_büyükse(İkilKod *ik);             /* JG rel32 */
int x86_atla_küçük_eşitse(İkilKod *ik);        /* JLE rel32 */
int x86_atla_büyük_eşitse(İkilKod *ik);        /* JGE rel32 */

/* Atlama hedefini yamala: rel32 = hedef_ofset - (yama_ofset + 4) */
void x86_atlama_yamala(İkilKod *ik, int yama_ofseti, int hedef_ofseti);

/* ═══════════════════════════════════════════════════════════════════════════
 *                      ÇAĞRI / DÖNÜŞ
 * ═══════════════════════════════════════════════════════════════════════════ */

int  x86_çağır_yakın(İkilKod *ik);              /* CALL rel32, yama ofseti döner */
void x86_çağır_dolaylı(İkilKod *ik, YazmaçNo yaz); /* CALL *yazmaç (FF /2) */
void x86_dön(İkilKod *ik);                      /* RET (C3) */

/* ═══════════════════════════════════════════════════════════════════════════
 *                      YIĞIN İŞLEMLERİ
 * ═══════════════════════════════════════════════════════════════════════════ */

void x86_yığına_it(İkilKod *ik, YazmaçNo yaz);      /* PUSH yazmaç */
void x86_yığından_çek(İkilKod *ik, YazmaçNo yaz);   /* POP yazmaç */

/* ═══════════════════════════════════════════════════════════════════════════
 *                      SİSTEM ÇAĞRISI
 * ═══════════════════════════════════════════════════════════════════════════ */

void x86_sistem_çağrısı(İkilKod *ik);           /* SYSCALL (0F 05) */

/* ═══════════════════════════════════════════════════════════════════════════
 *                      ADRES KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* LEA yazmaç, [taban+ofset] */
void x86_adres_yükle(İkilKod *ik, YazmaçNo hedef, YazmaçNo taban, int32_t ofset);

/* LEA yazmaç, [RIP+ofset32] — RIP-bağıl adresleme
 * Dönüş: disp32 alanının buffer ofseti (yama için) */
int x86_adres_yükle_rip_bağıl(İkilKod *ik, YazmaçNo hedef);

/* MOV yazmaç, [RIP+ofset32] — RIP-bağıl bellek okuma
 * Dönüş: disp32 alanının buffer ofseti */
int x86_taşı_yazmaç_rip_bağıl(İkilKod *ik, YazmaçNo hedef);

/* MOV [RIP+ofset32], yazmaç — RIP-bağıl bellek yazma
 * Dönüş: disp32 alanının buffer ofseti */
int x86_taşı_rip_bağıl_yazmaç(İkilKod *ik, YazmaçNo kaynak);

/* ═══════════════════════════════════════════════════════════════════════════
 *                      MANTIK / BİT İŞLEMLERİ
 * ═══════════════════════════════════════════════════════════════════════════ */

void x86_özel_veya_yazmaç_yazmaç(İkilKod *ik, YazmaçNo sol, YazmaçNo sağ);
void x86_ve_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak);
void x86_veya_yazmaç_yazmaç(İkilKod *ik, YazmaçNo hedef, YazmaçNo kaynak);
void x86_ve_yazmaç_sabit32(İkilKod *ik, YazmaçNo hedef, int32_t değer);

void x86_sola_kaydır_cl(İkilKod *ik, YazmaçNo hedef);
void x86_sağa_kaydır_cl(İkilKod *ik, YazmaçNo hedef);

/* ═══════════════════════════════════════════════════════════════════════════
 *                      SSE2 ONDALIK (double) KOMUTLARI
 * ═══════════════════════════════════════════════════════════════════════════ */

/* MOVSD — çift duyarlıklı kayan nokta taşıma */
void x86_ondalık_taşı_yazmaç_bellek(İkilKod *ik, int xmm_hedef, YazmaçNo taban, int32_t ofset);
void x86_ondalık_taşı_bellek_yazmaç(İkilKod *ik, YazmaçNo taban, int32_t ofset, int xmm_kaynak);
void x86_ondalık_taşı_yazmaç_yazmaç(İkilKod *ik, int xmm_hedef, int xmm_kaynak);

/* MOVSD xmm, [RIP+ofset32] — RIP-bağıl double yükleme
 * Dönüş: disp32 alanının buffer ofseti */
int x86_ondalık_taşı_rip_bağıl(İkilKod *ik, int xmm_hedef);

/* Aritmetik */
void x86_ondalık_topla(İkilKod *ik, int xmm_hedef, int xmm_kaynak);    /* ADDSD */
void x86_ondalık_çıkar(İkilKod *ik, int xmm_hedef, int xmm_kaynak);   /* SUBSD */
void x86_ondalık_çarp(İkilKod *ik, int xmm_hedef, int xmm_kaynak);    /* MULSD */
void x86_ondalık_böl(İkilKod *ik, int xmm_hedef, int xmm_kaynak);     /* DIVSD */

/* Karşılaştırma */
void x86_ondalık_karşılaştır(İkilKod *ik, int xmm_sol, int xmm_sağ);  /* UCOMISD */

/* Dönüşüm */
void x86_tam_ondalığa_çevir(İkilKod *ik, int xmm_hedef, YazmaçNo kaynak);  /* CVTSI2SD */
void x86_ondalık_tama_çevir(İkilKod *ik, YazmaçNo hedef, int xmm_kaynak);  /* CVTTSD2SI */

/* XORPD — ondalık yazmaç sıfırlama */
void x86_ondalık_sıfırla(İkilKod *ik, int xmm);

/* ═══════════════════════════════════════════════════════════════════════════
 *                      DİĞER
 * ═══════════════════════════════════════════════════════════════════════════ */

void x86_nop(İkilKod *ik);                      /* NOP (90) */

/* REP MOVSB — byte kopyalama (rsi→rdi, rcx adet) */
void x86_tekrarla_kopyala(İkilKod *ik);

/* REP STOSB — byte doldurma (al→rdi, rcx adet) */
void x86_tekrarla_doldur(İkilKod *ik);

/* LEAVE — mov rsp,rbp; pop rbp */
void x86_ayrıl(İkilKod *ik);

/* CDQE — eax'ı rax'a sign-extend et */
void x86_işaret_genişlet_eax(İkilKod *ik);

#endif /* X86_KODLAYICI_H */
