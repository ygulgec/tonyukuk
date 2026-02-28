/* Dosya modülü — çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * Kapsamlı dosya sistemi işlemleri
 *
 * Tüm değişken ve fonksiyon isimleri Türkçe
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <pwd.h>
#include <libgen.h>
#include "runtime.h"

/* ========== YARDIMCI FONKSİYONLAR ========== */

/* Metin parametresini C string'e çevirir */
static char *metin_to_cstr(const char *ptr, long long uzunluk) {
    char *cstr = (char *)malloc(uzunluk + 1);
    if (!cstr) return NULL;
    memcpy(cstr, ptr, uzunluk);
    cstr[uzunluk] = '\0';
    return cstr;
}

/* Boş TrMetin döndürür */
static TrMetin bos_metin(void) {
    TrMetin m = {NULL, 0};
    return m;
}

/* C string'den TrMetin oluşturur */
static TrMetin cstr_to_metin(const char *cstr) {
    TrMetin m;
    if (!cstr) {
        m.ptr = NULL;
        m.len = 0;
        return m;
    }
    m.len = (long long)strlen(cstr);
    m.ptr = (char *)malloc(m.len + 1);
    if (m.ptr) {
        memcpy(m.ptr, cstr, m.len);
        m.ptr[m.len] = '\0';
    }
    return m;
}

/* Boş TrDizi döndürür */
static TrDizi bos_dizi(void) {
    TrDizi d = {NULL, 0};
    return d;
}

/* ========== OKUMA/YAZMA ========== */

/* dosya.oku(yol: metin) -> metin */
TrMetin _tr_dosya_oku(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_metin();

    FILE *dosya = fopen(yol, "rb");
    free(yol);
    if (!dosya) return bos_metin();

    /* Dosya boyutunu bul */
    fseek(dosya, 0, SEEK_END);
    long boyut = ftell(dosya);
    fseek(dosya, 0, SEEK_SET);

    if (boyut < 0) {
        fclose(dosya);
        return bos_metin();
    }

    TrMetin sonuç;
    sonuç.ptr = (char *)malloc(boyut + 1);
    if (!sonuç.ptr) {
        fclose(dosya);
        return bos_metin();
    }

    size_t okunan = fread(sonuç.ptr, 1, boyut, dosya);
    fclose(dosya);

    sonuç.ptr[okunan] = '\0';
    sonuç.len = (long long)okunan;
    return sonuç;
}

/* dosya.yaz(yol: metin, içerik: metin) -> tam */
long long _tr_dosya_yaz(const char *yol_ptr, long long yol_uzunluk,
                         const char *icerik_ptr, long long icerik_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    FILE *dosya = fopen(yol, "wb");
    free(yol);
    if (!dosya) return -1;

    size_t yazilan = fwrite(icerik_ptr, 1, icerik_uzunluk, dosya);
    fclose(dosya);

    return (yazilan == (size_t)icerik_uzunluk) ? 0 : -1;
}

/* dosya.ekle(yol: metin, içerik: metin) -> tam */
long long _tr_dosya_ekle(const char *yol_ptr, long long yol_uzunluk,
                          const char *icerik_ptr, long long icerik_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    FILE *dosya = fopen(yol, "ab");
    free(yol);
    if (!dosya) return -1;

    size_t yazilan = fwrite(icerik_ptr, 1, icerik_uzunluk, dosya);
    fclose(dosya);

    return (yazilan == (size_t)icerik_uzunluk) ? 0 : -1;
}

/* dosya.satirlar(yol: metin) -> dizi<metin> */
TrDizi _tr_dosya_satirlar(const char *yol_ptr, long long yol_uzunluk) {
    TrMetin icerik = _tr_dosya_oku(yol_ptr, yol_uzunluk);
    if (!icerik.ptr) return bos_dizi();

    /* Satır sayısını hesapla */
    int satir_sayisi = 1;
    for (long long i = 0; i < icerik.len; i++) {
        if (icerik.ptr[i] == '\n') satir_sayisi++;
    }

    /* Dizi oluştur - her eleman (ptr, len) çifti = 16 byte */
    TrDizi sonuç;
    sonuç.ptr = (long long *)malloc(satir_sayisi * 16);
    if (!sonuç.ptr) {
        free(icerik.ptr);
        return bos_dizi();
    }
    sonuç.count = 0;

    /* Satırları ayır */
    char *başlangıç = icerik.ptr;
    char *p = icerik.ptr;
    while (p <= icerik.ptr + icerik.len) {
        if (*p == '\n' || *p == '\0') {
            long long satir_uzunluk = p - başlangıç;
            /* \r'yi kaldır */
            if (satir_uzunluk > 0 && başlangıç[satir_uzunluk - 1] == '\r') {
                satir_uzunluk--;
            }

            char *satir = (char *)malloc(satir_uzunluk + 1);
            if (satir) {
                memcpy(satir, başlangıç, satir_uzunluk);
                satir[satir_uzunluk] = '\0';

                /* (ptr, len) çifti olarak kaydet */
                sonuç.ptr[sonuç.count * 2] = (long long)(intptr_t)satir;
                sonuç.ptr[sonuç.count * 2 + 1] = satir_uzunluk;
                sonuç.count++;
            }

            başlangıç = p + 1;
        }
        if (*p == '\0') break;
        p++;
    }

    free(icerik.ptr);
    return sonuç;
}

/* ========== VARLIK KONTROLLERİ ========== */

/* dosya.var_mi(yol: metin) -> mantık */
long long _tr_dosya_var_mi(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return 0;

    struct stat bilgi;
    int sonuç = stat(yol, &bilgi);
    free(yol);

    return (sonuç == 0) ? 1 : 0;
}

/* dosya.dizin_mi(yol: metin) -> mantık */
long long _tr_dosya_dizin_mi(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return 0;

    struct stat bilgi;
    int sonuç = stat(yol, &bilgi);
    free(yol);

    return (sonuç == 0 && S_ISDIR(bilgi.st_mode)) ? 1 : 0;
}

/* dosya.dosya_mi(yol: metin) -> mantık */
long long _tr_dosya_dosya_mi(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return 0;

    struct stat bilgi;
    int sonuç = stat(yol, &bilgi);
    free(yol);

    return (sonuç == 0 && S_ISREG(bilgi.st_mode)) ? 1 : 0;
}

/* dosya.okunabilir_mi(yol: metin) -> mantık */
long long _tr_dosya_okunabilir_mi(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return 0;

    int sonuç = access(yol, R_OK);
    free(yol);

    return (sonuç == 0) ? 1 : 0;
}

/* dosya.yazilabilir_mi(yol: metin) -> mantık */
long long _tr_dosya_yazilabilir_mi(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return 0;

    int sonuç = access(yol, W_OK);
    free(yol);

    return (sonuç == 0) ? 1 : 0;
}

/* ========== DİZİN İŞLEMLERİ ========== */

/* dosya.klasor_olustur(yol: metin) -> tam */
long long _tr_dosya_klasor_olustur(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    int sonuç = mkdir(yol, 0755);
    free(yol);

    return (sonuç == 0 || errno == EEXIST) ? 0 : -1;
}

/* dosya.klasor_olustur_hepsi(yol: metin) -> tam (mkdir -p) */
long long _tr_dosya_klasor_olustur_hepsi(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    char *p = yol;
    while (*p) {
        /* Sonraki '/' karakterini bul */
        if (*p == '/') {
            *p = '\0';
            if (yol[0] != '\0') {
                mkdir(yol, 0755);
            }
            *p = '/';
        }
        p++;
    }
    /* Son dizini oluştur */
    int sonuç = mkdir(yol, 0755);
    free(yol);

    return (sonuç == 0 || errno == EEXIST) ? 0 : -1;
}

/* dosya.listele(yol: metin) -> dizi<metin> */
TrDizi _tr_dosya_listele(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_dizi();

    DIR *dizin = opendir(yol);
    free(yol);
    if (!dizin) return bos_dizi();

    /* Önce say */
    int kapasite = 64;
    TrDizi sonuç;
    sonuç.ptr = (long long *)malloc(kapasite * 16);
    if (!sonuç.ptr) {
        closedir(dizin);
        return bos_dizi();
    }
    sonuç.count = 0;

    struct dirent *girdi;
    while ((girdi = readdir(dizin)) != NULL) {
        /* . ve .. atla */
        if (strcmp(girdi->d_name, ".") == 0 || strcmp(girdi->d_name, "..") == 0)
            continue;

        /* Kapasite kontrolü */
        if (sonuç.count >= kapasite) {
            kapasite *= 2;
            sonuç.ptr = (long long *)realloc(sonuç.ptr, kapasite * 16);
            if (!sonuç.ptr) {
                closedir(dizin);
                return bos_dizi();
            }
        }

        /* İsmi kopyala */
        size_t isim_uzunluk = strlen(girdi->d_name);
        char *isim = (char *)malloc(isim_uzunluk + 1);
        if (isim) {
            memcpy(isim, girdi->d_name, isim_uzunluk + 1);
            sonuç.ptr[sonuç.count * 2] = (long long)(intptr_t)isim;
            sonuç.ptr[sonuç.count * 2 + 1] = (long long)isim_uzunluk;
            sonuç.count++;
        }
    }

    closedir(dizin);
    return sonuç;
}

/* dosya.alt_dizinler(yol: metin) -> dizi<metin> */
TrDizi _tr_dosya_alt_dizinler(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_dizi();

    DIR *dizin = opendir(yol);
    if (!dizin) {
        free(yol);
        return bos_dizi();
    }

    int kapasite = 32;
    TrDizi sonuç;
    sonuç.ptr = (long long *)malloc(kapasite * 16);
    if (!sonuç.ptr) {
        free(yol);
        closedir(dizin);
        return bos_dizi();
    }
    sonuç.count = 0;

    struct dirent *girdi;
    while ((girdi = readdir(dizin)) != NULL) {
        if (strcmp(girdi->d_name, ".") == 0 || strcmp(girdi->d_name, "..") == 0)
            continue;

        /* Tam yolu oluştur ve dizin mi kontrol et */
        char tam_yol[PATH_MAX];
        snprintf(tam_yol, sizeof(tam_yol), "%s/%s", yol, girdi->d_name);

        struct stat bilgi;
        if (stat(tam_yol, &bilgi) == 0 && S_ISDIR(bilgi.st_mode)) {
            if (sonuç.count >= kapasite) {
                kapasite *= 2;
                sonuç.ptr = (long long *)realloc(sonuç.ptr, kapasite * 16);
                if (!sonuç.ptr) break;
            }

            size_t isim_uzunluk = strlen(girdi->d_name);
            char *isim = (char *)malloc(isim_uzunluk + 1);
            if (isim) {
                memcpy(isim, girdi->d_name, isim_uzunluk + 1);
                sonuç.ptr[sonuç.count * 2] = (long long)(intptr_t)isim;
                sonuç.ptr[sonuç.count * 2 + 1] = (long long)isim_uzunluk;
                sonuç.count++;
            }
        }
    }

    free(yol);
    closedir(dizin);
    return sonuç;
}

/* dosya.dosyalar(yol: metin) -> dizi<metin> */
TrDizi _tr_dosya_dosyalar(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_dizi();

    DIR *dizin = opendir(yol);
    if (!dizin) {
        free(yol);
        return bos_dizi();
    }

    int kapasite = 64;
    TrDizi sonuç;
    sonuç.ptr = (long long *)malloc(kapasite * 16);
    if (!sonuç.ptr) {
        free(yol);
        closedir(dizin);
        return bos_dizi();
    }
    sonuç.count = 0;

    struct dirent *girdi;
    while ((girdi = readdir(dizin)) != NULL) {
        if (strcmp(girdi->d_name, ".") == 0 || strcmp(girdi->d_name, "..") == 0)
            continue;

        char tam_yol[PATH_MAX];
        snprintf(tam_yol, sizeof(tam_yol), "%s/%s", yol, girdi->d_name);

        struct stat bilgi;
        if (stat(tam_yol, &bilgi) == 0 && S_ISREG(bilgi.st_mode)) {
            if (sonuç.count >= kapasite) {
                kapasite *= 2;
                sonuç.ptr = (long long *)realloc(sonuç.ptr, kapasite * 16);
                if (!sonuç.ptr) break;
            }

            size_t isim_uzunluk = strlen(girdi->d_name);
            char *isim = (char *)malloc(isim_uzunluk + 1);
            if (isim) {
                memcpy(isim, girdi->d_name, isim_uzunluk + 1);
                sonuç.ptr[sonuç.count * 2] = (long long)(intptr_t)isim;
                sonuç.ptr[sonuç.count * 2 + 1] = (long long)isim_uzunluk;
                sonuç.count++;
            }
        }
    }

    free(yol);
    closedir(dizin);
    return sonuç;
}

/* ========== SİLME/TAŞIMA/KOPYALAMA ========== */

/* dosya.sil(yol: metin) -> tam */
long long _tr_dosya_sil(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    struct stat bilgi;
    int sonuç;

    if (stat(yol, &bilgi) == 0) {
        if (S_ISDIR(bilgi.st_mode)) {
            sonuç = rmdir(yol);
        } else {
            sonuç = unlink(yol);
        }
    } else {
        sonuç = -1;
    }

    free(yol);
    return (sonuç == 0) ? 0 : -1;
}

/* Özyinelemeli silme yardımcısı */
static int sil_ozyineli_yardimci(const char *yol) {
    struct stat bilgi;
    if (lstat(yol, &bilgi) != 0) return -1;

    if (S_ISDIR(bilgi.st_mode)) {
        DIR *dizin = opendir(yol);
        if (!dizin) return -1;

        struct dirent *girdi;
        while ((girdi = readdir(dizin)) != NULL) {
            if (strcmp(girdi->d_name, ".") == 0 || strcmp(girdi->d_name, "..") == 0)
                continue;

            char alt_yol[PATH_MAX];
            snprintf(alt_yol, sizeof(alt_yol), "%s/%s", yol, girdi->d_name);
            sil_ozyineli_yardimci(alt_yol);
        }

        closedir(dizin);
        return rmdir(yol);
    } else {
        return unlink(yol);
    }
}

/* dosya.sil_ozyineli(yol: metin) -> tam */
long long _tr_dosya_sil_ozyineli(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    int sonuç = sil_ozyineli_yardimci(yol);
    free(yol);

    return (sonuç == 0) ? 0 : -1;
}

/* dosya.tasi(kaynak: metin, hedef: metin) -> tam */
long long _tr_dosya_tasi(const char *kaynak_ptr, long long kaynak_uzunluk,
                          const char *hedef_ptr, long long hedef_uzunluk) {
    char *kaynak = metin_to_cstr(kaynak_ptr, kaynak_uzunluk);
    char *hedef = metin_to_cstr(hedef_ptr, hedef_uzunluk);
    if (!kaynak || !hedef) {
        free(kaynak);
        free(hedef);
        return -1;
    }

    int sonuç = rename(kaynak, hedef);
    free(kaynak);
    free(hedef);

    return (sonuç == 0) ? 0 : -1;
}

/* dosya.kopyala(kaynak: metin, hedef: metin) -> tam */
long long _tr_dosya_kopyala(const char *kaynak_ptr, long long kaynak_uzunluk,
                             const char *hedef_ptr, long long hedef_uzunluk) {
    char *kaynak = metin_to_cstr(kaynak_ptr, kaynak_uzunluk);
    char *hedef = metin_to_cstr(hedef_ptr, hedef_uzunluk);
    if (!kaynak || !hedef) {
        free(kaynak);
        free(hedef);
        return -1;
    }

    FILE *kaynak_dosya = fopen(kaynak, "rb");
    if (!kaynak_dosya) {
        free(kaynak);
        free(hedef);
        return -1;
    }

    FILE *hedef_dosya = fopen(hedef, "wb");
    if (!hedef_dosya) {
        fclose(kaynak_dosya);
        free(kaynak);
        free(hedef);
        return -1;
    }

    char tampon[8192];
    size_t okunan;
    int hata = 0;

    while ((okunan = fread(tampon, 1, sizeof(tampon), kaynak_dosya)) > 0) {
        if (fwrite(tampon, 1, okunan, hedef_dosya) != okunan) {
            hata = 1;
            break;
        }
    }

    fclose(kaynak_dosya);
    fclose(hedef_dosya);
    free(kaynak);
    free(hedef);

    return hata ? -1 : 0;
}

/* dosya.yeniden_adlandir - tasi ile aynı */
long long _tr_dosya_yeniden_adlandir(const char *eski_ptr, long long eski_uzunluk,
                                      const char *yeni_ptr, long long yeni_uzunluk) {
    return _tr_dosya_tasi(eski_ptr, eski_uzunluk, yeni_ptr, yeni_uzunluk);
}

/* ========== META VERİ ========== */

/* dosya.boyut(yol: metin) -> tam */
long long _tr_dosya_boyut(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    struct stat bilgi;
    int sonuç = stat(yol, &bilgi);
    free(yol);

    return (sonuç == 0) ? (long long)bilgi.st_size : -1;
}

/* dosya.degistirilme_zamani(yol: metin) -> tam */
long long _tr_dosya_degistirilme_zamani(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    struct stat bilgi;
    int sonuç = stat(yol, &bilgi);
    free(yol);

    return (sonuç == 0) ? (long long)bilgi.st_mtime : -1;
}

/* dosya.olusturulma_zamani(yol: metin) -> tam */
long long _tr_dosya_olusturulma_zamani(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    struct stat bilgi;
    int sonuç = stat(yol, &bilgi);
    free(yol);

    /* Linux'ta gerçek oluşturulma zamanı yok, ctime kullan */
    return (sonuç == 0) ? (long long)bilgi.st_ctime : -1;
}

/* dosya.izinler(yol: metin) -> tam */
long long _tr_dosya_izinler(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    struct stat bilgi;
    int sonuç = stat(yol, &bilgi);
    free(yol);

    return (sonuç == 0) ? (long long)(bilgi.st_mode & 07777) : -1;
}

/* dosya.izin_ayarla(yol: metin, mod: tam) -> tam */
long long _tr_dosya_izin_ayarla(const char *yol_ptr, long long yol_uzunluk, long long mod) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    int sonuç = chmod(yol, (mode_t)mod);
    free(yol);

    return (sonuç == 0) ? 0 : -1;
}

/* ========== YOL İŞLEMLERİ ========== */

/* dosya.mutlak_yol(yol: metin) -> metin */
TrMetin _tr_dosya_mutlak_yol(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_metin();

    char *mutlak = realpath(yol, NULL);
    free(yol);

    if (!mutlak) return bos_metin();

    TrMetin sonuç = cstr_to_metin(mutlak);
    free(mutlak);
    return sonuç;
}

/* dosya.ust_dizin(yol: metin) -> metin */
TrMetin _tr_dosya_ust_dizin(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_metin();

    char *ust = dirname(yol);
    TrMetin sonuç = cstr_to_metin(ust);
    free(yol);
    return sonuç;
}

/* dosya.temel_ad(yol: metin) -> metin */
TrMetin _tr_dosya_temel_ad(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_metin();

    char *temel = basename(yol);
    TrMetin sonuç = cstr_to_metin(temel);
    free(yol);
    return sonuç;
}

/* dosya.uzanti(yol: metin) -> metin */
TrMetin _tr_dosya_uzanti(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_metin();

    char *temel = basename(yol);
    char *nokta = strrchr(temel, '.');

    TrMetin sonuç;
    if (nokta && nokta != temel) {
        sonuç = cstr_to_metin(nokta);
    } else {
        sonuç = cstr_to_metin("");
    }

    free(yol);
    return sonuç;
}

/* dosya.adsiz(yol: metin) -> metin (uzantısız dosya adı) */
TrMetin _tr_dosya_adsiz(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_metin();

    char *temel = basename(yol);
    char *kopya = strdup(temel);
    if (!kopya) {
        free(yol);
        return bos_metin();
    }

    char *nokta = strrchr(kopya, '.');
    if (nokta && nokta != kopya) {
        *nokta = '\0';
    }

    TrMetin sonuç = cstr_to_metin(kopya);
    free(kopya);
    free(yol);
    return sonuç;
}

/* dosya.yol_birlestir(yol1: metin, yol2: metin) -> metin */
TrMetin _tr_dosya_yol_birlestir(const char *yol1_ptr, long long yol1_uzunluk,
                                 const char *yol2_ptr, long long yol2_uzunluk) {
    char *yol1 = metin_to_cstr(yol1_ptr, yol1_uzunluk);
    char *yol2 = metin_to_cstr(yol2_ptr, yol2_uzunluk);
    if (!yol1 || !yol2) {
        free(yol1);
        free(yol2);
        return bos_metin();
    }

    /* yol2 mutlak ise sadece yol2'yi döndür */
    if (yol2[0] == '/') {
        free(yol1);
        TrMetin sonuç = cstr_to_metin(yol2);
        free(yol2);
        return sonuç;
    }

    /* Birleştir */
    size_t toplam = strlen(yol1) + strlen(yol2) + 2;
    char *birlesik = (char *)malloc(toplam);
    if (!birlesik) {
        free(yol1);
        free(yol2);
        return bos_metin();
    }

    /* Sonda / varsa ekleme */
    size_t yol1_len = strlen(yol1);
    if (yol1_len > 0 && yol1[yol1_len - 1] == '/') {
        snprintf(birlesik, toplam, "%s%s", yol1, yol2);
    } else {
        snprintf(birlesik, toplam, "%s/%s", yol1, yol2);
    }

    TrMetin sonuç = cstr_to_metin(birlesik);
    free(birlesik);
    free(yol1);
    free(yol2);
    return sonuç;
}

/* ========== GLOB DESENLERI ========== */

/* Basit glob implementasyonu */
TrDizi _tr_dosya_glob(const char *desen_ptr, long long desen_uzunluk) {
    char *desen = metin_to_cstr(desen_ptr, desen_uzunluk);
    if (!desen) return bos_dizi();

    /* Dizin ve desen ayır */
    char dizin_yolu[PATH_MAX] = ".";
    char *desen_kismi = desen;

    char *son_slash = strrchr(desen, '/');
    if (son_slash) {
        *son_slash = '\0';
        strncpy(dizin_yolu, desen, sizeof(dizin_yolu) - 1);
        desen_kismi = son_slash + 1;
    }

    DIR *dizin = opendir(dizin_yolu);
    if (!dizin) {
        free(desen);
        return bos_dizi();
    }

    int kapasite = 64;
    TrDizi sonuç;
    sonuç.ptr = (long long *)malloc(kapasite * 16);
    if (!sonuç.ptr) {
        free(desen);
        closedir(dizin);
        return bos_dizi();
    }
    sonuç.count = 0;

    struct dirent *girdi;
    while ((girdi = readdir(dizin)) != NULL) {
        if (fnmatch(desen_kismi, girdi->d_name, 0) == 0) {
            if (sonuç.count >= kapasite) {
                kapasite *= 2;
                sonuç.ptr = (long long *)realloc(sonuç.ptr, kapasite * 16);
                if (!sonuç.ptr) break;
            }

            /* Tam yolu oluştur */
            char tam_yol[PATH_MAX];
            if (strcmp(dizin_yolu, ".") == 0) {
                snprintf(tam_yol, sizeof(tam_yol), "%s", girdi->d_name);
            } else {
                snprintf(tam_yol, sizeof(tam_yol), "%s/%s", dizin_yolu, girdi->d_name);
            }

            size_t isim_uzunluk = strlen(tam_yol);
            char *isim = (char *)malloc(isim_uzunluk + 1);
            if (isim) {
                memcpy(isim, tam_yol, isim_uzunluk + 1);
                sonuç.ptr[sonuç.count * 2] = (long long)(intptr_t)isim;
                sonuç.ptr[sonuç.count * 2 + 1] = (long long)isim_uzunluk;
                sonuç.count++;
            }
        }
    }

    free(desen);
    closedir(dizin);
    return sonuç;
}

/* Özyinelemeli glob yardımcısı */
static void glob_ozyineli_yardimci(const char *dizin_yolu, const char *desen,
                                    TrDizi *sonuç, int *kapasite) {
    DIR *dizin = opendir(dizin_yolu);
    if (!dizin) return;

    struct dirent *girdi;
    while ((girdi = readdir(dizin)) != NULL) {
        if (strcmp(girdi->d_name, ".") == 0 || strcmp(girdi->d_name, "..") == 0)
            continue;

        char tam_yol[PATH_MAX];
        snprintf(tam_yol, sizeof(tam_yol), "%s/%s", dizin_yolu, girdi->d_name);

        struct stat bilgi;
        if (stat(tam_yol, &bilgi) != 0) continue;

        if (S_ISDIR(bilgi.st_mode)) {
            /* Özyinelemeli olarak alt dizine gir */
            glob_ozyineli_yardimci(tam_yol, desen, sonuç, kapasite);
        } else if (fnmatch(desen, girdi->d_name, 0) == 0) {
            /* Eşleşen dosyayı ekle */
            if (sonuç->count >= *kapasite) {
                *kapasite *= 2;
                sonuç->ptr = (long long *)realloc(sonuç->ptr, *kapasite * 16);
                if (!sonuç->ptr) break;
            }

            size_t isim_uzunluk = strlen(tam_yol);
            char *isim = (char *)malloc(isim_uzunluk + 1);
            if (isim) {
                memcpy(isim, tam_yol, isim_uzunluk + 1);
                sonuç->ptr[sonuç->count * 2] = (long long)(intptr_t)isim;
                sonuç->ptr[sonuç->count * 2 + 1] = (long long)isim_uzunluk;
                sonuç->count++;
            }
        }
    }

    closedir(dizin);
}

/* dosya.glob_ozyineli(desen: metin) -> dizi<metin> */
TrDizi _tr_dosya_glob_ozyineli(const char *desen_ptr, long long desen_uzunluk) {
    char *desen = metin_to_cstr(desen_ptr, desen_uzunluk);
    if (!desen) return bos_dizi();

    int kapasite = 128;
    TrDizi sonuç;
    sonuç.ptr = (long long *)malloc(kapasite * 16);
    if (!sonuç.ptr) {
        free(desen);
        return bos_dizi();
    }
    sonuç.count = 0;

    glob_ozyineli_yardimci(".", desen, &sonuç, &kapasite);

    free(desen);
    return sonuç;
}

/* ========== GEÇİCİ DOSYALAR ========== */

/* dosya.gecici_dosya() -> metin */
TrMetin _tr_dosya_gecici_dosya(void) {
    char sablon[] = "/tmp/tonyukuk-XXXXXX";
    int fd = mkstemp(sablon);
    if (fd < 0) return bos_metin();

    close(fd);
    return cstr_to_metin(sablon);
}

/* dosya.gecici_dizin() -> metin */
TrMetin _tr_dosya_gecici_dizin(void) {
    char sablon[] = "/tmp/tonyukuk-XXXXXX";
    char *sonuç = mkdtemp(sablon);
    if (!sonuç) return bos_metin();

    return cstr_to_metin(sonuç);
}

/* ========== SEMBOLİK BAĞLANTILAR ========== */

/* dosya.sembolik_bag_olustur(hedef: metin, bag: metin) -> tam */
long long _tr_dosya_sembolik_bag_olustur(const char *hedef_ptr, long long hedef_uzunluk,
                                          const char *bag_ptr, long long bag_uzunluk) {
    char *hedef = metin_to_cstr(hedef_ptr, hedef_uzunluk);
    char *bag = metin_to_cstr(bag_ptr, bag_uzunluk);
    if (!hedef || !bag) {
        free(hedef);
        free(bag);
        return -1;
    }

    int sonuç = symlink(hedef, bag);
    free(hedef);
    free(bag);

    return (sonuç == 0) ? 0 : -1;
}

/* dosya.sembolik_bag_mi(yol: metin) -> mantık */
long long _tr_dosya_sembolik_bag_mi(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return 0;

    struct stat bilgi;
    int sonuç = lstat(yol, &bilgi);
    free(yol);

    return (sonuç == 0 && S_ISLNK(bilgi.st_mode)) ? 1 : 0;
}

/* dosya.sembolik_bag_oku(yol: metin) -> metin */
TrMetin _tr_dosya_sembolik_bag_oku(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return bos_metin();

    char tampon[PATH_MAX];
    ssize_t uzunluk = readlink(yol, tampon, sizeof(tampon) - 1);
    free(yol);

    if (uzunluk < 0) return bos_metin();

    tampon[uzunluk] = '\0';
    return cstr_to_metin(tampon);
}

/* ========== MEVCUT DİZİN ========== */

/* dosya.mevcut_dizin() -> metin */
TrMetin _tr_dosya_mevcut_dizin(void) {
    char tampon[PATH_MAX];
    if (!getcwd(tampon, sizeof(tampon))) return bos_metin();
    return cstr_to_metin(tampon);
}

/* dosya.dizin_degistir(yol: metin) -> tam */
long long _tr_dosya_dizin_degistir(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    int sonuç = chdir(yol);
    free(yol);

    return (sonuç == 0) ? 0 : -1;
}

/* dosya.ev_dizini() -> metin */
TrMetin _tr_dosya_ev_dizini(void) {
    const char *ev = getenv("HOME");
    if (ev) return cstr_to_metin(ev);

    /* HOME yoksa passwd'den al */
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return cstr_to_metin(pw->pw_dir);
    }

    return cstr_to_metin("/");
}
