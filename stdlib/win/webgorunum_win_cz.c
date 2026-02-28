/* WebGörünüm modülü — Windows çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * Microsoft WebView2 (Edge/Chromium)
 * COM arayüzlerini doğrudan vtable erişimiyle kullanır.
 */

#include "win_ortak.h"

/* ========== MİNİMAL COM TANIMLAMALARI ========== */

/* Genel COM nesne yapısı (vtable pointer array olarak erişim) */
typedef struct { void **lpVtbl; } ComObj;

/* COM metod çağırma makroları (vtable indeksi ile) */
#define COM_CALL0(obj, idx) \
    ((HRESULT (__stdcall *)(void *))(((ComObj*)(obj))->lpVtbl[(idx)]))(obj)
#define COM_CALL1(obj, idx, a1) \
    ((HRESULT (__stdcall *)(void *, void *))(((ComObj*)(obj))->lpVtbl[(idx)]))(obj, (void*)(a1))
#define COM_CALL2(obj, idx, a1, a2) \
    ((HRESULT (__stdcall *)(void *, void *, void *))(((ComObj*)(obj))->lpVtbl[(idx)]))(obj, (void*)(a1), (void*)(a2))

/* ICoreWebView2 vtable indeksleri */
#define WV2_GET_SETTINGS         3
#define WV2_GET_SOURCE           4
#define WV2_NAVIGATE             5
#define WV2_NAVIGATE_TO_STRING   6
#define WV2_ADD_SOURCE_CHANGED   11
#define WV2_ADD_NAV_COMPLETED    15
#define WV2_EXECUTE_SCRIPT       29
#define WV2_RELOAD               31
#define WV2_GET_CAN_GO_BACK      38
#define WV2_GET_CAN_GO_FORWARD   39
#define WV2_GO_BACK              40
#define WV2_GO_FORWARD           41
#define WV2_STOP                 43
#define WV2_ADD_DOC_TITLE_CHANGED 46
#define WV2_GET_DOC_TITLE        48

/* ICoreWebView2Controller vtable indeksleri */
#define CTRL_PUT_BOUNDS          6
#define CTRL_GET_ZOOM            7
#define CTRL_PUT_ZOOM            8
#define CTRL_CLOSE               24
#define CTRL_GET_WEBVIEW         25

/* ICoreWebView2Environment vtable indeksleri */
#define ENV_CREATE_CONTROLLER    3

/* WebView2Loader.dll fonksiyon tipi */
typedef HRESULT (__stdcall *CreateEnvWithOptionsFn)(
    LPCWSTR browserExePath,
    LPCWSTR userDataFolder,
    void *envOptions,
    void *envCompletedHandler
);

/* ========== OLAY İŞLEYİCİ (Completion Handler) ========== */

/* Basit COM handler: IUnknown + Invoke */
/* EnvironmentCompleted handler */
typedef struct {
    void **lpVtbl;
    LONG ref_count;
    void *environment; /* Sonuç: ICoreWebView2Environment* */
    BOOL tamamlandi;
} EnvHandler;

static HRESULT __stdcall eh_qi(void *This, const IID *riid, void **ppv) {
    (void)riid;
    *ppv = This;
    return 0; /* S_OK */
}
static ULONG __stdcall eh_addref(void *This) {
    return InterlockedIncrement(&((EnvHandler *)This)->ref_count);
}
static ULONG __stdcall eh_release(void *This) {
    return InterlockedDecrement(&((EnvHandler *)This)->ref_count);
}
static HRESULT __stdcall eh_invoke(void *This, HRESULT errorCode, void *env) {
    EnvHandler *h = (EnvHandler *)This;
    if (SUCCEEDED(errorCode) && env) {
        h->environment = env;
        /* AddRef */
        ((ULONG (__stdcall *)(void *))((ComObj *)env)->lpVtbl[1])(env);
    }
    h->tamamlandi = TRUE;
    return 0;
}

static void *eh_vtbl_data[4] = {
    (void *)eh_qi, (void *)eh_addref, (void *)eh_release, (void *)eh_invoke
};

/* ControllerCompleted handler */
typedef struct {
    void **lpVtbl;
    LONG ref_count;
    void *controller; /* Sonuç: ICoreWebView2Controller* */
    BOOL tamamlandi;
} CtrlHandler;

static HRESULT __stdcall ch_qi(void *This, const IID *riid, void **ppv) {
    (void)riid;
    *ppv = This;
    return 0;
}
static ULONG __stdcall ch_addref(void *This) {
    return InterlockedIncrement(&((CtrlHandler *)This)->ref_count);
}
static ULONG __stdcall ch_release(void *This) {
    return InterlockedDecrement(&((CtrlHandler *)This)->ref_count);
}
static HRESULT __stdcall ch_invoke(void *This, HRESULT errorCode, void *ctrl) {
    CtrlHandler *h = (CtrlHandler *)This;
    if (SUCCEEDED(errorCode) && ctrl) {
        h->controller = ctrl;
        ((ULONG (__stdcall *)(void *))((ComObj *)ctrl)->lpVtbl[1])(ctrl);
    }
    h->tamamlandi = TRUE;
    return 0;
}

static void *ch_vtbl_data[4] = {
    (void *)ch_qi, (void *)ch_addref, (void *)ch_release, (void *)ch_invoke
};

/* DocumentTitleChanged olay işleyici */
typedef struct {
    void **lpVtbl;
    LONG ref_count;
    int *bayrak; /* İşaret edilecek bayrak */
} TitleHandler;

static HRESULT __stdcall th_qi(void *This, const IID *riid, void **ppv) {
    (void)riid; *ppv = This; return 0;
}
static ULONG __stdcall th_addref(void *This) {
    return InterlockedIncrement(&((TitleHandler *)This)->ref_count);
}
static ULONG __stdcall th_release(void *This) {
    LONG r = InterlockedDecrement(&((TitleHandler *)This)->ref_count);
    if (r == 0) free(This);
    return r;
}
static HRESULT __stdcall th_invoke(void *This, void *sender, void *args) {
    (void)sender; (void)args;
    TitleHandler *h = (TitleHandler *)This;
    if (h->bayrak) *(h->bayrak) = 1;
    return 0;
}

static void *th_vtbl_data[4] = {
    (void *)th_qi, (void *)th_addref, (void *)th_release, (void *)th_invoke
};

/* SourceChanged olay işleyici */
typedef struct {
    void **lpVtbl;
    LONG ref_count;
    int *bayrak;
} SourceHandler;

static HRESULT __stdcall sh_qi(void *This, const IID *riid, void **ppv) {
    (void)riid; *ppv = This; return 0;
}
static ULONG __stdcall sh_addref(void *This) {
    return InterlockedIncrement(&((SourceHandler *)This)->ref_count);
}
static ULONG __stdcall sh_release(void *This) {
    LONG r = InterlockedDecrement(&((SourceHandler *)This)->ref_count);
    if (r == 0) free(This);
    return r;
}
static HRESULT __stdcall sh_invoke(void *This, void *sender, void *args) {
    (void)sender; (void)args;
    SourceHandler *h = (SourceHandler *)This;
    if (h->bayrak) *(h->bayrak) = 1;
    return 0;
}

static void *sh_vtbl_data[4] = {
    (void *)sh_qi, (void *)sh_addref, (void *)sh_release, (void *)sh_invoke
};

/* NavigationCompleted olay işleyici */
typedef struct {
    void **lpVtbl;
    LONG ref_count;
    int *bayrak;
} NavCompHandler;

static HRESULT __stdcall nc_qi(void *This, const IID *riid, void **ppv) {
    (void)riid; *ppv = This; return 0;
}
static ULONG __stdcall nc_addref(void *This) {
    return InterlockedIncrement(&((NavCompHandler *)This)->ref_count);
}
static ULONG __stdcall nc_release(void *This) {
    LONG r = InterlockedDecrement(&((NavCompHandler *)This)->ref_count);
    if (r == 0) free(This);
    return r;
}
static HRESULT __stdcall nc_invoke(void *This, void *sender, void *args) {
    (void)sender; (void)args;
    NavCompHandler *h = (NavCompHandler *)This;
    if (h->bayrak) *(h->bayrak) = 1;
    return 0;
}

static void *nc_vtbl_data[4] = {
    (void *)nc_qi, (void *)nc_addref, (void *)nc_release, (void *)nc_invoke
};

/* ========== DAHİLİ DURUM ========== */

#define MAKS_WEBGORUNUM 256

typedef struct {
    void *controller;    /* ICoreWebView2Controller* */
    void *webview;       /* ICoreWebView2* */
    HWND host_hwnd;      /* WebView2'nin barındığı pencere */
    long long arayuz_id; /* Widget sistemi ID'si */
    int baslik_degisti;
    int adres_degisti;
    int yuklenme_bitti;
    int hazir;           /* WebView2 başlatıldı mı? */
} WebGorunumBilgi;

static WebGorunumBilgi gorunumler[MAKS_WEBGORUNUM];
static int gorunum_sayisi = 0;

/* WebView2 environment (paylaşılan) */
static void *g_environment = NULL;
static HMODULE g_loader_dll = NULL;
static CreateEnvWithOptionsFn g_create_env_fn = NULL;

/* Mesaj pompası ile bekleme */
static void mesaj_pompala_bekle(BOOL *tamamlandi, DWORD timeout_ms) {
    DWORD başlangıç = GetTickCount();
    while (!(*tamamlandi)) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (GetTickCount() - başlangıç > timeout_ms) break;
        Sleep(10);
    }
}

/* WebView2Loader.dll yükle */
static int loader_yukle(void) {
    if (g_create_env_fn) return 1;

    /* Önce aynı dizinden dene */
    g_loader_dll = LoadLibraryW(L"WebView2Loader.dll");
    if (!g_loader_dll) {
        /* Exe dizininden dene */
        wchar_t yol[MAX_PATH];
        GetModuleFileNameW(NULL, yol, MAX_PATH);
        wchar_t *son = wcsrchr(yol, L'\\');
        if (son) {
            wcscpy(son + 1, L"WebView2Loader.dll");
            g_loader_dll = LoadLibraryW(yol);
        }
    }
    if (!g_loader_dll) return 0;

    g_create_env_fn = (CreateEnvWithOptionsFn)GetProcAddress(
        g_loader_dll, "CreateCoreWebView2EnvironmentWithOptions");
    return g_create_env_fn ? 1 : 0;
}

/* Environment oluştur (tek sefer) */
static int environment_olustur(void) {
    if (g_environment) return 1;
    if (!loader_yukle()) return 0;

    EnvHandler handler;
    handler.lpVtbl = eh_vtbl_data;
    handler.ref_count = 1;
    handler.environment = NULL;
    handler.tamamlandi = FALSE;

    HRESULT hr = g_create_env_fn(NULL, NULL, NULL, &handler);
    if (FAILED(hr)) return 0;

    mesaj_pompala_bekle(&handler.tamamlandi, 30000);
    if (!handler.environment) return 0;

    g_environment = handler.environment;
    return 1;
}

/* ========== OLUŞTURMA ========== */

/* WebView2 host pencere WndProc */
static LRESULT CALLBACK wv_host_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SIZE) {
        /* WebView'ı yeniden boyutlandır */
        for (int i = 0; i < gorunum_sayisi; i++) {
            if (gorunumler[i].host_hwnd == hwnd && gorunumler[i].controller) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                /* put_Bounds(controller, rc) */
                typedef HRESULT (__stdcall *PutBoundsFn)(void *, RECT);
                PutBoundsFn fn = (PutBoundsFn)((ComObj *)gorunumler[i].controller)->lpVtbl[CTRL_PUT_BOUNDS];
                fn(gorunumler[i].controller, rc);
                break;
            }
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int wv_host_sinif_kayitli = 0;

long long _tr_webgorunum_olustur(void) {
    if (gorunum_sayisi >= MAKS_WEBGORUNUM) return -1;

    /* Environment hazırla */
    if (!environment_olustur()) return -1;

    /* Host pencere sınıfını kaydet */
    if (!wv_host_sinif_kayitli) {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = wv_host_wndproc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"TrWebViewHost";
        RegisterClassExW(&wc);
        wv_host_sinif_kayitli = 1;
    }

    /* Host HWND oluştur */
    HWND host = CreateWindowExW(0, L"TrWebViewHost", NULL,
                                 WS_CHILD | WS_CLIPCHILDREN,
                                 0, 0, 800, 600,
                                 GetDesktopWindow(), NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!host) return -1;

    long long id = gorunum_sayisi;
    memset(&gorunumler[id], 0, sizeof(WebGorunumBilgi));
    gorunumler[id].host_hwnd = host;
    gorunumler[id].arayuz_id = -1;
    gorunum_sayisi++;

    /* Controller oluştur (senkron bekleme) */
    CtrlHandler handler;
    handler.lpVtbl = ch_vtbl_data;
    handler.ref_count = 1;
    handler.controller = NULL;
    handler.tamamlandi = FALSE;

    /* ICoreWebView2Environment::CreateCoreWebView2Controller */
    typedef HRESULT (__stdcall *CreateCtrlFn)(void *, HWND, void *);
    CreateCtrlFn fn = (CreateCtrlFn)((ComObj *)g_environment)->lpVtbl[ENV_CREATE_CONTROLLER];
    HRESULT hr = fn(g_environment, host, &handler);
    if (FAILED(hr)) return -1;

    mesaj_pompala_bekle(&handler.tamamlandi, 30000);
    if (!handler.controller) return -1;

    gorunumler[id].controller = handler.controller;

    /* CoreWebView2 al */
    void *wv = NULL;
    COM_CALL1(handler.controller, CTRL_GET_WEBVIEW, &wv);
    if (!wv) return -1;
    gorunumler[id].webview = wv;

    /* Bounds ayarla */
    RECT rc;
    GetClientRect(host, &rc);
    typedef HRESULT (__stdcall *PutBoundsFn)(void *, RECT);
    PutBoundsFn pbfn = (PutBoundsFn)((ComObj *)handler.controller)->lpVtbl[CTRL_PUT_BOUNDS];
    pbfn(handler.controller, rc);

    /* Olay işleyicilerini kaydet */
    /* DocumentTitleChanged */
    {
        TitleHandler *th = (TitleHandler *)malloc(sizeof(TitleHandler));
        if (th) {
            th->lpVtbl = th_vtbl_data;
            th->ref_count = 1;
            th->bayrak = &gorunumler[id].baslik_degisti;
            long long token[2] = {0}; /* EventRegistrationToken = 8 byte */
            COM_CALL2(wv, WV2_ADD_DOC_TITLE_CHANGED, th, token);
        }
    }

    /* SourceChanged */
    {
        SourceHandler *sh = (SourceHandler *)malloc(sizeof(SourceHandler));
        if (sh) {
            sh->lpVtbl = sh_vtbl_data;
            sh->ref_count = 1;
            sh->bayrak = &gorunumler[id].adres_degisti;
            long long token[2] = {0};
            COM_CALL2(wv, WV2_ADD_SOURCE_CHANGED, sh, token);
        }
    }

    /* NavigationCompleted */
    {
        NavCompHandler *nc = (NavCompHandler *)malloc(sizeof(NavCompHandler));
        if (nc) {
            nc->lpVtbl = nc_vtbl_data;
            nc->ref_count = 1;
            nc->bayrak = &gorunumler[id].yuklenme_bitti;
            long long token[2] = {0};
            COM_CALL2(wv, WV2_ADD_NAV_COMPLETED, nc, token);
        }
    }

    gorunumler[id].hazir = 1;
    return id;
}

/* ========== GEZİNTİ ========== */

long long _tr_webgorunum_yukle(long long id,
                                 const char *url_ptr, long long url_uzunluk) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    char *url = metin_to_cstr(url_ptr, url_uzunluk);
    if (!url) return -1;

    /* Protokol yoksa https:// ekle */
    char *tam_url = url;
    int ozel_alloc = 0;
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0 &&
        strncmp(url, "file://", 7) != 0 && strncmp(url, "about:", 6) != 0) {
        tam_url = (char *)malloc(url_uzunluk + 10);
        if (tam_url) {
            snprintf(tam_url, (size_t)url_uzunluk + 10, "https://%s", url);
            ozel_alloc = 1;
        }
    }

    wchar_t *wurl = utf8_to_wide(tam_url);
    if (ozel_alloc) free(tam_url);
    free(url);
    if (!wurl) return -1;

    COM_CALL1(gorunumler[id].webview, WV2_NAVIGATE, wurl);
    free(wurl);
    return 0;
}

long long _tr_webgorunum_geri(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;
    BOOL canGo = FALSE;
    COM_CALL1(gorunumler[id].webview, WV2_GET_CAN_GO_BACK, &canGo);
    if (canGo) {
        COM_CALL0(gorunumler[id].webview, WV2_GO_BACK);
        return 0;
    }
    return -1;
}

long long _tr_webgorunum_ileri(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;
    BOOL canGo = FALSE;
    COM_CALL1(gorunumler[id].webview, WV2_GET_CAN_GO_FORWARD, &canGo);
    if (canGo) {
        COM_CALL0(gorunumler[id].webview, WV2_GO_FORWARD);
        return 0;
    }
    return -1;
}

long long _tr_webgorunum_yenile(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;
    COM_CALL0(gorunumler[id].webview, WV2_RELOAD);
    return 0;
}

long long _tr_webgorunum_durdur(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;
    COM_CALL0(gorunumler[id].webview, WV2_STOP);
    return 0;
}

/* ========== BİLGİ SORGULAMA ========== */

TrMetin _tr_webgorunum_baslik_al(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return bos_metin();

    LPWSTR title = NULL;
    COM_CALL1(gorunumler[id].webview, WV2_GET_DOC_TITLE, &title);
    if (!title) return cstr_to_metin("Yeni Sekme");

    char *utf8 = wide_to_utf8(title);
    CoTaskMemFree(title);
    if (!utf8) return cstr_to_metin("Yeni Sekme");

    TrMetin m = cstr_to_metin(utf8);
    free(utf8);
    return m;
}

TrMetin _tr_webgorunum_adres_al(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return bos_metin();

    LPWSTR uri = NULL;
    COM_CALL1(gorunumler[id].webview, WV2_GET_SOURCE, &uri);
    if (!uri) return cstr_to_metin("");

    char *utf8 = wide_to_utf8(uri);
    CoTaskMemFree(uri);
    if (!utf8) return cstr_to_metin("");

    TrMetin m = cstr_to_metin(utf8);
    free(utf8);
    return m;
}

long long _tr_webgorunum_geri_gidebilir_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return 0;
    BOOL canGo = FALSE;
    COM_CALL1(gorunumler[id].webview, WV2_GET_CAN_GO_BACK, &canGo);
    return canGo ? 1 : 0;
}

long long _tr_webgorunum_ileri_gidebilir_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return 0;
    BOOL canGo = FALSE;
    COM_CALL1(gorunumler[id].webview, WV2_GET_CAN_GO_FORWARD, &canGo);
    return canGo ? 1 : 0;
}

long long _tr_webgorunum_yukleniyor_mu(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return 0;
    /* WebView2'de doğrudan "isLoading" yok, NavigationCompleted bayrak mekanizmasıyla takip */
    return gorunumler[id].yuklenme_bitti ? 0 : 1;
}

long long _tr_webgorunum_yuklenme_yuzdesi(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return 0;
    /* WebView2'de estimated load progress yok, basitçe 0 veya 100 döndür */
    return gorunumler[id].yuklenme_bitti ? 100 : 50;
}

/* ========== OLAY YOKLAMA ========== */

long long _tr_webgorunum_baslik_degisti_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi) return 0;
    if (gorunumler[id].baslik_degisti) {
        gorunumler[id].baslik_degisti = 0;
        return 1;
    }
    return 0;
}

long long _tr_webgorunum_adres_degisti_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi) return 0;
    if (gorunumler[id].adres_degisti) {
        gorunumler[id].adres_degisti = 0;
        return 1;
    }
    return 0;
}

long long _tr_webgorunum_yuklenme_bitti_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi) return 0;
    if (gorunumler[id].yuklenme_bitti) {
        gorunumler[id].yuklenme_bitti = 0;
        return 1;
    }
    return 0;
}

/* ========== YAKINLAŞTIRMA ========== */

long long _tr_webgorunum_yakinlastir(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].controller) return -1;
    double zoom = 1.0;
    typedef HRESULT (__stdcall *GetZoomFn)(void *, double *);
    GetZoomFn gzfn = (GetZoomFn)((ComObj *)gorunumler[id].controller)->lpVtbl[CTRL_GET_ZOOM];
    gzfn(gorunumler[id].controller, &zoom);

    typedef HRESULT (__stdcall *PutZoomFn)(void *, double);
    PutZoomFn pzfn = (PutZoomFn)((ComObj *)gorunumler[id].controller)->lpVtbl[CTRL_PUT_ZOOM];
    pzfn(gorunumler[id].controller, zoom + 0.1);
    return 0;
}

long long _tr_webgorunum_uzaklastir(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].controller) return -1;
    double zoom = 1.0;
    typedef HRESULT (__stdcall *GetZoomFn)(void *, double *);
    GetZoomFn gzfn = (GetZoomFn)((ComObj *)gorunumler[id].controller)->lpVtbl[CTRL_GET_ZOOM];
    gzfn(gorunumler[id].controller, &zoom);
    if (zoom > 0.2) {
        typedef HRESULT (__stdcall *PutZoomFn)(void *, double);
        PutZoomFn pzfn = (PutZoomFn)((ComObj *)gorunumler[id].controller)->lpVtbl[CTRL_PUT_ZOOM];
        pzfn(gorunumler[id].controller, zoom - 0.1);
    }
    return 0;
}

long long _tr_webgorunum_yakinlik_sifirla(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].controller) return -1;
    typedef HRESULT (__stdcall *PutZoomFn)(void *, double);
    PutZoomFn pzfn = (PutZoomFn)((ComObj *)gorunumler[id].controller)->lpVtbl[CTRL_PUT_ZOOM];
    pzfn(gorunumler[id].controller, 1.0);
    return 0;
}

/* ========== İLERİ DÜZEY ========== */

long long _tr_webgorunum_js_calistir(long long id,
                                       const char *kod_ptr, long long kod_uzunluk) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;
    wchar_t *wkod = metin_to_wide(kod_ptr, kod_uzunluk);
    if (!wkod) return -1;
    COM_CALL2(gorunumler[id].webview, WV2_EXECUTE_SCRIPT, wkod, NULL);
    free(wkod);
    return 0;
}

long long _tr_webgorunum_html_yukle(long long id,
                                      const char *html_ptr, long long html_uzunluk) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;
    wchar_t *whtml = metin_to_wide(html_ptr, html_uzunluk);
    if (!whtml) return -1;
    COM_CALL1(gorunumler[id].webview, WV2_NAVIGATE_TO_STRING, whtml);
    free(whtml);
    return 0;
}

long long _tr_webgorunum_widget_id(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].host_hwnd) return -1;
    if (gorunumler[id].arayuz_id >= 0) return gorunumler[id].arayuz_id;

    long long aid = _tr_arayuz_widget_kaydet_harici_win(
        gorunumler[id].host_hwnd, WT_WEBVIEW_HOST);
    gorunumler[id].arayuz_id = aid;
    return aid;
}
