#include "uretici.h"
#include "anlam.h"
#include "modul.h"
#include "hata.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* Yardımcı: assembly satırı yaz */
static void yaz(Üretici *u, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->cikti, buf);
}

static void veri_yaz(Üretici *u, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->veri_bolumu, buf);
}

static void yardimci_yaz(Üretici *u, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->yardimcilar, buf);
}

static void bss_yaz(Üretici *u, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->bss_bolumu, buf);
}

static int yeni_etiket(Üretici *u) {
    return u->etiket_sayac++;
}

/* İleri bildirimler */
static void ifade_üret(Üretici *u, Düğüm *d);
static void bildirim_uret(Üretici *u, Düğüm *d);
static void blok_uret(Üretici *u, Düğüm *blok);
static void hata_bolme_sifir_uret(Üretici *u);
static void hata_dizi_sinir_uret(Üretici *u);
static void hata_bellek_uret(Üretici *u);
static void islev_uret(Üretici *u, Düğüm *d);

/* ---- String literal ---- */
static int metin_literal_ekle(Üretici *u, const char *metin, int uzunluk) {
    int idx = u->metin_sayac++;
    veri_yaz(u, ".LC%d:", idx);

    /* Byte dizisi olarak yaz (UTF-8 güvenli, escape sequence destekli) */
    Metin buf;
    metin_baslat(&buf);
    metin_ekle(&buf, "    .byte ");

    int gercek_uzunluk = 0;
    for (int i = 0; i < uzunluk; i++) {
        if (gercek_uzunluk > 0) metin_ekle(&buf, ",");
        char num[8];
        unsigned char c = (unsigned char)metin[i];

        if (c == '\\' && i + 1 < uzunluk) {
            char sonraki = metin[i + 1];
            int escape_degeri = -1;
            switch (sonraki) {
                case 'n':  escape_degeri = 10;  break;  /* newline */
                case 't':  escape_degeri = 9;   break;  /* tab */
                case 'r':  escape_degeri = 13;  break;  /* carriage return */
                case '\\': escape_degeri = 92;  break;  /* backslash */
                case '"':  escape_degeri = 34;  break;  /* double quote */
                case '0':  escape_degeri = 0;   break;  /* null */
            }
            if (escape_degeri >= 0) {
                snprintf(num, sizeof(num), "%d", escape_degeri);
                metin_ekle(&buf, num);
                gercek_uzunluk++;
                i++;  /* sonraki karakteri atla */
                continue;
            }
        }

        snprintf(num, sizeof(num), "%d", c);
        metin_ekle(&buf, num);
        gercek_uzunluk++;
    }

    veri_yaz(u, "%s", buf.veri);
    veri_yaz(u, "    .LC%d_len = %d", idx, gercek_uzunluk);
    metin_serbest(&buf);
    return idx;
}

/* ---- Double sabit desteği ---- */

/* Double sabitini .rodata'ya ekle, etiket indeksini döndür */
static int ondalik_sabit_ekle(Üretici *u, double deger) {
    int idx = u->ondalik_sayac++;
    /* IEEE 754 double'ı 8 byte olarak sakla */
    union { double d; uint64_t u64; } cvt;
    cvt.d = deger;
    veri_yaz(u, ".LD%d:", idx);
    veri_yaz(u, "    .quad   %llu", (unsigned long long)cvt.u64);
    return idx;
}

/* ---- İfade üretimi (sonuç rax'ta, ondalık ise xmm0'da) ---- */

static void tam_sayi_uret(Üretici *u, Düğüm *d) {
    yaz(u, "    movq    $%lld, %%rax", (long long)d->veri.tam_deger);
}

static void metin_degeri_uret(Üretici *u, Düğüm *d) {
    int idx = metin_literal_ekle(u, d->veri.metin_değer,
                                  (int)strlen(d->veri.metin_değer));
    /* rax = pointer, rbx = length */
    yaz(u, "    leaq    .LC%d(%%rip), %%rax", idx);
    yaz(u, "    movq    $.LC%d_len, %%rbx", idx);
}

static void mantik_uret(Üretici *u, Düğüm *d) {
    yaz(u, "    movq    $%d, %%rax", d->veri.mantık_değer);
}

static void tanimlayici_uret(Üretici *u, Düğüm *d) {
    Sembol *s = sembol_ara(u->kapsam, d->veri.tanimlayici.isim);
    if (!s) return;

    if (s->global_mi) {
        /* Global değişken: RIP-relative erişim */
        if (s->tip == TİP_METİN || s->tip == TİP_DİZİ) {
            yaz(u, "    movq    _genel_%s(%%rip), %%rax", d->veri.tanimlayici.isim);
            yaz(u, "    movq    _genel_%s+8(%%rip), %%rbx", d->veri.tanimlayici.isim);
        } else if (s->tip == TİP_ONDALIK) {
            yaz(u, "    movsd   _genel_%s(%%rip), %%xmm0", d->veri.tanimlayici.isim);
        } else {
            yaz(u, "    movq    _genel_%s(%%rip), %%rax", d->veri.tanimlayici.isim);
        }
        return;
    }

    int offset = (s->yerel_indeks + 1) * 8;
    if (s->tip == TİP_METİN || s->tip == TİP_DİZİ) {
        yaz(u, "    movq    -%d(%%rbp), %%rax", offset);
        yaz(u, "    movq    -%d(%%rbp), %%rbx", offset + 8);
    } else if (s->tip == TİP_ONDALIK) {
        yaz(u, "    movsd   -%d(%%rbp), %%xmm0", offset);
    } else {
        /* TİP_TAM, TİP_MANTIK, TİP_SINIF (pointer) */
        yaz(u, "    movq    -%d(%%rbp), %%rax", offset);
    }
}

static void metin_birlestir_uret_fn(Üretici *u);
static void metin_karsilastir_uret_fn(Üretici *u);

/* Yardımcı: düğümün sınıf adını bul */
/* Kapanış: lambda gövdesindeki serbest değişkenleri topla */
static void kapanis_yakala_topla(Düğüm *d, Kapsam *lambda_kapsam,
                                  Kapsam *dis_kapsam, Arena *a,
                                  char ***isimler, int **indeksler, int *sayac) {
    if (!d) return;
    if (d->tur == DÜĞÜM_TANIMLAYICI && d->veri.tanimlayici.isim) {
        const char *isim = d->veri.tanimlayici.isim;
        /* Lambda kapsam dışında mı? (parametrelerde değilse) */
        Sembol *ic = NULL;
        /* Lambda kapsamında ara (parametreler) */
        unsigned int idx = 0;
        for (int i = 0; i < TABLO_BOYUT; i++) {
            if (lambda_kapsam->tablo[i] &&
                strcmp(lambda_kapsam->tablo[i]->isim, isim) == 0) {
                ic = lambda_kapsam->tablo[i];
                break;
            }
        }
        (void)idx;
        if (!ic) {
            /* Dış kapsamda var mı? */
            Sembol *dis = sembol_ara(dis_kapsam, isim);
            if (dis && !dis->global_mi && !dis->parametre_mi) {
                /* Zaten listeye eklendi mi kontrol et */
                for (int j = 0; j < *sayac; j++) {
                    if (strcmp((*isimler)[j], isim) == 0) return;
                }
                /* Ekle */
                if (*sayac < 32) {
                    (*isimler)[*sayac] = arena_strdup(a, isim);
                    (*indeksler)[*sayac] = dis->yerel_indeks;
                    (*sayac)++;
                }
            }
        }
    }
    /* Alt düğümleri tara */
    for (int i = 0; i < d->çocuk_sayısı; i++) {
        kapanis_yakala_topla(d->çocuklar[i], lambda_kapsam, dis_kapsam, a,
                             isimler, indeksler, sayac);
    }
}

static char *sinif_adi_bul(Üretici *u, Düğüm *nesne) {
    if (nesne->tur == DÜĞÜM_TANIMLAYICI) {
        Sembol *s = sembol_ara(u->kapsam, nesne->veri.tanimlayici.isim);
        if (s && s->sınıf_adı) return s->sınıf_adı;
    }
    return NULL;
}

static void ikili_islem_uret(Üretici *u, Düğüm *d) {
    /* Operatör yükleme: sol taraf sınıf ise metot çağrısı yap */
    if (d->çocuklar[0]->sonuç_tipi == TİP_SINIF) {
        SözcükTürü op = d->veri.islem.islem;
        const char *metot = NULL;
        if (op == TOK_ARTI) metot = "topla";
        else if (op == TOK_EKSI) metot = "cikar";
        else if (op == TOK_ÇARPIM) metot = "carp";
        else if (op == TOK_EŞİT_EŞİT) metot = "esit";
        else if (op == TOK_EŞİT_DEĞİL) metot = "esit_degil";
        else if (op == TOK_BÖLME) metot = "bol";
        else if (op == TOK_YÜZDE) metot = "mod";
        else if (op == TOK_KÜÇÜK) metot = "kucuk";
        else if (op == TOK_BÜYÜK) metot = "buyuk";
        else if (op == TOK_KÜÇÜK_EŞİT) metot = "kucuk_esit";
        else if (op == TOK_BÜYÜK_EŞİT) metot = "buyuk_esit";

        if (metot) {
            char *sınıf_adı = sinif_adi_bul(u, d->çocuklar[0]);
            if (sınıf_adı) {
                /* Sağ operandı hesapla, stack'e at */
                ifade_üret(u, d->çocuklar[1]);
                yaz(u, "    pushq   %%rax");
                /* Sol operandı hesapla -> rdi (bu/self) */
                ifade_üret(u, d->çocuklar[0]);
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    popq    %%rsi");
                /* SinifAdi_metot çağrısı */
                yaz(u, "    call    %s_%s", sınıf_adı, metot);
                return;
            }
        }
    }

    /* Boş birleştirme operatörü: sol ?? sağ */
    if (d->veri.islem.islem == TOK_SORU_SORU) {
        int lbl_not_null = yeni_etiket(u);
        int lbl_end = yeni_etiket(u);

        /* Sol tarafı hesapla -> rax, rbx */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");            /* sol.len kaydet */
        yaz(u, "    pushq   %%rax");            /* sol.ptr kaydet */
        /* Boş kontrolü: rax == 0 ise null */
        yaz(u, "    testq   %%rax, %%rax");
        yaz(u, "    jnz     .L%d", lbl_not_null);  /* rax != 0 -> boş değil */
        /* Sol boş: stack'i temizle, sağ tarafı hesapla */
        yaz(u, "    addq    $16, %%rsp");
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    jmp     .L%d", lbl_end);
        /* Sol boş değil: geri yükle */
        yaz(u, ".L%d:", lbl_not_null);
        yaz(u, "    popq    %%rax");
        yaz(u, "    popq    %%rbx");
        yaz(u, ".L%d:", lbl_end);
        return;
    }

    /* Metin birleştirme: sol + sağ */
    if (d->sonuç_tipi == TİP_METİN && d->veri.islem.islem == TOK_ARTI) {
        metin_birlestir_uret_fn(u);
        /* Sol metin -> stack'e kaydet (rax=ptr, rbx=len) */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");  /* sol length */
        yaz(u, "    pushq   %%rax");  /* sol pointer */
        /* Sağ metin */
        ifade_üret(u, d->çocuklar[1]);
        /* Çağrı: _metin_birlestir(sol_ptr, sol_len, sag_ptr, sag_len) */
        yaz(u, "    movq    %%rax, %%rdx");   /* sag_ptr -> rdx */
        yaz(u, "    movq    %%rbx, %%rcx");   /* sag_len -> rcx */
        yaz(u, "    popq    %%rdi");           /* sol_ptr -> rdi */
        yaz(u, "    popq    %%rsi");           /* sol_len -> rsi */
        yaz(u, "    call    _metin_birlestir");
        /* Sonuç: rax = yeni ptr, rbx = yeni len */
        return;
    }

    /* Metin karşılaştırma */
    if (d->çocuklar[0]->sonuç_tipi == TİP_METİN &&
        d->çocuklar[1]->sonuç_tipi == TİP_METİN &&
        (d->veri.islem.islem == TOK_EŞİT_EŞİT ||
         d->veri.islem.islem == TOK_EŞİT_DEĞİL)) {
        metin_karsilastir_uret_fn(u);
        /* Sol metin -> stack */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");  /* sol len */
        yaz(u, "    pushq   %%rax");  /* sol ptr */
        /* Sağ metin */
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    movq    %%rax, %%rdx");   /* sag ptr */
        yaz(u, "    movq    %%rbx, %%rcx");   /* sag len */
        yaz(u, "    popq    %%rdi");           /* sol ptr */
        yaz(u, "    popq    %%rsi");           /* sol len */
        yaz(u, "    call    _metin_karsilastir");
        /* rax = 1 eşit, 0 farklı */
        if (d->veri.islem.islem == TOK_EŞİT_DEĞİL) {
            yaz(u, "    xorq    $1, %%rax");   /* sonucu ters çevir */
        }
        return;
    }

    /* Ondalık aritmetik (SSE) */
    if (d->sonuç_tipi == TİP_ONDALIK) {
        SözcükTürü op = d->veri.islem.islem;
        /* Sol -> xmm0 */
        ifade_üret(u, d->çocuklar[0]);
        /* Sol tam sayı ise xmm0'a dönüştür */
        if (d->çocuklar[0]->sonuç_tipi == TİP_TAM) {
            yaz(u, "    cvtsi2sd %%rax, %%xmm0");
        }
        /* xmm0'ı stack'e kaydet */
        yaz(u, "    subq    $8, %%rsp");
        yaz(u, "    movsd   %%xmm0, (%%rsp)");
        /* Sağ -> xmm1 */
        ifade_üret(u, d->çocuklar[1]);
        if (d->çocuklar[1]->sonuç_tipi == TİP_TAM) {
            yaz(u, "    cvtsi2sd %%rax, %%xmm1");
        } else {
            yaz(u, "    movsd   %%xmm0, %%xmm1");
        }
        /* Sol'u geri yükle */
        yaz(u, "    movsd   (%%rsp), %%xmm0");
        yaz(u, "    addq    $8, %%rsp");

        if (op == TOK_ARTI) yaz(u, "    addsd   %%xmm1, %%xmm0");
        else if (op == TOK_EKSI) yaz(u, "    subsd   %%xmm1, %%xmm0");
        else if (op == TOK_ÇARPIM) yaz(u, "    mulsd   %%xmm1, %%xmm0");
        else if (op == TOK_BÖLME) yaz(u, "    divsd   %%xmm1, %%xmm0");
        /* Karşılaştırma -> integer sonuç */
        else if (op == TOK_KÜÇÜK || op == TOK_BÜYÜK || op == TOK_KÜÇÜK_EŞİT ||
                 op == TOK_BÜYÜK_EŞİT || op == TOK_EŞİT_EŞİT || op == TOK_EŞİT_DEĞİL) {
            yaz(u, "    ucomisd %%xmm1, %%xmm0");
            if (op == TOK_KÜÇÜK)       yaz(u, "    setb    %%al");
            else if (op == TOK_BÜYÜK)  yaz(u, "    seta    %%al");
            else if (op == TOK_KÜÇÜK_EŞİT) yaz(u, "    setbe   %%al");
            else if (op == TOK_BÜYÜK_EŞİT) yaz(u, "    setae   %%al");
            else if (op == TOK_EŞİT_EŞİT)  yaz(u, "    sete    %%al");
            else if (op == TOK_EŞİT_DEĞİL) yaz(u, "    setne   %%al");
            yaz(u, "    movzbq  %%al, %%rax");
        }
        return;
    }

    /* Tam sayı aritmetik */
    /* Sol tarafı hesapla, stack'e at */
    ifade_üret(u, d->çocuklar[0]);
    yaz(u, "    pushq   %%rax");

    /* Sağ tarafı hesapla */
    ifade_üret(u, d->çocuklar[1]);
    yaz(u, "    movq    %%rax, %%rcx");  /* sağ -> rcx */
    yaz(u, "    popq    %%rax");          /* sol -> rax */

    switch (d->veri.islem.islem) {
    case TOK_ARTI:
        yaz(u, "    addq    %%rcx, %%rax");
        break;
    case TOK_EKSI:
        yaz(u, "    subq    %%rcx, %%rax");
        break;
    case TOK_ÇARPIM:
        yaz(u, "    imulq   %%rcx, %%rax");
        break;
    case TOK_BÖLME: {
        hata_bolme_sifir_uret(u);
        int lbl = yeni_etiket(u);
        yaz(u, "    testq   %%rcx, %%rcx");
        yaz(u, "    jnz     .L%d", lbl);
        yaz(u, "    call    _hata_bolme_sifir");
        yaz(u, ".L%d:", lbl);
        yaz(u, "    cqto");
        yaz(u, "    idivq   %%rcx");
        break;
    }
    case TOK_YÜZDE: {
        hata_bolme_sifir_uret(u);
        int lbl = yeni_etiket(u);
        yaz(u, "    testq   %%rcx, %%rcx");
        yaz(u, "    jnz     .L%d", lbl);
        yaz(u, "    call    _hata_bolme_sifir");
        yaz(u, ".L%d:", lbl);
        yaz(u, "    cqto");
        yaz(u, "    idivq   %%rcx");
        yaz(u, "    movq    %%rdx, %%rax");
        break;
    }

    /* Karşılaştırmalar */
    case TOK_EŞİT_EŞİT:
        yaz(u, "    cmpq    %%rcx, %%rax");
        yaz(u, "    sete    %%al");
        yaz(u, "    movzbq  %%al, %%rax");
        break;
    case TOK_EŞİT_DEĞİL:
        yaz(u, "    cmpq    %%rcx, %%rax");
        yaz(u, "    setne   %%al");
        yaz(u, "    movzbq  %%al, %%rax");
        break;
    case TOK_KÜÇÜK:
        yaz(u, "    cmpq    %%rcx, %%rax");
        yaz(u, "    setl    %%al");
        yaz(u, "    movzbq  %%al, %%rax");
        break;
    case TOK_BÜYÜK:
        yaz(u, "    cmpq    %%rcx, %%rax");
        yaz(u, "    setg    %%al");
        yaz(u, "    movzbq  %%al, %%rax");
        break;
    case TOK_KÜÇÜK_EŞİT:
        yaz(u, "    cmpq    %%rcx, %%rax");
        yaz(u, "    setle   %%al");
        yaz(u, "    movzbq  %%al, %%rax");
        break;
    case TOK_BÜYÜK_EŞİT:
        yaz(u, "    cmpq    %%rcx, %%rax");
        yaz(u, "    setge   %%al");
        yaz(u, "    movzbq  %%al, %%rax");
        break;

    /* Mantık */
    case TOK_VE:
        yaz(u, "    testq   %%rax, %%rax");
        yaz(u, "    setne   %%al");
        yaz(u, "    testq   %%rcx, %%rcx");
        yaz(u, "    setne   %%cl");
        yaz(u, "    andb    %%cl, %%al");
        yaz(u, "    movzbq  %%al, %%rax");
        break;
    case TOK_VEYA:
        yaz(u, "    orq     %%rcx, %%rax");
        yaz(u, "    testq   %%rax, %%rax");
        yaz(u, "    setne   %%al");
        yaz(u, "    movzbq  %%al, %%rax");
        break;

    /* Bit işlemleri */
    case TOK_BİT_VE:
        yaz(u, "    andq    %%rcx, %%rax");
        break;
    case TOK_BİT_VEYA:
        yaz(u, "    orq     %%rcx, %%rax");
        break;
    case TOK_BİT_XOR:
        yaz(u, "    xorq    %%rcx, %%rax");
        break;
    case TOK_SOL_KAYDIR:
        yaz(u, "    shlq    %%cl, %%rax");
        break;
    case TOK_SAĞ_KAYDIR:
        yaz(u, "    shrq    %%cl, %%rax");
        break;

    default:
        break;
    }
}

static void tekli_islem_uret(Üretici *u, Düğüm *d) {
    ifade_üret(u, d->çocuklar[0]);
    if (d->veri.islem.islem == TOK_EKSI) {
        yaz(u, "    negq    %%rax");
    } else if (d->veri.islem.islem == TOK_DEĞİL) {
        yaz(u, "    testq   %%rax, %%rax");
        yaz(u, "    sete    %%al");
        yaz(u, "    movzbq  %%al, %%rax");
    } else if (d->veri.islem.islem == TOK_BİT_DEĞİL) {
        yaz(u, "    notq    %%rax");
    }
}

/* yazdır — tam sayı yazdırma rutini (yardımcılar buffer'ına yaz) */
static void yazdir_tam_sayi_uret(Üretici *u) {
    if (u->yazdir_tam_uretildi) return;
    u->yazdir_tam_uretildi = 1;

    yardimci_yaz(u, "");
    yardimci_yaz(u, "# tam sayı yazdırma rutini");
    yardimci_yaz(u, "_yazdir_tam:");
    yardimci_yaz(u, "    pushq   %%rbp");
    yardimci_yaz(u, "    movq    %%rsp, %%rbp");
    yardimci_yaz(u, "    subq    $32, %%rsp");
    yardimci_yaz(u, "    movq    %%rdi, %%rax");
    yardimci_yaz(u, "    movq    %%rsp, %%rsi");
    yardimci_yaz(u, "    addq    $31, %%rsi");
    yardimci_yaz(u, "    movb    $10, (%%rsi)");
    yardimci_yaz(u, "    movq    $1, %%rcx");
    yardimci_yaz(u, "    testq   %%rax, %%rax");
    yardimci_yaz(u, "    jns     .Lpositif");
    yardimci_yaz(u, "    negq    %%rax");
    yardimci_yaz(u, "    movq    $1, %%r8");
    yardimci_yaz(u, "    jmp     .Ldigit_loop");
    yardimci_yaz(u, ".Lpositif:");
    yardimci_yaz(u, "    xorq    %%r8, %%r8");
    yardimci_yaz(u, ".Ldigit_loop:");
    yardimci_yaz(u, "    xorq    %%rdx, %%rdx");
    yardimci_yaz(u, "    movq    $10, %%r9");
    yardimci_yaz(u, "    divq    %%r9");
    yardimci_yaz(u, "    addb    $48, %%dl");
    yardimci_yaz(u, "    decq    %%rsi");
    yardimci_yaz(u, "    movb    %%dl, (%%rsi)");
    yardimci_yaz(u, "    incq    %%rcx");
    yardimci_yaz(u, "    testq   %%rax, %%rax");
    yardimci_yaz(u, "    jnz     .Ldigit_loop");
    yardimci_yaz(u, "    testq   %%r8, %%r8");
    yardimci_yaz(u, "    jz      .Lprint_it");
    yardimci_yaz(u, "    decq    %%rsi");
    yardimci_yaz(u, "    movb    $45, (%%rsi)");
    yardimci_yaz(u, "    incq    %%rcx");
    yardimci_yaz(u, ".Lprint_it:");
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    movq    %%rcx, %%rdx");
    yardimci_yaz(u, "    syscall");
    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");
}

/* yazdır — metin yazdırma (newline ile, yardımcılar buffer'ına yaz) */
static void yazdir_metin_uret_fn(Üretici *u) {
    if (u->yazdir_metin_uretildi) return;
    u->yazdir_metin_uretildi = 1;

    yardimci_yaz(u, "");
    yardimci_yaz(u, "# metin yazdırma rutini");
    yardimci_yaz(u, "_yazdir_metin:");
    yardimci_yaz(u, "    pushq   %%rbp");
    yardimci_yaz(u, "    movq    %%rsp, %%rbp");
    yardimci_yaz(u, "    movq    %%rsi, %%rdx");
    yardimci_yaz(u, "    movq    %%rdi, %%rsi");
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    syscall");
    yardimci_yaz(u, "    subq    $8, %%rsp");
    yardimci_yaz(u, "    movb    $10, (%%rsp)");
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    movq    %%rsp, %%rsi");
    yardimci_yaz(u, "    movq    $1, %%rdx");
    yardimci_yaz(u, "    syscall");
    yardimci_yaz(u, "    addq    $8, %%rsp");
    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");
}

/* metin birleştirme: rdi=sol_ptr, rsi=sol_len, rdx=sag_ptr, rcx=sag_len
   Sonuç: rax=yeni_ptr, rbx=toplam_len
   mmap ile bellek ayır, iki string'i kopyala */
static int metin_birlestir_uretildi = 0;
static void metin_birlestir_uret_fn(Üretici *u) {
    if (metin_birlestir_uretildi) return;
    metin_birlestir_uretildi = 1;
    hata_bellek_uret(u);

    yardimci_yaz(u, "");
    yardimci_yaz(u, "# metin birleştirme rutini");
    yardimci_yaz(u, "_metin_birlestir:");
    yardimci_yaz(u, "    pushq   %%rbp");
    yardimci_yaz(u, "    movq    %%rsp, %%rbp");
    yardimci_yaz(u, "    subq    $48, %%rsp");

    /* Parametreleri kaydet */
    yardimci_yaz(u, "    movq    %%rdi, -8(%%rbp)");   /* sol_ptr */
    yardimci_yaz(u, "    movq    %%rsi, -16(%%rbp)");  /* sol_len */
    yardimci_yaz(u, "    movq    %%rdx, -24(%%rbp)");  /* sag_ptr */
    yardimci_yaz(u, "    movq    %%rcx, -32(%%rbp)");  /* sag_len */

    /* Toplam uzunluk */
    yardimci_yaz(u, "    movq    %%rsi, %%rax");
    yardimci_yaz(u, "    addq    %%rcx, %%rax");
    yardimci_yaz(u, "    movq    %%rax, -40(%%rbp)");  /* toplam_len */

    /* _tr_nesne_olustur ile bellek ayır */
    yardimci_yaz(u, "    movq    $0, %%rdi");            /* NESNE_TIP_METIN */
    yardimci_yaz(u, "    movq    -40(%%rbp), %%rsi");   /* boyut = toplam_len */
    yardimci_yaz(u, "    call    _tr_nesne_olustur");
    hata_bellek_uret(u);
    yardimci_yaz(u, "    testq   %%rax, %%rax");
    yardimci_yaz(u, "    jnz     .Lmb_mmap_ok");
    yardimci_yaz(u, "    call    _hata_bellek");
    yardimci_yaz(u, ".Lmb_mmap_ok:");
    yardimci_yaz(u, "    movq    %%rax, -48(%%rbp)");   /* yeni_ptr */

    /* Sol string'i kopyala */
    yardimci_yaz(u, "    movq    -48(%%rbp), %%rdi");   /* hedef */
    yardimci_yaz(u, "    movq    -8(%%rbp), %%rsi");    /* kaynak */
    yardimci_yaz(u, "    movq    -16(%%rbp), %%rcx");   /* uzunluk */
    yardimci_yaz(u, "    rep movsb");

    /* Sağ string'i kopyala (rdi zaten doğru pozisyonda) */
    yardimci_yaz(u, "    movq    -24(%%rbp), %%rsi");   /* kaynak */
    yardimci_yaz(u, "    movq    -32(%%rbp), %%rcx");   /* uzunluk */
    yardimci_yaz(u, "    rep movsb");

    /* Sonuç */
    yardimci_yaz(u, "    movq    -48(%%rbp), %%rax");   /* yeni_ptr */
    yardimci_yaz(u, "    movq    -40(%%rbp), %%rbx");   /* toplam_len */

    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");
}

static int metin_karsilastir_uretildi = 0;
static void metin_karsilastir_uret_fn(Üretici *u) {
    if (metin_karsilastir_uretildi) return;
    metin_karsilastir_uretildi = 1;

    /* _metin_karsilastir(rdi=sol_ptr, rsi=sol_len, rdx=sag_ptr, rcx=sag_len) -> rax (1=eşit, 0=farklı) */
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# metin karşılaştırma rutini");
    yardimci_yaz(u, "_metin_karsilastir:");
    yardimci_yaz(u, "    pushq   %%rbp");
    yardimci_yaz(u, "    movq    %%rsp, %%rbp");

    /* Önce uzunluk kontrolü */
    yardimci_yaz(u, "    cmpq    %%rcx, %%rsi");
    yardimci_yaz(u, "    jne     .Lmk_farkli");

    /* Uzunluklar eşit, byte-by-byte karşılaştır */
    /* rdi = sol_ptr, rdx = sag_ptr, rsi = uzunluk (sayaç olarak kullan) */
    yardimci_yaz(u, "    movq    %%rsi, %%rcx");  /* rcx = uzunluk (sayaç) */
    yardimci_yaz(u, "    movq    %%rdx, %%rsi");  /* rsi = sag_ptr */
    /* rdi zaten sol_ptr */
    yardimci_yaz(u, "    repe cmpsb");
    yardimci_yaz(u, "    jne     .Lmk_farkli");

    /* Eşit */
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");

    yardimci_yaz(u, ".Lmk_farkli:");
    yardimci_yaz(u, "    xorq    %%rax, %%rax");
    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");
}

/* yazdır — ondalık yazdırma rutini
   Strateji: sayıyı 10^6 ile çarparak tam sayıya çevir, sonra bölmelerle yazdır.
   Bu, floating point aritmetiği minimize eder. */
static void yazdir_ondalik_uret_fn(Üretici *u) {
    if (u->yazdir_ondalik_uretildi) return;
    u->yazdir_ondalik_uretildi = 1;

    yardimci_yaz(u, "");
    yardimci_yaz(u, "# ondalık yazdırma rutini (xmm0 = double)");
    yardimci_yaz(u, "_yazdir_ondalik:");
    yardimci_yaz(u, "    pushq   %%rbp");
    yardimci_yaz(u, "    movq    %%rsp, %%rbp");
    yardimci_yaz(u, "    subq    $64, %%rsp");

    /* xmm0'ı kaydet */
    yardimci_yaz(u, "    movsd   %%xmm0, -8(%%rbp)");

    /* Negatif mi kontrol et */
    yardimci_yaz(u, "    xorpd   %%xmm1, %%xmm1");
    yardimci_yaz(u, "    ucomisd %%xmm0, %%xmm1");
    yardimci_yaz(u, "    jbe     .Lond_pos");

    /* Negatifse '-' yazdır ve mutlak değer al */
    yardimci_yaz(u, "    movb    $45, -48(%%rbp)");
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    leaq    -48(%%rbp), %%rsi");
    yardimci_yaz(u, "    movq    $1, %%rdx");
    yardimci_yaz(u, "    syscall");

    /* Mutlak değer: sign bit'i temizle */
    yardimci_yaz(u, "    movsd   -8(%%rbp), %%xmm0");
    yardimci_yaz(u, "    subsd   %%xmm0, %%xmm1");  /* 0 - x = -x */
    yardimci_yaz(u, "    movsd   %%xmm1, %%xmm0");
    yardimci_yaz(u, "    movsd   %%xmm0, -8(%%rbp)");

    yardimci_yaz(u, ".Lond_pos:");

    /* Tam kısım: cvttsd2si -> rax */
    yardimci_yaz(u, "    movsd   -8(%%rbp), %%xmm0");
    yardimci_yaz(u, "    cvttsd2si %%xmm0, %%rax");
    yardimci_yaz(u, "    movq    %%rax, -16(%%rbp)");  /* tam kısım sakla */

    /* Tam kısmı buffer'a yaz (sağdan sola) */
    yardimci_yaz(u, "    leaq    -32(%%rbp), %%rsi");
    yardimci_yaz(u, "    addq    $15, %%rsi");
    yardimci_yaz(u, "    movq    $0, %%rcx");
    yardimci_yaz(u, ".Lond_tl:");
    yardimci_yaz(u, "    xorq    %%rdx, %%rdx");
    yardimci_yaz(u, "    movq    $10, %%r9");
    yardimci_yaz(u, "    divq    %%r9");
    yardimci_yaz(u, "    addb    $48, %%dl");
    yardimci_yaz(u, "    movb    %%dl, (%%rsi)");
    yardimci_yaz(u, "    decq    %%rsi");
    yardimci_yaz(u, "    incq    %%rcx");
    yardimci_yaz(u, "    testq   %%rax, %%rax");
    yardimci_yaz(u, "    jnz     .Lond_tl");
    /* Yazdır: rsi+1'den rcx byte */
    yardimci_yaz(u, "    incq    %%rsi");
    yardimci_yaz(u, "    movq    %%rcx, %%rdx");
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    syscall");

    /* '.' yazdır */
    yardimci_yaz(u, "    movb    $46, -48(%%rbp)");
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    leaq    -48(%%rbp), %%rsi");
    yardimci_yaz(u, "    movq    $1, %%rdx");
    yardimci_yaz(u, "    syscall");

    /* Kesirli kısım: (deger - tam) * 1000000 */
    yardimci_yaz(u, "    movsd   -8(%%rbp), %%xmm0");         /* orijinal değer */
    yardimci_yaz(u, "    cvtsi2sdq -16(%%rbp), %%xmm1");       /* tam -> double */
    yardimci_yaz(u, "    subsd   %%xmm1, %%xmm0");             /* kesirli kısım */
    yardimci_yaz(u, "    movq    $1000000, %%rax");
    yardimci_yaz(u, "    cvtsi2sdq %%rax, %%xmm1");
    yardimci_yaz(u, "    mulsd   %%xmm1, %%xmm0");
    /* Yuvarlama: 0.5 ekle */
    yardimci_yaz(u, "    movq    $0x3FE0000000000000, %%rax"); /* 0.5 */
    yardimci_yaz(u, "    movq    %%rax, -56(%%rbp)");
    yardimci_yaz(u, "    addsd   -56(%%rbp), %%xmm0");
    yardimci_yaz(u, "    cvttsd2si %%xmm0, %%rax");

    /* 6 basamak yaz (başta sıfırlar dahil) */
    yardimci_yaz(u, "    leaq    -48(%%rbp), %%rsi");
    yardimci_yaz(u, "    addq    $5, %%rsi");
    yardimci_yaz(u, "    movq    $6, %%rcx");
    yardimci_yaz(u, ".Lond_kl:");
    yardimci_yaz(u, "    xorq    %%rdx, %%rdx");
    yardimci_yaz(u, "    movq    $10, %%r9");
    yardimci_yaz(u, "    divq    %%r9");
    yardimci_yaz(u, "    addb    $48, %%dl");
    yardimci_yaz(u, "    movb    %%dl, (%%rsi)");
    yardimci_yaz(u, "    decq    %%rsi");
    yardimci_yaz(u, "    decq    %%rcx");
    yardimci_yaz(u, "    jnz     .Lond_kl");
    /* Sondaki sıfırları kaldır (trailing zero suppression) */
    yardimci_yaz(u, "    incq    %%rsi");           /* rsi = başlangıç */
    yardimci_yaz(u, "    leaq    5(%%rsi), %%rdi"); /* rdi = son basamak */
    yardimci_yaz(u, "    movq    $6, %%rdx");       /* varsayilan uzunluk */
    yardimci_yaz(u, ".Lond_trim:");
    yardimci_yaz(u, "    cmpq    $1, %%rdx");       /* en az 1 basamak */
    yardimci_yaz(u, "    jle     .Lond_print");
    yardimci_yaz(u, "    cmpb    $48, (%%rdi)");    /* '0' mi? */
    yardimci_yaz(u, "    jne     .Lond_print");
    yardimci_yaz(u, "    decq    %%rdi");
    yardimci_yaz(u, "    decq    %%rdx");
    yardimci_yaz(u, "    jmp     .Lond_trim");
    yardimci_yaz(u, ".Lond_print:");
    /* Yazdır: rsi'den rdx byte */
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    syscall");

    /* Newline */
    yardimci_yaz(u, "    movb    $10, -48(%%rbp)");
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    leaq    -48(%%rbp), %%rsi");
    yardimci_yaz(u, "    movq    $1, %%rdx");
    yardimci_yaz(u, "    syscall");

    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");
}

/* ---- Runtime hata yardımcıları ---- */

static void hata_bolme_sifir_uret(Üretici *u) {
    if (u->hata_bolme_sifir_uretildi) return;
    u->hata_bolme_sifir_uretildi = 1;

    /* İstisna çerçevesi BSS'i gerekiyor (aktif handler kontrolü için) */
    if (!u->istisna_bss_uretildi) {
        bss_yaz(u, "    .comm   _istisna_cerceve, 56, 8");
        u->istisna_bss_uretildi = 1;
    }

    int idx = u->metin_sayac++;
    veri_yaz(u, ".LC%d:", idx);
    veri_yaz(u, "    .byte 72,97,116,97,58,32,115,105,102,105,114,97,32,98,111,108,109,101,10");
    veri_yaz(u, "    .LC%d_len = 19", idx);

    yardimci_yaz(u, "");
    yardimci_yaz(u, "# runtime hata: sifira bolme");
    yardimci_yaz(u, "_hata_bolme_sifir:");
    /* Aktif istisna çerçevesi var mı kontrol et */
    yardimci_yaz(u, "    cmpq    $0, _istisna_cerceve+24(%%rip)");
    yardimci_yaz(u, "    je      .Lbolme_exit_%d", idx);
    /* Aktif handler var: istisna fırlat */
    yardimci_yaz(u, "    leaq    .LC%d(%%rip), %%rax", idx);
    yardimci_yaz(u, "    movq    $.LC%d_len, %%rbx", idx);
    yardimci_yaz(u, "    movq    %%rax, _istisna_cerceve+32(%%rip)");
    yardimci_yaz(u, "    movq    %%rbx, _istisna_cerceve+40(%%rip)");
    yardimci_yaz(u, "    movq    $5, _istisna_cerceve+48(%%rip)");  /* BolmeHatasi=5 */
    yardimci_yaz(u, "    movq    _istisna_cerceve(%%rip), %%rbp");
    yardimci_yaz(u, "    movq    _istisna_cerceve+8(%%rip), %%rsp");
    yardimci_yaz(u, "    jmpq    *_istisna_cerceve+16(%%rip)");
    /* Handler yok: stderr'e yaz ve çık */
    yardimci_yaz(u, ".Lbolme_exit_%d:", idx);
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $2, %%rdi");           /* stderr */
    yardimci_yaz(u, "    leaq    .LC%d(%%rip), %%rsi", idx);
    yardimci_yaz(u, "    movq    $.LC%d_len, %%rdx", idx);
    yardimci_yaz(u, "    syscall");
    yardimci_yaz(u, "    movq    $60, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    syscall");
}

static void hata_dizi_sinir_uret(Üretici *u) {
    if (u->hata_dizi_sinir_uretildi) return;
    u->hata_dizi_sinir_uretildi = 1;

    /* İstisna çerçevesi BSS'i gerekiyor (aktif handler kontrolü için) */
    if (!u->istisna_bss_uretildi) {
        bss_yaz(u, "    .comm   _istisna_cerceve, 56, 8");
        u->istisna_bss_uretildi = 1;
    }

    int idx = u->metin_sayac++;
    veri_yaz(u, ".LC%d:", idx);
    /* "Hata: dizi sinir asimi\n" */
    veri_yaz(u, "    .byte 72,97,116,97,58,32,100,105,122,105,32,115,105,110,105,114,32,97,115,105,109,105,10");
    veri_yaz(u, "    .LC%d_len = 23", idx);

    yardimci_yaz(u, "");
    yardimci_yaz(u, "# runtime hata: dizi sinir asimi");
    yardimci_yaz(u, "_hata_dizi_sinir:");
    /* Aktif istisna çerçevesi var mı kontrol et */
    yardimci_yaz(u, "    cmpq    $0, _istisna_cerceve+24(%%rip)");
    yardimci_yaz(u, "    je      .Ldizi_exit_%d", idx);
    /* Aktif handler var: istisna fırlat */
    yardimci_yaz(u, "    leaq    .LC%d(%%rip), %%rax", idx);
    yardimci_yaz(u, "    movq    $.LC%d_len, %%rbx", idx);
    yardimci_yaz(u, "    movq    %%rax, _istisna_cerceve+32(%%rip)");
    yardimci_yaz(u, "    movq    %%rbx, _istisna_cerceve+40(%%rip)");
    yardimci_yaz(u, "    movq    $3, _istisna_cerceve+48(%%rip)");  /* DizinHatasi=3 */
    yardimci_yaz(u, "    movq    _istisna_cerceve(%%rip), %%rbp");
    yardimci_yaz(u, "    movq    _istisna_cerceve+8(%%rip), %%rsp");
    yardimci_yaz(u, "    jmpq    *_istisna_cerceve+16(%%rip)");
    /* Handler yok: stderr'e yaz ve çık */
    yardimci_yaz(u, ".Ldizi_exit_%d:", idx);
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $2, %%rdi");           /* stderr */
    yardimci_yaz(u, "    leaq    .LC%d(%%rip), %%rsi", idx);
    yardimci_yaz(u, "    movq    $.LC%d_len, %%rdx", idx);
    yardimci_yaz(u, "    syscall");
    yardimci_yaz(u, "    movq    $60, %%rax");
    yardimci_yaz(u, "    movq    $1, %%rdi");
    yardimci_yaz(u, "    syscall");
}

static void hata_bellek_uret(Üretici *u) {
    if (u->hata_bellek_uretildi) return;
    u->hata_bellek_uretildi = 1;

    int idx = u->metin_sayac++;
    veri_yaz(u, ".LC%d:", idx);
    /* "Hata: bellek ayrilamadi\n" */
    veri_yaz(u, "    .byte 72,97,116,97,58,32,98,101,108,108,101,107,32,97,121,114,105,108,97,109,97,100,105,10");
    veri_yaz(u, "    .LC%d_len = 24", idx);

    yardimci_yaz(u, "");
    yardimci_yaz(u, "# runtime hata: bellek ayrilamadi");
    yardimci_yaz(u, "_hata_bellek:");
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    $2, %%rdi");           /* stderr */
    yardimci_yaz(u, "    leaq    .LC%d(%%rip), %%rsi", idx);
    yardimci_yaz(u, "    movq    $.LC%d_len, %%rdx", idx);
    yardimci_yaz(u, "    syscall");
    yardimci_yaz(u, "    movq    $60, %%rax");
    yardimci_yaz(u, "    movq    $137, %%rdi");
    yardimci_yaz(u, "    syscall");
}

/* ---- matematik modülü yardımcıları ---- */

static void mat_mutlak_uret(Üretici *u) {
    if (u->mat_mutlak_uretildi) return;
    u->mat_mutlak_uretildi = 1;
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# mutlak(x) - mutlak deger");
    yardimci_yaz(u, "_mat_mutlak:");
    yardimci_yaz(u, "    movq    %%rdi, %%rax");
    yardimci_yaz(u, "    testq   %%rax, %%rax");
    yardimci_yaz(u, "    jns     .Lmut_pos");
    yardimci_yaz(u, "    negq    %%rax");
    yardimci_yaz(u, ".Lmut_pos:");
    yardimci_yaz(u, "    ret");
}

static void mat_kuvvet_uret(Üretici *u) {
    if (u->mat_kuvvet_uretildi) return;
    u->mat_kuvvet_uretildi = 1;
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# kuvvet(x,y) - x^y");
    yardimci_yaz(u, "_mat_kuvvet:");
    yardimci_yaz(u, "    pushq   %%rbp");
    yardimci_yaz(u, "    movq    %%rsp, %%rbp");
    yardimci_yaz(u, "    movq    $1, %%rax");
    yardimci_yaz(u, "    movq    %%rdi, %%rcx");
    yardimci_yaz(u, "    movq    %%rsi, %%rdx");
    yardimci_yaz(u, ".Lkuv_loop:");
    yardimci_yaz(u, "    testq   %%rdx, %%rdx");
    yardimci_yaz(u, "    jz      .Lkuv_done");
    yardimci_yaz(u, "    imulq   %%rcx, %%rax");
    yardimci_yaz(u, "    decq    %%rdx");
    yardimci_yaz(u, "    jmp     .Lkuv_loop");
    yardimci_yaz(u, ".Lkuv_done:");
    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");
}

static void mat_karekok_uret(Üretici *u) {
    if (u->mat_karekok_uretildi) return;
    u->mat_karekok_uretildi = 1;
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# karekok(x) - karekok (SSE2 sqrtsd)");
    yardimci_yaz(u, "_mat_karekok:");
    yardimci_yaz(u, "    sqrtsd  %%xmm0, %%xmm0");
    yardimci_yaz(u, "    ret");
}

static void mat_min_uret(Üretici *u) {
    if (u->mat_min_uretildi) return;
    u->mat_min_uretildi = 1;
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# min(x,y)");
    yardimci_yaz(u, "_mat_min:");
    yardimci_yaz(u, "    movq    %%rdi, %%rax");
    yardimci_yaz(u, "    cmpq    %%rsi, %%rdi");
    yardimci_yaz(u, "    cmovgq  %%rsi, %%rax");
    yardimci_yaz(u, "    ret");
}

static void mat_maks_uret(Üretici *u) {
    if (u->mat_maks_uretildi) return;
    u->mat_maks_uretildi = 1;
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# maks(x,y)");
    yardimci_yaz(u, "_mat_maks:");
    yardimci_yaz(u, "    movq    %%rdi, %%rax");
    yardimci_yaz(u, "    cmpq    %%rsi, %%rdi");
    yardimci_yaz(u, "    cmovlq  %%rsi, %%rax");
    yardimci_yaz(u, "    ret");
}

static void mat_mod_uret(Üretici *u) {
    if (u->mat_mod_uretildi) return;
    u->mat_mod_uretildi = 1;
    hata_bolme_sifir_uret(u);
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# mod(x,y)");
    yardimci_yaz(u, "_mat_mod:");
    yardimci_yaz(u, "    testq   %%rsi, %%rsi");
    yardimci_yaz(u, "    jnz     .Lmod_ok");
    yardimci_yaz(u, "    call    _hata_bolme_sifir");
    yardimci_yaz(u, ".Lmod_ok:");
    yardimci_yaz(u, "    movq    %%rdi, %%rax");
    yardimci_yaz(u, "    cqto");
    yardimci_yaz(u, "    idivq   %%rsi");
    yardimci_yaz(u, "    movq    %%rdx, %%rax");
    yardimci_yaz(u, "    ret");
}

/* ---- metin modülü yardımcıları ---- */

static void mtn_harf_buyut_uret(Üretici *u) {
    if (u->mtn_harf_buyut_uretildi) return;
    u->mtn_harf_buyut_uretildi = 1;
    hata_bellek_uret(u);
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# harf_buyut(m) - buyuk harfe cevir");
    yardimci_yaz(u, "_mtn_harf_buyut:");
    yardimci_yaz(u, "    pushq   %%rbp");
    yardimci_yaz(u, "    movq    %%rsp, %%rbp");
    yardimci_yaz(u, "    subq    $32, %%rsp");
    yardimci_yaz(u, "    movq    %%rdi, -8(%%rbp)");
    yardimci_yaz(u, "    movq    %%rsi, -16(%%rbp)");
    /* _tr_nesne_olustur ile bellek ayır */
    yardimci_yaz(u, "    movq    $0, %%rdi");            /* NESNE_TIP_METIN */
    yardimci_yaz(u, "    movq    -16(%%rbp), %%rsi");   /* boyut = len */
    yardimci_yaz(u, "    call    _tr_nesne_olustur");
    yardimci_yaz(u, "    testq   %%rax, %%rax");
    yardimci_yaz(u, "    jnz     .Lhb_mmap_ok");
    yardimci_yaz(u, "    call    _hata_bellek");
    yardimci_yaz(u, ".Lhb_mmap_ok:");
    yardimci_yaz(u, "    movq    %%rax, -24(%%rbp)");
    /* Copy loop with uppercase */
    yardimci_yaz(u, "    movq    -8(%%rbp), %%rsi");
    yardimci_yaz(u, "    movq    -24(%%rbp), %%rdi");
    yardimci_yaz(u, "    movq    -16(%%rbp), %%rcx");
    yardimci_yaz(u, ".Lhb_loop:");
    yardimci_yaz(u, "    testq   %%rcx, %%rcx");
    yardimci_yaz(u, "    jz      .Lhb_done");
    yardimci_yaz(u, "    movb    (%%rsi), %%al");
    yardimci_yaz(u, "    cmpb    $97, %%al");
    yardimci_yaz(u, "    jb      .Lhb_copy");
    yardimci_yaz(u, "    cmpb    $122, %%al");
    yardimci_yaz(u, "    ja      .Lhb_copy");
    yardimci_yaz(u, "    subb    $32, %%al");
    yardimci_yaz(u, ".Lhb_copy:");
    yardimci_yaz(u, "    movb    %%al, (%%rdi)");
    yardimci_yaz(u, "    incq    %%rsi");
    yardimci_yaz(u, "    incq    %%rdi");
    yardimci_yaz(u, "    decq    %%rcx");
    yardimci_yaz(u, "    jmp     .Lhb_loop");
    yardimci_yaz(u, ".Lhb_done:");
    yardimci_yaz(u, "    movq    -24(%%rbp), %%rax");
    yardimci_yaz(u, "    movq    -16(%%rbp), %%rbx");
    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");
}

static void mtn_harf_kucult_uret(Üretici *u) {
    if (u->mtn_harf_kucult_uretildi) return;
    u->mtn_harf_kucult_uretildi = 1;
    hata_bellek_uret(u);
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# harf_kucult(m) - kucuk harfe cevir");
    yardimci_yaz(u, "_mtn_harf_kucult:");
    yardimci_yaz(u, "    pushq   %%rbp");
    yardimci_yaz(u, "    movq    %%rsp, %%rbp");
    yardimci_yaz(u, "    subq    $32, %%rsp");
    yardimci_yaz(u, "    movq    %%rdi, -8(%%rbp)");
    yardimci_yaz(u, "    movq    %%rsi, -16(%%rbp)");
    /* _tr_nesne_olustur ile bellek ayır */
    yardimci_yaz(u, "    movq    $0, %%rdi");            /* NESNE_TIP_METIN */
    yardimci_yaz(u, "    movq    -16(%%rbp), %%rsi");   /* boyut = len */
    yardimci_yaz(u, "    call    _tr_nesne_olustur");
    yardimci_yaz(u, "    testq   %%rax, %%rax");
    yardimci_yaz(u, "    jnz     .Lhk_mmap_ok");
    yardimci_yaz(u, "    call    _hata_bellek");
    yardimci_yaz(u, ".Lhk_mmap_ok:");
    yardimci_yaz(u, "    movq    %%rax, -24(%%rbp)");
    /* Copy loop with lowercase */
    yardimci_yaz(u, "    movq    -8(%%rbp), %%rsi");
    yardimci_yaz(u, "    movq    -24(%%rbp), %%rdi");
    yardimci_yaz(u, "    movq    -16(%%rbp), %%rcx");
    yardimci_yaz(u, ".Lhk_loop:");
    yardimci_yaz(u, "    testq   %%rcx, %%rcx");
    yardimci_yaz(u, "    jz      .Lhk_done");
    yardimci_yaz(u, "    movb    (%%rsi), %%al");
    yardimci_yaz(u, "    cmpb    $65, %%al");
    yardimci_yaz(u, "    jb      .Lhk_copy");
    yardimci_yaz(u, "    cmpb    $90, %%al");
    yardimci_yaz(u, "    ja      .Lhk_copy");
    yardimci_yaz(u, "    addb    $32, %%al");
    yardimci_yaz(u, ".Lhk_copy:");
    yardimci_yaz(u, "    movb    %%al, (%%rdi)");
    yardimci_yaz(u, "    incq    %%rsi");
    yardimci_yaz(u, "    incq    %%rdi");
    yardimci_yaz(u, "    decq    %%rcx");
    yardimci_yaz(u, "    jmp     .Lhk_loop");
    yardimci_yaz(u, ".Lhk_done:");
    yardimci_yaz(u, "    movq    -24(%%rbp), %%rax");
    yardimci_yaz(u, "    movq    -16(%%rbp), %%rbx");
    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");
}

static void mtn_kes_uret(Üretici *u) {
    if (u->mtn_kes_uretildi) return;
    u->mtn_kes_uretildi = 1;
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# kes(m, başlangıç, uzunluk) - alt metin");
    yardimci_yaz(u, "_mtn_kes:");
    yardimci_yaz(u, "    movq    %%rcx, %%rbx");
    yardimci_yaz(u, "    addq    %%rdx, %%rdi");
    yardimci_yaz(u, "    movq    %%rdi, %%rax");
    yardimci_yaz(u, "    ret");
}

static void mtn_bul_uret(Üretici *u) {
    if (u->mtn_bul_uretildi) return;
    u->mtn_bul_uretildi = 1;
    yardimci_yaz(u, "");
    yardimci_yaz(u, "# bul(m, aranan) - alt metin ara");
    yardimci_yaz(u, "_mtn_bul:");
    yardimci_yaz(u, "    pushq   %%rbp");
    yardimci_yaz(u, "    movq    %%rsp, %%rbp");
    yardimci_yaz(u, "    pushq   %%r12");
    yardimci_yaz(u, "    pushq   %%r13");
    yardimci_yaz(u, "    pushq   %%r14");
    yardimci_yaz(u, "    pushq   %%r15");
    yardimci_yaz(u, "    movq    %%rdi, %%r12");   /* haystack_ptr */
    yardimci_yaz(u, "    movq    %%rsi, %%r13");   /* haystack_len */
    yardimci_yaz(u, "    movq    %%rdx, %%r14");   /* needle_ptr */
    yardimci_yaz(u, "    movq    %%rcx, %%r15");   /* needle_len */
    /* needle_len == 0 -> return 0 */
    yardimci_yaz(u, "    testq   %%r15, %%r15");
    yardimci_yaz(u, "    jz      .Lbul_zero");
    /* needle_len > haystack_len -> return -1 */
    yardimci_yaz(u, "    cmpq    %%r13, %%r15");
    yardimci_yaz(u, "    jg      .Lbul_notfound");
    /* max_i = haystack_len - needle_len + 1 */
    yardimci_yaz(u, "    movq    %%r13, %%rax");
    yardimci_yaz(u, "    subq    %%r15, %%rax");
    yardimci_yaz(u, "    incq    %%rax");
    yardimci_yaz(u, "    xorq    %%r8, %%r8");     /* i = 0 */
    yardimci_yaz(u, ".Lbul_outer:");
    yardimci_yaz(u, "    cmpq    %%rax, %%r8");
    yardimci_yaz(u, "    jge     .Lbul_notfound");
    /* memcmp(haystack+i, needle, needle_len) */
    yardimci_yaz(u, "    leaq    (%%r12, %%r8), %%rdi");
    yardimci_yaz(u, "    movq    %%r14, %%rsi");
    yardimci_yaz(u, "    movq    %%r15, %%rcx");
    yardimci_yaz(u, "    repe cmpsb");
    yardimci_yaz(u, "    je      .Lbul_found");
    yardimci_yaz(u, "    incq    %%r8");
    yardimci_yaz(u, "    jmp     .Lbul_outer");
    yardimci_yaz(u, ".Lbul_found:");
    yardimci_yaz(u, "    movq    %%r8, %%rax");
    yardimci_yaz(u, "    jmp     .Lbul_ret");
    yardimci_yaz(u, ".Lbul_zero:");
    yardimci_yaz(u, "    xorq    %%rax, %%rax");
    yardimci_yaz(u, "    jmp     .Lbul_ret");
    yardimci_yaz(u, ".Lbul_notfound:");
    yardimci_yaz(u, "    movq    $-1, %%rax");
    yardimci_yaz(u, ".Lbul_ret:");
    yardimci_yaz(u, "    popq    %%r15");
    yardimci_yaz(u, "    popq    %%r14");
    yardimci_yaz(u, "    popq    %%r13");
    yardimci_yaz(u, "    popq    %%r12");
    yardimci_yaz(u, "    leave");
    yardimci_yaz(u, "    ret");
}

/* Modüler kütüphane: metadata'dan otomatik kod üretimi */
static void stdlib_cagri_uret(Üretici *u, Düğüm *d, const ModülFonksiyon *fn) {
    int arg_sayisi = fn->param_sayisi;
    int verilen_arg = d->çocuk_sayısı;

    const char *int_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

    /* 1. Tüm argümanları ters sırada değerlendir ve stack'e push et */
    for (int i = arg_sayisi - 1; i >= 0; i--) {
        if (i < verilen_arg)
            ifade_üret(u, d->çocuklar[i]);
        else
            yaz(u, "    xorq    %%rax, %%rax");

        /* Tam→ondalık otomatik dönüşüm */
        if (fn->param_tipleri[i] == TİP_ONDALIK &&
            i < verilen_arg && d->çocuklar[i]->sonuç_tipi == TİP_TAM) {
            yaz(u, "    cvtsi2sd %%rax, %%xmm0");
        }

        switch (fn->param_tipleri[i]) {
        case TİP_ONDALIK:
            yaz(u, "    subq    $8, %%rsp");
            yaz(u, "    movsd   %%xmm0, (%%rsp)");
            break;
        case TİP_DİZİ:
        case TİP_METİN:
            yaz(u, "    pushq   %%rbx");   /* count/len */
            yaz(u, "    pushq   %%rax");   /* ptr */
            break;
        default: /* TİP_TAM, TİP_MANTIK */
            yaz(u, "    pushq   %%rax");
            break;
        }
    }

    /* 2. Stack'ten System V ABI registerlarına pop et */
    int int_idx = 0, xmm_idx = 0;
    for (int i = 0; i < arg_sayisi; i++) {
        switch (fn->param_tipleri[i]) {
        case TİP_ONDALIK:
            if (xmm_idx < 8) {
                yaz(u, "    movsd   (%%rsp), %%xmm%d", xmm_idx++);
                yaz(u, "    addq    $8, %%rsp");
            }
            break;
        case TİP_DİZİ:
        case TİP_METİN:
            if (int_idx + 1 < 6) {
                yaz(u, "    popq    %%%s", int_regs[int_idx++]); /* ptr */
                yaz(u, "    popq    %%%s", int_regs[int_idx++]); /* count/len */
            }
            break;
        default:
            if (int_idx < 6) {
                yaz(u, "    popq    %%%s", int_regs[int_idx++]);
            }
            break;
        }
    }

    /* 3. Fonksiyonu çağır */
    yaz(u, "    xorq    %%rax, %%rax");
    yaz(u, "    call    %s", fn->runtime_isim);

    /* 4. Return fixup: C TrDizi/TrMetin → {rax, rdx} → {rax, rbx} */
    if (fn->dönüş_tipi == TİP_DİZİ || fn->dönüş_tipi == TİP_METİN) {
        yaz(u, "    movq    %%rdx, %%rbx");
    }
}

static void cagri_uret(Üretici *u, Düğüm *d) {
    if (!d->veri.tanimlayici.isim) return;

    /* yazdır özel durumu */
    if (strcmp(d->veri.tanimlayici.isim, "yazdır") == 0) {
        if (d->çocuk_sayısı > 0) {
            Düğüm *arg = d->çocuklar[0];
            if (arg->tur == DÜĞÜM_METİN_DEĞERİ) {
                /* Metin literal yazdır */
                yazdir_metin_uret_fn(u);
                int idx = metin_literal_ekle(u, arg->veri.metin_değer,
                                              (int)strlen(arg->veri.metin_değer));
                yaz(u, "    leaq    .LC%d(%%rip), %%rdi", idx);
                yaz(u, "    movq    $.LC%d_len, %%rsi", idx);
                yaz(u, "    call    _yazdir_metin");
            } else if (arg->sonuç_tipi == TİP_METİN) {
                /* Metin değişkeni yazdır */
                yazdir_metin_uret_fn(u);
                ifade_üret(u, arg);
                yaz(u, "    movq    %%rax, %%rdi");  /* pointer */
                yaz(u, "    movq    %%rbx, %%rsi");  /* length */
                yaz(u, "    call    _yazdir_metin");
            } else if (arg->sonuç_tipi == TİP_ONDALIK) {
                /* Ondalık yazdır */
                yazdir_ondalik_uret_fn(u);
                ifade_üret(u, arg);
                /* xmm0 zaten double değeri içeriyor */
                yaz(u, "    call    _yazdir_ondalik");
            } else {
                /* Tam sayı / mantık yazdır */
                yazdir_tam_sayi_uret(u);
                ifade_üret(u, arg);
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    call    _yazdir_tam");
            }
        }
        return;
    }

    /* doğrula() — test assertion */
    if (strcmp(d->veri.tanimlayici.isim, "do\xc4\x9frula") == 0) {
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
            /* rax = condition (0/1) */
            /* call _tr_dogrula(condition, test_name_ptr, test_name_len, line) */
            yaz(u, "    movq    %%rax, %%rdi");     /* condition */
            /* test name and line will be set by surrounding test block via globals */
            yaz(u, "    leaq    _test_isim_ptr(%%rip), %%rsi");
            yaz(u, "    movq    (%%rsi), %%rsi");   /* test_name_ptr */
            yaz(u, "    leaq    _test_isim_len(%%rip), %%rdx");
            yaz(u, "    movq    (%%rdx), %%rdx");   /* test_name_len */
            yaz(u, "    movq    $%d, %%rcx", d->satir);  /* line */
            yaz(u, "    call    _tr_dogrula");
        }
        return;
    }

    /* doğrula_eşit(beklenen, gerçek) — beklenen/gerçek değer gösteren assertion */
    if (strcmp(d->veri.tanimlayici.isim, "do\xc4\x9frula_e\xc5\x9fit") == 0 ||
        strcmp(d->veri.tanimlayici.isim, "dogrula_esit") == 0) {
        if (d->çocuk_sayısı >= 2) {
            /* beklenen değeri üret -> stack'e kaydet */
            ifade_üret(u, d->çocuklar[0]);
            yaz(u, "    pushq   %%rax");
            /* gerçek değeri üret */
            ifade_üret(u, d->çocuklar[1]);
            yaz(u, "    movq    %%rax, %%rsi");     /* gercek (arg2) */
            yaz(u, "    popq    %%rdi");             /* beklenen (arg1) */
            /* isim ptr/len */
            yaz(u, "    leaq    _test_isim_ptr(%%rip), %%rdx");
            yaz(u, "    movq    (%%rdx), %%rdx");   /* isim_ptr (arg3) */
            yaz(u, "    leaq    _test_isim_len(%%rip), %%rcx");
            yaz(u, "    movq    (%%rcx), %%rcx");   /* isim_len (arg4) */
            yaz(u, "    movq    $%d, %%r8", d->satir);  /* satir (arg5) */
            yaz(u, "    call    _tr_dogrula_esit_tam");
        }
        return;
    }

    /* doğrula_farklı(a, b) — a != b assertion */
    if (strcmp(d->veri.tanimlayici.isim, "do\xc4\x9frula_farkl\xc4\xb1") == 0 ||
        strcmp(d->veri.tanimlayici.isim, "dogrula_farkli") == 0) {
        if (d->çocuk_sayısı >= 2) {
            /* a değerini üret -> stack'e kaydet */
            ifade_üret(u, d->çocuklar[0]);
            yaz(u, "    pushq   %%rax");
            /* b değerini üret */
            ifade_üret(u, d->çocuklar[1]);
            yaz(u, "    popq    %%rcx");             /* a */
            /* a != b -> condition = 1, a == b -> condition = 0 */
            yaz(u, "    cmpq    %%rax, %%rcx");
            yaz(u, "    setne   %%al");
            yaz(u, "    movzbq  %%al, %%rdi");       /* condition (arg1) */
            /* isim ptr/len */
            yaz(u, "    leaq    _test_isim_ptr(%%rip), %%rsi");
            yaz(u, "    movq    (%%rsi), %%rsi");    /* isim_ptr (arg2) */
            yaz(u, "    leaq    _test_isim_len(%%rip), %%rdx");
            yaz(u, "    movq    (%%rdx), %%rdx");    /* isim_len (arg3) */
            yaz(u, "    movq    $%d, %%rcx", d->satir);  /* satir (arg4) */
            yaz(u, "    call    _tr_dogrula");
        }
        return;
    }

    /* uzunluk() yerleşik fonksiyonu */
    if (d->veri.tanimlayici.isim &&
        strcmp(d->veri.tanimlayici.isim, "uzunluk") == 0) {
        if (d->çocuk_sayısı > 0) {
            /* Sınıf nesnesi ise uzunluk metodu çağır */
            if (d->çocuklar[0]->sonuç_tipi == TİP_SINIF) {
                char *sa = sinif_adi_bul(u, d->çocuklar[0]);
                if (sa) {
                    ifade_üret(u, d->çocuklar[0]);
                    yaz(u, "    movq    %%rax, %%rdi");
                    yaz(u, "    call    %s_uzunluk", sa);
                    return;
                }
            }
            ifade_üret(u, d->çocuklar[0]);
            /* Metin ise UTF-8 karakter sayısını döndür */
            if (d->çocuklar[0]->sonuç_tipi == TİP_METİN) {
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    movq    %%rbx, %%rsi");
                yaz(u, "    call    _tr_utf8_karakter_say");
            } else {
                /* Dizi için eleman sayısı */
                yaz(u, "    movq    %%rbx, %%rax");
            }
        }
        return;
    }

    /* === matematik modülü fonksiyonları === */

    if (strcmp(d->veri.tanimlayici.isim, "mutlak") == 0) {
        mat_mutlak_uret(u);
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    movq    %%rax, %%rdi");
        yaz(u, "    call    _mat_mutlak");
        return;
    }

    if (strcmp(d->veri.tanimlayici.isim, "kuvvet") == 0) {
        mat_kuvvet_uret(u);
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rax");
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    movq    %%rax, %%rsi");
        yaz(u, "    popq    %%rdi");
        yaz(u, "    call    _mat_kuvvet");
        return;
    }

    if (strcmp(d->veri.tanimlayici.isim, "karek\xc3\xb6" "k") == 0) {
        mat_karekok_uret(u);
        ifade_üret(u, d->çocuklar[0]);
        if (d->çocuklar[0]->sonuç_tipi == TİP_TAM) {
            yaz(u, "    cvtsi2sd %%rax, %%xmm0");
        }
        yaz(u, "    call    _mat_karekok");
        return;
    }

    if (strcmp(d->veri.tanimlayici.isim, "min") == 0) {
        mat_min_uret(u);
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rax");
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    movq    %%rax, %%rsi");
        yaz(u, "    popq    %%rdi");
        yaz(u, "    call    _mat_min");
        return;
    }

    if (strcmp(d->veri.tanimlayici.isim, "maks") == 0) {
        mat_maks_uret(u);
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rax");
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    movq    %%rax, %%rsi");
        yaz(u, "    popq    %%rdi");
        yaz(u, "    call    _mat_maks");
        return;
    }

    if (strcmp(d->veri.tanimlayici.isim, "mod") == 0) {
        mat_mod_uret(u);
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rax");
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    movq    %%rax, %%rsi");
        yaz(u, "    popq    %%rdi");
        yaz(u, "    call    _mat_mod");
        return;
    }

    /* === metin modülü fonksiyonları (inline asm) === */

    if (strcmp(d->veri.tanimlayici.isim, "harf_buyut") == 0) {
        mtn_harf_buyut_uret(u);
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    movq    %%rax, %%rdi");
        yaz(u, "    movq    %%rbx, %%rsi");
        yaz(u, "    call    _mtn_harf_buyut");
        return;
    }

    if (strcmp(d->veri.tanimlayici.isim, "harf_kucult") == 0) {
        mtn_harf_kucult_uret(u);
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    movq    %%rax, %%rdi");
        yaz(u, "    movq    %%rbx, %%rsi");
        yaz(u, "    call    _mtn_harf_kucult");
        return;
    }

    if (strcmp(d->veri.tanimlayici.isim, "kes") == 0) {
        /* 3 arg: metin, karakter_baslangic, karakter_uzunluk */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");  /* byte len */
        yaz(u, "    pushq   %%rax");  /* ptr */
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    pushq   %%rax");  /* karakter_baslangic */
        ifade_üret(u, d->çocuklar[2]);
        yaz(u, "    movq    %%rax, %%rcx");   /* karakter_uzunluk */
        yaz(u, "    popq    %%rdx");           /* karakter_baslangic */
        yaz(u, "    popq    %%rdi");           /* ptr */
        yaz(u, "    popq    %%rsi");           /* byte len */
        yaz(u, "    call    _tr_utf8_kes");
        /* TrMetin dönüş: rax=ptr, rdx=len */
        yaz(u, "    movq    %%rdx, %%rbx");
        return;
    }

    if (strcmp(d->veri.tanimlayici.isim, "bul") == 0) {
        /* 2 arg: haystack (metin), needle (metin) */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");  /* haystack len */
        yaz(u, "    pushq   %%rax");  /* haystack ptr */
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    movq    %%rax, %%rdx");   /* needle ptr */
        yaz(u, "    movq    %%rbx, %%rcx");   /* needle len */
        yaz(u, "    popq    %%rdi");           /* haystack ptr */
        yaz(u, "    popq    %%rsi");           /* haystack len */
        yaz(u, "    call    _tr_utf8_bul");
        return;
    }

    if (strcmp(d->veri.tanimlayici.isim, "i\xc3\xa7" "erir") == 0) {
        mtn_bul_uret(u);
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");
        yaz(u, "    pushq   %%rax");
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    movq    %%rax, %%rdx");
        yaz(u, "    movq    %%rbx, %%rcx");
        yaz(u, "    popq    %%rdi");
        yaz(u, "    popq    %%rsi");
        yaz(u, "    call    _mtn_bul");
        /* Convert: rax >= 0 ? 1 : 0 */
        yaz(u, "    testq   %%rax, %%rax");
        yaz(u, "    setns   %%al");
        yaz(u, "    movzbq  %%al, %%rax");
        return;
    }

    /* kırp, tersle, tekrarla, başlar_mi, biter_mi, değiştir:
     * stdlib_cagri_uret() ile otomatik işlenir */

    /* Runtime matematik, matris, istatistik, sayısal analiz, dizi, metin
     * fonksiyonları: stdlib_cagri_uret() ile otomatik işlenir */

    /* === Gelişmiş Metin İşlemleri (özel durumlar) === */

    /* metin_uzunluk(m: metin) -> tam (karakter sayısı) */
    if (strcmp(d->veri.tanimlayici.isim, "metin_uzunluk") == 0) {
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    movq    %%rax, %%rdi");
        yaz(u, "    movq    %%rbx, %%rsi");
        yaz(u, "    call    _tr_utf8_karakter_say");
        return;
    }

    /* biçimle(deger, spec) -> metin */
    if ((strcmp(d->veri.tanimlayici.isim, "bi\xc3\xa7imle") == 0 ||
         strcmp(d->veri.tanimlayici.isim, "bicimle") == 0) && d->çocuk_sayısı > 1) {
        /* İlk argümanın tipine göre farklı runtime fonksiyonu çağır */
        int arg_tip = d->çocuklar[0]->sonuç_tipi;
        /* Spec (ikinci argüman) → stack'e kaydet */
        ifade_üret(u, d->çocuklar[1]);  /* spec string: rax=ptr, rbx=len */
        yaz(u, "    pushq   %%rbx");    /* spec len */
        yaz(u, "    pushq   %%rax");    /* spec ptr */
        /* Değer (ilk argüman) */
        ifade_üret(u, d->çocuklar[0]);
        if (arg_tip == TİP_ONDALIK) {
            /* _tr_bicimle_ondalik(double deger, spec_ptr, spec_len) */
            /* deger xmm0'da, spec rsi, rdx */
            yaz(u, "    movsd   %%xmm0, %%xmm0");  /* zaten xmm0'da */
            yaz(u, "    popq    %%rdi");  /* spec ptr → rdi */
            yaz(u, "    popq    %%rsi");  /* spec len → rsi */
            yaz(u, "    call    _tr_bicimle_ondalik");
        } else if (arg_tip == TİP_METİN) {
            /* _tr_bicimle_metin(ptr, len, spec_ptr, spec_len) */
            yaz(u, "    movq    %%rax, %%rdi");  /* metin ptr */
            yaz(u, "    movq    %%rbx, %%rsi");  /* metin len */
            yaz(u, "    popq    %%rdx");          /* spec ptr */
            yaz(u, "    popq    %%rcx");          /* spec len */
            yaz(u, "    call    _tr_bicimle_metin");
        } else {
            /* _tr_bicimle_tam(long long deger, spec_ptr, spec_len) */
            yaz(u, "    movq    %%rax, %%rdi");  /* deger */
            yaz(u, "    popq    %%rsi");          /* spec ptr */
            yaz(u, "    popq    %%rdx");          /* spec len */
            yaz(u, "    call    _tr_bicimle_tam");
        }
        yaz(u, "    movq    %%rdx, %%rbx");  /* TrMetin.len → rbx */
        return;
    }

    /* büyük_harf, küçük_harf, böl, birleştir_metin, dosya_satirlar,
     * dosya_ekle, sözlük_*: stdlib_cagri_uret() ile otomatik işlenir */

    /* === Birinci Sınıf Fonksiyon Desteği (eşle/filtre/indirge) === */

    /* eşlem(d: dizi, fonk_adi) -> dizi  (map) */
    if (strcmp(d->veri.tanimlayici.isim, "e\xc5\x9flem") == 0) {
        /* arg0: dizi (rax=ptr, rbx=count) */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");  /* count */
        yaz(u, "    pushq   %%rax");  /* ptr */
        /* arg1: fonksiyon adresi */
        if (d->çocuklar[1]->tur == DÜĞÜM_TANIMLAYICI) {
            yaz(u, "    leaq    %s(%%rip), %%rdx", d->çocuklar[1]->veri.tanimlayici.isim);
        } else {
            ifade_üret(u, d->çocuklar[1]);
            yaz(u, "    movq    %%rax, %%rdx");
        }
        yaz(u, "    popq    %%rdi");   /* ptr */
        yaz(u, "    popq    %%rsi");   /* count */
        yaz(u, "    call    _tr_esle");
        yaz(u, "    movq    %%rdx, %%rbx");  /* TrDizi.count */
        return;
    }

    /* filtre(d: dizi, fonk_adi) -> dizi */
    if (strcmp(d->veri.tanimlayici.isim, "filtre") == 0) {
        /* arg0: dizi (rax=ptr, rbx=count) */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");  /* count */
        yaz(u, "    pushq   %%rax");  /* ptr */
        /* arg1: fonksiyon adresi */
        if (d->çocuklar[1]->tur == DÜĞÜM_TANIMLAYICI) {
            yaz(u, "    leaq    %s(%%rip), %%rdx", d->çocuklar[1]->veri.tanimlayici.isim);
        } else {
            ifade_üret(u, d->çocuklar[1]);
            yaz(u, "    movq    %%rax, %%rdx");
        }
        yaz(u, "    popq    %%rdi");   /* ptr */
        yaz(u, "    popq    %%rsi");   /* count */
        yaz(u, "    call    _tr_filtre");
        yaz(u, "    movq    %%rdx, %%rbx");  /* TrDizi.count */
        return;
    }

    /* indirge(d: dizi, başlangıç: tam, fonk_adi) -> tam */
    if (strcmp(d->veri.tanimlayici.isim, "indirge") == 0) {
        /* arg0: dizi (rax=ptr, rbx=count) */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");  /* count */
        yaz(u, "    pushq   %%rax");  /* ptr */
        /* arg1: başlangıç deger */
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    pushq   %%rax");  /* başlangıç */
        /* arg2: fonksiyon adresi */
        if (d->çocuklar[2]->tur == DÜĞÜM_TANIMLAYICI) {
            yaz(u, "    leaq    %s(%%rip), %%rcx", d->çocuklar[2]->veri.tanimlayici.isim);
        } else {
            ifade_üret(u, d->çocuklar[2]);
            yaz(u, "    movq    %%rax, %%rcx");
        }
        yaz(u, "    popq    %%rdx");   /* başlangıç */
        yaz(u, "    popq    %%rdi");   /* ptr */
        yaz(u, "    popq    %%rsi");   /* count */
        yaz(u, "    call    _tr_indirge");
        return;
    }

    /* küme, numarala/eşleştir/ters/dilimle, tekrarlayıcı, zaman modülleri:
     * stdlib_cagri_uret() ile otomatik işlenir */

    /* json, ağ, düzeni modülleri: stdlib_cagri_uret() ile otomatik işlenir */

    /* === Paralel modülü: iş_oluştur özel durum === */

    /* iş_oluştur(fonk_ptr: tam) -> tam */
    if (strcmp(d->veri.tanimlayici.isim, "i\xc5\x9f_olu\xc5\x9ftur") == 0) {
        if (d->çocuklar[0]->tur == DÜĞÜM_TANIMLAYICI) {
            yaz(u, "    leaq    %s(%%rip), %%rdi", d->çocuklar[0]->veri.tanimlayici.isim);
        } else {
            ifade_üret(u, d->çocuklar[0]);
            yaz(u, "    movq    %%rax, %%rdi");
        }
        yaz(u, "    call    _tr_is_olustur");
        return;
    }

    /* iş_bekle, kilit_oluştur, kilitle, kilit_bırak,
     * veritabanı, kripto, ortam, argüman, soket modülleri:
     * stdlib_cagri_uret() ile otomatik işlenir */

    /* Sınıf yapıcı çağrısı: SinifAdi(arg1, arg2, ...) */
    SinifBilgi *sb = sınıf_bul(u->kapsam, d->veri.tanimlayici.isim);
    if (sb) {
        int boyut = sb->boyut > 0 ? sb->boyut : 8;
        /* _tr_nesne_olustur ile nesne ayır */
        yaz(u, "    movq    $%d, %%rdi", 3);        /* NESNE_TIP_SINIF */
        yaz(u, "    movq    $%d, %%rsi", boyut);    /* boyut */
        yaz(u, "    call    _tr_nesne_olustur");
        hata_bellek_uret(u);
        {
            int lbl = yeni_etiket(u);
            yaz(u, "    testq   %%rax, %%rax");
            yaz(u, "    jnz     .L%d", lbl);
            yaz(u, "    call    _hata_bellek");
            yaz(u, ".L%d:", lbl);
        }
        yaz(u, "    pushq   %%rax");  /* nesne pointer'ını kaydet */

        /* Alanları yapıcı argümanlarıyla doldur */
        int arg_sayisi = d->çocuk_sayısı;
        for (int i = 0; i < arg_sayisi && i < sb->alan_sayisi; i++) {
            yaz(u, "    pushq   (%%rsp)");  /* nesne pointer'ını koru */
            ifade_üret(u, d->çocuklar[i]);
            int offset = sb->alanlar[i].offset;
            TipTürü alan_tipi = sb->alanlar[i].tip;
            if (alan_tipi == TİP_METİN || alan_tipi == TİP_DİZİ) {
                /* metin/dizi: hem ptr hem len sakla */
                yaz(u, "    movq    %%rax, %%rcx");  /* ptr */
                yaz(u, "    movq    %%rbx, %%r8");   /* len */
                yaz(u, "    popq    %%rax");          /* nesne pointer */
                yaz(u, "    movq    %%rcx, %d(%%rax)", offset);      /* ptr */
                yaz(u, "    movq    %%r8, %d(%%rax)", offset + 8);   /* len */
            } else {
                yaz(u, "    movq    %%rax, %%rcx");  /* değer */
                yaz(u, "    popq    %%rax");          /* nesne pointer */
                yaz(u, "    movq    %%rcx, %d(%%rax)", offset);
            }
        }

        /* Sonuç: rax = nesne pointer */
        yaz(u, "    popq    %%rax");
        return;
    }

    /* Metot çağrısı: nesne.metot(args...) */
    if (d->veri.tanimlayici.tip &&
        strcmp(d->veri.tanimlayici.tip, "metot") == 0 &&
        d->çocuk_sayısı > 0) {
        /* İlk argüman nesne referansı (bu) */
        Düğüm *nesne = d->çocuklar[0];
        char *metot_adi = d->veri.tanimlayici.isim;

        /* === Metin metotları: m.metot(args) === */
        if (nesne->sonuç_tipi == TİP_METİN) {
            /* Receiver'ı değerlendir → rax=ptr, rbx=len */
            ifade_üret(u, nesne);

            /* uzunluk() — UTF-8 karakter sayısı */
            if (strcmp(metot_adi, "uzunluk") == 0) {
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    movq    %%rbx, %%rsi");
                yaz(u, "    call    _tr_utf8_karakter_say");
                return;
            }
            /* byte_uzunluk() — byte cinsinden uzunluk */
            if (strcmp(metot_adi, "byte_uzunluk") == 0) {
                yaz(u, "    movq    %%rbx, %%rax");
                return;
            }
            /* bosmu() */
            if (strcmp(metot_adi, "bosmu") == 0 ||
                strcmp(metot_adi, "bo\xc5\x9fmu") == 0) {
                yaz(u, "    testq   %%rbx, %%rbx");
                yaz(u, "    sete    %%al");
                yaz(u, "    movzbq  %%al, %%rax");
                return;
            }
            /* kirp() */
            if (strcmp(metot_adi, "kirp") == 0 ||
                strcmp(metot_adi, "k\xc4\xb1rp") == 0) {
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    movq    %%rbx, %%rsi");
                yaz(u, "    call    _tr_kirp");
                /* TrMetin dönüşü: rax=ptr, rdx=len */
                yaz(u, "    movq    %%rdx, %%rbx");
                return;
            }
            /* tersle() */
            if (strcmp(metot_adi, "tersle") == 0) {
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    movq    %%rbx, %%rsi");
                yaz(u, "    call    _tr_tersle");
                yaz(u, "    movq    %%rdx, %%rbx");
                return;
            }
            /* say(alt_metin) */
            if (strcmp(metot_adi, "say") == 0 && d->çocuk_sayısı > 1) {
                yaz(u, "    pushq   %%rax");
                yaz(u, "    pushq   %%rbx");
                ifade_üret(u, d->çocuklar[1]);
                yaz(u, "    movq    %%rax, %%rdx");  /* alt_ptr */
                yaz(u, "    movq    %%rbx, %%rcx");  /* alt_len */
                yaz(u, "    popq    %%rsi");          /* len */
                yaz(u, "    popq    %%rdi");          /* ptr */
                yaz(u, "    call    _tr_metin_say");
                return;
            }
            /* baslar(onek) / başlar(önek) */
            if ((strcmp(metot_adi, "baslar") == 0 ||
                 strcmp(metot_adi, "ba\xc5\x9flar") == 0) && d->çocuk_sayısı > 1) {
                yaz(u, "    pushq   %%rax");
                yaz(u, "    pushq   %%rbx");
                ifade_üret(u, d->çocuklar[1]);
                yaz(u, "    movq    %%rax, %%rdx");
                yaz(u, "    movq    %%rbx, %%rcx");
                yaz(u, "    popq    %%rsi");
                yaz(u, "    popq    %%rdi");
                yaz(u, "    call    _tr_baslar_mi");
                return;
            }
            /* biter(sonek) */
            if (strcmp(metot_adi, "biter") == 0 && d->çocuk_sayısı > 1) {
                yaz(u, "    pushq   %%rax");
                yaz(u, "    pushq   %%rbx");
                ifade_üret(u, d->çocuklar[1]);
                yaz(u, "    movq    %%rax, %%rdx");
                yaz(u, "    movq    %%rbx, %%rcx");
                yaz(u, "    popq    %%rsi");
                yaz(u, "    popq    %%rdi");
                yaz(u, "    call    _tr_biter_mi");
                return;
            }
            /* icerir(alt_metin) / içerir */
            if ((strcmp(metot_adi, "icerir") == 0 ||
                 strcmp(metot_adi, "i\xc3\xa7erir") == 0) && d->çocuk_sayısı > 1) {
                yaz(u, "    pushq   %%rax");
                yaz(u, "    pushq   %%rbx");
                ifade_üret(u, d->çocuklar[1]);
                yaz(u, "    movq    %%rax, %%rdx");
                yaz(u, "    movq    %%rbx, %%rcx");
                yaz(u, "    popq    %%rsi");
                yaz(u, "    popq    %%rdi");
                yaz(u, "    call    _tr_metin_icerir");
                return;
            }
            /* degistir(eski, yeni) / değiştir */
            if ((strcmp(metot_adi, "degistir") == 0 ||
                 strcmp(metot_adi, "de\xc4\x9fi\xc5\x9ftir") == 0) && d->çocuk_sayısı > 2) {
                yaz(u, "    pushq   %%rax");
                yaz(u, "    pushq   %%rbx");
                /* İkinci argüman (yeni) */
                ifade_üret(u, d->çocuklar[2]);
                yaz(u, "    pushq   %%rax");  /* yeni_ptr */
                yaz(u, "    pushq   %%rbx");  /* yeni_len */
                /* Birinci argüman (eski) */
                ifade_üret(u, d->çocuklar[1]);
                yaz(u, "    movq    %%rax, %%rdx");  /* eski_ptr */
                yaz(u, "    movq    %%rbx, %%rcx");  /* eski_len */
                yaz(u, "    popq    %%r9");           /* yeni_len */
                yaz(u, "    popq    %%r8");           /* yeni_ptr */
                yaz(u, "    popq    %%rsi");          /* len */
                yaz(u, "    popq    %%rdi");          /* ptr */
                yaz(u, "    call    _tr_degistir");
                yaz(u, "    movq    %%rdx, %%rbx");
                return;
            }
            /* rakammi() / rakammı() */
            if (strcmp(metot_adi, "rakammi") == 0 ||
                strcmp(metot_adi, "rakamm\xc4\xb1") == 0) {
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    movq    %%rbx, %%rsi");
                yaz(u, "    call    _tr_metin_rakammi");
                return;
            }
            /* harfmi() */
            if (strcmp(metot_adi, "harfmi") == 0) {
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    movq    %%rbx, %%rsi");
                yaz(u, "    call    _tr_metin_harfmi");
                return;
            }
            /* boslukmu() / boşlukmu() */
            if (strcmp(metot_adi, "boslukmu") == 0 ||
                strcmp(metot_adi, "bo\xc5\x9flukmu") == 0) {
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    movq    %%rbx, %%rsi");
                yaz(u, "    call    _tr_metin_boslukmu");
                return;
            }
            /* buyuk_harf() / büyük_harf() */
            if (strcmp(metot_adi, "buyuk_harf") == 0 ||
                strcmp(metot_adi, "b\xc3\xbcy\xc3\xbck_harf") == 0) {
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    movq    %%rbx, %%rsi");
                yaz(u, "    call    _tr_buyuk_harf");
                yaz(u, "    movq    %%rdx, %%rbx");
                return;
            }
            /* kucuk_harf() / küçük_harf() */
            if (strcmp(metot_adi, "kucuk_harf") == 0 ||
                strcmp(metot_adi, "k\xc3\xbc\xc3\xa7\xc3\xbck_harf") == 0) {
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    movq    %%rbx, %%rsi");
                yaz(u, "    call    _tr_kucuk_harf");
                yaz(u, "    movq    %%rdx, %%rbx");
                return;
            }
            /* bol(ayirici) / böl(ayırıcı) */
            if ((strcmp(metot_adi, "bol") == 0 ||
                 strcmp(metot_adi, "b\xc3\xb6l") == 0) && d->çocuk_sayısı > 1) {
                yaz(u, "    pushq   %%rax");
                yaz(u, "    pushq   %%rbx");
                ifade_üret(u, d->çocuklar[1]);
                yaz(u, "    movq    %%rax, %%rdx");
                yaz(u, "    movq    %%rbx, %%rcx");
                yaz(u, "    popq    %%rsi");
                yaz(u, "    popq    %%rdi");
                yaz(u, "    call    _tr_bol");
                /* TrDizi dönüşü: rax=ptr, rdx=count → rbx=count */
                yaz(u, "    movq    %%rdx, %%rbx");
                return;
            }
            /* tekrarla(kez) */
            if (strcmp(metot_adi, "tekrarla") == 0 && d->çocuk_sayısı > 1) {
                yaz(u, "    pushq   %%rax");
                yaz(u, "    pushq   %%rbx");
                ifade_üret(u, d->çocuklar[1]);
                yaz(u, "    movq    %%rax, %%rdx");  /* kez */
                yaz(u, "    popq    %%rsi");          /* len */
                yaz(u, "    popq    %%rdi");          /* ptr */
                yaz(u, "    call    _tr_tekrarla");
                yaz(u, "    movq    %%rdx, %%rbx");
                return;
            }
        }

        /* Sınıf adını bul */
        char *sınıf_adı = NULL;
        if (nesne->tur == DÜĞÜM_TANIMLAYICI) {
            Sembol *ns = sembol_ara(u->kapsam, nesne->veri.tanimlayici.isim);
            if (ns && ns->sınıf_adı) sınıf_adı = ns->sınıf_adı;
        }

        if (sınıf_adı) {
            /* Argüman tiplerini belirle */
            TipTürü arg_tipleri[32] = {0};
            for (int i = 0; i < d->çocuk_sayısı && i < 32; i++) {
                arg_tipleri[i] = d->çocuklar[i]->sonuç_tipi;
            }

            /* Tüm argümanları stack'e at (ters sırada) */
            for (int i = d->çocuk_sayısı - 1; i >= 0; i--) {
                ifade_üret(u, d->çocuklar[i]);
                if (arg_tipleri[i] == TİP_METİN) {
                    yaz(u, "    pushq   %%rbx");  /* length */
                    yaz(u, "    pushq   %%rax");  /* pointer */
                } else {
                    yaz(u, "    pushq   %%rax");
                }
            }
            /* Stack'ten registerlara taşı */
            const char *int_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            int reg_idx = 0;
            for (int i = 0; i < d->çocuk_sayısı && reg_idx < 6; i++) {
                if (arg_tipleri[i] == TİP_METİN && reg_idx + 1 < 6) {
                    yaz(u, "    popq    %%%s", int_regs[reg_idx]);  /* pointer */
                    reg_idx++;
                    yaz(u, "    popq    %%%s", int_regs[reg_idx]);  /* length */
                    reg_idx++;
                } else {
                    yaz(u, "    popq    %%%s", int_regs[reg_idx]);
                    reg_idx++;
                }
            }
            /* Mangled metot ismini çağır: SinifAdi_metotAdi */
            yaz(u, "    xorq    %%rax, %%rax");
            yaz(u, "    call    %s_%s", sınıf_adı, metot_adi);
            return;
        }
    }

    /* Modüler kütüphane dispatch: kullanıcı fonksiyonu yoksa modül fonksiyonlarını dene */
    {
        Sembol *sym = sembol_ara(u->kapsam, d->veri.tanimlayici.isim);
        if (!sym) {
            const ModülFonksiyon *mf = modul_fonksiyon_bul(d->veri.tanimlayici.isim);
            if (mf) {
                stdlib_cagri_uret(u, d, mf);
                return;
            }
        }
    }

    /* Genel fonksiyon çağrısı - System V ABI */
    Sembol *fn = sembol_ara(u->kapsam, d->veri.tanimlayici.isim);
    int verilen_arg = d->çocuk_sayısı;
    /* Varsayılan parametrelerle toplam argüman sayısını hesapla */
    int arg_sayisi = verilen_arg;
    if (fn && verilen_arg < fn->param_sayisi && fn->varsayilan_sayisi > 0) {
        arg_sayisi = fn->param_sayisi;
    }

    const char *int_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

    /* Her argümanın tipini belirle */
    TipTürü arg_tipleri[32] = {0};
    for (int i = 0; i < arg_sayisi && i < 32; i++) {
        if (fn && i < fn->param_sayisi) {
            arg_tipleri[i] = fn->param_tipleri[i];
        } else if (i < verilen_arg && d->çocuklar[i]->sonuç_tipi != TİP_BİLİNMİYOR) {
            arg_tipleri[i] = d->çocuklar[i]->sonuç_tipi;
        } else {
            arg_tipleri[i] = TİP_TAM;
        }
    }

    /* Tüm argümanları hesapla ve stack'e at (ters sırada) */
    for (int i = arg_sayisi - 1; i >= 0; i--) {
        if (i < verilen_arg) {
            ifade_üret(u, d->çocuklar[i]);
        } else if (fn && fn->varsayilan_dugumler[i]) {
            /* Varsayılan değeri üret */
            ifade_üret(u, (Düğüm *)fn->varsayilan_dugumler[i]);
        } else {
            yaz(u, "    xorq    %%rax, %%rax");  /* fallback: 0 */
        }
        if (arg_tipleri[i] == TİP_ONDALIK) {
            yaz(u, "    subq    $8, %%rsp");
            yaz(u, "    movsd   %%xmm0, (%%rsp)");
        } else if (arg_tipleri[i] == TİP_METİN) {
            yaz(u, "    pushq   %%rbx");  /* length */
            yaz(u, "    pushq   %%rax");  /* pointer */
        } else {
            yaz(u, "    pushq   %%rax");
        }
    }

    /* Stack'ten doğru registerlara taşı */
    int int_reg_idx = 0;
    int xmm_reg_idx = 0;
    for (int i = 0; i < arg_sayisi; i++) {
        if (arg_tipleri[i] == TİP_ONDALIK) {
            if (xmm_reg_idx < 8) {
                yaz(u, "    movsd   (%%rsp), %%xmm%d", xmm_reg_idx);
                yaz(u, "    addq    $8, %%rsp");
                xmm_reg_idx++;
            }
        } else if (arg_tipleri[i] == TİP_METİN) {
            if (int_reg_idx + 1 < 6) {
                yaz(u, "    popq    %%%s", int_regs[int_reg_idx]);
                int_reg_idx++;
                yaz(u, "    popq    %%%s", int_regs[int_reg_idx]);
                int_reg_idx++;
            }
        } else {
            if (int_reg_idx < 6) {
                yaz(u, "    popq    %%%s", int_regs[int_reg_idx]);
                int_reg_idx++;
            }
        }
    }

    yaz(u, "    xorq    %%rax, %%rax");  /* varargs için AL = 0 */
    /* Modül fonksiyonları için runtime ismini kullan */
    if (fn && fn->runtime_isim) {
        yaz(u, "    call    %s", fn->runtime_isim);
        /* C ABI fixup: TrMetin/TrDizi {rax,rdx} → {rax,rbx} dönüşümü */
        if (fn->dönüş_tipi == TİP_METİN || fn->dönüş_tipi == TİP_DİZİ) {
            yaz(u, "    movq    %%rdx, %%rbx");
        }
    } else {
        yaz(u, "    call    %s", d->veri.tanimlayici.isim);
    }
}

static void ifade_üret(Üretici *u, Düğüm *d) {
    if (!d) return;

    switch (d->tur) {
    case DÜĞÜM_TAM_SAYI:
        tam_sayi_uret(u, d);
        break;
    case DÜĞÜM_ONDALIK_SAYI: {
        /* Double sabiti .rodata'ya koy, xmm0'a yükle */
        int idx = ondalik_sabit_ekle(u, d->veri.ondalık_değer);
        yaz(u, "    movsd   .LD%d(%%rip), %%xmm0", idx);
        break;
    }
    case DÜĞÜM_METİN_DEĞERİ:
        metin_degeri_uret(u, d);
        break;
    case DÜĞÜM_MANTIK_DEĞERİ:
        mantik_uret(u, d);
        break;
    case DÜĞÜM_BOŞ_DEĞER:
        yaz(u, "    xorq    %%rax, %%rax");
        yaz(u, "    xorq    %%rbx, %%rbx");
        break;
    case DÜĞÜM_TANIMLAYICI:
        tanimlayici_uret(u, d);
        break;
    case DÜĞÜM_İKİLİ_İŞLEM:
        ikili_islem_uret(u, d);
        break;
    case DÜĞÜM_TEKLİ_İŞLEM:
        tekli_islem_uret(u, d);
        break;
    case DÜĞÜM_ÇAĞRI:
        cagri_uret(u, d);
        break;
    case DÜĞÜM_BORU:
        /* a |> b  =>  b(a) */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    movq    %%rax, %%rdi");
        if (d->çocuklar[1]->tur == DÜĞÜM_TANIMLAYICI) {
            /* Fonksiyon pointer olabilir mi kontrol et */
            Sembol *boru_s = sembol_ara(u->kapsam, d->çocuklar[1]->veri.tanimlayici.isim);
            if (boru_s && boru_s->tip == TİP_İŞLEV) {
                /* Fonksiyon pointer: indirect call */
                yaz(u, "    pushq   %%rdi");
                tanimlayici_uret(u, d->çocuklar[1]);
                yaz(u, "    movq    %%rax, %%r11");
                yaz(u, "    popq    %%rdi");
                yaz(u, "    xorq    %%rax, %%rax");
                yaz(u, "    call    *%%r11");
            } else {
                yaz(u, "    call    %s", d->çocuklar[1]->veri.tanimlayici.isim);
            }
        } else {
            ifade_üret(u, d->çocuklar[1]);
        }
        break;

    case DÜĞÜM_DİZİ_DEĞERİ: {
        /* Dizi literal: [e1, e2, ...] -> _tr_nesne_olustur ile heap tahsisi */
        int eleman_sayisi = d->çocuk_sayısı;
        int boyut = eleman_sayisi * 8;

        /* _tr_nesne_olustur: bellek ayır */
        yaz(u, "    movq    $%d, %%rdi", 1);        /* NESNE_TIP_DIZI */
        yaz(u, "    movq    $%d, %%rsi", boyut > 0 ? boyut : 8);  /* boyut */
        yaz(u, "    call    _tr_nesne_olustur");
        hata_bellek_uret(u);
        {
            int lbl = yeni_etiket(u);
            yaz(u, "    testq   %%rax, %%rax");
            yaz(u, "    jnz     .L%d", lbl);
            yaz(u, "    call    _hata_bellek");
            yaz(u, ".L%d:", lbl);
        }
        /* rax = tahsis edilen adres */
        yaz(u, "    pushq   %%rax");  /* adresi kaydet */

        /* Elemanları doldur */
        for (int i = 0; i < eleman_sayisi; i++) {
            yaz(u, "    pushq   (%%rsp)");  /* adresi koru */
            ifade_üret(u, d->çocuklar[i]);
            yaz(u, "    movq    %%rax, %%rcx");  /* değer */
            yaz(u, "    popq    %%rax");          /* adres */
            yaz(u, "    movq    %%rcx, %d(%%rax)", i * 8);
        }

        /* Sonuç: rax = pointer, rbx = eleman sayısı */
        yaz(u, "    popq    %%rax");
        yaz(u, "    movq    $%d, %%rbx", eleman_sayisi);
        break;
    }

    case DÜĞÜM_KÜME_DEĞERİ: {
        /* Küme literal: küme{e1, e2, ...} */
        yaz(u, "    call    _tr_kume_yeni");
        yaz(u, "    pushq   %%rax");  /* küme pointer'ını kaydet */
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            yaz(u, "    pushq   (%%rsp)");  /* küme ptr koru */
            ifade_üret(u, d->çocuklar[i]);
            yaz(u, "    movq    %%rax, %%rsi");  /* deger */
            yaz(u, "    popq    %%rdi");          /* küme */
            yaz(u, "    call    _tr_kume_ekle");
        }
        yaz(u, "    popq    %%rax");  /* küme ptr → rax */
        break;
    }

    case DÜĞÜM_LİSTE_ÜRETİMİ: {
        /* [ifade her x için kaynak eğer koşul] */
        /* çocuklar[0]=ifade, [1]=kaynak, [2]=filtre(opsiyonel) */

        /* Kaynak diziyi değerlendir → (ptr, count) */
        ifade_üret(u, d->çocuklar[1]);
        /* Stack: [count] [ptr] */
        yaz(u, "    pushq   %%rbx");  /* kaynak count */
        yaz(u, "    pushq   %%rax");  /* kaynak ptr */

        /* Sonuç dizisi oluştur: kaynak count kadar yer ayır (max) */
        yaz(u, "    imulq   $8, %%rbx, %%rsi");
        yaz(u, "    cmpq    $8, %%rsi");
        yaz(u, "    jge     .Llu_alloc_%d", u->etiket_sayac);
        yaz(u, "    movq    $8, %%rsi");
        yaz(u, ".Llu_alloc_%d:", u->etiket_sayac);
        yaz(u, "    movq    $1, %%rdi");  /* NESNE_TIP_DIZI */
        yaz(u, "    call    _tr_nesne_olustur");
        u->etiket_sayac++;

        /* Stack: [count] [ptr] [sonuc_ptr] [sonuc_idx=0] */
        yaz(u, "    pushq   %%rax");  /* sonuç ptr */
        yaz(u, "    pushq   $0");     /* sonuç index */

        /* Döngü index */
        yaz(u, "    pushq   $0");     /* kaynak index */

        int basla = yeni_etiket(u);
        int atla = yeni_etiket(u);
        int son = yeni_etiket(u);

        /* Yeni kapsam: döngü değişkeni */
        Kapsam *onceki_kapsam = u->kapsam;
        u->kapsam = kapsam_oluştur(u->arena, onceki_kapsam);
        Sembol *elem = sembol_ekle(u->arena, u->kapsam, d->veri.dongu.isim, TİP_TAM);
        elem->yerel_indeks = u->kapsam->yerel_sayac++;

        yaz(u, ".L%d:", basla);
        /* index < count? */
        yaz(u, "    movq    (%%rsp), %%rcx");        /* kaynak index */
        yaz(u, "    cmpq    32(%%rsp), %%rcx");      /* count */
        yaz(u, "    jge     .L%d", son);

        /* kaynak[index] → döngü değişkeni */
        yaz(u, "    movq    24(%%rsp), %%rdx");      /* kaynak ptr */
        yaz(u, "    movq    (%%rdx,%%rcx,8), %%rax");
        yaz(u, "    movq    %%rax, -%d(%%rbp)", (elem->yerel_indeks + 1) * 8);

        /* Filtre varsa koşul kontrol */
        if (d->çocuk_sayısı > 2) {
            ifade_üret(u, d->çocuklar[2]);
            yaz(u, "    testq   %%rax, %%rax");
            yaz(u, "    jz      .L%d", atla);
        }

        /* İfadeyi değerlendir */
        ifade_üret(u, d->çocuklar[0]);

        /* sonuç[sonuç_idx] = rax */
        yaz(u, "    movq    8(%%rsp), %%rcx");       /* sonuç index */
        yaz(u, "    movq    16(%%rsp), %%rdx");      /* sonuç ptr */
        yaz(u, "    movq    %%rax, (%%rdx,%%rcx,8)");
        yaz(u, "    incq    8(%%rsp)");              /* sonuç index++ */

        yaz(u, ".L%d:", atla);
        yaz(u, "    incq    (%%rsp)");               /* kaynak index++ */
        yaz(u, "    jmp     .L%d", basla);

        yaz(u, ".L%d:", son);
        /* Sonuç: rax = sonuç ptr, rbx = sonuç count */
        yaz(u, "    popq    %%rcx");   /* kaynak index (discard) */
        yaz(u, "    popq    %%rbx");   /* sonuç count */
        yaz(u, "    popq    %%rax");   /* sonuç ptr */
        yaz(u, "    addq    $16, %%rsp");  /* kaynak count + ptr temizle */

        u->kapsam = onceki_kapsam;
        break;
    }

    case DÜĞÜM_SÖZLÜK_DEĞERİ: {
        /* {k1: v1, k2: v2, ...} → sözlük oluştur */
        /* çocuklar: [k1, v1, k2, v2, ...] çiftler halinde */
        yaz(u, "    call    _tr_sozluk_yeni");
        yaz(u, "    pushq   %%rax");  /* sözlük ptr */
        for (int i = 0; i + 1 < d->çocuk_sayısı; i += 2) {
            /* Anahtar (metin) değerlendir */
            ifade_üret(u, d->çocuklar[i]);
            yaz(u, "    pushq   %%rbx");  /* anahtar len */
            yaz(u, "    pushq   %%rax");  /* anahtar ptr */
            /* Değer değerlendir */
            ifade_üret(u, d->çocuklar[i + 1]);
            yaz(u, "    movq    %%rax, %%rcx");         /* deger → rcx */
            yaz(u, "    popq    %%rsi");                /* anahtar ptr → rsi */
            yaz(u, "    popq    %%rdx");                /* anahtar len → rdx */
            yaz(u, "    movq    (%%rsp), %%rdi");       /* sözlük ptr → rdi */
            yaz(u, "    call    _tr_sozluk_ekle");
        }
        yaz(u, "    popq    %%rax");  /* sözlük ptr */
        break;
    }

    case DÜĞÜM_SÖZLÜK_ÜRETİMİ: {
        /* {k: v her x için kaynak eğer koşul} */
        /* çocuklar[0]=key_expr, [1]=val_expr, [2]=kaynak, [3]=filtre(opsiyonel) */

        /* Kaynak diziyi değerlendir → (ptr, count) */
        ifade_üret(u, d->çocuklar[2]);
        yaz(u, "    pushq   %%rbx");  /* kaynak count */
        yaz(u, "    pushq   %%rax");  /* kaynak ptr */

        /* Yeni sözlük oluştur */
        yaz(u, "    call    _tr_sozluk_yeni");
        yaz(u, "    pushq   %%rax");  /* sözlük ptr */

        /* Döngü index */
        yaz(u, "    pushq   $0");     /* kaynak index */

        int basla = yeni_etiket(u);
        int atla = yeni_etiket(u);
        int son = yeni_etiket(u);

        /* Yeni kapsam: döngü değişkeni */
        Kapsam *onceki_kapsam = u->kapsam;
        u->kapsam = kapsam_oluştur(u->arena, onceki_kapsam);
        Sembol *elem = sembol_ekle(u->arena, u->kapsam, d->veri.dongu.isim, TİP_TAM);
        elem->yerel_indeks = u->kapsam->yerel_sayac++;

        yaz(u, ".L%d:", basla);
        /* Stack: [kaynak_count(24)] [kaynak_ptr(16)] [sozluk_ptr(8)] [index(0)] */
        yaz(u, "    movq    (%%rsp), %%rcx");        /* kaynak index */
        yaz(u, "    cmpq    24(%%rsp), %%rcx");      /* count */
        yaz(u, "    jge     .L%d", son);

        /* kaynak[index] → döngü değişkeni */
        yaz(u, "    movq    16(%%rsp), %%rdx");      /* kaynak ptr */
        yaz(u, "    movq    (%%rdx,%%rcx,8), %%rax");
        yaz(u, "    movq    %%rax, -%d(%%rbp)", (elem->yerel_indeks + 1) * 8);

        /* Filtre varsa koşul kontrol */
        if (d->çocuk_sayısı > 3) {
            ifade_üret(u, d->çocuklar[3]);
            yaz(u, "    testq   %%rax, %%rax");
            yaz(u, "    jz      .L%d", atla);
        }

        /* Anahtar ifadesini değerlendir (metin bekleniyor) */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rbx");  /* anahtar len */
        yaz(u, "    pushq   %%rax");  /* anahtar ptr */

        /* Değer ifadesini değerlendir */
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    movq    %%rax, %%rcx");         /* deger → rcx */
        yaz(u, "    popq    %%rsi");                /* anahtar ptr → rsi */
        yaz(u, "    popq    %%rdx");                /* anahtar len → rdx */
        yaz(u, "    movq    8(%%rsp), %%rdi");      /* sözlük ptr → rdi */
        yaz(u, "    call    _tr_sozluk_ekle");

        yaz(u, ".L%d:", atla);
        yaz(u, "    incq    (%%rsp)");               /* kaynak index++ */
        yaz(u, "    jmp     .L%d", basla);

        yaz(u, ".L%d:", son);
        /* Sonuç: rax = sözlük ptr */
        yaz(u, "    popq    %%rcx");   /* index (discard) */
        yaz(u, "    popq    %%rax");   /* sözlük ptr */
        yaz(u, "    addq    $16, %%rsp");  /* kaynak count + ptr temizle */

        u->kapsam = onceki_kapsam;
        break;
    }

    case DÜĞÜM_DİZİ_ERİŞİM: {
        /* dizi[indeks] -> elemanı yükle */
        /* çocuklar[0] = dizi ifadesi, çocuklar[1] = indeks ifadesi */

        /* Operatör yükleme: base sınıf ise indeks_oku metotu çağır */
        if (d->çocuklar[0]->sonuç_tipi == TİP_SINIF) {
            char *sa = sinif_adi_bul(u, d->çocuklar[0]);
            if (sa) {
                ifade_üret(u, d->çocuklar[1]);  /* index → rax */
                yaz(u, "    pushq   %%rax");
                ifade_üret(u, d->çocuklar[0]);  /* self → rax */
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    popq    %%rsi");
                yaz(u, "    call    %s_indeks_oku", sa);
                break;
            }
        }

        /* Metin karakter indeksleme: metin[i] -> tek karakter metin */
        if (d->çocuklar[0]->sonuç_tipi == TİP_METİN) {
            ifade_üret(u, d->çocuklar[1]);  /* indeks → rax */
            yaz(u, "    pushq   %%rax");     /* indeks kaydet */
            ifade_üret(u, d->çocuklar[0]);  /* metin → rax=ptr, rbx=len */
            yaz(u, "    movq    %%rax, %%rdi");   /* ptr */
            yaz(u, "    movq    %%rbx, %%rsi");   /* byte_len */
            yaz(u, "    popq    %%rdx");           /* karakter_indeks */
            yaz(u, "    call    _tr_utf8_karakter_al");
            /* TrMetin dönüş: rax=ptr, rdx=len */
            yaz(u, "    movq    %%rdx, %%rbx");
            break;
        }

        /* İndeksi hesapla ve kaydet */
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    pushq   %%rax");  /* indeks */

        /* Dizi ifadesini hesapla */
        ifade_üret(u, d->çocuklar[0]);
        /* rax = dizi pointer'ı, rbx = eleman sayısı */

        /* rbx (uzunluk) ve rax (pointer) kaydet */
        yaz(u, "    pushq   %%rbx");  /* uzunluk */
        yaz(u, "    pushq   %%rax");  /* pointer */

        /* İndeksi geri al ve sınır kontrolü yap */
        yaz(u, "    popq    %%rax");          /* pointer */
        yaz(u, "    popq    %%rdx");          /* uzunluk */
        yaz(u, "    popq    %%rcx");          /* indeks */

        /* Sınır kontrolü: 0 <= rcx < rdx */
        hata_dizi_sinir_uret(u);
        int lbl = yeni_etiket(u);
        yaz(u, "    cmpq    $0, %%rcx");
        yaz(u, "    jl      .Ldsn%d", lbl);
        yaz(u, "    cmpq    %%rdx, %%rcx");
        yaz(u, "    jl      .Ldok%d", lbl);
        yaz(u, ".Ldsn%d:", lbl);
        yaz(u, "    call    _hata_dizi_sinir");
        yaz(u, ".Ldok%d:", lbl);

        /* Elemanı yükle: *(rax + rcx*8) */
        yaz(u, "    movq    (%%rax, %%rcx, 8), %%rax");
        break;
    }

    case DÜĞÜM_ERİŞİM: {
        /* nesne.alan -> alan değerini yükle */
        /* çocuklar[0] = nesne ifadesi, veri.tanimlayici.isim = alan adı */
        if (d->çocuk_sayısı > 0 && d->veri.tanimlayici.isim) {
            /* Nesne ifadesini hesapla -> rax = nesne pointer */
            ifade_üret(u, d->çocuklar[0]);

            /* Alan offset'ini bul */
            char *alan_adi = d->veri.tanimlayici.isim;

            /* Nesne değişkeninin sınıf adını bul */
            Düğüm *nesne = d->çocuklar[0];
            char *sınıf_adı = NULL;
            if (nesne->tur == DÜĞÜM_TANIMLAYICI) {
                Sembol *ns = sembol_ara(u->kapsam, nesne->veri.tanimlayici.isim);
                if (ns && ns->sınıf_adı) sınıf_adı = ns->sınıf_adı;
            }

            /* "bu" için mevcut sınıf kapsamındaki sınıfı bul */
            if (!sınıf_adı && nesne->tur == DÜĞÜM_TANIMLAYICI &&
                strcmp(nesne->veri.tanimlayici.isim, "bu") == 0) {
                Sembol *bu_s = sembol_ara(u->kapsam, "bu");
                if (bu_s && bu_s->sınıf_adı) sınıf_adı = bu_s->sınıf_adı;
            }

            if (sınıf_adı) {
                SinifBilgi *sb = sınıf_bul(u->kapsam, sınıf_adı);
                if (sb) {
                    int bulundu = 0;
                    for (int i = 0; i < sb->alan_sayisi; i++) {
                        if (strcmp(sb->alanlar[i].isim, alan_adi) == 0) {
                            TipTürü alan_tipi = sb->alanlar[i].tip;
                            if (sb->alanlar[i].statik) {
                                /* Statik alan: RIP-relative erişim */
                                if (alan_tipi == TİP_METİN || alan_tipi == TİP_DİZİ) {
                                    yaz(u, "    movq    _%s_%s(%%rip), %%rax", sınıf_adı, alan_adi);
                                    yaz(u, "    movq    _%s_%s+8(%%rip), %%rbx", sınıf_adı, alan_adi);
                                } else {
                                    yaz(u, "    movq    _%s_%s(%%rip), %%rax", sınıf_adı, alan_adi);
                                }
                            } else {
                                int offset = sb->alanlar[i].offset;
                                if (alan_tipi == TİP_METİN || alan_tipi == TİP_DİZİ) {
                                    /* metin/dizi: hem ptr hem len yükle */
                                    yaz(u, "    movq    %d(%%rax), %%rbx", offset + 8);  /* len */
                                    yaz(u, "    movq    %d(%%rax), %%rax", offset);       /* ptr */
                                } else {
                                    yaz(u, "    movq    %d(%%rax), %%rax", offset);
                                }
                            }
                            bulundu = 1;
                            break;
                        }
                    }
                    /* Getter: alan bulunamadıysa al_<alan> metodunu dene */
                    if (!bulundu) {
                        char getter_adi[256];
                        snprintf(getter_adi, sizeof(getter_adi), "al_%s", alan_adi);
                        for (int i = 0; i < sb->metot_sayisi; i++) {
                            if (sb->metot_isimleri[i] && strcmp(sb->metot_isimleri[i], getter_adi) == 0) {
                                /* rax zaten nesne pointer'ı = bu */
                                yaz(u, "    movq    %%rax, %%rdi");
                                yaz(u, "    call    %s_%s", sınıf_adı, getter_adi);
                                bulundu = 1;
                                break;
                            }
                        }
                    }
                    /* @özellik dekoratörlü getter: alan adıyla aynı isimli metot */
                    if (!bulundu) {
                        for (int i = 0; i < sb->ozellik_getter_sayisi; i++) {
                            if (sb->ozellik_getters[i] && strcmp(sb->ozellik_getters[i], alan_adi) == 0) {
                                yaz(u, "    movq    %%rax, %%rdi");
                                yaz(u, "    call    %s_%s", sınıf_adı, alan_adi);
                                bulundu = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
        break;
    }

    case DÜĞÜM_LAMBDA: {
        /* Lambda: fonksiyon gövdesini ayrı üret, adresi rax'a yükle */
        char *lambda_isim = d->veri.islev.isim;

        /* Yakalanan değişkenleri tespit et */
        /* Lambda parametrelerinin kapsamını oluştur */
        Kapsam *param_kapsam = kapsam_oluştur(u->arena, NULL);
        if (d->çocuk_sayısı > 0) {
            Düğüm *params = d->çocuklar[0];
            for (int pi = 0; pi < params->çocuk_sayısı; pi++) {
                sembol_ekle(u->arena, param_kapsam,
                           params->çocuklar[pi]->veri.değişken.isim, TİP_TAM);
            }
        }
        char **yak_isimler = (char **)arena_ayir(u->arena, 32 * sizeof(char *));
        int *yak_indeksler = (int *)arena_ayir(u->arena, 32 * sizeof(int));
        int yak_sayisi = 0;
        /* Gövdedeki serbest değişkenleri topla */
        if (d->çocuk_sayısı > 1) {
            kapanis_yakala_topla(d->çocuklar[1], param_kapsam, u->kapsam,
                                u->arena, &yak_isimler, &yak_indeksler, &yak_sayisi);
        }
        d->veri.islev.yakalanan_isimler = yak_isimler;
        d->veri.islev.yakalanan_indeksler = yak_indeksler;
        d->veri.islev.yakalanan_sayisi = yak_sayisi;

        /* Lambda fonksiyonunu yardımcılar bölümüne üret */
        Metin onceki = u->cikti;
        metin_baslat(&u->cikti);

        Kapsam *onceki_kapsam = u->kapsam;
        /* Lambda'yı normal işlev olarak üret */
        islev_uret(u, d);
        u->kapsam = onceki_kapsam;

        /* Üretilen kodu yardımcılara ekle */
        metin_ekle(&u->yardimcilar, u->cikti.veri);
        metin_serbest(&u->cikti);
        u->cikti = onceki;

        if (yak_sayisi > 0) {
            /* Kapanış oluştur: ortam dizisi ayır */
            yaz(u, "    movq    $5, %%rdi");  /* NESNE_TIP_KAPANIS */
            yaz(u, "    movq    $%d, %%rsi", (yak_sayisi + 2) * 8);
            yaz(u, "    call    _tr_nesne_olustur");
            yaz(u, "    pushq   %%rax");
            /* [0] = fn_ptr */
            yaz(u, "    leaq    %s(%%rip), %%rcx", lambda_isim);
            yaz(u, "    movq    %%rcx, 0(%%rax)");
            /* [1] = count */
            yaz(u, "    movq    $%d, 8(%%rax)", yak_sayisi);
            /* [2..] = yakalanan değerler */
            for (int ci = 0; ci < yak_sayisi; ci++) {
                int src_offset = (yak_indeksler[ci] + 1) * 8;
                yaz(u, "    movq    -%d(%%rbp), %%rcx", src_offset);
                yaz(u, "    movq    (%%rsp), %%rax");
                yaz(u, "    movq    %%rcx, %d(%%rax)", (ci + 2) * 8);
            }
            yaz(u, "    popq    %%rax");  /* kapanış ptr */
            /* Ortam pointer'ını global'e kaydet */
            yaz(u, "    movq    %%rax, _tr_kapanis_ortam(%%rip)");
            /* Lambda adresini döndür (bare fn_ptr — uyumlu) */
            yaz(u, "    movq    0(%%rax), %%rax");
        } else {
            /* Yakalama yok: sadece fonksiyon adresi */
            yaz(u, "    leaq    %s(%%rip), %%rax", lambda_isim);
        }
        break;
    }

    case DÜĞÜM_BEKLE: {
        /* bekle ifade -> iş parçacığı oluştur ve bekle */
        /* çocuklar[0] = çağrı ifadesi */
        if (d->çocuk_sayısı > 0) {
            Düğüm *cagri = d->çocuklar[0];
            if (cagri->tur == DÜĞÜM_ÇAĞRI && cagri->veri.tanimlayici.isim) {
                int arg_sayisi = cagri->çocuk_sayısı;

                /* Argümanları hesapla ve stack'e at (ters sırada) */
                for (int i = arg_sayisi - 1; i >= 0; i--) {
                    ifade_üret(u, cagri->çocuklar[i]);
                    yaz(u, "    pushq   %%rax");
                }

                /* _tr_async_olustur(fn_ptr, arg_count, a0, a1, a2, a3) */
                yaz(u, "    leaq    %s(%%rip), %%rdi", cagri->veri.tanimlayici.isim);
                yaz(u, "    movq    $%d, %%rsi", arg_sayisi);  /* arg_count */

                /* Argümanları registerlara taşı */
                if (arg_sayisi > 0) yaz(u, "    popq    %%rdx");  else yaz(u, "    xorq    %%rdx, %%rdx");
                if (arg_sayisi > 1) yaz(u, "    popq    %%rcx");  else yaz(u, "    xorq    %%rcx, %%rcx");
                if (arg_sayisi > 2) yaz(u, "    popq    %%r8");   else yaz(u, "    xorq    %%r8, %%r8");
                if (arg_sayisi > 3) yaz(u, "    popq    %%r9");   else yaz(u, "    xorq    %%r9, %%r9");

                yaz(u, "    xorq    %%rax, %%rax");
                yaz(u, "    call    _tr_async_olustur");

                /* rax = handle, async_bekle çağır */
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    call    _tr_async_bekle");
                /* rax = sonuç */
            } else {
                /* Fallback: ifadeyi doğrudan çalıştır */
                ifade_üret(u, d->çocuklar[0]);
            }
        }
        break;
    }

    case DÜĞÜM_ÜÇLÜ: {
        /* Üçlü ifade: koşul ? değer1 : değer2 */
        /* çocuklar[0]=koşul, çocuklar[1]=doğru değer, çocuklar[2]=yanlış değer */
        int yanlis_etiket = yeni_etiket(u);
        int son_etiket = yeni_etiket(u);

        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    testq   %%rax, %%rax");
        yaz(u, "    jz      .L%d", yanlis_etiket);

        /* Doğru değer */
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    jmp     .L%d", son_etiket);

        /* Yanlış değer */
        yaz(u, ".L%d:", yanlis_etiket);
        ifade_üret(u, d->çocuklar[2]);

        yaz(u, ".L%d:", son_etiket);
        break;
    }

    case DÜĞÜM_DİLİM: {
        /* Dizi/metin dilim: a[baş:son] */
        /* çocuklar[0]=kaynak, çocuklar[1]=başlangıç, çocuklar[2]=bitiş */
        /* Başlangıç indeksini hesapla */
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    pushq   %%rax");  /* başlangıç */

        /* Bitiş indeksini hesapla */
        ifade_üret(u, d->çocuklar[2]);
        yaz(u, "    pushq   %%rax");  /* bitiş */

        /* Kaynak diziyi/metni hesapla */
        ifade_üret(u, d->çocuklar[0]);
        /* rax=ptr, rbx=count/len */

        /* Parametreleri ayarla: rdi=ptr, rsi=count, rdx=start, rcx=end */
        yaz(u, "    movq    %%rax, %%rdi");  /* ptr */
        yaz(u, "    movq    %%rbx, %%rsi");  /* count/len */
        yaz(u, "    popq    %%rcx");          /* end */
        yaz(u, "    popq    %%rdx");          /* start */

        if (d->çocuklar[0]->sonuç_tipi == TİP_METİN) {
            yaz(u, "    call    _tr_utf8_dilim");
            /* TrMetin dönüş: rax=ptr, rdx=len */
            yaz(u, "    movq    %%rdx, %%rbx");
        } else {
            yaz(u, "    call    _tr_dizi_dilim");
            /* TrDizi dönüş: rax=ptr, rdx=count */
            yaz(u, "    movq    %%rdx, %%rbx");
        }
        break;
    }

    case DÜĞÜM_DEMET: {
        /* Demet: sadece ilk elemanı rax'a, ikinciyi rdx'e koy */
        if (d->çocuk_sayısı >= 2) {
            ifade_üret(u, d->çocuklar[1]);
            yaz(u, "    pushq   %%rax");
            ifade_üret(u, d->çocuklar[0]);
            yaz(u, "    popq    %%rdx");
        } else if (d->çocuk_sayısı == 1) {
            ifade_üret(u, d->çocuklar[0]);
        }
        break;
    }

    case DÜĞÜM_WALRUS: {
        /* isim := ifade → değeri hesapla, değişkene ata, rax'ta bırak */
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
        }
        if (d->veri.tanimlayici.isim) {
            Sembol *ws = sembol_ara(u->kapsam, d->veri.tanimlayici.isim);
            if (!ws) {
                ws = sembol_ekle(u->arena, u->kapsam, d->veri.tanimlayici.isim,
                                 d->sonuç_tipi);
                ws->yerel_indeks = u->kapsam->yerel_sayac++;
            }
            int walrus_off = (ws->yerel_indeks + 1) * 8;
            yaz(u, "    movq    %%rax, -%d(%%rbp)", walrus_off);
        }
        break;
    }

    /* ====== SONUÇ/SEÇENEK TİP SİSTEMİ ====== */

    case DÜĞÜM_SONUÇ_OLUŞTUR: {
        /* Tamam(değer) veya Hata(değer)
         * Memory layout: rax = tag (0=Tamam, 1=Hata), rbx = data
         */
        int varyant = d->veri.sonuç_seçenek.varyant;
        if (d->çocuk_sayısı > 0) {
            /* Değeri hesapla */
            ifade_üret(u, d->çocuklar[0]);
            yaz(u, "    movq    %%rax, %%rbx");  /* data -> rbx */
        } else {
            yaz(u, "    xorq    %%rbx, %%rbx");  /* data = 0 */
        }
        yaz(u, "    movq    $%d, %%rax", varyant);  /* tag -> rax */
        break;
    }

    case DÜĞÜM_SEÇENEK_OLUŞTUR: {
        /* Bir(değer) veya Hiç
         * Memory layout: rax = tag (0=Bir, 1=Hiç), rbx = data
         */
        int varyant = d->veri.sonuç_seçenek.varyant;
        if (varyant == 0 && d->çocuk_sayısı > 0) {
            /* Bir(değer) - değeri hesapla */
            ifade_üret(u, d->çocuklar[0]);
            yaz(u, "    movq    %%rax, %%rbx");  /* data -> rbx */
            yaz(u, "    xorq    %%rax, %%rax");  /* tag = 0 (Bir) */
        } else {
            /* Hiç - değer yok */
            yaz(u, "    xorq    %%rbx, %%rbx");  /* data = 0 */
            yaz(u, "    movq    $1, %%rax");     /* tag = 1 (Hiç) */
        }
        break;
    }

    case DÜĞÜM_SORU_OP: {
        /* ifade? - Hata yayılımı operatörü
         * Eğer ifade Hata/Hiç ise, fonksiyondan döndür
         * Aksi halde değeri (rbx) rax'a taşı ve devam et
         */
        int basari_etiket = yeni_etiket(u);

        /* İfadeyi hesapla: rax = tag, rbx = data */
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
        }

        /* Tag kontrolü: rax == 0 ise başarılı (Tamam/Bir) */
        yaz(u, "    testq   %%rax, %%rax");
        yaz(u, "    jz      .L%d", basari_etiket);

        /* Hata durumu: fonksiyondan dön (rax=tag, rbx=data olarak) */
        yaz(u, "    # ? operatörü: Hata/Hiç durumu - işlevden dön");
        yaz(u, "    leave");
        yaz(u, "    ret");

        /* Başarı durumu: değeri (rbx) rax'a taşı */
        yaz(u, ".L%d:", basari_etiket);
        yaz(u, "    movq    %%rbx, %%rax");  /* başarı değeri rax'a */
        break;
    }

    default:
        break;
    }
}

/* ---- Bildirim üretimi ---- */

static void degisken_uret(Üretici *u, Düğüm *d) {
    TipTürü tip = tip_adı_çevir(d->veri.değişken.tip);
    /* Sınıf tipi kontrolü */
    if (tip == TİP_BİLİNMİYOR && d->veri.değişken.tip) {
        SinifBilgi *sb = sınıf_bul(u->kapsam, d->veri.değişken.tip);
        if (sb) tip = TİP_SINIF;
    }

    /* Çoklu dönüş: ikinci değişken (rdx'ten) */
    if (d->veri.değişken.genel == 2) {
        Sembol *s = sembol_ekle(u->arena, u->kapsam, d->veri.değişken.isim, tip);
        int offset = (s->yerel_indeks + 1) * 8;
        /* rdx stack'te saklanmış olmalı (çoklu dönüş bloğu tarafından) */
        yaz(u, "    popq    %%rax");
        yaz(u, "    movq    %%rax, -%d(%%rbp)", offset);
        return;
    }

    /* Global değişken: .bss'de alan ayır, RIP-relative eriş */
    if (d->veri.değişken.genel) {
        int boyut = 8;
        if (tip == TİP_METİN || tip == TİP_DİZİ) boyut = 16;
        bss_yaz(u, "    .comm   _genel_%s, %d, 8", d->veri.değişken.isim, boyut);

        Sembol *s = sembol_ekle(u->arena, u->kapsam, d->veri.değişken.isim, tip);
        s->global_mi = 1;
        if (tip == TİP_SINIF) s->sınıf_adı = d->veri.değişken.tip;

        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
            if (tip == TİP_METİN || tip == TİP_DİZİ) {
                yaz(u, "    movq    %%rax, _genel_%s(%%rip)", d->veri.değişken.isim);
                yaz(u, "    movq    %%rbx, _genel_%s+8(%%rip)", d->veri.değişken.isim);
            } else if (tip == TİP_ONDALIK) {
                yaz(u, "    movsd   %%xmm0, _genel_%s(%%rip)", d->veri.değişken.isim);
            } else {
                yaz(u, "    movq    %%rax, _genel_%s(%%rip)", d->veri.değişken.isim);
            }
        }
        return;
    }

    /* Sınıf nesneleri tek slot (64-bit pointer) */
    if (tip == TİP_SINIF) {
        Sembol *s = sembol_ekle(u->arena, u->kapsam, d->veri.değişken.isim, tip);
        s->sınıf_adı = d->veri.değişken.tip;
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
            int offset = (s->yerel_indeks + 1) * 8;
            yaz(u, "    movq    %%rax, -%d(%%rbp)", offset);
        }
        return;
    }
    /* metin ve dizi tipi 2 slot kullanır (pointer + length/count) */
    if (tip == TİP_METİN || tip == TİP_DİZİ) {
        Sembol *s = sembol_ekle(u->arena, u->kapsam, d->veri.değişken.isim, tip);
        /* İkinci slot için sayacı bir artır */
        u->kapsam->yerel_sayac++;

        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
            int offset = (s->yerel_indeks + 1) * 8;
            yaz(u, "    movq    %%rax, -%d(%%rbp)", offset);       /* pointer */
            yaz(u, "    movq    %%rbx, -%d(%%rbp)", offset + 8);   /* length/count */
        }
    } else if (tip == TİP_ONDALIK) {
        Sembol *s = sembol_ekle(u->arena, u->kapsam, d->veri.değişken.isim, tip);
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
            int offset = (s->yerel_indeks + 1) * 8;
            yaz(u, "    movsd   %%xmm0, -%d(%%rbp)", offset);
        }
    } else {
        Sembol *s = sembol_ekle(u->arena, u->kapsam, d->veri.değişken.isim, tip);
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
            int offset = (s->yerel_indeks + 1) * 8;
            yaz(u, "    movq    %%rax, -%d(%%rbp)", offset);
        }
    }
}

static void atama_uret(Üretici *u, Düğüm *d) {
    if (d->çocuk_sayısı > 0) {
        ifade_üret(u, d->çocuklar[0]);

        Sembol *s = sembol_ara(u->kapsam, d->veri.tanimlayici.isim);
        if (s) {
            if (s->global_mi) {
                /* Global değişken ataması: RIP-relative */
                if (s->tip == TİP_METİN || s->tip == TİP_DİZİ) {
                    yaz(u, "    movq    %%rax, _genel_%s(%%rip)", d->veri.tanimlayici.isim);
                    yaz(u, "    movq    %%rbx, _genel_%s+8(%%rip)", d->veri.tanimlayici.isim);
                } else if (s->tip == TİP_ONDALIK) {
                    yaz(u, "    movsd   %%xmm0, _genel_%s(%%rip)", d->veri.tanimlayici.isim);
                } else {
                    yaz(u, "    movq    %%rax, _genel_%s(%%rip)", d->veri.tanimlayici.isim);
                }
            } else {
                int offset = (s->yerel_indeks + 1) * 8;
                if (s->tip == TİP_METİN || s->tip == TİP_DİZİ) {
                    yaz(u, "    movq    %%rax, -%d(%%rbp)", offset);
                    yaz(u, "    movq    %%rbx, -%d(%%rbp)", offset + 8);
                } else {
                    yaz(u, "    movq    %%rax, -%d(%%rbp)", offset);
                }
            }
        }
    }
}

static void eger_uret(Üretici *u, Düğüm *d) {
    int yoksa_etiket = yeni_etiket(u);
    int son_etiket = yeni_etiket(u);

    /* Koşul */
    ifade_üret(u, d->çocuklar[0]);
    yaz(u, "    testq   %%rax, %%rax");
    yaz(u, "    jz      .L%d", yoksa_etiket);

    /* Doğru bloğu */
    if (d->çocuk_sayısı > 1) blok_uret(u, d->çocuklar[1]);
    yaz(u, "    jmp     .L%d", son_etiket);

    yaz(u, ".L%d:", yoksa_etiket);

    /* yoksa eğer / yoksa blokları */
    int i = 2;
    while (i < d->çocuk_sayısı) {
        if (d->çocuklar[i]->tur != DÜĞÜM_BLOK) {
            /* yoksa eğer koşulu */
            int sonraki = yeni_etiket(u);
            ifade_üret(u, d->çocuklar[i]);
            yaz(u, "    testq   %%rax, %%rax");
            yaz(u, "    jz      .L%d", sonraki);
            i++;
            if (i < d->çocuk_sayısı) {
                blok_uret(u, d->çocuklar[i]);
                i++;
            }
            yaz(u, "    jmp     .L%d", son_etiket);
            yaz(u, ".L%d:", sonraki);
        } else {
            /* yoksa bloğu */
            blok_uret(u, d->çocuklar[i]);
            i++;
        }
    }

    yaz(u, ".L%d:", son_etiket);
}

static void dongu_uret(Üretici *u, Düğüm *d) {
    int başlangıç = yeni_etiket(u);
    int bitis = yeni_etiket(u);

    int onceki_baslangic = u->dongu_baslangic_etiket;
    int onceki_bitis = u->dongu_bitis_etiket;
    u->dongu_baslangic_etiket = başlangıç;
    u->dongu_bitis_etiket = bitis;

    /* Etiketli kır/devam için yığına it */
    int onceki_derinlik = u->dongu_derinligi;
    if (d->veri.dongu.isim && u->dongu_derinligi < URETICI_MAKS_DONGU) {
        u->dongu_yigini[u->dongu_derinligi].isim = d->veri.dongu.isim;
        u->dongu_yigini[u->dongu_derinligi].bitis_etiket = bitis;
        u->dongu_yigini[u->dongu_derinligi].baslangic_etiket = başlangıç;
        u->dongu_derinligi++;
    }

    /* Döngü değişkeni */
    Kapsam *onceki_kapsam = u->kapsam;
    u->kapsam = kapsam_oluştur(u->arena, onceki_kapsam);
    Sembol *sayac = sembol_ekle(u->arena, u->kapsam, d->veri.dongu.isim, TİP_TAM);
    sayac->global_mi = 0;

    /* Adım değeri varsa: 4 çocuk (başlangıç, bitiş, adım, gövde)
     * Yoksa: 3 çocuk (başlangıç, bitiş, gövde) */
    int adim_var = (d->çocuk_sayısı > 3);
    int govde_idx = adim_var ? 3 : 2;

    /* Başlangıç değeri */
    ifade_üret(u, d->çocuklar[0]);
    int sayac_offset = (sayac->yerel_indeks + 1) * 8;
    yaz(u, "    movq    %%rax, -%d(%%rbp)", sayac_offset);

    /* Bitiş değerini hesapla ve sakla */
    ifade_üret(u, d->çocuklar[1]);
    yaz(u, "    pushq   %%rax");  /* bitiş değeri stack'te */

    /* Adım değerini hesapla ve sakla */
    if (adim_var) {
        ifade_üret(u, d->çocuklar[2]);
    } else {
        yaz(u, "    movq    $1, %%rax");
    }
    yaz(u, "    pushq   %%rax");  /* adım değeri stack'te */

    yaz(u, ".L%d:", başlangıç);
    /* Sayacı yükle ve karşılaştır */
    yaz(u, "    movq    -%d(%%rbp), %%rax", sayac_offset);
    /* Adım pozitif mi kontrol et */
    yaz(u, "    cmpq    $0, (%%rsp)");
    int ileri_etiketi = yeni_etiket(u);
    yaz(u, "    jge     .L%d", ileri_etiketi);
    /* Negatif adım: sayac >= bitis mi? */
    yaz(u, "    cmpq    8(%%rsp), %%rax");
    yaz(u, "    jl      .L%d", bitis);
    int govde_etiketi = yeni_etiket(u);
    yaz(u, "    jmp     .L%d", govde_etiketi);
    /* Pozitif adım: sayac <= bitis mi? */
    yaz(u, ".L%d:", ileri_etiketi);
    yaz(u, "    cmpq    8(%%rsp), %%rax");
    yaz(u, "    jg      .L%d", bitis);
    yaz(u, ".L%d:", govde_etiketi);

    /* Gövde */
    if (d->çocuk_sayısı > govde_idx) blok_uret(u, d->çocuklar[govde_idx]);

    /* Sayacı adım kadar artır */
    yaz(u, "    movq    (%%rsp), %%rax");
    yaz(u, "    addq    %%rax, -%d(%%rbp)", sayac_offset);
    yaz(u, "    jmp     .L%d", başlangıç);

    yaz(u, ".L%d:", bitis);
    yaz(u, "    addq    $16, %%rsp");  /* bitiş + adım değerlerini temizle */

    u->kapsam = onceki_kapsam;
    u->dongu_derinligi = onceki_derinlik;
    u->dongu_baslangic_etiket = onceki_baslangic;
    u->dongu_bitis_etiket = onceki_bitis;
}

static void iken_uret(Üretici *u, Düğüm *d) {
    int başlangıç = yeni_etiket(u);
    int bitis = yeni_etiket(u);
    int yoksa_var = (d->çocuk_sayısı > 2);
    int bitis_kir = yoksa_var ? yeni_etiket(u) : bitis;
    int son_etiket = yoksa_var ? yeni_etiket(u) : bitis;

    int onceki_baslangic = u->dongu_baslangic_etiket;
    int onceki_bitis = u->dongu_bitis_etiket;
    int onceki_yoksa = u->dongu_yoksa_etiket;
    u->dongu_baslangic_etiket = başlangıç;
    u->dongu_bitis_etiket = yoksa_var ? bitis_kir : bitis;
    u->dongu_yoksa_etiket = yoksa_var ? bitis_kir : 0;

    yaz(u, ".L%d:", başlangıç);
    ifade_üret(u, d->çocuklar[0]);
    yaz(u, "    testq   %%rax, %%rax");
    yaz(u, "    jz      .L%d", bitis);

    if (d->çocuk_sayısı > 1) blok_uret(u, d->çocuklar[1]);

    yaz(u, "    jmp     .L%d", başlangıç);
    yaz(u, ".L%d:", bitis);

    /* Yoksa bloğu (kır olmadan tamamlandı) */
    if (yoksa_var) {
        blok_uret(u, d->çocuklar[2]);
        yaz(u, "    jmp     .L%d", son_etiket);
        yaz(u, ".L%d:", bitis_kir);
        yaz(u, ".L%d:", son_etiket);
    }

    u->dongu_baslangic_etiket = onceki_baslangic;
    u->dongu_bitis_etiket = onceki_bitis;
    u->dongu_yoksa_etiket = onceki_yoksa;
}

/* Özelleştirilmiş generic fonksiyon üretimi için yardımcı */
static void islev_uret_ozel(Üretici *u, Düğüm *d, const char *ozel_isim,
                            const char *tip_parametre, const char *somut_tip);

static void islev_uret(Üretici *u, Düğüm *d) {
    char *isim = d->veri.islev.isim;
    char *dekorator = d->veri.islev.dekorator;
    TipTürü donus = d->veri.islev.dönüş_tipi ?
        tip_adı_çevir(d->veri.islev.dönüş_tipi) : TİP_BOŞLUK;

    /* Generic fonksiyonları atla - özelleştirilmiş versiyonları ayrıca üretilir */
    if (d->veri.islev.tip_parametre != NULL) {
        return;
    }

    /* Dekoratör: orijinal fonksiyonu __dekoratsiz_<isim> olarak üret */
    char gercek_isim[256];
    if (dekorator) {
        snprintf(gercek_isim, sizeof(gercek_isim), "__dekoratsiz_%s", isim);
    } else {
        snprintf(gercek_isim, sizeof(gercek_isim), "%s", isim);
    }

    /* Fonksiyon sembolünü üst kapsamda kaydet (param bilgisi ile) */
    Sembol *fn_sem = sembol_ekle(u->arena, u->kapsam, isim, donus);
    fn_sem->dönüş_tipi = donus;

    /* Parametre tiplerini topla */
    int param_sayisi = 0;
    TipTürü param_tipleri[32] = {0};
    if (d->çocuk_sayısı > 0) {
        Düğüm *params = d->çocuklar[0];
        param_sayisi = params->çocuk_sayısı;
        for (int i = 0; i < param_sayisi && i < 32; i++) {
            param_tipleri[i] = tip_adı_çevir(params->çocuklar[i]->veri.değişken.tip);
            fn_sem->param_tipleri[i] = param_tipleri[i];
        }
    }
    fn_sem->param_sayisi = param_sayisi;

    /* Mevcut işlev dönüş tipini kaydet */
    TipTürü onceki_donus = u->mevcut_islev_donus_tipi;
    u->mevcut_islev_donus_tipi = donus;

    yaz(u, "");
    yaz(u, "    .globl  %s", gercek_isim);
    yaz(u, "%s:", gercek_isim);
    yaz(u, "    pushq   %%rbp");
    yaz(u, "    movq    %%rsp, %%rbp");

    /* Yeni kapsam */
    Kapsam *onceki = u->kapsam;
    u->kapsam = kapsam_oluştur(u->arena, onceki);
    u->kapsam->yerel_sayac = 0;

    /* Sınıf metotu ise "bu" parametresini ekle */
    if (u->mevcut_sinif) {
        SinifBilgi *sb = sınıf_bul(onceki, u->mevcut_sinif);
        Sembol *bu_s = sembol_ekle(u->arena, u->kapsam, "bu", TİP_SINIF);
        bu_s->sınıf_adı = u->mevcut_sinif;
        bu_s->sınıf_bilgi = sb;
        bu_s->parametre_mi = 1;
    }

    /* Parametreleri tanımla */
    if (d->çocuk_sayısı > 0) {
        Düğüm *params = d->çocuklar[0];
        for (int i = 0; i < params->çocuk_sayısı; i++) {
            Düğüm *p = params->çocuklar[i];
            TipTürü p_tip = tip_adı_çevir(p->veri.değişken.tip);
            /* Sınıf tipi kontrolü: bilinmeyen tip sınıf olabilir */
            if (p_tip == TİP_BİLİNMİYOR && p->veri.değişken.tip) {
                SinifBilgi *psb = sınıf_bul(u->kapsam, p->veri.değişken.tip);
                if (psb) p_tip = TİP_SINIF;
            }
            Sembol *s = sembol_ekle(u->arena, u->kapsam, p->veri.değişken.isim, p_tip);
            s->parametre_mi = 1;
            s->global_mi = 0;
            /* Sınıf parametresi: sınıf_adı ayarla */
            if (p_tip == TİP_SINIF && p->veri.değişken.tip) {
                s->sınıf_adı = p->veri.değişken.tip;
            }
            /* Metin parametreler 2 slot kullanır */
            if (p_tip == TİP_METİN) {
                u->kapsam->yerel_sayac++;
            }
        }
    }

    /* Yeterli stack alanı ayır (en az 64 byte) */
    int stack_boyut = (u->kapsam->yerel_sayac + 8) * 8;
    /* 16-byte hizala */
    stack_boyut = (stack_boyut + 15) & ~15;
    yaz(u, "    subq    $%d, %%rsp", stack_boyut);

    /* Parametreleri stack'e kopyala - tip bazlı register ataması */
    /* System V ABI: integer args -> rdi,rsi,rdx,rcx,r8,r9; float args -> xmm0-xmm7 */

    /* Sınıf metotu ise "bu" pointer'ını rdi'den kaydet */
    if (u->mevcut_sinif) {
        Sembol *bu_s = sembol_ara(u->kapsam, "bu");
        if (bu_s) {
            int bu_offset = (bu_s->yerel_indeks + 1) * 8;
            yaz(u, "    movq    %%rdi, -%d(%%rbp)", bu_offset);
        }
    }

    if (d->çocuk_sayısı > 0) {
        Düğüm *params = d->çocuklar[0];
        const char *int_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        int int_reg_idx = u->mevcut_sinif ? 1 : 0;  /* metotlarda rdi = bu */
        int xmm_reg_idx = 0;

        for (int i = 0; i < params->çocuk_sayısı; i++) {
            Sembol *s = sembol_ara(u->kapsam, params->çocuklar[i]->veri.değişken.isim);
            if (!s) continue;
            int offset = (s->yerel_indeks + 1) * 8;
            TipTürü p_tip = param_tipleri[i];

            if (p_tip == TİP_ONDALIK) {
                yaz(u, "    movsd   %%xmm%d, -%d(%%rbp)", xmm_reg_idx, offset);
                xmm_reg_idx++;
            } else if (p_tip == TİP_METİN && int_reg_idx + 1 < 6) {
                /* Metin: pointer + length */
                yaz(u, "    movq    %%%s, -%d(%%rbp)", int_regs[int_reg_idx], offset);
                int_reg_idx++;
                yaz(u, "    movq    %%%s, -%d(%%rbp)", int_regs[int_reg_idx], offset + 8);
                int_reg_idx++;
            } else if (int_reg_idx < 6) {
                yaz(u, "    movq    %%%s, -%d(%%rbp)", int_regs[int_reg_idx], offset);
                int_reg_idx++;
            }
        }
    }

    /* Profil modu: giriş kaydı */
    int profil_isim_idx = -1;
    if (u->profil_modu) {
        profil_isim_idx = metin_literal_ekle(u, gercek_isim, (int)strlen(gercek_isim));
        u->profil_mevcut_islev = gercek_isim;
        u->profil_isim_idx = profil_isim_idx;
        yaz(u, "    leaq    .LC%d(%%rip), %%rdi", profil_isim_idx);
        yaz(u, "    movq    $.LC%d_len, %%rsi", profil_isim_idx);
        yaz(u, "    call    _tr_profil_giris");
    }

    /* Kapanış: yakalanan değişkenleri ortamdan yükle */
    if (d->tur == DÜĞÜM_LAMBDA && d->veri.islev.yakalanan_sayisi > 0) {
        int yak_say = d->veri.islev.yakalanan_sayisi;
        char **yak_isimleri = d->veri.islev.yakalanan_isimler;
        /* _tr_kapanis_ortam global pointer'ından oku */
        yaz(u, "    movq    _tr_kapanis_ortam(%%rip), %%r12");
        for (int ci = 0; ci < yak_say; ci++) {
            Sembol *cs = sembol_ekle(u->arena, u->kapsam, yak_isimleri[ci], TİP_TAM);
            cs->yerel_indeks = u->kapsam->yerel_sayac++;
            int dst_off = (cs->yerel_indeks + 1) * 8;
            yaz(u, "    movq    %d(%%r12), %%rax", (ci + 2) * 8);
            yaz(u, "    movq    %%rax, -%d(%%rbp)", dst_off);
        }
    }

    /* Gövde */
    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1]);
    }

    /* Profil modu: çıkış kaydı (varsayılan dönüş) */
    if (u->profil_modu && profil_isim_idx >= 0) {
        yaz(u, "    leaq    .LC%d(%%rip), %%rdi", profil_isim_idx);
        yaz(u, "    movq    $.LC%d_len, %%rsi", profil_isim_idx);
        yaz(u, "    call    _tr_profil_cikis");
    }

    /* Varsayılan dönüş */
    if (donus == TİP_ONDALIK) {
        yaz(u, "    xorpd   %%xmm0, %%xmm0");
    } else {
        yaz(u, "    xorq    %%rax, %%rax");
    }
    yaz(u, "    leave");
    yaz(u, "    ret");

    if (u->profil_modu) {
        u->profil_mevcut_islev = NULL;
    }

    u->kapsam = onceki;
    u->mevcut_islev_donus_tipi = onceki_donus;

    /* Dekoratör wrapper: <isim> çağrıldığında orijinal + dekoratör çağır */
    if (dekorator) {
        yaz(u, "");
        yaz(u, "    .globl  %s", isim);
        yaz(u, "%s:", isim);
        yaz(u, "    pushq   %%rbp");
        yaz(u, "    movq    %%rsp, %%rbp");
        /* Argümanları olduğu gibi aktar (register'lar korunur) */
        yaz(u, "    call    %s", gercek_isim);
        /* Sonucu dekoratör fonksiyonuna geçir: rax -> rdi */
        yaz(u, "    movq    %%rax, %%rdi");
        yaz(u, "    call    %s", dekorator);
        yaz(u, "    leave");
        yaz(u, "    ret");
    }
}

/* Özelleştirilmiş generic fonksiyon üretimi
 * tip_parametre: orijinal tip parametresi adı (örn: "T")
 * somut_tip: somut tip adı (örn: "tam")
 */
static void islev_uret_ozel(Üretici *u, Düğüm *d, const char *ozel_isim,
                            const char *tip_parametre, const char *somut_tip) {
    TipTürü somut_tip_turu = tip_adı_çevir(somut_tip);
    TipTürü donus = d->veri.islev.dönüş_tipi ?
        tip_adı_çevir(d->veri.islev.dönüş_tipi) : TİP_BOŞLUK;

    /* Dönüş tipi T ise somut tipe çevir */
    if (d->veri.islev.dönüş_tipi && tip_parametre &&
        strcmp(d->veri.islev.dönüş_tipi, tip_parametre) == 0) {
        donus = somut_tip_turu;
    }

    /* Fonksiyon sembolünü kaydet */
    Sembol *fn_sem = sembol_ekle(u->arena, u->kapsam, ozel_isim, donus);
    fn_sem->dönüş_tipi = donus;

    /* Parametre tiplerini topla */
    int param_sayisi = 0;
    TipTürü param_tipleri[32] = {0};
    if (d->çocuk_sayısı > 0) {
        Düğüm *params = d->çocuklar[0];
        param_sayisi = params->çocuk_sayısı;
        for (int i = 0; i < param_sayisi && i < 32; i++) {
            char *p_tip_adi = params->çocuklar[i]->veri.değişken.tip;
            /* T -> somut_tip */
            if (p_tip_adi && tip_parametre && strcmp(p_tip_adi, tip_parametre) == 0) {
                param_tipleri[i] = somut_tip_turu;
            } else {
                param_tipleri[i] = tip_adı_çevir(p_tip_adi);
            }
            fn_sem->param_tipleri[i] = param_tipleri[i];
        }
    }
    fn_sem->param_sayisi = param_sayisi;

    /* Mevcut işlev dönüş tipini kaydet */
    TipTürü onceki_donus = u->mevcut_islev_donus_tipi;
    u->mevcut_islev_donus_tipi = donus;

    yaz(u, "");
    yaz(u, "    .globl  %s", ozel_isim);
    yaz(u, "%s:", ozel_isim);
    yaz(u, "    pushq   %%rbp");
    yaz(u, "    movq    %%rsp, %%rbp");

    /* Yeni kapsam */
    Kapsam *onceki = u->kapsam;
    u->kapsam = kapsam_oluştur(u->arena, onceki);
    u->kapsam->yerel_sayac = 0;

    /* Parametreleri tanımla - tip parametresini somut tipe çevir */
    if (d->çocuk_sayısı > 0) {
        Düğüm *params = d->çocuklar[0];
        for (int i = 0; i < params->çocuk_sayısı; i++) {
            Düğüm *p = params->çocuklar[i];
            char *p_tip_adi = p->veri.değişken.tip;
            TipTürü p_tip;
            /* T -> somut_tip */
            if (p_tip_adi && tip_parametre && strcmp(p_tip_adi, tip_parametre) == 0) {
                p_tip = somut_tip_turu;
            } else {
                p_tip = tip_adı_çevir(p_tip_adi);
            }
            Sembol *s = sembol_ekle(u->arena, u->kapsam, p->veri.değişken.isim, p_tip);
            s->parametre_mi = 1;
            s->global_mi = 0;
            /* Metin parametreler 2 slot kullanır */
            if (p_tip == TİP_METİN) {
                u->kapsam->yerel_sayac++;
            }
        }
    }

    /* Yeterli stack alanı ayır */
    int stack_boyut = (u->kapsam->yerel_sayac + 8) * 8;
    stack_boyut = (stack_boyut + 15) & ~15;
    yaz(u, "    subq    $%d, %%rsp", stack_boyut);

    /* Parametreleri stack'e kopyala */
    if (d->çocuk_sayısı > 0) {
        Düğüm *params = d->çocuklar[0];
        const char *int_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        int int_idx = 0;
        int xmm_idx = 0;

        for (int i = 0; i < params->çocuk_sayısı; i++) {
            Düğüm *p = params->çocuklar[i];
            Sembol *s = sembol_ara(u->kapsam, p->veri.değişken.isim);
            if (!s) continue;

            int offset = (s->yerel_indeks + 1) * 8;
            char *p_tip_adi = p->veri.değişken.tip;
            TipTürü p_tip;
            if (p_tip_adi && tip_parametre && strcmp(p_tip_adi, tip_parametre) == 0) {
                p_tip = somut_tip_turu;
            } else {
                p_tip = tip_adı_çevir(p_tip_adi);
            }

            if (p_tip == TİP_ONDALIK) {
                if (xmm_idx < 8) {
                    yaz(u, "    movsd   %%xmm%d, -%d(%%rbp)", xmm_idx, offset);
                    xmm_idx++;
                }
            } else if (p_tip == TİP_METİN) {
                if (int_idx < 6) {
                    yaz(u, "    movq    %%%s, -%d(%%rbp)", int_regs[int_idx], offset);
                    int_idx++;
                }
                if (int_idx < 6) {
                    yaz(u, "    movq    %%%s, -%d(%%rbp)", int_regs[int_idx], offset + 8);
                    int_idx++;
                }
            } else {
                if (int_idx < 6) {
                    yaz(u, "    movq    %%%s, -%d(%%rbp)", int_regs[int_idx], offset);
                    int_idx++;
                }
            }
        }
    }

    /* Gövdeyi üret */
    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1]);
    }

    /* Varsayılan dönüş */
    if (donus == TİP_BOŞLUK) {
        yaz(u, "    leave");
        yaz(u, "    ret");
    } else {
        yaz(u, "    xorq    %%rax, %%rax");
        yaz(u, "    leave");
        yaz(u, "    ret");
    }

    u->mevcut_islev_donus_tipi = onceki_donus;
    u->kapsam = onceki;
}

static void blok_uret(Üretici *u, Düğüm *blok) {
    if (!blok) return;
    for (int i = 0; i < blok->çocuk_sayısı; i++) {
        bildirim_uret(u, blok->çocuklar[i]);
    }
}

static void bildirim_uret(Üretici *u, Düğüm *d) {
    if (!d) return;

    /* Debug modu: satır bilgisi ekle */
    if (u->debug_modu && d->satir > 0) {
        yaz(u, "    .loc 1 %d 0", d->satir);
    }

    switch (d->tur) {
    case DÜĞÜM_BLOK:
        /* Çoklu dönüş bloğu: tam x, tam y = fonk() */
        if (d->çocuk_sayısı == 2 &&
            d->çocuklar[0]->tur == DÜĞÜM_DEĞİŞKEN &&
            d->çocuklar[1]->tur == DÜĞÜM_DEĞİŞKEN &&
            d->çocuklar[1]->veri.değişken.genel == 2) {
            /* İlk değişkeni üret (bu fonksiyon çağrısını yapar) */
            degisken_uret(u, d->çocuklar[0]);
            /* rdx'i stack'e kaydet */
            yaz(u, "    pushq   %%rdx");
            /* İkinci değişkeni üret (rdx'ten alacak) */
            degisken_uret(u, d->çocuklar[1]);
        } else {
            blok_uret(u, d);
        }
        break;
    case DÜĞÜM_DEĞİŞKEN:
        degisken_uret(u, d);
        break;
    case DÜĞÜM_İŞLEV:
        islev_uret(u, d);
        break;
    case DÜĞÜM_SINIF:
        /* Sınıf tanımı: sadece sınıf bilgisini sembol tablosuna kaydet */
        /* Metotlar ayrıca üretilir */
        break;
    case DÜĞÜM_KULLAN:
        /* Modül bildirimi: modül fonksiyonlarını scope'a kaydet */
        if (d->veri.kullan.modul) {
            const ModülTanım *mt = modul_bul(d->veri.kullan.modul);
            if (mt) {
                int yerel_oncesi = u->kapsam->yerel_sayac;
                for (int mi = 0; mi < mt->fonksiyon_sayisi; mi++) {
                    const ModülFonksiyon *mf = &mt->fonksiyonlar[mi];
                    Sembol *ms = sembol_ekle(u->arena, u->kapsam, mf->isim, mf->dönüş_tipi);
                    if (ms) {
                        ms->runtime_isim = mf->runtime_isim;
                        ms->dönüş_tipi = mf->dönüş_tipi;
                        ms->param_sayisi = mf->param_sayisi;
                        for (int mj = 0; mj < mf->param_sayisi && mj < 32; mj++)
                            ms->param_tipleri[mj] = mf->param_tipleri[mj];
                    }
                    if (mf->ascii_isim) {
                        Sembol *ms2 = sembol_ekle(u->arena, u->kapsam, mf->ascii_isim, mf->dönüş_tipi);
                        if (ms2) {
                            ms2->runtime_isim = mf->runtime_isim;
                            ms2->dönüş_tipi = mf->dönüş_tipi;
                            ms2->param_sayisi = mf->param_sayisi;
                            for (int mj = 0; mj < mf->param_sayisi && mj < 32; mj++)
                                ms2->param_tipleri[mj] = mf->param_tipleri[mj];
                        }
                    }
                }
                /* Fonksiyon sembolleri stack slot kullanmaz — yerel_sayac'ı geri al */
                u->kapsam->yerel_sayac = yerel_oncesi;
            }
        }
        break;
    case DÜĞÜM_TEST:
        /* Test blokları kod_üret'te ayrı fonksiyon olarak üretilir */
        break;
    case DÜĞÜM_EĞER:
        eger_uret(u, d);
        break;
    case DÜĞÜM_DÖNGÜ:
        dongu_uret(u, d);
        break;
    case DÜĞÜM_İKEN:
        iken_uret(u, d);
        break;
    case DÜĞÜM_DÖNDÜR:
        if (d->çocuk_sayısı == 1) {
            ifade_üret(u, d->çocuklar[0]);
        } else if (d->çocuk_sayısı >= 2) {
            /* Çoklu dönüş: ilk değer rax, ikinci rdx */
            ifade_üret(u, d->çocuklar[1]);
            yaz(u, "    pushq   %%rax");
            ifade_üret(u, d->çocuklar[0]);
            yaz(u, "    popq    %%rdx");
        }
        /* Profil modu: çıkış kaydı (döndür ile) */
        if (u->profil_modu && u->profil_mevcut_islev) {
            /* rax'ı korumak için pushq */
            yaz(u, "    pushq   %%rax");
            yaz(u, "    leaq    .LC%d(%%rip), %%rdi", u->profil_isim_idx);
            yaz(u, "    movq    $.LC%d_len, %%rsi", u->profil_isim_idx);
            yaz(u, "    call    _tr_profil_cikis");
            yaz(u, "    popq    %%rax");
        }
        yaz(u, "    leave");
        yaz(u, "    ret");
        break;
    case DÜĞÜM_KIR: {
        if (u->her_icin_temizlik) {
            yaz(u, "    addq    $24, %%rsp");
        }
        int hedef_etiket = u->dongu_bitis_etiket;
        if (d->veri.tanimlayici.isim) {
            for (int i = u->dongu_derinligi - 1; i >= 0; i--) {
                if (u->dongu_yigini[i].isim &&
                    strcmp(u->dongu_yigini[i].isim, d->veri.tanimlayici.isim) == 0) {
                    hedef_etiket = u->dongu_yigini[i].bitis_etiket;
                    break;
                }
            }
        }
        yaz(u, "    jmp     .L%d", hedef_etiket);
        break;
    }
    case DÜĞÜM_DEVAM: {
        int hedef_etiket = u->dongu_baslangic_etiket;
        if (d->veri.tanimlayici.isim) {
            for (int i = u->dongu_derinligi - 1; i >= 0; i--) {
                if (u->dongu_yigini[i].isim &&
                    strcmp(u->dongu_yigini[i].isim, d->veri.tanimlayici.isim) == 0) {
                    hedef_etiket = u->dongu_yigini[i].baslangic_etiket;
                    break;
                }
            }
        }
        yaz(u, "    jmp     .L%d", hedef_etiket);
        break;
    }
    case DÜĞÜM_ATAMA:
        atama_uret(u, d);
        break;

    case DÜĞÜM_PAKET_AÇ: {
        /* [a, b, c] = ifade veya x, y = ifade */
        /* Son çocuk = kaynak, diğerleri = hedef değişkenler */
        int hedef_sayisi = d->çocuk_sayısı - 1;
        Düğüm *kaynak = d->çocuklar[hedef_sayisi];

        /* 2 değerli demet unpacking: rax + rdx */
        if (hedef_sayisi == 2 && kaynak->tur == DÜĞÜM_ÇAĞRI) {
            ifade_üret(u, kaynak);
            /* rax = ilk, rdx = ikinci */
            for (int i = 0; i < 2; i++) {
                Düğüm *hedef = d->çocuklar[i];
                if (hedef->tur == DÜĞÜM_TANIMLAYICI) {
                    Sembol *s = sembol_ara(u->kapsam, hedef->veri.tanimlayici.isim);
                    if (!s) {
                        s = sembol_ekle(u->arena, u->kapsam, hedef->veri.tanimlayici.isim, TİP_TAM);
                        s->yerel_indeks = u->kapsam->yerel_sayac++;
                    }
                    if (i == 0) {
                        yaz(u, "    movq    %%rax, -%d(%%rbp)", (s->yerel_indeks + 1) * 8);
                    } else {
                        yaz(u, "    movq    %%rdx, -%d(%%rbp)", (s->yerel_indeks + 1) * 8);
                    }
                }
            }
            break;
        }

        /* Kaynak diziyi değerlendir: rax=ptr, rbx=count */
        ifade_üret(u, kaynak);
        yaz(u, "    pushq   %%rbx");  /* count */
        yaz(u, "    pushq   %%rax");  /* ptr */

        for (int i = 0; i < hedef_sayisi; i++) {
            Düğüm *hedef = d->çocuklar[i];
            if (hedef->tur == DÜĞÜM_TANIMLAYICI) {
                /* Rest pattern: ...değişken */
                if (hedef->veri.tanimlayici.isim[0] == '.' &&
                    hedef->veri.tanimlayici.isim[1] == '.' &&
                    hedef->veri.tanimlayici.isim[2] == '.') {
                    /* Gerçek değişken adı: ...isim → isim (3 karakter atla) */
                    char *rest_isim = hedef->veri.tanimlayici.isim + 3;
                    int sonra = hedef_sayisi - i - 1;  /* rest'ten sonraki eleman sayısı */

                    Sembol *rs = sembol_ara(u->kapsam, rest_isim);
                    if (!rs) {
                        rs = sembol_ekle(u->arena, u->kapsam, rest_isim, TİP_DİZİ);
                        rs->yerel_indeks = u->kapsam->yerel_sayac;
                        u->kapsam->yerel_sayac += 2;  /* ptr + count */
                    }
                    int rs_off = (rs->yerel_indeks + 1) * 8;

                    /* rest_count = total_count - i - sonra */
                    yaz(u, "    movq    8(%%rsp), %%rcx");  /* count */
                    yaz(u, "    subq    $%d, %%rcx", i + sonra);  /* rest_count */
                    /* rest_count < 0 ise 0 yap */
                    yaz(u, "    testq   %%rcx, %%rcx");
                    int rest_ok = yeni_etiket(u);
                    yaz(u, "    jg      .L%d", rest_ok);
                    yaz(u, "    xorq    %%rcx, %%rcx");
                    yaz(u, ".L%d:", rest_ok);
                    yaz(u, "    pushq   %%rcx");  /* rest_count kaydet */

                    /* Yeni dizi oluştur: _tr_nesne_olustur(NESNE_TIP_DIZI, rest_count*8) */
                    yaz(u, "    movq    $1, %%rdi");     /* NESNE_TIP_DIZI */
                    yaz(u, "    leaq    (,%%rcx,8), %%rsi");
                    yaz(u, "    call    _tr_nesne_olustur");

                    /* Elemanları kopyala: kaynak[i..i+rest_count-1] → yeni dizi */
                    yaz(u, "    movq    %%rax, %%r10");  /* yeni dizi ptr */
                    yaz(u, "    movq    8(%%rsp), %%rdx");  /* kaynak ptr (stack: [rest_count][ptr][count]) */
                    yaz(u, "    popq    %%rcx");  /* rest_count */
                    yaz(u, "    xorq    %%r11, %%r11");  /* j = 0 */
                    int kopyala_bas = yeni_etiket(u);
                    int kopyala_bit = yeni_etiket(u);
                    yaz(u, ".L%d:", kopyala_bas);
                    yaz(u, "    cmpq    %%rcx, %%r11");
                    yaz(u, "    jge     .L%d", kopyala_bit);
                    yaz(u, "    movq    %d(%%rdx,%%r11,8), %%rax", i * 8);  /* kaynak[i+j] */
                    yaz(u, "    movq    %%rax, (%%r10,%%r11,8)");  /* yeni[j] */
                    yaz(u, "    incq    %%r11");
                    yaz(u, "    jmp     .L%d", kopyala_bas);
                    yaz(u, ".L%d:", kopyala_bit);

                    /* rest değişkenine ata: ptr=r10, count=rcx */
                    yaz(u, "    movq    %%r10, -%d(%%rbp)", rs_off);
                    yaz(u, "    movq    %%rcx, -%d(%%rbp)", rs_off + 8);

                    /* Sonraki elemanlar: kaynak[total_count - sonra + j] */
                    for (int j = 0; j < sonra; j++) {
                        Düğüm *sonraki = d->çocuklar[i + 1 + j];
                        if (sonraki->tur == DÜĞÜM_TANIMLAYICI) {
                            Sembol *ss = sembol_ara(u->kapsam, sonraki->veri.tanimlayici.isim);
                            if (!ss) {
                                ss = sembol_ekle(u->arena, u->kapsam, sonraki->veri.tanimlayici.isim, TİP_TAM);
                                ss->yerel_indeks = u->kapsam->yerel_sayac++;
                            }
                            /* kaynak[total_count - sonra + j] */
                            yaz(u, "    movq    8(%%rsp), %%rcx");  /* count */
                            yaz(u, "    subq    $%d, %%rcx", sonra - j);
                            yaz(u, "    movq    (%%rsp), %%rdx");   /* ptr */
                            yaz(u, "    movq    (%%rdx,%%rcx,8), %%rax");
                            yaz(u, "    movq    %%rax, -%d(%%rbp)", (ss->yerel_indeks + 1) * 8);
                        }
                    }
                    i = hedef_sayisi;  /* döngüden çık (sonrakileri zaten işledik) */
                } else {
                    Sembol *s = sembol_ara(u->kapsam, hedef->veri.tanimlayici.isim);
                    if (!s) {
                        s = sembol_ekle(u->arena, u->kapsam, hedef->veri.tanimlayici.isim, TİP_TAM);
                        s->yerel_indeks = u->kapsam->yerel_sayac++;
                    }
                    /* dizi[i] → değişken */
                    yaz(u, "    movq    (%%rsp), %%rax");  /* ptr */
                    yaz(u, "    movq    %d(%%rax), %%rax", i * 8);
                    yaz(u, "    movq    %%rax, -%d(%%rbp)", (s->yerel_indeks + 1) * 8);
                }
            }
        }

        yaz(u, "    addq    $16, %%rsp");  /* ptr + count temizle */
        break;
    }

    case DÜĞÜM_DİZİ_ATAMA: {
        /* çocuklar[0]=dizi, çocuklar[1]=indeks, çocuklar[2]=değer */

        /* Operatör yükleme: base sınıf ise indeks_yaz metotu çağır */
        if (d->çocuklar[0]->sonuç_tipi == TİP_SINIF) {
            char *sa = sinif_adi_bul(u, d->çocuklar[0]);
            if (sa) {
                /* değer → stack */
                ifade_üret(u, d->çocuklar[2]);
                yaz(u, "    pushq   %%rax");
                /* index → stack */
                ifade_üret(u, d->çocuklar[1]);
                yaz(u, "    pushq   %%rax");
                /* self → rdi */
                ifade_üret(u, d->çocuklar[0]);
                yaz(u, "    movq    %%rax, %%rdi");
                yaz(u, "    popq    %%rsi");   /* index */
                yaz(u, "    popq    %%rdx");   /* value */
                yaz(u, "    call    %s_indeks_yaz", sa);
                break;
            }
        }

        /* Değeri hesapla ve kaydet */
        ifade_üret(u, d->çocuklar[2]);
        yaz(u, "    pushq   %%rax");  /* değer */
        /* İndeksi hesapla */
        ifade_üret(u, d->çocuklar[1]);
        yaz(u, "    pushq   %%rax");  /* indeks */
        /* Dizi pointer'ını al */
        ifade_üret(u, d->çocuklar[0]);
        /* rax = dizi pointer, rbx = eleman sayısı */

        /* Sınır kontrolü */
        yaz(u, "    popq    %%rcx");  /* indeks */
        yaz(u, "    popq    %%rdx");  /* değer */

        hata_dizi_sinir_uret(u);
        int lbl = yeni_etiket(u);
        yaz(u, "    cmpq    $0, %%rcx");
        yaz(u, "    jl      .Ldsn%d", lbl);
        yaz(u, "    cmpq    %%rbx, %%rcx");
        yaz(u, "    jl      .Ldok%d", lbl);
        yaz(u, ".Ldsn%d:", lbl);
        yaz(u, "    call    _hata_dizi_sinir");
        yaz(u, ".Ldok%d:", lbl);

        yaz(u, "    movq    %%rdx, (%%rax, %%rcx, 8)");
        break;
    }
    case DÜĞÜM_ERİŞİM_ATAMA: {
        /* nesne.alan = değer */
        /* çocuklar[0] = nesne, çocuklar[1] = değer */
        /* veri.tanimlayici.isim = alan adı */
        if (d->çocuk_sayısı >= 2 && d->veri.tanimlayici.isim) {
            /* Önce hedef alan tipini bul (metin mi, tam mı?) */
            TipTürü hedef_tip = TİP_TAM;
            Düğüm *nesne_tmp = d->çocuklar[0];
            char *sinif_adi_tmp = NULL;
            if (nesne_tmp->tur == DÜĞÜM_TANIMLAYICI) {
                Sembol *ns_tmp = sembol_ara(u->kapsam, nesne_tmp->veri.tanimlayici.isim);
                if (ns_tmp && ns_tmp->sınıf_adı) sinif_adi_tmp = ns_tmp->sınıf_adı;
            }
            if (sinif_adi_tmp) {
                SinifBilgi *sb_tmp = sınıf_bul(u->kapsam, sinif_adi_tmp);
                if (sb_tmp) {
                    for (int i = 0; i < sb_tmp->alan_sayisi; i++) {
                        if (strcmp(sb_tmp->alanlar[i].isim, d->veri.tanimlayici.isim) == 0) {
                            hedef_tip = sb_tmp->alanlar[i].tip;
                            break;
                        }
                    }
                }
            }

            /* Değeri hesapla */
            ifade_üret(u, d->çocuklar[1]);
            if (hedef_tip == TİP_METİN || hedef_tip == TİP_DİZİ) {
                yaz(u, "    pushq   %%rbx");  /* len */
                yaz(u, "    pushq   %%rax");  /* ptr */
            } else {
                yaz(u, "    pushq   %%rax");  /* değer */
            }

            /* Nesne pointer'ını al */
            ifade_üret(u, d->çocuklar[0]);
            /* rax = nesne pointer */

            /* Sınıf adını bul */
            Düğüm *nesne = d->çocuklar[0];
            char *sınıf_adı = NULL;
            if (nesne->tur == DÜĞÜM_TANIMLAYICI) {
                Sembol *ns = sembol_ara(u->kapsam, nesne->veri.tanimlayici.isim);
                if (ns && ns->sınıf_adı) sınıf_adı = ns->sınıf_adı;
            }

            if (sınıf_adı) {
                SinifBilgi *sb = sınıf_bul(u->kapsam, sınıf_adı);
                if (sb) {
                    int bulundu = 0;
                    for (int i = 0; i < sb->alan_sayisi; i++) {
                        if (strcmp(sb->alanlar[i].isim, d->veri.tanimlayici.isim) == 0) {
                            TipTürü alan_tipi = sb->alanlar[i].tip;
                            if (sb->alanlar[i].statik) {
                                /* Statik alan: RIP-relative atama */
                                if (alan_tipi == TİP_METİN || alan_tipi == TİP_DİZİ) {
                                    yaz(u, "    popq    %%rcx");   /* ptr */
                                    yaz(u, "    popq    %%r8");    /* len */
                                    yaz(u, "    movq    %%rcx, _%s_%s(%%rip)", sınıf_adı, d->veri.tanimlayici.isim);
                                    yaz(u, "    movq    %%r8, _%s_%s+8(%%rip)", sınıf_adı, d->veri.tanimlayici.isim);
                                } else {
                                    yaz(u, "    popq    %%rcx");  /* değer */
                                    yaz(u, "    movq    %%rcx, _%s_%s(%%rip)", sınıf_adı, d->veri.tanimlayici.isim);
                                }
                            } else {
                                int offset = sb->alanlar[i].offset;
                                if (alan_tipi == TİP_METİN || alan_tipi == TİP_DİZİ) {
                                    yaz(u, "    popq    %%rcx");   /* ptr */
                                    yaz(u, "    popq    %%r8");    /* len */
                                    yaz(u, "    movq    %%rcx, %d(%%rax)", offset);       /* ptr */
                                    yaz(u, "    movq    %%r8, %d(%%rax)", offset + 8);    /* len */
                                } else {
                                    yaz(u, "    popq    %%rcx");  /* değer */
                                    yaz(u, "    movq    %%rcx, %d(%%rax)", offset);
                                }
                            }
                            bulundu = 1;
                            break;
                        }
                    }
                    /* Setter: alan bulunamadıysa koy_<alan> metodunu dene */
                    if (!bulundu) {
                        char setter_adi[256];
                        snprintf(setter_adi, sizeof(setter_adi), "koy_%s", d->veri.tanimlayici.isim);
                        for (int i = 0; i < sb->metot_sayisi; i++) {
                            if (sb->metot_isimleri[i] && strcmp(sb->metot_isimleri[i], setter_adi) == 0) {
                                /* rax = nesne pointer (bu), stack'te değer */
                                yaz(u, "    movq    %%rax, %%rdi");  /* bu */
                                yaz(u, "    popq    %%rsi");         /* değer */
                                yaz(u, "    call    %s_%s", sınıf_adı, setter_adi);
                                bulundu = 1;
                                break;
                            }
                        }
                    }
                    /* @ozellik_ata dekoratörlü setter */
                    if (!bulundu) {
                        for (int i = 0; i < sb->ozellik_setter_sayisi; i++) {
                            if (sb->ozellik_setters[i] && strcmp(sb->ozellik_setters[i], d->veri.tanimlayici.isim) == 0) {
                                yaz(u, "    movq    %%rax, %%rdi");
                                yaz(u, "    popq    %%rsi");
                                yaz(u, "    call    %s_%s", sınıf_adı, d->veri.tanimlayici.isim);
                                bulundu = 1;
                                break;
                            }
                        }
                    }
                    if (!bulundu) {
                        yaz(u, "    popq    %%rcx");  /* değeri temizle */
                    }
                }
            } else {
                yaz(u, "    popq    %%rcx");  /* değeri temizle */
            }
        }
        break;
    }
    case DÜĞÜM_İFADE_BİLDİRİMİ:
        if (d->çocuk_sayısı > 0) {
            Düğüm *ifade = d->çocuklar[0];
            if (ifade->tur == DÜĞÜM_ÇAĞRI) {
                cagri_uret(u, ifade);
            } else if (ifade->tur == DÜĞÜM_ATAMA) {
                atama_uret(u, ifade);
            } else if (ifade->tur == DÜĞÜM_DİZİ_ATAMA) {
                bildirim_uret(u, ifade);
            } else if (ifade->tur == DÜĞÜM_ERİŞİM_ATAMA) {
                bildirim_uret(u, ifade);
            } else if (ifade->tur == DÜĞÜM_PAKET_AÇ) {
                bildirim_uret(u, ifade);
            } else {
                ifade_üret(u, ifade);
            }
        }
        break;
    case DÜĞÜM_EŞLE: {
        /* eşle/durum/varsayılan */
        int son_etiket = yeni_etiket(u);

        /* Eşlenen ifadeyi hesapla ve stack'e at */
        ifade_üret(u, d->çocuklar[0]);
        yaz(u, "    pushq   %%rax");

        /* Durum blokları: çocuklar[1]=deger1, çocuklar[2]=blok1, çocuklar[3]=deger2, çocuklar[4]=blok2, ... */
        int i = 1;
        while (i + 1 < d->çocuk_sayısı) {
            if (d->çocuklar[i]->tur == DÜĞÜM_BLOK) {
                /* Bu varsayılan blok (tek kalan son çocuk) */
                break;
            }
            int sonraki_etiket = yeni_etiket(u);

            if (d->çocuklar[i]->tur == DÜĞÜM_ARALIK) {
                /* Aralık deseni: alt..üst -> val >= alt && val <= üst */
                Düğüm *aralik = d->çocuklar[i];
                /* Alt sınır */
                ifade_üret(u, aralik->çocuklar[0]);
                yaz(u, "    movq    %%rax, %%rcx");  /* alt -> rcx */
                /* Üst sınır */
                ifade_üret(u, aralik->çocuklar[1]);
                yaz(u, "    movq    %%rax, %%rdx");  /* üst -> rdx */
                /* val >= alt? */
                yaz(u, "    movq    (%%rsp), %%rax");
                yaz(u, "    cmpq    %%rcx, %%rax");
                yaz(u, "    jl      .L%d", sonraki_etiket);
                /* val <= üst? */
                yaz(u, "    cmpq    %%rdx, %%rax");
                yaz(u, "    jg      .L%d", sonraki_etiket);
            } else {
                /* Durum değerini hesapla */
                ifade_üret(u, d->çocuklar[i]);
                /* Eşlenen değerle karşılaştır */
                yaz(u, "    cmpq    (%%rsp), %%rax");
                yaz(u, "    jne     .L%d", sonraki_etiket);
            }

            /* Eşleşen blok */
            blok_uret(u, d->çocuklar[i + 1]);
            yaz(u, "    jmp     .L%d", son_etiket);
            yaz(u, ".L%d:", sonraki_etiket);
            i += 2;
        }

        /* Varsayılan blok (opsiyonel) - kalan son çocuk eğer BLOK ise */
        if (i < d->çocuk_sayısı && d->çocuklar[i]->tur == DÜĞÜM_BLOK) {
            blok_uret(u, d->çocuklar[i]);
        }

        /* Stack temizle */
        yaz(u, ".L%d:", son_etiket);
        yaz(u, "    addq    $8, %%rsp");
        break;
    }
    case DÜĞÜM_İLE_İSE: {
        /* ile ifade olarak d ise ... son */
        /* çocuklar[0]=kaynak, çocuklar[1]=gövde */

        /* Kaynak ifadeyi değerlendir */
        ifade_üret(u, d->çocuklar[0]);

        /* Yeni kapsam oluştur */
        Kapsam *onceki_ile = u->kapsam;
        u->kapsam = kapsam_oluştur(u->arena, onceki_ile);

        /* Değişken varsa ata */
        if (d->veri.tanimlayici.isim) {
            Sembol *s = sembol_ekle(u->arena, u->kapsam, d->veri.tanimlayici.isim, TİP_TAM);
            s->yerel_indeks = u->kapsam->yerel_sayac++;
            yaz(u, "    movq    %%rax, -%d(%%rbp)", (s->yerel_indeks + 1) * 8);
        }

        /* Gövdeyi çalıştır */
        if (d->çocuk_sayısı > 1) blok_uret(u, d->çocuklar[1]);

        u->kapsam = onceki_ile;
        break;
    }

    case DÜĞÜM_HER_İÇİN: {
        /* her eleman için dizi ise ... [yoksa ...] son */
        /* Dizi ifadesini hesapla: rax=ptr, rbx=count */
        ifade_üret(u, d->çocuklar[0]);

        /* Stack düzeni: [count] [ptr] [index=0] <- rsp */
        yaz(u, "    pushq   %%rbx");          /* count */
        yaz(u, "    pushq   %%rax");          /* ptr */
        yaz(u, "    pushq   $0");             /* index = 0 */

        int başlangıç = yeni_etiket(u);
        int bitis = yeni_etiket(u);
        int yoksa_var = (d->çocuk_sayısı > 2);
        int bitis_kir = yoksa_var ? yeni_etiket(u) : bitis;
        int son_etiket = yoksa_var ? yeni_etiket(u) : bitis;

        int onceki_baslangic = u->dongu_baslangic_etiket;
        int onceki_bitis = u->dongu_bitis_etiket;
        int onceki_yoksa = u->dongu_yoksa_etiket;
        u->dongu_baslangic_etiket = başlangıç;
        u->dongu_bitis_etiket = yoksa_var ? bitis_kir : bitis;
        u->dongu_yoksa_etiket = yoksa_var ? bitis_kir : 0;
        u->her_icin_temizlik = 1;

        yaz(u, ".L%d:", başlangıç);
        /* index < count kontrol */
        yaz(u, "    movq    (%%rsp), %%rcx");          /* index */
        yaz(u, "    cmpq    16(%%rsp), %%rcx");        /* count ile karşılaştır */
        yaz(u, "    jge     .L%d", bitis);

        /* array[index] -> eleman değişkeni */
        yaz(u, "    movq    8(%%rsp), %%rdx");         /* ptr */
        yaz(u, "    movq    (%%rdx,%%rcx,8), %%rax");  /* array[index] */

        /* Eleman değişkenini yerel olarak ata */
        /* Yeni kapsam: döngü değişkeni ilk yerel */
        Kapsam *onceki_kapsam = u->kapsam;
        u->kapsam = kapsam_oluştur(u->arena, onceki_kapsam);
        Sembol *elem = sembol_ekle(u->arena, u->kapsam, d->veri.dongu.isim, TİP_TAM);
        elem->yerel_indeks = u->kapsam->yerel_sayac++;

        yaz(u, "    movq    %%rax, -%d(%%rbp)", (elem->yerel_indeks + 1) * 8);

        /* Gövde */
        if (d->çocuk_sayısı > 1) blok_uret(u, d->çocuklar[1]);

        /* index++ */
        yaz(u, "    incq    (%%rsp)");
        yaz(u, "    jmp     .L%d", başlangıç);

        yaz(u, ".L%d:", bitis);
        /* Stack temizle (24 byte = 3 pushq) */
        yaz(u, "    addq    $24, %%rsp");

        /* Yoksa bloğu (kır olmadan tamamlandı) */
        if (yoksa_var) {
            blok_uret(u, d->çocuklar[2]);
            yaz(u, "    jmp     .L%d", son_etiket);
            /* kır hedefi: yoksa bloğunu atla (stack zaten kır tarafından temizlendi) */
            yaz(u, ".L%d:", bitis_kir);
            yaz(u, ".L%d:", son_etiket);
        }

        u->kapsam = onceki_kapsam;
        u->dongu_baslangic_etiket = onceki_baslangic;
        u->dongu_bitis_etiket = onceki_bitis;
        u->dongu_yoksa_etiket = onceki_yoksa;
        u->her_icin_temizlik = 0;
        break;
    }
    case DÜĞÜM_DENE_YAKALA: {
        /* dene ... yakala [TipAdi] [hata] ... [sonunda ...] son */
        /* İstisna çerçevesi: 56 byte [rbp, rsp, catch_label, aktif, deger_ptr, deger_len, tip_kodu] */
        if (!u->istisna_bss_uretildi) {
            bss_yaz(u, "    .comm   _istisna_cerceve, 56, 8");
            u->istisna_bss_uretildi = 1;
        }
        int yakala_etiket = yeni_etiket(u);
        int son_etiket = yeni_etiket(u);

        int sonunda_var = (d->veri.tanimlayici.tip &&
                           strcmp(d->veri.tanimlayici.tip, "sonunda") == 0);
        int yakala_son = sonunda_var ? d->çocuk_sayısı - 1 : d->çocuk_sayısı;
        int sonunda_idx = sonunda_var ? d->çocuk_sayısı - 1 : -1;

        /* Önceki çerçeveyi stack'e kaydet (7 qword) */
        yaz(u, "    pushq   _istisna_cerceve(%%rip)");
        yaz(u, "    pushq   _istisna_cerceve+8(%%rip)");
        yaz(u, "    pushq   _istisna_cerceve+16(%%rip)");
        yaz(u, "    pushq   _istisna_cerceve+24(%%rip)");
        yaz(u, "    pushq   _istisna_cerceve+32(%%rip)");
        yaz(u, "    pushq   _istisna_cerceve+40(%%rip)");
        yaz(u, "    pushq   _istisna_cerceve+48(%%rip)");

        /* Yeni çerçeveyi kur */
        yaz(u, "    movq    %%rbp, _istisna_cerceve(%%rip)");
        yaz(u, "    movq    %%rsp, _istisna_cerceve+8(%%rip)");
        yaz(u, "    leaq    .L%d(%%rip), %%rax", yakala_etiket);
        yaz(u, "    movq    %%rax, _istisna_cerceve+16(%%rip)");
        yaz(u, "    movq    $1, _istisna_cerceve+24(%%rip)");  /* aktif flag */
        yaz(u, "    movq    $0, _istisna_cerceve+48(%%rip)");  /* tip kodu sıfırla */

        /* Dene bloğu */
        if (d->çocuk_sayısı > 0) blok_uret(u, d->çocuklar[0]);

        /* Normal çıkış: çerçeveyi geri yükle, yakala bloğunu atla */
        yaz(u, "    popq    _istisna_cerceve+48(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+40(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+32(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+24(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+16(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+8(%%rip)");
        yaz(u, "    popq    _istisna_cerceve(%%rip)");
        /* sonunda bloğu (normal çıkış yolu) */
        if (sonunda_idx >= 0) blok_uret(u, d->çocuklar[sonunda_idx]);
        yaz(u, "    jmp     .L%d", son_etiket);

        /* Yakala giriş noktası */
        yaz(u, ".L%d:", yakala_etiket);
        /* İstisna değerini al: rax=ptr, rbx=len, rcx=tip_kodu */
        yaz(u, "    movq    _istisna_cerceve+32(%%rip), %%rax");
        yaz(u, "    movq    _istisna_cerceve+40(%%rip), %%rbx");
        yaz(u, "    movq    _istisna_cerceve+48(%%rip), %%rcx");
        /* Önceki çerçeveyi geri yükle (stack'ten) */
        yaz(u, "    popq    _istisna_cerceve+48(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+40(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+32(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+24(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+16(%%rip)");
        yaz(u, "    popq    _istisna_cerceve+8(%%rip)");
        yaz(u, "    popq    _istisna_cerceve(%%rip)");

        /* rax=hata_ptr, rbx=hata_len, rcx=tip_kodu — stack'e kaydet */
        yaz(u, "    pushq   %%rcx");  /* tip kodu */
        yaz(u, "    pushq   %%rbx");  /* hata len */
        yaz(u, "    pushq   %%rax");  /* hata ptr */

        /* Çoklu yakala dispatch */
        for (int i = 1; i < yakala_son; i++) {
            Düğüm *yb = d->çocuklar[i];
            char *tip_adı = yb->veri.tanimlayici.tip;
            char *hata_isim = yb->veri.tanimlayici.isim;
            int sonraki_yakala = yeni_etiket(u);

            /* Tip kontrolü (tip_adı NULL ise hepsini yakala) */
            if (tip_adı) {
                int tip_kodu = 0;
                if (strcmp(tip_adı, "TipHatasi") == 0) tip_kodu = 1;
                else if (strcmp(tip_adı, "DegerHatasi") == 0) tip_kodu = 2;
                else if (strcmp(tip_adı, "DizinHatasi") == 0) tip_kodu = 3;
                else if (strcmp(tip_adı, "AnahtarHatasi") == 0) tip_kodu = 4;
                else if (strcmp(tip_adı, "BolmeHatasi") == 0) tip_kodu = 5;
                else if (strcmp(tip_adı, "DosyaHatasi") == 0) tip_kodu = 6;
                else if (strcmp(tip_adı, "BellekHatasi") == 0) tip_kodu = 7;
                else if (strcmp(tip_adı, "Hata") == 0) tip_kodu = -1; /* catch all */

                if (tip_kodu >= 0) {
                    yaz(u, "    cmpq    $%d, 16(%%rsp)", tip_kodu);
                    yaz(u, "    jne     .L%d", sonraki_yakala);
                }
                /* tip_kodu == -1 (Hata) → hepsini yakala, kontrol yok */
            }

            /* Hata değişkeni bağlama */
            if (hata_isim) {
                Kapsam *onceki_kapsam = u->kapsam;
                u->kapsam = kapsam_oluştur(u->arena, onceki_kapsam);
                Sembol *hata_s = sembol_ekle(u->arena, u->kapsam, hata_isim, TİP_METİN);
                hata_s->yerel_indeks = u->kapsam->yerel_sayac;
                u->kapsam->yerel_sayac += 2;  /* ptr + len */
                int offset = (hata_s->yerel_indeks + 1) * 8;
                yaz(u, "    movq    (%%rsp), %%rax");
                yaz(u, "    movq    8(%%rsp), %%rbx");
                yaz(u, "    movq    %%rax, -%d(%%rbp)", offset);
                yaz(u, "    movq    %%rbx, -%d(%%rbp)", offset + 8);

                blok_uret(u, yb);
                u->kapsam = onceki_kapsam;
            } else {
                blok_uret(u, yb);
            }
            /* Stack temizle ve sonunda'ya git */
            yaz(u, "    addq    $24, %%rsp");
            if (sonunda_idx >= 0) blok_uret(u, d->çocuklar[sonunda_idx]);
            yaz(u, "    jmp     .L%d", son_etiket);

            yaz(u, ".L%d:", sonraki_yakala);
        }

        /* Hiçbir yakala eşleşmezse: stack temizle, sonunda çalıştır */
        yaz(u, "    addq    $24, %%rsp");
        if (sonunda_idx >= 0) blok_uret(u, d->çocuklar[sonunda_idx]);

        yaz(u, ".L%d:", son_etiket);
        break;
    }

    case DÜĞÜM_FIRLAT: {
        /* fırlat ifade */
        if (!u->istisna_bss_uretildi) {
            bss_yaz(u, "    .comm   _istisna_cerceve, 56, 8");
            u->istisna_bss_uretildi = 1;
        }
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
        }
        /* rax = fırlatılan değer (ptr), rbx = len (metin ise) */
        /* İstisna değerini çerçeveye kaydet */
        yaz(u, "    movq    %%rax, _istisna_cerceve+32(%%rip)");
        yaz(u, "    movq    %%rbx, _istisna_cerceve+40(%%rip)");
        yaz(u, "    movq    $0, _istisna_cerceve+48(%%rip)");  /* tip: genel Hata */
        /* Çerçeveden rbp/rsp'yi geri yükle, catch label'a zıpla */
        yaz(u, "    movq    _istisna_cerceve(%%rip), %%rbp");
        yaz(u, "    movq    _istisna_cerceve+8(%%rip), %%rsp");
        yaz(u, "    jmpq    *_istisna_cerceve+16(%%rip)");
        break;
    }

    case DÜĞÜM_SAYIM: {
        /* Sayım (enum) tanımı: her değeri .data bölümünde tam sayı olarak sakla */
        for (int i = 0; i < d->çocuk_sayısı; i++) {
            Düğüm *deger = d->çocuklar[i];
            if (deger->veri.tanimlayici.isim) {
                /* Global sabit olarak BSS'te sakla */
                bss_yaz(u, "    .comm   _genel_%s, 8, 8", deger->veri.tanimlayici.isim);
                /* Değerini ata (i = sıra numarası) */
                yaz(u, "    movq    $%d, _genel_%s(%%rip)", i, deger->veri.tanimlayici.isim);
                /* Sembol tablosuna ekle */
                Sembol *s = sembol_ekle(u->arena, u->kapsam, deger->veri.tanimlayici.isim, TİP_TAM);
                if (s) { s->global_mi = 1; s->sabit_mi = 1; }
            }
        }
        break;
    }

    case DÜĞÜM_ARAYÜZ:
        /* Arayüzler derleme zamanı yapısıdır, kod üretimi gerekmez */
        break;

    case DÜĞÜM_TİP_TANIMI:
        /* Tip takma adı: derleme zamanı yapısı, kod üretimi gerekmez */
        break;

    case DÜĞÜM_ÜRET:
        /* Üreteç yield: basitleştirilmiş - sadece değeri döndür */
        if (d->çocuk_sayısı > 0) {
            ifade_üret(u, d->çocuklar[0]);
        }
        /* Değeri döndür (basit üreteç simülasyonu) */
        yaz(u, "    leave");
        yaz(u, "    ret");
        break;

    default:
        break;
    }
}

/* ---- Ana üretim ---- */

void kod_üret(Üretici *u, Düğüm *program, Arena *arena) {
    u->arena = arena;
    u->etiket_sayac = 0;
    u->metin_sayac = 0;
    u->dongu_baslangic_etiket = 0;
    u->dongu_bitis_etiket = 0;
    u->mevcut_islev_donus_tipi = TİP_BOŞLUK;
    u->mevcut_sinif = NULL;
    u->yazdir_tam_uretildi = 0;
    u->yazdir_metin_uretildi = 0;
    u->yazdir_ondalik_uretildi = 0;
    u->ondalik_sayac = 0;
    u->hata_bolme_sifir_uretildi = 0;
    u->hata_dizi_sinir_uretildi = 0;
    u->hata_bellek_uretildi = 0;
    u->istisna_bss_uretildi = 0;
    /* NOT: generic_ozellestirilmisler ve generic_ozellestirme_sayisi
     * ana.c'de ayarlanıyor, burada sıfırlamıyoruz */
    metin_baslat(&u->cikti);
    metin_baslat(&u->veri_bolumu);
    metin_baslat(&u->bss_bolumu);
    metin_baslat(&u->yardimcilar);
    u->kapsam = kapsam_oluştur(arena, NULL);

    /* İlk geçiş: fonksiyon tanımlarını topla */
    /* (şimdilik atlıyoruz, tek geçiş yapıyoruz) */

    /* Fonksiyonları bul ve ayrı üret */
    Metin fonksiyonlar;
    metin_baslat(&fonksiyonlar);

    /* Üst-düzey kodları main'de topla */
    yaz(u, ".section .text");

    /* Debug modu: DWARF dosya bilgisi */
    if (u->debug_modu && u->kaynak_dosya) {
        yaz(u, "    .file 1 \"%s\"", u->kaynak_dosya);
    }

    yaz(u, "    .globl  main");
    yaz(u, "main:");
    yaz(u, "    pushq   %%rbp");
    yaz(u, "    movq    %%rsp, %%rbp");

    /* argc/argv'yi global değişkenlere kaydet (argüman modülü için) */
    yaz(u, "    movl    %%edi, _tr_argc(%%rip)");
    yaz(u, "    movq    %%rsi, _tr_argv(%%rip)");

    /* Stack boyutu gövde üretildikten sonra hesaplanacak */
    Metin _govde_buf;
    metin_baslat(&_govde_buf);
    Metin _ana_cikti = u->cikti;
    u->cikti = _govde_buf;

    /* Profil modu: atexit ile rapor fonksiyonunu kaydet */
    if (u->profil_modu) {
        yaz(u, "    leaq    _tr_profil_rapor(%%rip), %%rdi");
        yaz(u, "    call    atexit");
    }

    /* Birinci geçiş: sınıf tanımlarını kaydet */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur == DÜĞÜM_SINIF) {
            /* Sınıf bilgisini sembol tablosuna kaydet */
            SinifBilgi *sb = (SinifBilgi *)arena_ayir(arena, sizeof(SinifBilgi));
            sb->isim = d->veri.sinif.isim;
            sb->alan_sayisi = 0;
            sb->metot_sayisi = 0;
            sb->ozellik_getter_sayisi = 0;
            sb->ozellik_setter_sayisi = 0;
            sb->boyut = 0;

            /* Kalıtım: ebeveyn alanlarını kopyala */
            int mevcut_offset = 0;
            if (d->veri.sinif.ebeveyn) {
                SinifBilgi *ebeveyn = sınıf_bul(u->kapsam, d->veri.sinif.ebeveyn);
                if (ebeveyn) {
                    for (int j = 0; j < ebeveyn->alan_sayisi && sb->alan_sayisi < 64; j++) {
                        sb->alanlar[sb->alan_sayisi] = ebeveyn->alanlar[j];
                        sb->alan_sayisi++;
                    }
                    for (int j = 0; j < ebeveyn->metot_sayisi && sb->metot_sayisi < 64; j++) {
                        sb->metot_isimleri[sb->metot_sayisi] = ebeveyn->metot_isimleri[j];
                        sb->metot_sayisi++;
                    }
                    /* Ebeveyn boyutundan devam et */
                    mevcut_offset = ebeveyn->boyut;
                }
            }
            for (int j = 0; j < d->çocuk_sayısı; j++) {
                Düğüm *cocuk = d->çocuklar[j];
                if (cocuk->tur == DÜĞÜM_DEĞİŞKEN && sb->alan_sayisi < 64) {
                    sb->alanlar[sb->alan_sayisi].isim = cocuk->veri.değişken.isim;
                    TipTürü alan_tipi = tip_adı_çevir(cocuk->veri.değişken.tip);
                    sb->alanlar[sb->alan_sayisi].tip = alan_tipi;
                    sb->alanlar[sb->alan_sayisi].statik = cocuk->veri.değişken.statik;
                    sb->alanlar[sb->alan_sayisi].erisim = cocuk->veri.değişken.erisim;
                    if (cocuk->veri.değişken.statik) {
                        /* Statik alan: offset -1 işareti, global BSS'te saklanır */
                        sb->alanlar[sb->alan_sayisi].offset = -1;
                        /* BSS girişi oluştur: _SinifAdi_alan */
                        int alan_boyut = (alan_tipi == TİP_METİN || alan_tipi == TİP_DİZİ) ? 16 : 8;
                        bss_yaz(u, "    .comm   _%s_%s, %d, 8",
                                d->veri.sinif.isim, cocuk->veri.değişken.isim, alan_boyut);
                    } else {
                        sb->alanlar[sb->alan_sayisi].offset = mevcut_offset;
                        /* metin ve dizi 16 byte (ptr + len), diğerleri 8 byte */
                        int alan_boyut = (alan_tipi == TİP_METİN || alan_tipi == TİP_DİZİ) ? 16 : 8;
                        mevcut_offset += alan_boyut;
                    }
                    sb->alan_sayisi++;
                } else if (cocuk->tur == DÜĞÜM_İŞLEV && sb->metot_sayisi < 64) {
                    sb->metot_isimleri[sb->metot_sayisi] = cocuk->veri.islev.isim;
                    sb->metot_sayisi++;
                    /* Dekoratöre göre özellik getter/setter kaydet */
                    if (cocuk->veri.islev.dekorator) {
                        if (strcmp(cocuk->veri.islev.dekorator, "özellik") == 0 ||
                            strcmp(cocuk->veri.islev.dekorator, "\xc3\xb6zellik") == 0) {
                            if (sb->ozellik_getter_sayisi < 64)
                                sb->ozellik_getters[sb->ozellik_getter_sayisi++] = cocuk->veri.islev.isim;
                        } else if (strcmp(cocuk->veri.islev.dekorator, "ozellik_ata") == 0 ||
                                   strcmp(cocuk->veri.islev.dekorator, "\xc3\xb6zellik_ata") == 0) {
                            if (sb->ozellik_setter_sayisi < 64)
                                sb->ozellik_setters[sb->ozellik_setter_sayisi++] = cocuk->veri.islev.isim;
                        }
                    }
                }
            }
            sb->boyut = mevcut_offset;

            Sembol *s = sembol_ekle(u->arena, u->kapsam, d->veri.sinif.isim, TİP_SINIF);
            if (s) s->sınıf_bilgi = sb;
        }
    }

    /* İkinci geçiş: fonksiyon sembollerini ön-kaydet (ileri çağrı desteği) */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur == DÜĞÜM_İŞLEV) {
            TipTürü donus = d->veri.islev.dönüş_tipi ?
                tip_adı_çevir(d->veri.islev.dönüş_tipi) : TİP_BOŞLUK;
            Sembol *fn_sem = sembol_ekle(u->arena, u->kapsam, d->veri.islev.isim, donus);
            if (fn_sem) {
                fn_sem->dönüş_tipi = donus;
                if (d->çocuk_sayısı > 0) {
                    Düğüm *params = d->çocuklar[0];
                    fn_sem->param_sayisi = params->çocuk_sayısı;
                    int varsayilan = 0;
                    for (int j = 0; j < params->çocuk_sayısı && j < 32; j++) {
                        fn_sem->param_tipleri[j] = tip_adı_çevir(params->çocuklar[j]->veri.değişken.tip);
                        if (params->çocuklar[j]->çocuk_sayısı > 0) {
                            fn_sem->varsayilan_dugumler[j] = params->çocuklar[j]->çocuklar[0];
                            varsayilan++;
                        }
                    }
                    fn_sem->varsayilan_sayisi = varsayilan;
                }
            }
        }
    }

    /* Test modu: test bloklarını topla */
    int test_indeksler[256];
    int test_toplam = 0;

    /* Üst-düzey bildirimleri işle */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur == DÜĞÜM_İŞLEV) {
            /* Fonksiyonları ayrı kaydet, sonra ekle */
            Metin onceki = u->cikti;
            metin_baslat(&u->cikti);

            Kapsam *onceki_kapsam = u->kapsam;
            islev_uret(u, d);
            u->kapsam = onceki_kapsam;

            metin_ekle(&fonksiyonlar, u->cikti.veri);
            metin_serbest(&u->cikti);
            u->cikti = onceki;
        } else if (d->tur == DÜĞÜM_SINIF) {
            /* Sınıf metotlarını üret */
            char *sınıf_adı = d->veri.sinif.isim;

            /* Çocuk sınıfın kendi metot isimlerini topla (override tespiti) */
            char *kendi_metotlari[64];
            int kendi_metot_sayisi = 0;
            for (int j = 0; j < d->çocuk_sayısı; j++) {
                if (d->çocuklar[j]->tur == DÜĞÜM_İŞLEV && kendi_metot_sayisi < 64) {
                    kendi_metotlari[kendi_metot_sayisi++] = d->çocuklar[j]->veri.islev.isim;
                }
            }

            for (int j = 0; j < d->çocuk_sayısı; j++) {
                Düğüm *cocuk = d->çocuklar[j];
                if (cocuk->tur == DÜĞÜM_İŞLEV) {
                    /* Metot ismini mangle et: SinifAdi_metotAdi */
                    char *orijinal_isim = cocuk->veri.islev.isim;
                    char mangled[256];
                    snprintf(mangled, sizeof(mangled), "%s_%s", sınıf_adı, orijinal_isim);
                    cocuk->veri.islev.isim = arena_strdup(arena, mangled);

                    /* Metodu üret (ayrı buffer'a) */
                    Metin onceki = u->cikti;
                    metin_baslat(&u->cikti);

                    Kapsam *onceki_kapsam = u->kapsam;

                    /* islev_uret'e sınıf bilgisini aktar */
                    u->mevcut_sinif = sınıf_adı;
                    islev_uret(u, cocuk);
                    u->mevcut_sinif = NULL;
                    u->kapsam = onceki_kapsam;

                    metin_ekle(&fonksiyonlar, u->cikti.veri);
                    metin_serbest(&u->cikti);
                    u->cikti = onceki;

                    /* İsmi geri al */
                    cocuk->veri.islev.isim = orijinal_isim;
                }
            }

            /* Kalıtım: ebeveyn metotları için trampoline üret */
            if (d->veri.sinif.ebeveyn) {
                SinifBilgi *ebeveyn_sb = sınıf_bul(u->kapsam, d->veri.sinif.ebeveyn);
                if (ebeveyn_sb) {
                    for (int j = 0; j < ebeveyn_sb->metot_sayisi; j++) {
                        char *metot = ebeveyn_sb->metot_isimleri[j];
                        /* Override kontrolü: çocuk kendi tanımlamışsa atla */
                        int override = 0;
                        for (int k = 0; k < kendi_metot_sayisi; k++) {
                            if (strcmp(kendi_metotlari[k], metot) == 0) {
                                override = 1;
                                break;
                            }
                        }
                        if (!override) {
                            char buf[512];
                            snprintf(buf, sizeof(buf),
                                "\n    .globl  %s_%s\n%s_%s:\n    jmp     %s_%s\n",
                                sınıf_adı, metot, sınıf_adı, metot,
                                d->veri.sinif.ebeveyn, metot);
                            metin_ekle(&fonksiyonlar, buf);
                        }
                    }
                }
            }
        } else if (d->tur == DÜĞÜM_TEST) {
            /* Test bloğunu topla, sonra ayrı fonksiyon olarak üret */
            if (test_toplam < 256) {
                test_indeksler[test_toplam++] = i;
            }
        } else {
            bildirim_uret(u, d);
        }
    }

    /* Test modu: test fonksiyonlarını çağır */
    if (u->test_modu && test_toplam > 0) {
        /* BSS: _test_isim_ptr, _test_isim_len */
        bss_yaz(u, "    .comm   _test_isim_ptr, 8, 8");
        bss_yaz(u, "    .comm   _test_isim_len, 8, 8");

        for (int t = 0; t < test_toplam; t++) {
            yaz(u, "    call    _test_%d", t);
        }
        /* Rapor */
        yaz(u, "    call    _tr_test_rapor");
    }

    /* Çıkış: return 0 */
    yaz(u, "    xorq    %%rax, %%rax");
    yaz(u, "    leave");
    yaz(u, "    ret");

    /* Gövde tamamlandı: gerçek stack boyutunu hesapla ve birleştir */
    {
        Metin _govde_tamamlandi = u->cikti;
        u->cikti = _ana_cikti;
        int stack_boyut = (u->kapsam->yerel_sayac + 8) * 8;
        stack_boyut = (stack_boyut + 15) & ~15;
        if (stack_boyut < 256) stack_boyut = 256;
        yaz(u, "    subq    $%d, %%rsp", stack_boyut);
        if (_govde_tamamlandi.veri) metin_ekle(&u->cikti, _govde_tamamlandi.veri);
        metin_serbest(&_govde_tamamlandi);
    }

    /* Test fonksiyonlarını üret */
    if (u->test_modu && test_toplam > 0) {
        for (int t = 0; t < test_toplam; t++) {
            Düğüm *td = program->çocuklar[test_indeksler[t]];
            char *test_isim = td->veri.test.isim;
            int isim_len = (int)strlen(test_isim);

            /* Test ismini .rodata'ya yaz */
            int str_etiket = u->metin_sayac++;
            veri_yaz(u, "_test_str_%d:", str_etiket);
            {
                Metin tmp;
                metin_baslat(&tmp);
                metin_ekle(&tmp, "    .byte   ");
                for (int b = 0; b < isim_len; b++) {
                    char num[8];
                    if (b > 0) metin_ekle(&tmp, ",");
                    snprintf(num, sizeof(num), "%d", (unsigned char)test_isim[b]);
                    metin_ekle(&tmp, num);
                }
                veri_yaz(u, "%s", tmp.veri ? tmp.veri : "    .byte 0");
                metin_serbest(&tmp);
            }

            /* Test fonksiyonu */
            Metin onceki_cikti = u->cikti;
            metin_baslat(&u->cikti);

            yaz(u, "    .globl  _test_%d", t);
            yaz(u, "_test_%d:", t);
            yaz(u, "    pushq   %%rbp");
            yaz(u, "    movq    %%rsp, %%rbp");

            /* Stack boyutu test gövdesi üretildikten sonra belirlenecek */
            Metin _test_prolog = u->cikti;
            metin_baslat(&u->cikti);

            /* _test_isim_ptr ve _test_isim_len ayarla */
            yaz(u, "    leaq    _test_str_%d(%%rip), %%rax", str_etiket);
            yaz(u, "    movq    %%rax, _test_isim_ptr(%%rip)");
            yaz(u, "    movq    $%d, _test_isim_len(%%rip)", isim_len);

            /* Test gövdesini üret */
            Kapsam *onceki_kapsam2 = u->kapsam;
            u->kapsam = kapsam_oluştur(u->arena, onceki_kapsam2);
            if (td->çocuk_sayısı > 0) {
                blok_uret(u, td->çocuklar[0]);
            }
            int _test_yerel = u->kapsam->yerel_sayac;
            u->kapsam = onceki_kapsam2;

            yaz(u, "    leave");
            yaz(u, "    ret");

            /* Stack boyutunu hesapla ve prolog ile birleştir */
            {
                Metin _test_govde = u->cikti;
                u->cikti = _test_prolog;
                int _ts = (_test_yerel + 8) * 8;
                _ts = (_ts + 15) & ~15;
                if (_ts < 256) _ts = 256;
                yaz(u, "    subq    $%d, %%rsp", _ts);
                if (_test_govde.veri) metin_ekle(&u->cikti, _test_govde.veri);
                metin_serbest(&_test_govde);
            }

            metin_ekle(&fonksiyonlar, u->cikti.veri);
            metin_serbest(&u->cikti);
            u->cikti = onceki_cikti;
        }
    }

    /* Fonksiyonları ekle */
    metin_ekle(&u->cikti, fonksiyonlar.veri);
    metin_serbest(&fonksiyonlar);

    /* Monomorphization: Özelleştirilmiş generic fonksiyonları üret */
    if (u->generic_ozellestirilmisler && u->generic_ozellestirme_sayisi > 0) {
        GenericÖzelleştirme *ozler = (GenericÖzelleştirme *)u->generic_ozellestirilmisler;
        for (int i = 0; i < u->generic_ozellestirme_sayisi; i++) {
            GenericÖzelleştirme *oz = &ozler[i];
            if (!oz->uretildi && oz->orijinal_dugum) {
                Metin onceki_cikti = u->cikti;
                metin_baslat(&u->cikti);

                islev_uret_ozel(u, oz->orijinal_dugum, oz->ozel_isim,
                               oz->tip_parametre, oz->somut_tip);

                metin_ekle(&onceki_cikti, u->cikti.veri);
                metin_serbest(&u->cikti);
                u->cikti = onceki_cikti;
                oz->uretildi = 1;
            }
        }
    }

    /* Yardımcı fonksiyonları ekle (_yazdir_tam, _yazdir_metin vb.) */
    if (u->yardimcilar.uzunluk > 0) {
        metin_ekle(&u->cikti, u->yardimcilar.veri);
    }

    /* Veri bölümü */
    if (u->veri_bolumu.uzunluk > 0) {
        metin_ekle(&u->cikti, "\n.section .rodata\n");
        metin_ekle(&u->cikti, u->veri_bolumu.veri);
    }

    /* Global değişkenler için BSS */
    if (u->bss_bolumu.uzunluk > 0) {
        metin_ekle(&u->cikti, "\n.section .bss\n");
        metin_ekle(&u->cikti, u->bss_bolumu.veri);
    }
}

int assembly_yaz(Üretici *u, const char *dosya_adi) {
    FILE *f = fopen(dosya_adi, "w");
    if (!f) {
        hata_genel("assembly dosyası oluşturulamadı: %s", dosya_adi);
        return 1;
    }
    fprintf(f, "# Tonyukuk Derleyici tarafından üretildi\n");
    fprintf(f, ".section .note.GNU-stack,\"\",@progbits\n");
    fprintf(f, "%s", u->cikti.veri);
    fclose(f);
    return 0;
}

void üretici_serbest(Üretici *u) {
    metin_serbest(&u->cikti);
    metin_serbest(&u->veri_bolumu);
    metin_serbest(&u->bss_bolumu);
    metin_serbest(&u->yardimcilar);
}
