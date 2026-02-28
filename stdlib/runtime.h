/* Ortak runtime tipleri ve fonksiyon bildirimleri */
#ifndef TR_RUNTIME_H
#define TR_RUNTIME_H

#include <stdint.h>

/* Metin dönüşü: 16 byte struct -> rax=ptr, rdx=len (System V ABI) */
typedef struct { char *ptr; long long len; } TrMetin;

/* Dizi: ptr + count */
typedef struct { long long *ptr; long long count; } TrDizi;

/* 24-byte header: her heap nesnesinin öncesinde */
typedef struct {
    long long ref_sayisi;
    long long tip;
    long long boyut;
} NesneBaslik;

#define NESNE_BASLIK_BOYUT 24
#define NESNE_BASLIK(ptr) ((NesneBaslik *)((char *)(ptr) - NESNE_BASLIK_BOYUT))

/* Nesne tip sabitleri */
#define NESNE_TIP_METIN   0
#define NESNE_TIP_DIZI    1
#define NESNE_TIP_SOZLUK  2
#define NESNE_TIP_SINIF   3
#define NESNE_TIP_KUME    4
#define NESNE_TIP_KAPANIS 5

/* Core runtime fonksiyonları (calismazamani.c'de tanımlı) */
extern void *_tr_nesne_olustur(long long tip, long long boyut);
extern void  _tr_ref_artir(void *ptr);
extern void  _tr_ref_azalt(void *ptr);

#endif /* TR_RUNTIME_H */
