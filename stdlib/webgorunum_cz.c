/* Web görünüm modülü — çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * WebKit2GTK web görünümü
 */

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

/* ========== DAHİLİ DURUM ========== */

#define MAKS_WEBGORUNUM 256

typedef struct {
    WebKitWebView *webview;
    GtkWidget     *widget;      /* GtkWidget olarak (arayuz entegrasyonu) */
    long long      arayuz_id;   /* arayuz modülündeki widget id */
    int            baslik_degisti;
    int            adres_degisti;
    int            yuklenme_bitti;
} WebGorunumBilgi;

static WebGorunumBilgi gorunumler[MAKS_WEBGORUNUM];
static int gorunum_sayisi = 0;

/* ========== YARDIMCI FONKSİYONLAR ========== */

static char *metin_to_cstr_w(const char *ptr, long long uzunluk) {
    char *cstr = (char *)malloc(uzunluk + 1);
    if (!cstr) return NULL;
    memcpy(cstr, ptr, uzunluk);
    cstr[uzunluk] = '\0';
    return cstr;
}

static TrMetin bos_metin_w(void) {
    TrMetin m = {NULL, 0};
    return m;
}

static TrMetin cstr_to_metin_w(const char *cstr) {
    TrMetin m;
    if (!cstr) { m.ptr = NULL; m.len = 0; return m; }
    m.len = (long long)strlen(cstr);
    m.ptr = (char *)malloc(m.len + 1);
    if (m.ptr) { memcpy(m.ptr, cstr, m.len); m.ptr[m.len] = '\0'; }
    return m;
}

/* ========== OLAY GERİ ÇAĞRILARI ========== */

static void baslik_degisti_olayi(WebKitWebView *webview, GParamSpec *pspec, gpointer data) {
    (void)webview; (void)pspec;
    long long id = (long long)(intptr_t)data;
    if (id >= 0 && id < MAKS_WEBGORUNUM) {
        gorunumler[id].baslik_degisti = 1;
    }
}

static void adres_degisti_olayi(WebKitWebView *webview, GParamSpec *pspec, gpointer data) {
    (void)webview; (void)pspec;
    long long id = (long long)(intptr_t)data;
    if (id >= 0 && id < MAKS_WEBGORUNUM) {
        gorunumler[id].adres_degisti = 1;
    }
}

static void yuklenme_degisti_olayi(WebKitWebView *webview, WebKitLoadEvent event, gpointer data) {
    (void)webview;
    long long id = (long long)(intptr_t)data;
    if (id >= 0 && id < MAKS_WEBGORUNUM) {
        if (event == WEBKIT_LOAD_FINISHED) {
            gorunumler[id].yuklenme_bitti = 1;
        }
    }
}

/* ========== OLUŞTURMA ========== */

/* Dışarıdan erişim için (arayuz modülünden widget kaydetme) */
extern GtkWidget *_tr_arayuz_widget_al(long long id);

/* webgorunum.olustur() -> tam */
long long _tr_webgorunum_olustur(void) {
    if (gorunum_sayisi >= MAKS_WEBGORUNUM) return -1;

    long long id = gorunum_sayisi;

    /* WebKit ayarları */
    WebKitSettings *ayarlar = webkit_settings_new();
    webkit_settings_set_enable_javascript(ayarlar, TRUE);
    webkit_settings_set_enable_developer_extras(ayarlar, TRUE);
    webkit_settings_set_javascript_can_open_windows_automatically(ayarlar, TRUE);

    /* WebView oluştur */
    GtkWidget *widget = webkit_web_view_new_with_settings(ayarlar);
    WebKitWebView *webview = WEBKIT_WEB_VIEW(widget);

    gorunumler[id].webview = webview;
    gorunumler[id].widget = widget;
    gorunumler[id].arayuz_id = -1;
    gorunumler[id].baslik_degisti = 0;
    gorunumler[id].adres_degisti = 0;
    gorunumler[id].yuklenme_bitti = 0;

    /* Olayları bağla */
    g_signal_connect(webview, "notify::title",
                     G_CALLBACK(baslik_degisti_olayi), (gpointer)(intptr_t)id);
    g_signal_connect(webview, "notify::uri",
                     G_CALLBACK(adres_degisti_olayi), (gpointer)(intptr_t)id);
    g_signal_connect(webview, "load-changed",
                     G_CALLBACK(yuklenme_degisti_olayi), (gpointer)(intptr_t)id);

    gorunum_sayisi++;
    return id;
}

/* ========== GEZİNTİ ========== */

/* webgorunum.yukle(wv_id: tam, url: metin) -> tam */
long long _tr_webgorunum_yukle(long long id,
                                 const char *url_ptr, long long url_uzunluk) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    char *url = metin_to_cstr_w(url_ptr, url_uzunluk);
    if (!url) return -1;

    /* Eğer protokol belirtilmemişse http:// ekle */
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0 &&
        strncmp(url, "file://", 7) != 0 && strncmp(url, "about:", 6) != 0) {
        char *tam_url = (char *)malloc(url_uzunluk + 10);
        if (tam_url) {
            snprintf(tam_url, url_uzunluk + 10, "https://%s", url);
            webkit_web_view_load_uri(gorunumler[id].webview, tam_url);
            free(tam_url);
        }
    } else {
        webkit_web_view_load_uri(gorunumler[id].webview, url);
    }

    free(url);
    return 0;
}

/* webgorunum.geri(wv_id: tam) -> tam */
long long _tr_webgorunum_geri(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    if (webkit_web_view_can_go_back(gorunumler[id].webview)) {
        webkit_web_view_go_back(gorunumler[id].webview);
        return 0;
    }
    return -1;
}

/* webgorunum.ileri(wv_id: tam) -> tam */
long long _tr_webgorunum_ileri(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    if (webkit_web_view_can_go_forward(gorunumler[id].webview)) {
        webkit_web_view_go_forward(gorunumler[id].webview);
        return 0;
    }
    return -1;
}

/* webgorunum.yenile(wv_id: tam) -> tam */
long long _tr_webgorunum_yenile(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    webkit_web_view_reload(gorunumler[id].webview);
    return 0;
}

/* webgorunum.durdur(wv_id: tam) -> tam */
long long _tr_webgorunum_durdur(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    webkit_web_view_stop_loading(gorunumler[id].webview);
    return 0;
}

/* ========== BİLGİ SORGULAMA ========== */

/* webgorunum.baslik_al(wv_id: tam) -> metin */
TrMetin _tr_webgorunum_baslik_al(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return bos_metin_w();

    const gchar *baslik = webkit_web_view_get_title(gorunumler[id].webview);
    if (!baslik) return cstr_to_metin_w("Yeni Sekme");
    return cstr_to_metin_w(baslik);
}

/* webgorunum.adres_al(wv_id: tam) -> metin */
TrMetin _tr_webgorunum_adres_al(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return bos_metin_w();

    const gchar *uri = webkit_web_view_get_uri(gorunumler[id].webview);
    if (!uri) return cstr_to_metin_w("");
    return cstr_to_metin_w(uri);
}

/* webgorunum.geri_gidebilir_mi(wv_id: tam) -> tam */
long long _tr_webgorunum_geri_gidebilir_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return 0;

    return webkit_web_view_can_go_back(gorunumler[id].webview) ? 1 : 0;
}

/* webgorunum.ileri_gidebilir_mi(wv_id: tam) -> tam */
long long _tr_webgorunum_ileri_gidebilir_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return 0;

    return webkit_web_view_can_go_forward(gorunumler[id].webview) ? 1 : 0;
}

/* webgorunum.yukleniyor_mu(wv_id: tam) -> tam */
long long _tr_webgorunum_yukleniyor_mu(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return 0;

    return webkit_web_view_is_loading(gorunumler[id].webview) ? 1 : 0;
}

/* webgorunum.yuklenme_yuzdesi(wv_id: tam) -> tam */
long long _tr_webgorunum_yuklenme_yuzdesi(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return 0;

    gdouble yuzde = webkit_web_view_get_estimated_load_progress(gorunumler[id].webview);
    return (long long)(yuzde * 100.0);
}

/* ========== OLAY YOKLAMA ========== */

/* webgorunum.baslik_degisti_mi(wv_id: tam) -> tam */
long long _tr_webgorunum_baslik_degisti_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi) return 0;

    if (gorunumler[id].baslik_degisti) {
        gorunumler[id].baslik_degisti = 0;
        return 1;
    }
    return 0;
}

/* webgorunum.adres_degisti_mi(wv_id: tam) -> tam */
long long _tr_webgorunum_adres_degisti_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi) return 0;

    if (gorunumler[id].adres_degisti) {
        gorunumler[id].adres_degisti = 0;
        return 1;
    }
    return 0;
}

/* webgorunum.yuklenme_bitti_mi(wv_id: tam) -> tam */
long long _tr_webgorunum_yuklenme_bitti_mi(long long id) {
    if (id < 0 || id >= gorunum_sayisi) return 0;

    if (gorunumler[id].yuklenme_bitti) {
        gorunumler[id].yuklenme_bitti = 0;
        return 1;
    }
    return 0;
}

/* ========== YAKINLAŞTIRMA ========== */

/* webgorunum.yakinlastir(wv_id: tam) -> tam */
long long _tr_webgorunum_yakinlastir(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    gdouble mevcut = webkit_web_view_get_zoom_level(gorunumler[id].webview);
    webkit_web_view_set_zoom_level(gorunumler[id].webview, mevcut + 0.1);
    return 0;
}

/* webgorunum.uzaklastir(wv_id: tam) -> tam */
long long _tr_webgorunum_uzaklastir(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    gdouble mevcut = webkit_web_view_get_zoom_level(gorunumler[id].webview);
    if (mevcut > 0.2) {
        webkit_web_view_set_zoom_level(gorunumler[id].webview, mevcut - 0.1);
    }
    return 0;
}

/* webgorunum.yakinlik_sifirla(wv_id: tam) -> tam */
long long _tr_webgorunum_yakinlik_sifirla(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    webkit_web_view_set_zoom_level(gorunumler[id].webview, 1.0);
    return 0;
}

/* ========== İLERİ DÜZEY ========== */

/* webgorunum.js_calistir(wv_id: tam, kod: metin) -> tam */
long long _tr_webgorunum_js_calistir(long long id,
                                       const char *kod_ptr, long long kod_uzunluk) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    char *kod = metin_to_cstr_w(kod_ptr, kod_uzunluk);
    if (!kod) return -1;

    webkit_web_view_evaluate_javascript(gorunumler[id].webview,
                                         kod, (gssize)kod_uzunluk,
                                         NULL, NULL, NULL, NULL, NULL);
    free(kod);
    return 0;
}

/* webgorunum.html_yukle(wv_id: tam, html: metin) -> tam
 * HTML string'ini doğrudan yükler (speed dial vb. için) */
long long _tr_webgorunum_html_yukle(long long id,
                                      const char *html_ptr, long long html_uzunluk) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].webview) return -1;

    char *html = metin_to_cstr_w(html_ptr, html_uzunluk);
    if (!html) return -1;

    webkit_web_view_load_html(gorunumler[id].webview, html, NULL);
    free(html);
    return 0;
}

/* webgorunum.widget_id(wv_id: tam) -> tam
 * WebView'ın arayuz modülündeki widget id'sini döndürür.
 * Eğer daha önce kayıtlı değilse, arayuz modülüne kaydeder. */
long long _tr_webgorunum_widget_id(long long id) {
    if (id < 0 || id >= gorunum_sayisi || !gorunumler[id].widget) return -1;

    if (gorunumler[id].arayuz_id >= 0) {
        return gorunumler[id].arayuz_id;
    }

    /* Arayüz modülündeki widget dizisine kaydet */
    /* Bu fonksiyon arayuz_cz.c'den extern edilir */
    extern long long _tr_arayuz_widget_kaydet_harici(GtkWidget *w);
    long long aid = _tr_arayuz_widget_kaydet_harici(gorunumler[id].widget);
    gorunumler[id].arayuz_id = aid;
    return aid;
}
