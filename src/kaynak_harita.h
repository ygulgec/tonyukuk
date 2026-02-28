#ifndef KAYNAK_HARİTA_H
#define KAYNAK_HARİTA_H

#define MAKS_ESLESMELER 4096

typedef struct {
    int asm_satir;
    int kaynak_satir;
} HaritaEslesmesi;

typedef struct {
    const char *dosya;
    HaritaEslesmesi eslesmeler[MAKS_ESLESMELER];
    int eslesme_sayisi;
    int asm_satir_sayac;   /* mevcut assembly satır sayacı */
} KaynakHarita;

/* Kaynak harita başlat */
void kaynak_harita_baslat(KaynakHarita *kh, const char *dosya);

/* Eşleşme ekle */
void kaynak_harita_ekle(KaynakHarita *kh, int kaynak_satir);

/* Assembly satır sayacını artır */
void kaynak_harita_satir_artir(KaynakHarita *kh);

/* JSON dosyasına yaz */
int kaynak_harita_yaz(KaynakHarita *kh, const char *dosya_adi);

#endif
