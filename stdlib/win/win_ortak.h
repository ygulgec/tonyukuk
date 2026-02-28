/* Windows Ortak Tanımlar — Tonyukuk Win32 Runtime
 * Tüm Win32 modülleri bu dosyayı dahil eder.
 */
#ifndef TR_WIN_ORTAK_H
#define TR_WIN_ORTAK_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Runtime tipleri */
typedef struct { char *ptr; long long len; } TrMetin;
typedef struct { long long *ptr; long long count; } TrDizi;
typedef struct {
    long long ref_sayisi;
    long long tip;
    long long boyut;
} NesneBaslik;

#define NESNE_BASLIK_BOYUT 24
#define NESNE_BASLIK(ptr) ((NesneBaslik *)((char *)(ptr) - NESNE_BASLIK_BOYUT))

/* ========== Yardımcı Fonksiyonlar ========== */

static inline char *metin_to_cstr(const char *ptr, long long uzunluk) {
    char *cstr = (char *)malloc((size_t)uzunluk + 1);
    if (!cstr) return NULL;
    memcpy(cstr, ptr, (size_t)uzunluk);
    cstr[uzunluk] = '\0';
    return cstr;
}

static inline TrMetin bos_metin(void) {
    TrMetin m = {NULL, 0};
    return m;
}

static inline TrMetin cstr_to_metin(const char *cstr) {
    TrMetin m;
    if (!cstr) { m.ptr = NULL; m.len = 0; return m; }
    m.len = (long long)strlen(cstr);
    m.ptr = (char *)malloc((size_t)m.len + 1);
    if (m.ptr) { memcpy(m.ptr, cstr, (size_t)m.len); m.ptr[m.len] = '\0'; }
    return m;
}

/* UTF-8 -> UTF-16 dönüşüm */
static inline wchar_t *utf8_to_wide(const char *utf8) {
    if (!utf8) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len <= 0) return NULL;
    wchar_t *wide = (wchar_t *)malloc(len * sizeof(wchar_t));
    if (!wide) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}

/* UTF-16 -> UTF-8 dönüşüm */
static inline char *wide_to_utf8(const wchar_t *wide) {
    if (!wide) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char *utf8 = (char *)malloc(len);
    if (!utf8) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, len, NULL, NULL);
    return utf8;
}

/* Metin (ptr+len) -> wide string */
static inline wchar_t *metin_to_wide(const char *ptr, long long uzunluk) {
    char *cstr = metin_to_cstr(ptr, uzunluk);
    if (!cstr) return NULL;
    wchar_t *w = utf8_to_wide(cstr);
    free(cstr);
    return w;
}

/* ========== Widget Sistemi (arayuz modülü tarafından yönetilir) ========== */

#define MAKS_WIDGET 4096

typedef enum {
    WT_NONE = 0,
    WT_HBOX, WT_VBOX,
    WT_BUTTON, WT_LABEL, WT_ENTRY,
    WT_NOTEBOOK, WT_PROGRESS,
    WT_SEPARATOR, WT_SCROLLWIN,
    WT_IMAGE, WT_WEBVIEW_HOST
} WidgetTipi;

typedef struct {
    HWND hwnd;
    int expand, fill, padding;
} BoxCocuk;

#define MAKS_COCUK 128

typedef struct {
    HWND hwnd;
    WidgetTipi tip;
    int clicked;
    int enter;
    int tab_changed;
    int tab_close_requested;
    int tab_close_index;
    /* Box layout bilgileri */
    BoxCocuk çocuklar[MAKS_COCUK];
    int çocuk_sayısı;
    /* Boyut ayarları (-1 = auto) */
    int istenen_genislik;
    int istenen_yukseklik;
    /* Tab sayfaları */
    HWND tab_sayfalar[256];
    int tab_sayfa_sayisi;
    int tab_kapatmali[256];
} WidgetBilgi;

/* Extern: arayuz modülü tarafından tanımlanır */
extern WidgetBilgi win_widgetlar[MAKS_WIDGET];
extern int win_widget_sayisi;
extern HWND _tr_arayuz_widget_al_hwnd(long long id);
extern long long _tr_arayuz_widget_kaydet_harici_win(HWND hwnd, WidgetTipi tip);
extern void win_layout_yenile(long long id, int genislik, int yukseklik);

/* Box sınıf adı */
#define TR_BOX_CLASS L"TrBox"
#define TR_BOX_PROP L"TrBoxId"

/* Pencere sınıf adı */
#define TR_PENCERE_CLASS L"TrPencere"
#define TR_PENCERE_PROP L"TrPencereId"

/* Koyu tema renkleri (Catppuccin Mocha) */
#define RENK_ARKAPLAN    RGB(30, 30, 46)    /* #1e1e2e */
#define RENK_YUZEY       RGB(49, 50, 68)    /* #313244 */
#define RENK_METIN       RGB(205, 214, 244) /* #cdd6f4 */
#define RENK_ALTYAZI     RGB(108, 112, 134) /* #6c7086 */
#define RENK_MAVI        RGB(137, 180, 250) /* #89b4fa */
#define RENK_KARANLIK    RGB(17, 17, 27)    /* #11111b */

#endif /* TR_WIN_ORTAK_H */
