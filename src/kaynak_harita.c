#include "kaynak_harita.h"
#include <stdio.h>
#include <string.h>

void kaynak_harita_baslat(KaynakHarita *kh, const char *dosya) {
    kh->dosya = dosya;
    kh->eslesme_sayisi = 0;
    kh->asm_satir_sayac = 0;
}

void kaynak_harita_ekle(KaynakHarita *kh, int kaynak_satir) {
    if (kh->eslesme_sayisi >= MAKS_ESLESMELER) return;
    if (kaynak_satir <= 0) return;

    /* Aynı kaynak satırı için tekrar ekleme */
    if (kh->eslesme_sayisi > 0 &&
        kh->eslesmeler[kh->eslesme_sayisi - 1].kaynak_satir == kaynak_satir) {
        return;
    }

    kh->eslesmeler[kh->eslesme_sayisi].asm_satir = kh->asm_satir_sayac;
    kh->eslesmeler[kh->eslesme_sayisi].kaynak_satir = kaynak_satir;
    kh->eslesme_sayisi++;
}

void kaynak_harita_satir_artir(KaynakHarita *kh) {
    kh->asm_satir_sayac++;
}

int kaynak_harita_yaz(KaynakHarita *kh, const char *dosya_adi) {
    FILE *f = fopen(dosya_adi, "w");
    if (!f) return 1;

    fprintf(f, "{\n");
    fprintf(f, "  \"dosya\": \"%s\",\n", kh->dosya ? kh->dosya : "");
    fprintf(f, "  \"eslesmeler\": [\n");

    for (int i = 0; i < kh->eslesme_sayisi; i++) {
        fprintf(f, "    {\"asm_satir\": %d, \"kaynak_satir\": %d}",
                kh->eslesmeler[i].asm_satir,
                kh->eslesmeler[i].kaynak_satir);
        if (i < kh->eslesme_sayisi - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}
