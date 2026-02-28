/* Pencere modülü — Windows çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * Win32 pencere yönetimi
 */

#include "win_ortak.h"
#include <dwmapi.h>

/* ========== DAHİLİ DURUM ========== */

#define MAKS_PENCERE 64

typedef struct {
    HWND hwnd;
    int kapatildi;
    long long icerik_widget_id; /* -1 = yok */
} PencereBilgi;

static PencereBilgi pencereler[MAKS_PENCERE];
static int pencere_sayisi = 0;
static int win_baslatildi = 0;

/* ========== PENCERE WndProc ========== */

static LRESULT CALLBACK pencere_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    long long id = (long long)(intptr_t)GetPropW(hwnd, TR_PENCERE_PROP);

    switch (msg) {
    case WM_CLOSE:
        if (id >= 0 && id < pencere_sayisi) {
            pencereler[id].kapatildi = 1;
        }
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_SIZE: {
        if (id >= 0 && id < pencere_sayisi && pencereler[id].icerik_widget_id >= 0) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            long long cid = pencereler[id].icerik_widget_id;
            HWND child = _tr_arayuz_widget_al_hwnd(cid);
            if (child) {
                MoveWindow(child, 0, 0, rc.right, rc.bottom, TRUE);
                win_layout_yenile(cid, rc.right, rc.bottom);
            }
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, RENK_METIN);
        SetBkColor(hdc, RENK_ARKAPLAN);
        static HBRUSH hBrush = NULL;
        if (!hBrush) hBrush = CreateSolidBrush(RENK_ARKAPLAN);
        return (LRESULT)hBrush;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(RENK_ARKAPLAN);
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        return 1;
    }

    case WM_DESTROY:
        if (id >= 0 && id < pencere_sayisi) {
            pencereler[id].kapatildi = 1;
            pencereler[id].hwnd = NULL;
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ========== BAŞLATMA ========== */

long long _tr_pencere_baslat(void) {
    if (win_baslatildi) return 0;

    /* Common Controls başlat */
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_TAB_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    /* Pencere sınıfını kaydet */
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = pencere_wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = TR_PENCERE_CLASS;
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    memset(pencereler, 0, sizeof(pencereler));
    win_baslatildi = 1;
    return 0;
}

/* ========== PENCERE OLUŞTURMA ========== */

long long _tr_pencere_olustur(const char *baslik_ptr, long long baslik_uzunluk,
                               long long genislik, long long yukseklik) {
    if (!win_baslatildi) _tr_pencere_baslat();
    if (pencere_sayisi >= MAKS_PENCERE) return -1;

    wchar_t *baslik = metin_to_wide(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;

    long long id = pencere_sayisi;

    HWND hwnd = CreateWindowExW(
        0, TR_PENCERE_CLASS, baslik,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        (int)genislik, (int)yukseklik,
        NULL, NULL, GetModuleHandleW(NULL), NULL
    );
    free(baslik);

    if (!hwnd) return -1;

    SetPropW(hwnd, TR_PENCERE_PROP, (HANDLE)(intptr_t)id);

    /* DWM koyu mod (Windows 10 1903+) */
    BOOL useDarkMode = TRUE;
    /* DWMWA_USE_IMMERSIVE_DARK_MODE = 20 */
    DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));

    pencereler[id].hwnd = hwnd;
    pencereler[id].kapatildi = 0;
    pencereler[id].icerik_widget_id = -1;
    pencere_sayisi++;

    return id;
}

/* ========== PENCERE ÖZELLİKLERİ ========== */

long long _tr_pencere_baslik_ayarla(long long id,
                                     const char *baslik_ptr, long long baslik_uzunluk) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].hwnd) return -1;
    wchar_t *baslik = metin_to_wide(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;
    SetWindowTextW(pencereler[id].hwnd, baslik);
    free(baslik);
    return 0;
}

long long _tr_pencere_boyut_ayarla(long long id, long long genislik, long long yukseklik) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].hwnd) return -1;
    SetWindowPos(pencereler[id].hwnd, NULL, 0, 0,
                 (int)genislik, (int)yukseklik, SWP_NOMOVE | SWP_NOZORDER);
    return 0;
}

long long _tr_pencere_simge_ayarla(long long id,
                                    const char *yol_ptr, long long yol_uzunluk) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].hwnd) return -1;
    wchar_t *yol = metin_to_wide(yol_ptr, yol_uzunluk);
    if (!yol) return -1;
    HICON hIcon = (HICON)LoadImageW(NULL, yol, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    free(yol);
    if (hIcon) {
        SendMessageW(pencereler[id].hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessageW(pencereler[id].hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }
    return 0;
}

/* ========== GÖRÜNÜRLÜK ========== */

long long _tr_pencere_goster(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].hwnd) return -1;
    ShowWindow(pencereler[id].hwnd, SW_SHOW);
    UpdateWindow(pencereler[id].hwnd);

    /* İlk gösterimde icerik widget'ını boyutlandır */
    if (pencereler[id].icerik_widget_id >= 0) {
        RECT rc;
        GetClientRect(pencereler[id].hwnd, &rc);
        HWND child = _tr_arayuz_widget_al_hwnd(pencereler[id].icerik_widget_id);
        if (child) {
            MoveWindow(child, 0, 0, rc.right, rc.bottom, TRUE);
            win_layout_yenile(pencereler[id].icerik_widget_id, rc.right, rc.bottom);
        }
    }
    return 0;
}

long long _tr_pencere_gizle(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].hwnd) return -1;
    ShowWindow(pencereler[id].hwnd, SW_HIDE);
    return 0;
}

long long _tr_pencere_kapat(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].hwnd) return -1;
    DestroyWindow(pencereler[id].hwnd);
    pencereler[id].hwnd = NULL;
    pencereler[id].kapatildi = 1;
    return 0;
}

/* ========== TAM EKRAN ========== */

static RECT onceki_pencere_konumu[MAKS_PENCERE];
static DWORD onceki_pencere_stili[MAKS_PENCERE];

long long _tr_pencere_tam_ekran(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].hwnd) return -1;
    HWND hwnd = pencereler[id].hwnd;
    onceki_pencere_stili[id] = GetWindowLong(hwnd, GWL_STYLE);
    GetWindowRect(hwnd, &onceki_pencere_konumu[id]);
    SetWindowLong(hwnd, GWL_STYLE, onceki_pencere_stili[id] & ~WS_OVERLAPPEDWINDOW);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    SetWindowPos(hwnd, HWND_TOP,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    return 0;
}

long long _tr_pencere_tam_ekrandan_cik(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].hwnd) return -1;
    HWND hwnd = pencereler[id].hwnd;
    SetWindowLong(hwnd, GWL_STYLE, onceki_pencere_stili[id]);
    RECT *rc = &onceki_pencere_konumu[id];
    SetWindowPos(hwnd, NULL,
                 rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                 SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOZORDER);
    return 0;
}

/* ========== OLAY DÖNGÜSÜ ========== */

long long _tr_pencere_olaylari_isle(void) {
    if (!win_baslatildi) return 0;

    /* Bekleyen tüm Windows mesajlarını işle (non-blocking) */
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            /* Tüm pencereleri kapalı işaretle */
            for (int i = 0; i < pencere_sayisi; i++) {
                pencereler[i].kapatildi = 1;
            }
            return 0;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* Açık pencere var mı kontrol et */
    int acik_var = 0;
    for (int i = 0; i < pencere_sayisi; i++) {
        if (!pencereler[i].kapatildi) {
            acik_var = 1;
            break;
        }
    }
    return acik_var ? 1 : 0;
}

long long _tr_pencere_calistir(void) {
    if (!win_baslatildi) return -1;
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

/* ========== DURUM SORGULAMA ========== */

long long _tr_pencere_kapatildi_mi(long long id) {
    if (id < 0 || id >= pencere_sayisi) return 1;
    return pencereler[id].kapatildi ? 1 : 0;
}

/* ========== İÇERİK ========== */

long long _tr_pencere_icerik_ayarla(long long pencere_id, long long widget_id) {
    if (pencere_id < 0 || pencere_id >= pencere_sayisi || !pencereler[pencere_id].hwnd)
        return -1;

    HWND child = _tr_arayuz_widget_al_hwnd(widget_id);
    if (!child) return -1;

    SetParent(child, pencereler[pencere_id].hwnd);
    pencereler[pencere_id].icerik_widget_id = widget_id;

    /* Hemen boyutlandır */
    RECT rc;
    GetClientRect(pencereler[pencere_id].hwnd, &rc);
    MoveWindow(child, 0, 0, rc.right, rc.bottom, TRUE);
    win_layout_yenile(widget_id, rc.right, rc.bottom);

    return 0;
}
