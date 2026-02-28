#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

/* Bir UTF-8 codepoint oku. *pos ilerletilir. */
uint32_t utf8_codepoint_oku(const char *kaynak, int *pos);

/* Codepoint'i UTF-8 olarak yaz. Yazılan byte sayısını döndürür. */
int utf8_codepoint_yaz(char *hedef, uint32_t kod);

/* UTF-8 byte dizisinin byte uzunluğunu döndürür (ilk byte'a bakarak). */
int utf8_byte_uzunluk(unsigned char ilk_byte);

/* Codepoint tanımlayıcı başlangıcı mı? (harf veya _) */
int utf8_tanimlayici_baslangic(uint32_t kod);

/* Codepoint tanımlayıcı devamı mı? (harf, rakam veya _) */
int utf8_tanimlayici_devam(uint32_t kod);

/* Türkçe küçük harf dönüşümü */
uint32_t turkce_kucuk_harf(uint32_t kod);

/* Türkçe büyük harf dönüşümü */
uint32_t turkce_buyuk_harf(uint32_t kod);

#endif
