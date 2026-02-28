#include "utf8.h"

int utf8_byte_uzunluk(unsigned char ilk_byte) {
    if (ilk_byte < 0x80) return 1;
    if ((ilk_byte & 0xE0) == 0xC0) return 2;
    if ((ilk_byte & 0xF0) == 0xE0) return 3;
    if ((ilk_byte & 0xF8) == 0xF0) return 4;
    return 1; /* geçersiz byte, 1 olarak ilerle */
}

uint32_t utf8_codepoint_oku(const char *kaynak, int *pos) {
    unsigned char c = (unsigned char)kaynak[*pos];
    uint32_t kod;
    int uzunluk;

    if (c < 0x80) {
        kod = c;
        uzunluk = 1;
    } else if ((c & 0xE0) == 0xC0) {
        kod = c & 0x1F;
        uzunluk = 2;
    } else if ((c & 0xF0) == 0xE0) {
        kod = c & 0x0F;
        uzunluk = 3;
    } else if ((c & 0xF8) == 0xF0) {
        kod = c & 0x07;
        uzunluk = 4;
    } else {
        /* Geçersiz UTF-8, olduğu gibi döndür */
        kod = c;
        uzunluk = 1;
        (*pos)++;
        return kod;
    }

    for (int i = 1; i < uzunluk; i++) {
        unsigned char devam = (unsigned char)kaynak[*pos + i];
        if ((devam & 0xC0) != 0x80) {
            /* Geçersiz devam byte'ı */
            (*pos)++;
            return 0xFFFD; /* replacement character */
        }
        kod = (kod << 6) | (devam & 0x3F);
    }

    *pos += uzunluk;
    return kod;
}

int utf8_codepoint_yaz(char *hedef, uint32_t kod) {
    if (kod < 0x80) {
        hedef[0] = (char)kod;
        return 1;
    } else if (kod < 0x800) {
        hedef[0] = (char)(0xC0 | (kod >> 6));
        hedef[1] = (char)(0x80 | (kod & 0x3F));
        return 2;
    } else if (kod < 0x10000) {
        hedef[0] = (char)(0xE0 | (kod >> 12));
        hedef[1] = (char)(0x80 | ((kod >> 6) & 0x3F));
        hedef[2] = (char)(0x80 | (kod & 0x3F));
        return 3;
    } else {
        hedef[0] = (char)(0xF0 | (kod >> 18));
        hedef[1] = (char)(0x80 | ((kod >> 12) & 0x3F));
        hedef[2] = (char)(0x80 | ((kod >> 6) & 0x3F));
        hedef[3] = (char)(0x80 | (kod & 0x3F));
        return 4;
    }
}

int utf8_tanimlayici_baslangic(uint32_t kod) {
    if (kod == '_') return 1;
    /* ASCII harfler */
    if (kod >= 'a' && kod <= 'z') return 1;
    if (kod >= 'A' && kod <= 'Z') return 1;
    /* Türkçe özel karakterler */
    if (kod == 0x00C7 || kod == 0x00E7) return 1; /* Ç, ç */
    if (kod == 0x011E || kod == 0x011F) return 1; /* Ğ, ğ */
    if (kod == 0x0130 || kod == 0x0131) return 1; /* İ, ı */
    if (kod == 0x00D6 || kod == 0x00F6) return 1; /* Ö, ö */
    if (kod == 0x015E || kod == 0x015F) return 1; /* Ş, ş */
    if (kod == 0x00DC || kod == 0x00FC) return 1; /* Ü, ü */
    /* Genel Unicode harfler (basitleştirilmiş: Latin Extended vb.) */
    if (kod >= 0x00C0 && kod <= 0x024F) return 1;
    if (kod >= 0x0370 && kod <= 0x1FFF) return 1;
    return 0;
}

int utf8_tanimlayici_devam(uint32_t kod) {
    if (utf8_tanimlayici_baslangic(kod)) return 1;
    if (kod >= '0' && kod <= '9') return 1;
    return 0;
}

uint32_t turkce_kucuk_harf(uint32_t kod) {
    /* Türkçe özel durumlar */
    if (kod == 'I') return 0x0131;   /* I -> ı */
    if (kod == 0x0130) return 'i';   /* İ -> i */
    /* Standart ASCII */
    if (kod >= 'A' && kod <= 'Z') return kod + 32;
    /* Türkçe büyük harfler */
    if (kod == 0x00C7) return 0x00E7; /* Ç -> ç */
    if (kod == 0x011E) return 0x011F; /* Ğ -> ğ */
    if (kod == 0x00D6) return 0x00F6; /* Ö -> ö */
    if (kod == 0x015E) return 0x015F; /* Ş -> ş */
    if (kod == 0x00DC) return 0x00FC; /* Ü -> ü */
    return kod;
}

uint32_t turkce_buyuk_harf(uint32_t kod) {
    /* Türkçe özel durumlar */
    if (kod == 'i') return 0x0130;   /* i -> İ */
    if (kod == 0x0131) return 'I';   /* ı -> I */
    /* Standart ASCII */
    if (kod >= 'a' && kod <= 'z') return kod - 32;
    /* Türkçe küçük harfler */
    if (kod == 0x00E7) return 0x00C7; /* ç -> Ç */
    if (kod == 0x011F) return 0x011E; /* ğ -> Ğ */
    if (kod == 0x00F6) return 0x00D6; /* ö -> Ö */
    if (kod == 0x015F) return 0x015E; /* ş -> Ş */
    if (kod == 0x00FC) return 0x00DC; /* ü -> Ü */
    return kod;
}
