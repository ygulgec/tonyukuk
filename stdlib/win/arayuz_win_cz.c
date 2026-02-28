/* Arayüz modülü — Windows çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * Win32 widget'ları (düğme, etiket, giriş, kutu, sekmeler vb.)
 */

#include "win_ortak.h"
#include <windowsx.h>

/* ========== GLOBAL WİDGET DİZİSİ ========== */

WidgetBilgi win_widgetlar[MAKS_WIDGET];
int win_widget_sayisi = 0;

/* Koyu tema fırçaları */
static HBRUSH hBrushBg = NULL;
static HBRUSH hBrushSurface = NULL;
static HBRUSH hBrushDark = NULL;
static HFONT hDefaultFont = NULL;

static void init_resources(void) {
    if (!hBrushBg) hBrushBg = CreateSolidBrush(RENK_ARKAPLAN);
    if (!hBrushSurface) hBrushSurface = CreateSolidBrush(RENK_YUZEY);
    if (!hBrushDark) hBrushDark = CreateSolidBrush(RENK_KARANLIK);
    if (!hDefaultFont) {
        hDefaultFont = CreateFontW(
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
            L"Segoe UI"
        );
    }
}

/* ========== YARDIMCI FONKSİYONLAR ========== */

static long long widget_kaydet(HWND hwnd, WidgetTipi tip) {
    if (win_widget_sayisi >= MAKS_WIDGET || !hwnd) return -1;
    long long id = win_widget_sayisi;
    memset(&win_widgetlar[id], 0, sizeof(WidgetBilgi));
    win_widgetlar[id].hwnd = hwnd;
    win_widgetlar[id].tip = tip;
    win_widgetlar[id].tab_close_index = -1;
    win_widgetlar[id].istenen_genislik = -1;
    win_widgetlar[id].istenen_yukseklik = -1;
    win_widget_sayisi++;

    /* Varsayılan font ayarla */
    init_resources();
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);

    return id;
}

/* Extern erişim */
HWND _tr_arayuz_widget_al_hwnd(long long id) {
    if (id < 0 || id >= win_widget_sayisi) return NULL;
    return win_widgetlar[id].hwnd;
}

/* GTK uyumlu isim (pencere modülü kullanır) */
void *_tr_arayuz_widget_al(long long id) {
    return (void *)_tr_arayuz_widget_al_hwnd(id);
}

long long _tr_arayuz_widget_kaydet_harici_win(HWND hwnd, WidgetTipi tip) {
    return widget_kaydet(hwnd, tip);
}

/* GTK uyumlu isim (webgorunum modülü kullanır) */
long long _tr_arayuz_widget_kaydet_harici(void *w) {
    return widget_kaydet((HWND)w, WT_WEBVIEW_HOST);
}

/* Widget ID'den widget bul */
static long long widget_id_bul(HWND hwnd) {
    for (int i = 0; i < win_widget_sayisi; i++) {
        if (win_widgetlar[i].hwnd == hwnd) return i;
    }
    return -1;
}

/* ========== BOX LAYOUT ENGINE ========== */

void win_layout_yenile(long long id, int genislik, int yukseklik) {
    if (id < 0 || id >= win_widget_sayisi) return;
    WidgetBilgi *w = &win_widgetlar[id];

    if (w->tip != WT_HBOX && w->tip != WT_VBOX) return;
    if (w->çocuk_sayısı == 0) return;

    int yatay = (w->tip == WT_HBOX);

    /* Toplam boyut */
    int toplam = yatay ? genislik : yukseklik;
    int diger = yatay ? yukseklik : genislik;

    /* Sabit boyutlu çocukların toplam boyutunu hesapla */
    int sabit_toplam = 0;
    int genisleyen_sayisi = 0;
    int toplam_dolgu = 0;

    for (int i = 0; i < w->çocuk_sayısı; i++) {
        BoxCocuk *c = &w->çocuklar[i];
        toplam_dolgu += c->padding * 2;

        if (c->expand) {
            genisleyen_sayisi++;
        } else {
            /* Sabit boyutlu çocuğun boyutu */
            long long cid = widget_id_bul(c->hwnd);
            int boyut = 0;
            if (cid >= 0) {
                if (yatay) {
                    boyut = win_widgetlar[cid].istenen_genislik;
                } else {
                    boyut = win_widgetlar[cid].istenen_yukseklik;
                }
            }
            if (boyut <= 0) {
                /* Varsayılan minimum boyut */
                if (cid >= 0) {
                    switch (win_widgetlar[cid].tip) {
                    case WT_BUTTON: boyut = yatay ? 36 : 32; break;
                    case WT_LABEL: boyut = yatay ? 80 : 20; break;
                    case WT_ENTRY: boyut = yatay ? 200 : 28; break;
                    case WT_SEPARATOR: boyut = yatay ? 1 : 1; break;
                    case WT_PROGRESS: boyut = yatay ? 100 : 4; break;
                    default: boyut = yatay ? 48 : 32; break;
                    }
                } else {
                    boyut = 32;
                }
            }
            sabit_toplam += boyut;
        }
    }

    /* Genişleyen çocuklara kalan alan */
    int kalan = toplam - sabit_toplam - toplam_dolgu;
    if (kalan < 0) kalan = 0;
    int her_genisleyen = genisleyen_sayisi > 0 ? kalan / genisleyen_sayisi : 0;

    /* Çocukları yerleştir */
    int pozisyon = 0;

    for (int i = 0; i < w->çocuk_sayısı; i++) {
        BoxCocuk *c = &w->çocuklar[i];
        long long cid = widget_id_bul(c->hwnd);

        pozisyon += c->padding;

        int boyut;
        if (c->expand) {
            boyut = her_genisleyen;
        } else {
            if (cid >= 0) {
                if (yatay) {
                    boyut = win_widgetlar[cid].istenen_genislik;
                } else {
                    boyut = win_widgetlar[cid].istenen_yukseklik;
                }
            } else {
                boyut = 32;
            }
            if (boyut <= 0) {
                if (cid >= 0) {
                    switch (win_widgetlar[cid].tip) {
                    case WT_BUTTON: boyut = yatay ? 36 : 32; break;
                    case WT_LABEL: boyut = yatay ? 80 : 20; break;
                    case WT_ENTRY: boyut = yatay ? 200 : 28; break;
                    case WT_SEPARATOR: boyut = yatay ? 1 : 1; break;
                    case WT_PROGRESS: boyut = yatay ? 100 : 4; break;
                    default: boyut = yatay ? 48 : 32; break;
                    }
                } else {
                    boyut = 32;
                }
            }
        }

        int cx, cy, cw, ch;
        if (yatay) {
            cx = pozisyon; cy = 0;
            cw = boyut;
            ch = c->fill ? diger : (cid >= 0 && win_widgetlar[cid].istenen_yukseklik > 0 ?
                                     win_widgetlar[cid].istenen_yukseklik : diger);
        } else {
            cx = 0; cy = pozisyon;
            cw = c->fill ? diger : (cid >= 0 && win_widgetlar[cid].istenen_genislik > 0 ?
                                     win_widgetlar[cid].istenen_genislik : diger);
            ch = boyut;
        }

        MoveWindow(c->hwnd, cx, cy, cw, ch, TRUE);
        ShowWindow(c->hwnd, SW_SHOW);

        /* Alt kutuları da yenile */
        if (cid >= 0 && (win_widgetlar[cid].tip == WT_HBOX || win_widgetlar[cid].tip == WT_VBOX)) {
            win_layout_yenile(cid, cw, ch);
        }

        /* Tab kontrolü: içerik sayfasını boyutlandır */
        if (cid >= 0 && win_widgetlar[cid].tip == WT_NOTEBOOK) {
            RECT trc;
            GetClientRect(c->hwnd, &trc);
            SendMessageW(c->hwnd, TCM_ADJUSTRECT, FALSE, (LPARAM)&trc);
            for (int t = 0; t < win_widgetlar[cid].tab_sayfa_sayisi; t++) {
                if (win_widgetlar[cid].tab_sayfalar[t]) {
                    MoveWindow(win_widgetlar[cid].tab_sayfalar[t],
                               trc.left, trc.top,
                               trc.right - trc.left, trc.bottom - trc.top, TRUE);
                    long long tid = widget_id_bul(win_widgetlar[cid].tab_sayfalar[t]);
                    if (tid >= 0 && (win_widgetlar[tid].tip == WT_HBOX ||
                                     win_widgetlar[tid].tip == WT_VBOX)) {
                        win_layout_yenile(tid, trc.right - trc.left, trc.bottom - trc.top);
                    }
                }
            }
        }

        pozisyon += boyut + c->padding;
    }
}

/* ========== BOX WndProc ========== */

static LRESULT CALLBACK box_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE: {
        long long id = (long long)(intptr_t)GetPropW(hwnd, TR_BOX_PROP);
        RECT rc;
        GetClientRect(hwnd, &rc);
        win_layout_yenile(id, rc.right, rc.bottom);
        return 0;
    }

    case WM_COMMAND: {
        /* Buton tıklama veya Entry Enter */
        HWND ctrl = (HWND)lp;
        WORD code = HIWORD(wp);
        long long cid = widget_id_bul(ctrl);
        if (cid >= 0) {
            if (code == BN_CLICKED && win_widgetlar[cid].tip == WT_BUTTON) {
                win_widgetlar[cid].clicked = 1;
            }
            if (code == EN_CHANGE) {
                /* Placeholder */
            }
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lp;
        long long nbid = widget_id_bul(nm->hwndFrom);
        if (nbid >= 0 && win_widgetlar[nbid].tip == WT_NOTEBOOK) {
            if (nm->code == TCN_SELCHANGE) {
                win_widgetlar[nbid].tab_changed = 1;
                /* Aktif sayfayı göster, diğerlerini gizle */
                int secili = (int)SendMessageW(nm->hwndFrom, TCM_GETCURSEL, 0, 0);
                for (int i = 0; i < win_widgetlar[nbid].tab_sayfa_sayisi; i++) {
                    if (win_widgetlar[nbid].tab_sayfalar[i]) {
                        ShowWindow(win_widgetlar[nbid].tab_sayfalar[i],
                                   i == secili ? SW_SHOW : SW_HIDE);
                    }
                }
                /* Seçili sayfayı boyutlandır */
                if (secili >= 0 && secili < win_widgetlar[nbid].tab_sayfa_sayisi) {
                    RECT trc;
                    GetClientRect(nm->hwndFrom, &trc);
                    SendMessageW(nm->hwndFrom, TCM_ADJUSTRECT, FALSE, (LPARAM)&trc);
                    HWND sayfa = win_widgetlar[nbid].tab_sayfalar[secili];
                    if (sayfa) {
                        MoveWindow(sayfa, trc.left, trc.top,
                                   trc.right - trc.left, trc.bottom - trc.top, TRUE);
                        long long sid = widget_id_bul(sayfa);
                        if (sid >= 0) {
                            win_layout_yenile(sid, trc.right - trc.left, trc.bottom - trc.top);
                        }
                    }
                }
            }
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, RENK_METIN);
        SetBkColor(hdc, RENK_ARKAPLAN);
        return (LRESULT)hBrushBg;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, RENK_METIN);
        SetBkColor(hdc, RENK_YUZEY);
        return (LRESULT)hBrushSurface;
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, RENK_METIN);
        SetBkColor(hdc, RENK_ARKAPLAN);
        return (LRESULT)hBrushBg;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, hBrushBg);
        return 1;
    }
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int box_sinif_kayitli = 0;

static void box_sinif_kaydet(void) {
    if (box_sinif_kayitli) return;
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = box_wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hbrBackground = NULL;
    wc.lpszClassName = TR_BOX_CLASS;
    RegisterClassExW(&wc);
    box_sinif_kayitli = 1;
}

/* ========== KONTEYNERLER ========== */

long long _tr_arayuz_kutu_yatay(void) {
    init_resources();
    box_sinif_kaydet();
    HWND hwnd = CreateWindowExW(0, TR_BOX_CLASS, NULL,
                                 WS_CHILD | WS_CLIPCHILDREN,
                                 0, 0, 100, 100,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!hwnd) return -1;
    long long id = widget_kaydet(hwnd, WT_HBOX);
    if (id >= 0) SetPropW(hwnd, TR_BOX_PROP, (HANDLE)(intptr_t)id);
    return id;
}

long long _tr_arayuz_kutu_dikey(void) {
    init_resources();
    box_sinif_kaydet();
    HWND hwnd = CreateWindowExW(0, TR_BOX_CLASS, NULL,
                                 WS_CHILD | WS_CLIPCHILDREN,
                                 0, 0, 100, 100,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!hwnd) return -1;
    long long id = widget_kaydet(hwnd, WT_VBOX);
    if (id >= 0) SetPropW(hwnd, TR_BOX_PROP, (HANDLE)(intptr_t)id);
    return id;
}

long long _tr_arayuz_kaydirma(void) {
    init_resources();
    /* Win32'de basit ScrolledWindow = bir box gibi davranır */
    box_sinif_kaydet();
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, TR_BOX_CLASS, NULL,
                                 WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
                                 0, 0, 100, 100,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!hwnd) return -1;
    long long id = widget_kaydet(hwnd, WT_VBOX); /* Dikey box gibi davranır */
    if (id >= 0) SetPropW(hwnd, TR_BOX_PROP, (HANDLE)(intptr_t)id);
    return id;
}

/* ========== TEMEL WİDGET'LAR ========== */

long long _tr_arayuz_dugme(const char *etiket_ptr, long long etiket_uzunluk) {
    init_resources();
    wchar_t *etiket = metin_to_wide(etiket_ptr, etiket_uzunluk);
    if (!etiket) return -1;

    HWND hwnd = CreateWindowExW(0, L"BUTTON", etiket,
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
                                 0, 0, 80, 30,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    free(etiket);
    if (!hwnd) return -1;
    return widget_kaydet(hwnd, WT_BUTTON);
}

long long _tr_arayuz_etiket(const char *metin_ptr, long long metin_uzunluk) {
    init_resources();
    wchar_t *metin = metin_to_wide(metin_ptr, metin_uzunluk);
    if (!metin) return -1;

    HWND hwnd = CreateWindowExW(0, L"STATIC", metin,
                                 WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 0, 0, 200, 20,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    free(metin);
    if (!hwnd) return -1;
    return widget_kaydet(hwnd, WT_LABEL);
}

long long _tr_arayuz_giris(void) {
    init_resources();
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 0, 0, 200, 28,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!hwnd) return -1;
    long long id = widget_kaydet(hwnd, WT_ENTRY);

    /* Enter tuşu için alt sınıf */
    if (id >= 0) {
        /* Orijinal WndProc'u sakla */
        SetPropW(hwnd, L"TrEntryId", (HANDLE)(intptr_t)id);
    }
    return id;
}

long long _tr_arayuz_ayirici(void) {
    init_resources();
    HWND hwnd = CreateWindowExW(0, L"STATIC", NULL,
                                 WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                                 0, 0, 100, 2,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!hwnd) return -1;
    return widget_kaydet(hwnd, WT_SEPARATOR);
}

long long _tr_arayuz_resim_dosyadan(const char *yol_ptr, long long yol_uzunluk) {
    init_resources();
    wchar_t *yol = metin_to_wide(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    HBITMAP hbmp = (HBITMAP)LoadImageW(NULL, yol, IMAGE_BITMAP, 0, 0,
                                        LR_LOADFROMFILE);
    free(yol);

    HWND hwnd = CreateWindowExW(0, L"STATIC", NULL,
                                 WS_CHILD | WS_VISIBLE | SS_BITMAP,
                                 0, 0, 64, 64,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!hwnd) return -1;
    if (hbmp) SendMessageW(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbmp);
    return widget_kaydet(hwnd, WT_IMAGE);
}

/* ========== GİRİŞ ALANI İŞLEMLERİ ========== */

TrMetin _tr_arayuz_giris_metni_al(long long id) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return bos_metin();
    int len = GetWindowTextLengthW(win_widgetlar[id].hwnd);
    wchar_t *buf = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!buf) return bos_metin();
    GetWindowTextW(win_widgetlar[id].hwnd, buf, len + 1);
    char *utf8 = wide_to_utf8(buf);
    free(buf);
    if (!utf8) return bos_metin();
    TrMetin m = cstr_to_metin(utf8);
    free(utf8);
    return m;
}

long long _tr_arayuz_giris_metni_ayarla(long long id,
                                          const char *metin_ptr, long long metin_uzunluk) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return -1;
    wchar_t *metin = metin_to_wide(metin_ptr, metin_uzunluk);
    if (!metin) return -1;
    SetWindowTextW(win_widgetlar[id].hwnd, metin);
    free(metin);
    return 0;
}

long long _tr_arayuz_giris_ipucu_ayarla(long long id,
                                          const char *ipucu_ptr, long long ipucu_uzunluk) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return -1;
    wchar_t *ipucu = metin_to_wide(ipucu_ptr, ipucu_uzunluk);
    if (!ipucu) return -1;
    SendMessageW(win_widgetlar[id].hwnd, EM_SETCUEBANNER, TRUE, (LPARAM)ipucu);
    free(ipucu);
    return 0;
}

/* ========== ETİKET İŞLEMLERİ ========== */

long long _tr_arayuz_etiket_metni_ayarla(long long id,
                                           const char *metin_ptr, long long metin_uzunluk) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return -1;
    wchar_t *metin = metin_to_wide(metin_ptr, metin_uzunluk);
    if (!metin) return -1;
    SetWindowTextW(win_widgetlar[id].hwnd, metin);
    free(metin);
    return 0;
}

TrMetin _tr_arayuz_etiket_metni_al(long long id) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return bos_metin();
    int len = GetWindowTextLengthW(win_widgetlar[id].hwnd);
    wchar_t *buf = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!buf) return bos_metin();
    GetWindowTextW(win_widgetlar[id].hwnd, buf, len + 1);
    char *utf8 = wide_to_utf8(buf);
    free(buf);
    if (!utf8) return bos_metin();
    TrMetin m = cstr_to_metin(utf8);
    free(utf8);
    return m;
}

/* ========== SEKME YÖNETİMİ (Tab Control) ========== */

long long _tr_arayuz_sekmeler(void) {
    init_resources();
    HWND hwnd = CreateWindowExW(0, WC_TABCONTROLW, NULL,
                                 WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FOCUSNEVER,
                                 0, 0, 400, 300,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!hwnd) return -1;
    long long id = widget_kaydet(hwnd, WT_NOTEBOOK);
    return id;
}

/* Sekme ekleme (basit) */
long long _tr_arayuz_sekme_ekle(long long nb_id, long long icerik_id,
                                  const char *baslik_ptr, long long baslik_uzunluk) {
    if (nb_id < 0 || nb_id >= win_widget_sayisi || !win_widgetlar[nb_id].hwnd) return -1;
    if (icerik_id < 0 || icerik_id >= win_widget_sayisi || !win_widgetlar[icerik_id].hwnd) return -1;

    wchar_t *baslik = metin_to_wide(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;

    WidgetBilgi *nb = &win_widgetlar[nb_id];
    int indeks = nb->tab_sayfa_sayisi;

    /* Tab ekle */
    TCITEMW tci = {0};
    tci.mask = TCIF_TEXT;
    tci.pszText = baslik;
    SendMessageW(nb->hwnd, TCM_INSERTITEMW, indeks, (LPARAM)&tci);
    free(baslik);

    /* Sayfa widget'ını tab kontrolünün çocuğu yap */
    HWND sayfa = win_widgetlar[icerik_id].hwnd;
    SetParent(sayfa, nb->hwnd);
    nb->tab_sayfalar[indeks] = sayfa;
    nb->tab_kapatmali[indeks] = 0;
    nb->tab_sayfa_sayisi++;

    /* İlk sekme ise göster, diğerlerini gizle */
    int secili = (int)SendMessageW(nb->hwnd, TCM_GETCURSEL, 0, 0);
    ShowWindow(sayfa, (indeks == secili || secili == -1) ? SW_SHOW : SW_HIDE);

    if (secili == -1) {
        SendMessageW(nb->hwnd, TCM_SETCURSEL, 0, 0);
    }

    /* Boyutlandır */
    RECT trc;
    GetClientRect(nb->hwnd, &trc);
    SendMessageW(nb->hwnd, TCM_ADJUSTRECT, FALSE, (LPARAM)&trc);
    MoveWindow(sayfa, trc.left, trc.top,
               trc.right - trc.left, trc.bottom - trc.top, TRUE);

    return (long long)indeks;
}

/* Kapatmalı sekme ekleme (tab text'ine X ekler) */
long long _tr_arayuz_sekme_kapatmali_ekle(long long nb_id, long long icerik_id,
                                            const char *baslik_ptr, long long baslik_uzunluk) {
    if (nb_id < 0 || nb_id >= win_widget_sayisi || !win_widgetlar[nb_id].hwnd) return -1;
    if (icerik_id < 0 || icerik_id >= win_widget_sayisi || !win_widgetlar[icerik_id].hwnd) return -1;

    /* Başlığa " ×" ekle */
    char *baslik = metin_to_cstr(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;
    int blen = (int)strlen(baslik);
    char *tam_baslik = (char *)malloc(blen + 4);
    if (!tam_baslik) { free(baslik); return -1; }
    snprintf(tam_baslik, blen + 4, "%s  x", baslik);
    free(baslik);

    wchar_t *wbaslik = utf8_to_wide(tam_baslik);
    free(tam_baslik);
    if (!wbaslik) return -1;

    WidgetBilgi *nb = &win_widgetlar[nb_id];
    int indeks = nb->tab_sayfa_sayisi;

    TCITEMW tci = {0};
    tci.mask = TCIF_TEXT;
    tci.pszText = wbaslik;
    SendMessageW(nb->hwnd, TCM_INSERTITEMW, indeks, (LPARAM)&tci);
    free(wbaslik);

    HWND sayfa = win_widgetlar[icerik_id].hwnd;
    SetParent(sayfa, nb->hwnd);
    nb->tab_sayfalar[indeks] = sayfa;
    nb->tab_kapatmali[indeks] = 1;
    nb->tab_sayfa_sayisi++;

    int secili = (int)SendMessageW(nb->hwnd, TCM_GETCURSEL, 0, 0);
    ShowWindow(sayfa, (indeks == secili || secili == -1) ? SW_SHOW : SW_HIDE);
    if (secili == -1) SendMessageW(nb->hwnd, TCM_SETCURSEL, 0, 0);

    RECT trc;
    GetClientRect(nb->hwnd, &trc);
    SendMessageW(nb->hwnd, TCM_ADJUSTRECT, FALSE, (LPARAM)&trc);
    MoveWindow(sayfa, trc.left, trc.top,
               trc.right - trc.left, trc.bottom - trc.top, TRUE);

    return (long long)indeks;
}

long long _tr_arayuz_sekme_kaldir(long long nb_id, long long indeks) {
    if (nb_id < 0 || nb_id >= win_widget_sayisi || !win_widgetlar[nb_id].hwnd) return -1;
    WidgetBilgi *nb = &win_widgetlar[nb_id];
    int idx = (int)indeks;
    if (idx < 0 || idx >= nb->tab_sayfa_sayisi) return -1;

    /* Sayfayı gizle */
    if (nb->tab_sayfalar[idx]) {
        ShowWindow(nb->tab_sayfalar[idx], SW_HIDE);
    }

    /* Tab'ı sil */
    SendMessageW(nb->hwnd, TCM_DELETEITEM, idx, 0);

    /* Sayfa dizisini kaydır */
    for (int i = idx; i < nb->tab_sayfa_sayisi - 1; i++) {
        nb->tab_sayfalar[i] = nb->tab_sayfalar[i + 1];
        nb->tab_kapatmali[i] = nb->tab_kapatmali[i + 1];
    }
    nb->tab_sayfa_sayisi--;

    /* Yeni seçili sayfayı göster */
    if (nb->tab_sayfa_sayisi > 0) {
        int yeni = idx < nb->tab_sayfa_sayisi ? idx : nb->tab_sayfa_sayisi - 1;
        SendMessageW(nb->hwnd, TCM_SETCURSEL, yeni, 0);
        for (int i = 0; i < nb->tab_sayfa_sayisi; i++) {
            if (nb->tab_sayfalar[i]) {
                ShowWindow(nb->tab_sayfalar[i], i == yeni ? SW_SHOW : SW_HIDE);
            }
        }
    }
    return 0;
}

long long _tr_arayuz_sekme_secili(long long nb_id) {
    if (nb_id < 0 || nb_id >= win_widget_sayisi || !win_widgetlar[nb_id].hwnd) return -1;
    return (long long)SendMessageW(win_widgetlar[nb_id].hwnd, TCM_GETCURSEL, 0, 0);
}

long long _tr_arayuz_sekme_sec(long long nb_id, long long indeks) {
    if (nb_id < 0 || nb_id >= win_widget_sayisi || !win_widgetlar[nb_id].hwnd) return -1;
    WidgetBilgi *nb = &win_widgetlar[nb_id];
    int idx = (int)indeks;

    SendMessageW(nb->hwnd, TCM_SETCURSEL, idx, 0);

    /* Sayfaları güncelle */
    for (int i = 0; i < nb->tab_sayfa_sayisi; i++) {
        if (nb->tab_sayfalar[i]) {
            ShowWindow(nb->tab_sayfalar[i], i == idx ? SW_SHOW : SW_HIDE);
        }
    }
    nb->tab_changed = 1;
    return 0;
}

long long _tr_arayuz_sekme_baslik_ayarla(long long nb_id, long long indeks,
                                           const char *baslik_ptr, long long baslik_uzunluk) {
    if (nb_id < 0 || nb_id >= win_widget_sayisi || !win_widgetlar[nb_id].hwnd) return -1;

    /* Kapatmalı sekme ise " x" ekle */
    char *baslik = metin_to_cstr(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;

    int idx = (int)indeks;
    wchar_t *wbaslik;

    if (idx >= 0 && idx < win_widgetlar[nb_id].tab_sayfa_sayisi &&
        win_widgetlar[nb_id].tab_kapatmali[idx]) {
        int blen = (int)strlen(baslik);
        char *tam = (char *)malloc(blen + 4);
        if (!tam) { free(baslik); return -1; }
        snprintf(tam, blen + 4, "%s  x", baslik);
        free(baslik);
        wbaslik = utf8_to_wide(tam);
        free(tam);
    } else {
        wbaslik = utf8_to_wide(baslik);
        free(baslik);
    }
    if (!wbaslik) return -1;

    TCITEMW tci = {0};
    tci.mask = TCIF_TEXT;
    tci.pszText = wbaslik;
    SendMessageW(win_widgetlar[nb_id].hwnd, TCM_SETITEMW, idx, (LPARAM)&tci);
    free(wbaslik);
    return 0;
}

long long _tr_arayuz_sekme_sayisi(long long nb_id) {
    if (nb_id < 0 || nb_id >= win_widget_sayisi || !win_widgetlar[nb_id].hwnd) return 0;
    return (long long)win_widgetlar[nb_id].tab_sayfa_sayisi;
}

long long _tr_arayuz_sekme_kapandi_mi(long long nb_id) {
    if (nb_id < 0 || nb_id >= win_widget_sayisi) return -1;
    if (win_widgetlar[nb_id].tab_close_requested) {
        win_widgetlar[nb_id].tab_close_requested = 0;
        return (long long)win_widgetlar[nb_id].tab_close_index;
    }
    return -1;
}

/* ========== YERLEŞIM ========== */

long long _tr_arayuz_ekle(long long konteyner_id, long long widget_id) {
    if (konteyner_id < 0 || konteyner_id >= win_widget_sayisi || !win_widgetlar[konteyner_id].hwnd)
        return -1;
    if (widget_id < 0 || widget_id >= win_widget_sayisi || !win_widgetlar[widget_id].hwnd)
        return -1;

    SetParent(win_widgetlar[widget_id].hwnd, win_widgetlar[konteyner_id].hwnd);

    /* Box konteyner ise çocuk listesine ekle */
    WidgetBilgi *k = &win_widgetlar[konteyner_id];
    if ((k->tip == WT_HBOX || k->tip == WT_VBOX) && k->çocuk_sayısı < MAKS_COCUK) {
        BoxCocuk *c = &k->çocuklar[k->çocuk_sayısı];
        c->hwnd = win_widgetlar[widget_id].hwnd;
        c->expand = 0;
        c->fill = 1;
        c->padding = 0;
        k->çocuk_sayısı++;
    }
    return 0;
}

long long _tr_arayuz_pakle(long long kutu_id, long long widget_id,
                             long long genisle, long long doldur, long long bosluk) {
    if (kutu_id < 0 || kutu_id >= win_widget_sayisi || !win_widgetlar[kutu_id].hwnd)
        return -1;
    if (widget_id < 0 || widget_id >= win_widget_sayisi || !win_widgetlar[widget_id].hwnd)
        return -1;

    SetParent(win_widgetlar[widget_id].hwnd, win_widgetlar[kutu_id].hwnd);

    WidgetBilgi *k = &win_widgetlar[kutu_id];
    if ((k->tip == WT_HBOX || k->tip == WT_VBOX) && k->çocuk_sayısı < MAKS_COCUK) {
        BoxCocuk *c = &k->çocuklar[k->çocuk_sayısı];
        c->hwnd = win_widgetlar[widget_id].hwnd;
        c->expand = genisle ? 1 : 0;
        c->fill = doldur ? 1 : 0;
        c->padding = (int)bosluk;
        k->çocuk_sayısı++;
    }
    return 0;
}

long long _tr_arayuz_goster_tumu(long long id) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return -1;
    ShowWindow(win_widgetlar[id].hwnd, SW_SHOW);
    /* Çocukları da göster */
    WidgetBilgi *w = &win_widgetlar[id];
    for (int i = 0; i < w->çocuk_sayısı; i++) {
        ShowWindow(w->çocuklar[i].hwnd, SW_SHOW);
    }
    return 0;
}

/* ========== OLAY YOKLAMA ========== */

long long _tr_arayuz_dugme_basildi_mi(long long id) {
    if (id < 0 || id >= win_widget_sayisi) return 0;
    if (win_widgetlar[id].clicked) {
        win_widgetlar[id].clicked = 0;
        return 1;
    }
    return 0;
}

long long _tr_arayuz_giris_enter_mi(long long id) {
    if (id < 0 || id >= win_widget_sayisi) return 0;
    if (win_widgetlar[id].enter) {
        win_widgetlar[id].enter = 0;
        return 1;
    }
    return 0;
}

long long _tr_arayuz_sekme_degisti_mi(long long id) {
    if (id < 0 || id >= win_widget_sayisi) return 0;
    if (win_widgetlar[id].tab_changed) {
        win_widgetlar[id].tab_changed = 0;
        return 1;
    }
    return 0;
}

/* ========== CSS TEMA (Win32'de stub) ========== */

long long _tr_arayuz_css_yukle(const char *css_ptr, long long css_uzunluk) {
    (void)css_ptr; (void)css_uzunluk;
    /* Win32'de CSS desteklenmiyor, koyu tema otomatik uygulanır */
    return 0;
}

long long _tr_arayuz_css_sinif_ekle(long long id,
                                      const char *sinif_ptr, long long sinif_uzunluk) {
    (void)id; (void)sinif_ptr; (void)sinif_uzunluk;
    return 0;
}

long long _tr_arayuz_css_sinif_kaldir(long long id,
                                        const char *sinif_ptr, long long sinif_uzunluk) {
    (void)id; (void)sinif_ptr; (void)sinif_uzunluk;
    return 0;
}

/* ========== WİDGET ÖZELLİKLERİ ========== */

long long _tr_arayuz_widget_genislik_ayarla(long long id, long long genislik) {
    if (id < 0 || id >= win_widget_sayisi) return -1;
    win_widgetlar[id].istenen_genislik = (int)genislik;
    return 0;
}

long long _tr_arayuz_widget_yukseklik_ayarla(long long id, long long yukseklik) {
    if (id < 0 || id >= win_widget_sayisi) return -1;
    win_widgetlar[id].istenen_yukseklik = (int)yukseklik;
    return 0;
}

/* ========== İLERLEME ÇUBUĞU ========== */

long long _tr_arayuz_ilerleme_cubugu(void) {
    init_resources();
    HWND hwnd = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                                 WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                 0, 0, 100, 4,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!hwnd) return -1;
    SendMessageW(hwnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    return widget_kaydet(hwnd, WT_PROGRESS);
}

long long _tr_arayuz_ilerleme_ayarla(long long id, long long deger) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return -1;
    if (deger < 0) deger = 0;
    if (deger > 100) deger = 100;
    SendMessageW(win_widgetlar[id].hwnd, PBM_SETPOS, (WPARAM)deger, 0);
    return 0;
}

/* ========== İPUCU ========== */

long long _tr_arayuz_ipucu_ayarla(long long id,
                                    const char *metin_ptr, long long metin_uzunluk) {
    (void)id; (void)metin_ptr; (void)metin_uzunluk;
    /* Win32 tooltip sistemi daha karmaşık, şimdilik stub */
    return 0;
}

/* ========== GÖRÜNÜRLÜK ========== */

long long _tr_arayuz_widget_gizle(long long id) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return -1;
    ShowWindow(win_widgetlar[id].hwnd, SW_HIDE);
    return 0;
}

long long _tr_arayuz_widget_goster(long long id) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return -1;
    ShowWindow(win_widgetlar[id].hwnd, SW_SHOW);
    return 0;
}

/* ========== DÜĞME İŞLEMLERİ ========== */

long long _tr_arayuz_dugme_etiket_ayarla(long long id,
                                           const char *metin_ptr, long long metin_uzunluk) {
    if (id < 0 || id >= win_widget_sayisi || !win_widgetlar[id].hwnd) return -1;
    wchar_t *metin = metin_to_wide(metin_ptr, metin_uzunluk);
    if (!metin) return -1;
    SetWindowTextW(win_widgetlar[id].hwnd, metin);
    free(metin);
    return 0;
}
