/* Arayüz modülü — çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * GTK3 widget'ları
 */

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

/* ========== DAHİLİ DURUM ========== */

#define MAKS_WIDGET 4096

typedef struct {
    GtkWidget *widget;
    int clicked;              /* dugme tıklama bayrağı */
    int enter;                /* giris Enter bayrağı */
    int tab_changed;          /* notebook sekme değişikliği bayrağı */
    int tab_close_requested;  /* sekme kapatma isteği */
    int tab_close_index;      /* kapatılması istenen sekme indeksi */
} WidgetBilgi;

static WidgetBilgi widgetlar[MAKS_WIDGET];
static int widget_sayisi = 0;

/* ========== YARDIMCI FONKSİYONLAR ========== */

static char *metin_to_cstr_a(const char *ptr, long long uzunluk) {
    char *cstr = (char *)malloc(uzunluk + 1);
    if (!cstr) return NULL;
    memcpy(cstr, ptr, uzunluk);
    cstr[uzunluk] = '\0';
    return cstr;
}

static TrMetin bos_metin_a(void) {
    TrMetin m = {NULL, 0};
    return m;
}

static TrMetin cstr_to_metin_a(const char *cstr) {
    TrMetin m;
    if (!cstr) { m.ptr = NULL; m.len = 0; return m; }
    m.len = (long long)strlen(cstr);
    m.ptr = (char *)malloc(m.len + 1);
    if (m.ptr) { memcpy(m.ptr, cstr, m.len); m.ptr[m.len] = '\0'; }
    return m;
}

static long long widget_kaydet(GtkWidget *w) {
    if (widget_sayisi >= MAKS_WIDGET || !w) return -1;
    long long id = widget_sayisi;
    widgetlar[id].widget = w;
    widgetlar[id].clicked = 0;
    widgetlar[id].enter = 0;
    widgetlar[id].tab_changed = 0;
    widgetlar[id].tab_close_requested = 0;
    widgetlar[id].tab_close_index = -1;
    widget_sayisi++;
    return id;
}

/* Dışarıdan erişim için (pencere modülü kullanır) */
GtkWidget *_tr_arayuz_widget_al(long long id) {
    if (id < 0 || id >= widget_sayisi) return NULL;
    return widgetlar[id].widget;
}

/* Dışarıdan widget kaydetme (webgorunum modülü kullanır) */
long long _tr_arayuz_widget_kaydet_harici(GtkWidget *w) {
    return widget_kaydet(w);
}

/* ========== OLAY GERİ ÇAĞRILARI ========== */

static void dugme_tikla_olayi(GtkWidget *widget, gpointer data) {
    (void)widget;
    long long id = (long long)(intptr_t)data;
    if (id >= 0 && id < MAKS_WIDGET) {
        widgetlar[id].clicked = 1;
    }
}

static void giris_enter_olayi(GtkEntry *entry, gpointer data) {
    (void)entry;
    long long id = (long long)(intptr_t)data;
    if (id >= 0 && id < MAKS_WIDGET) {
        widgetlar[id].enter = 1;
    }
}

static void sekme_degisti_olayi(GtkNotebook *notebook, GtkWidget *page,
                                 guint page_num, gpointer data) {
    (void)notebook; (void)page; (void)page_num;
    long long id = (long long)(intptr_t)data;
    if (id >= 0 && id < MAKS_WIDGET) {
        widgetlar[id].tab_changed = 1;
    }
}

/* Sekme kapatma düğmesi callback'i */
static void sekme_kapat_tikla(GtkWidget *button, gpointer data) {
    (void)data;
    long long nb_id = (long long)(intptr_t)g_object_get_data(G_OBJECT(button), "nb_id");
    GtkWidget *page = (GtkWidget *)g_object_get_data(G_OBJECT(button), "page");

    if (nb_id >= 0 && nb_id < MAKS_WIDGET && widgetlar[nb_id].widget && page) {
        gint indeks = gtk_notebook_page_num(
            GTK_NOTEBOOK(widgetlar[nb_id].widget), page
        );
        if (indeks >= 0) {
            widgetlar[nb_id].tab_close_requested = 1;
            widgetlar[nb_id].tab_close_index = indeks;
        }
    }
}

/* ========== KONTEYNERLER ========== */

/* arayuz.kutu_yatay() -> tam */
long long _tr_arayuz_kutu_yatay(void) {
    GtkWidget *kutu = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    return widget_kaydet(kutu);
}

/* arayuz.kutu_dikey() -> tam */
long long _tr_arayuz_kutu_dikey(void) {
    GtkWidget *kutu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    return widget_kaydet(kutu);
}

/* arayuz.kaydirma() -> tam */
long long _tr_arayuz_kaydirma(void) {
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    return widget_kaydet(sw);
}

/* ========== TEMEL WİDGET'LAR ========== */

/* arayuz.dugme(etiket: metin) -> tam */
long long _tr_arayuz_dugme(const char *etiket_ptr, long long etiket_uzunluk) {
    char *etiket = metin_to_cstr_a(etiket_ptr, etiket_uzunluk);
    if (!etiket) return -1;

    GtkWidget *dugme = gtk_button_new_with_label(etiket);
    free(etiket);

    long long id = widget_kaydet(dugme);
    if (id >= 0) {
        g_signal_connect(dugme, "clicked", G_CALLBACK(dugme_tikla_olayi),
                         (gpointer)(intptr_t)id);
    }
    return id;
}

/* arayuz.etiket(metin: metin) -> tam */
long long _tr_arayuz_etiket(const char *metin_ptr, long long metin_uzunluk) {
    char *metin = metin_to_cstr_a(metin_ptr, metin_uzunluk);
    if (!metin) return -1;

    GtkWidget *etiket = gtk_label_new(metin);
    free(metin);

    return widget_kaydet(etiket);
}

/* arayuz.giris() -> tam */
long long _tr_arayuz_giris(void) {
    GtkWidget *giris = gtk_entry_new();

    long long id = widget_kaydet(giris);
    if (id >= 0) {
        g_signal_connect(giris, "activate", G_CALLBACK(giris_enter_olayi),
                         (gpointer)(intptr_t)id);
    }
    return id;
}

/* arayuz.ayirici() -> tam */
long long _tr_arayuz_ayirici(void) {
    GtkWidget *ayirici = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    return widget_kaydet(ayirici);
}

/* arayuz.resim_dosyadan(yol: metin) -> tam */
long long _tr_arayuz_resim_dosyadan(const char *yol_ptr, long long yol_uzunluk) {
    char *yol = metin_to_cstr_a(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    GtkWidget *resim = gtk_image_new_from_file(yol);
    free(yol);

    return widget_kaydet(resim);
}

/* ========== GİRİŞ ALANI İŞLEMLERİ ========== */

/* arayuz.giris_metni_al(giris_id: tam) -> metin */
TrMetin _tr_arayuz_giris_metni_al(long long id) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return bos_metin_a();

    const gchar *metin = gtk_entry_get_text(GTK_ENTRY(widgetlar[id].widget));
    return cstr_to_metin_a(metin);
}

/* arayuz.giris_metni_ayarla(giris_id: tam, metin: metin) -> tam */
long long _tr_arayuz_giris_metni_ayarla(long long id,
                                          const char *metin_ptr, long long metin_uzunluk) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    char *metin = metin_to_cstr_a(metin_ptr, metin_uzunluk);
    if (!metin) return -1;

    gtk_entry_set_text(GTK_ENTRY(widgetlar[id].widget), metin);
    free(metin);
    return 0;
}

/* arayuz.giris_ipucu_ayarla(giris_id: tam, ipucu: metin) -> tam */
long long _tr_arayuz_giris_ipucu_ayarla(long long id,
                                          const char *ipucu_ptr, long long ipucu_uzunluk) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    char *ipucu = metin_to_cstr_a(ipucu_ptr, ipucu_uzunluk);
    if (!ipucu) return -1;

    gtk_entry_set_placeholder_text(GTK_ENTRY(widgetlar[id].widget), ipucu);
    free(ipucu);
    return 0;
}

/* ========== ETİKET İŞLEMLERİ ========== */

/* arayuz.etiket_metni_ayarla(etiket_id: tam, metin: metin) -> tam */
long long _tr_arayuz_etiket_metni_ayarla(long long id,
                                           const char *metin_ptr, long long metin_uzunluk) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    char *metin = metin_to_cstr_a(metin_ptr, metin_uzunluk);
    if (!metin) return -1;

    gtk_label_set_text(GTK_LABEL(widgetlar[id].widget), metin);
    free(metin);
    return 0;
}

/* arayuz.etiket_metni_al(etiket_id: tam) -> metin */
TrMetin _tr_arayuz_etiket_metni_al(long long id) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return bos_metin_a();

    const gchar *metin = gtk_label_get_text(GTK_LABEL(widgetlar[id].widget));
    return cstr_to_metin_a(metin);
}

/* ========== SEKME YÖNETİMİ (GtkNotebook) ========== */

/* arayuz.sekmeler() -> tam */
long long _tr_arayuz_sekmeler(void) {
    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);

    long long id = widget_kaydet(notebook);
    if (id >= 0) {
        g_signal_connect(notebook, "switch-page", G_CALLBACK(sekme_degisti_olayi),
                         (gpointer)(intptr_t)id);
    }
    return id;
}

/* arayuz.sekme_ekle(notebook_id: tam, icerik_id: tam, baslik: metin) -> tam */
long long _tr_arayuz_sekme_ekle(long long nb_id, long long icerik_id,
                                  const char *baslik_ptr, long long baslik_uzunluk) {
    if (nb_id < 0 || nb_id >= widget_sayisi || !widgetlar[nb_id].widget) return -1;
    if (icerik_id < 0 || icerik_id >= widget_sayisi || !widgetlar[icerik_id].widget) return -1;

    char *baslik = metin_to_cstr_a(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;

    GtkWidget *etiket = gtk_label_new(baslik);
    free(baslik);

    gint indeks = gtk_notebook_append_page(
        GTK_NOTEBOOK(widgetlar[nb_id].widget),
        widgetlar[icerik_id].widget,
        etiket
    );

    gtk_widget_show_all(widgetlar[icerik_id].widget);

    return (long long)indeks;
}

/* arayuz.sekme_kapatmali_ekle(notebook_id: tam, icerik_id: tam, baslik: metin) -> tam
 * Kapatma düğmeli sekme ekler */
long long _tr_arayuz_sekme_kapatmali_ekle(long long nb_id, long long icerik_id,
                                            const char *baslik_ptr, long long baslik_uzunluk) {
    if (nb_id < 0 || nb_id >= widget_sayisi || !widgetlar[nb_id].widget) return -1;
    if (icerik_id < 0 || icerik_id >= widget_sayisi || !widgetlar[icerik_id].widget) return -1;

    char *baslik = metin_to_cstr_a(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;

    /* Sekme widget'ı: hbox = [etiket | kapatma düğmesi] */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    GtkWidget *etiket = gtk_label_new(baslik);
    gtk_label_set_ellipsize(GTK_LABEL(etiket), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(etiket), 20);
    free(baslik);

    GtkWidget *kapat_btn = gtk_button_new_from_icon_name(
        "window-close-symbolic", GTK_ICON_SIZE_MENU
    );
    gtk_button_set_relief(GTK_BUTTON(kapat_btn), GTK_RELIEF_NONE);

    /* CSS sınıfı ekle */
    GtkStyleContext *btn_ctx = gtk_widget_get_style_context(kapat_btn);
    gtk_style_context_add_class(btn_ctx, "sekme-kapat");

    gtk_box_pack_start(GTK_BOX(hbox), etiket, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), kapat_btn, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);

    /* Kapatma callback verilerini düğmeye ekle */
    g_object_set_data(G_OBJECT(kapat_btn), "nb_id", (gpointer)(intptr_t)nb_id);
    g_object_set_data(G_OBJECT(kapat_btn), "page", widgetlar[icerik_id].widget);
    g_signal_connect(kapat_btn, "clicked", G_CALLBACK(sekme_kapat_tikla), NULL);

    /* Sayfayı ekle */
    gint indeks = gtk_notebook_append_page(
        GTK_NOTEBOOK(widgetlar[nb_id].widget),
        widgetlar[icerik_id].widget,
        hbox
    );

    gtk_widget_show_all(widgetlar[icerik_id].widget);

    return (long long)indeks;
}

/* arayuz.sekme_kaldir(notebook_id: tam, indeks: tam) -> tam */
long long _tr_arayuz_sekme_kaldir(long long nb_id, long long indeks) {
    if (nb_id < 0 || nb_id >= widget_sayisi || !widgetlar[nb_id].widget) return -1;

    gtk_notebook_remove_page(GTK_NOTEBOOK(widgetlar[nb_id].widget), (gint)indeks);
    return 0;
}

/* arayuz.sekme_secili(notebook_id: tam) -> tam */
long long _tr_arayuz_sekme_secili(long long nb_id) {
    if (nb_id < 0 || nb_id >= widget_sayisi || !widgetlar[nb_id].widget) return -1;

    return (long long)gtk_notebook_get_current_page(GTK_NOTEBOOK(widgetlar[nb_id].widget));
}

/* arayuz.sekme_sec(notebook_id: tam, indeks: tam) -> tam */
long long _tr_arayuz_sekme_sec(long long nb_id, long long indeks) {
    if (nb_id < 0 || nb_id >= widget_sayisi || !widgetlar[nb_id].widget) return -1;

    gtk_notebook_set_current_page(GTK_NOTEBOOK(widgetlar[nb_id].widget), (gint)indeks);
    return 0;
}

/* arayuz.sekme_baslik_ayarla(notebook_id: tam, indeks: tam, baslik: metin) -> tam
 * Hem normal hem kapatmalı sekmeleri destekler */
long long _tr_arayuz_sekme_baslik_ayarla(long long nb_id, long long indeks,
                                           const char *baslik_ptr, long long baslik_uzunluk) {
    if (nb_id < 0 || nb_id >= widget_sayisi || !widgetlar[nb_id].widget) return -1;

    char *baslik = metin_to_cstr_a(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;

    GtkWidget *sayfa = gtk_notebook_get_nth_page(
        GTK_NOTEBOOK(widgetlar[nb_id].widget), (gint)indeks
    );
    if (!sayfa) { free(baslik); return -1; }

    GtkWidget *mevcut = gtk_notebook_get_tab_label(
        GTK_NOTEBOOK(widgetlar[nb_id].widget), sayfa
    );

    if (mevcut && GTK_IS_BOX(mevcut)) {
        /* Kapatmalı sekme: hbox içindeki ilk çocuk GtkLabel */
        GList *children = gtk_container_get_children(GTK_CONTAINER(mevcut));
        if (children) {
            GtkWidget *label = GTK_WIDGET(children->data);
            if (GTK_IS_LABEL(label)) {
                gtk_label_set_text(GTK_LABEL(label), baslik);
            }
            g_list_free(children);
        }
    } else {
        /* Normal sekme: yeni etiket oluştur */
        GtkWidget *yeni_etiket = gtk_label_new(baslik);
        gtk_notebook_set_tab_label(
            GTK_NOTEBOOK(widgetlar[nb_id].widget), sayfa, yeni_etiket
        );
    }

    free(baslik);
    return 0;
}

/* arayuz.sekme_sayisi(notebook_id: tam) -> tam */
long long _tr_arayuz_sekme_sayisi(long long nb_id) {
    if (nb_id < 0 || nb_id >= widget_sayisi || !widgetlar[nb_id].widget) return 0;

    return (long long)gtk_notebook_get_n_pages(GTK_NOTEBOOK(widgetlar[nb_id].widget));
}

/* arayuz.sekme_kapandi_mi(notebook_id: tam) -> tam
 * Kapatma düğmesine basıldıysa sekme indeksini döndürür, yoksa -1 */
long long _tr_arayuz_sekme_kapandi_mi(long long nb_id) {
    if (nb_id < 0 || nb_id >= widget_sayisi) return -1;

    if (widgetlar[nb_id].tab_close_requested) {
        widgetlar[nb_id].tab_close_requested = 0;
        return (long long)widgetlar[nb_id].tab_close_index;
    }
    return -1;
}

/* ========== YERLEŞIM ========== */

/* arayuz.ekle(konteyner_id: tam, widget_id: tam) -> tam */
long long _tr_arayuz_ekle(long long konteyner_id, long long widget_id) {
    if (konteyner_id < 0 || konteyner_id >= widget_sayisi || !widgetlar[konteyner_id].widget)
        return -1;
    if (widget_id < 0 || widget_id >= widget_sayisi || !widgetlar[widget_id].widget)
        return -1;

    gtk_container_add(GTK_CONTAINER(widgetlar[konteyner_id].widget),
                      widgetlar[widget_id].widget);
    return 0;
}

/* arayuz.pakle(kutu_id: tam, widget_id: tam, genisle: tam, doldur: tam, bosluk: tam) -> tam */
long long _tr_arayuz_pakle(long long kutu_id, long long widget_id,
                             long long genisle, long long doldur, long long bosluk) {
    if (kutu_id < 0 || kutu_id >= widget_sayisi || !widgetlar[kutu_id].widget)
        return -1;
    if (widget_id < 0 || widget_id >= widget_sayisi || !widgetlar[widget_id].widget)
        return -1;

    gtk_box_pack_start(GTK_BOX(widgetlar[kutu_id].widget),
                       widgetlar[widget_id].widget,
                       genisle ? TRUE : FALSE,
                       doldur ? TRUE : FALSE,
                       (guint)bosluk);
    return 0;
}

/* arayuz.goster_tumu(widget_id: tam) -> tam */
long long _tr_arayuz_goster_tumu(long long id) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    gtk_widget_show_all(widgetlar[id].widget);
    return 0;
}

/* ========== OLAY YOKLAMA (Polling) ========== */

/* arayuz.dugme_basildi_mi(dugme_id: tam) -> tam */
long long _tr_arayuz_dugme_basildi_mi(long long id) {
    if (id < 0 || id >= widget_sayisi) return 0;

    if (widgetlar[id].clicked) {
        widgetlar[id].clicked = 0;
        return 1;
    }
    return 0;
}

/* arayuz.giris_enter_mi(giris_id: tam) -> tam */
long long _tr_arayuz_giris_enter_mi(long long id) {
    if (id < 0 || id >= widget_sayisi) return 0;

    if (widgetlar[id].enter) {
        widgetlar[id].enter = 0;
        return 1;
    }
    return 0;
}

/* arayuz.sekme_degisti_mi(notebook_id: tam) -> tam */
long long _tr_arayuz_sekme_degisti_mi(long long id) {
    if (id < 0 || id >= widget_sayisi) return 0;

    if (widgetlar[id].tab_changed) {
        widgetlar[id].tab_changed = 0;
        return 1;
    }
    return 0;
}

/* ========== CSS TEMA ========== */

/* arayuz.css_yukle(css: metin) -> tam */
long long _tr_arayuz_css_yukle(const char *css_ptr, long long css_uzunluk) {
    GtkCssProvider *provider = gtk_css_provider_new();

    GError *hata = NULL;
    gtk_css_provider_load_from_data(provider, css_ptr, (gssize)css_uzunluk, &hata);

    if (hata) {
        g_error_free(hata);
        g_object_unref(provider);
        return -1;
    }

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    g_object_unref(provider);
    return 0;
}

/* ========== WİDGET ÖZELLİKLERİ ========== */

/* arayuz.css_sinif_ekle(widget_id: tam, sinif: metin) -> tam */
long long _tr_arayuz_css_sinif_ekle(long long id,
                                      const char *sinif_ptr, long long sinif_uzunluk) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    char *sinif = metin_to_cstr_a(sinif_ptr, sinif_uzunluk);
    if (!sinif) return -1;

    GtkStyleContext *ctx = gtk_widget_get_style_context(widgetlar[id].widget);
    gtk_style_context_add_class(ctx, sinif);
    free(sinif);
    return 0;
}

/* arayuz.css_sinif_kaldir(widget_id: tam, sinif: metin) -> tam */
long long _tr_arayuz_css_sinif_kaldir(long long id,
                                        const char *sinif_ptr, long long sinif_uzunluk) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    char *sinif = metin_to_cstr_a(sinif_ptr, sinif_uzunluk);
    if (!sinif) return -1;

    GtkStyleContext *ctx = gtk_widget_get_style_context(widgetlar[id].widget);
    gtk_style_context_remove_class(ctx, sinif);
    free(sinif);
    return 0;
}

/* arayuz.widget_genislik_ayarla(widget_id: tam, genislik: tam) -> tam */
long long _tr_arayuz_widget_genislik_ayarla(long long id, long long genislik) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    gtk_widget_set_size_request(widgetlar[id].widget, (gint)genislik, -1);
    return 0;
}

/* arayuz.widget_yukseklik_ayarla(widget_id: tam, yukseklik: tam) -> tam */
long long _tr_arayuz_widget_yukseklik_ayarla(long long id, long long yukseklik) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    gtk_widget_set_size_request(widgetlar[id].widget, -1, (gint)yukseklik);
    return 0;
}

/* ========== İLERLEME ÇUBUĞU ========== */

/* arayuz.ilerleme_cubugu() -> tam — GtkProgressBar */
long long _tr_arayuz_ilerleme_cubugu(void) {
    GtkWidget *bar = gtk_progress_bar_new();
    return widget_kaydet(bar);
}

/* arayuz.ilerleme_ayarla(id: tam, deger: tam) -> tam — 0-100 yüzde */
long long _tr_arayuz_ilerleme_ayarla(long long id, long long deger) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    double fraction = (double)deger / 100.0;
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgetlar[id].widget), fraction);
    return 0;
}

/* ========== İPUCU ========== */

/* arayuz.ipucu_ayarla(widget_id: tam, metin: metin) -> tam */
long long _tr_arayuz_ipucu_ayarla(long long id,
                                    const char *metin_ptr, long long metin_uzunluk) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    char *metin = metin_to_cstr_a(metin_ptr, metin_uzunluk);
    if (!metin) return -1;

    gtk_widget_set_tooltip_text(widgetlar[id].widget, metin);
    free(metin);
    return 0;
}

/* ========== GÖRÜNÜRLÜK ========== */

/* arayuz.widget_gizle(widget_id: tam) -> tam */
long long _tr_arayuz_widget_gizle(long long id) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    gtk_widget_hide(widgetlar[id].widget);
    return 0;
}

/* arayuz.widget_goster(widget_id: tam) -> tam */
long long _tr_arayuz_widget_goster(long long id) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    gtk_widget_show_all(widgetlar[id].widget);
    return 0;
}

/* ========== DÜĞME İŞLEMLERİ ========== */

/* arayuz.dugme_etiket_ayarla(dugme_id: tam, metin: metin) -> tam */
long long _tr_arayuz_dugme_etiket_ayarla(long long id,
                                           const char *metin_ptr, long long metin_uzunluk) {
    if (id < 0 || id >= widget_sayisi || !widgetlar[id].widget) return -1;

    char *metin = metin_to_cstr_a(metin_ptr, metin_uzunluk);
    if (!metin) return -1;

    gtk_button_set_label(GTK_BUTTON(widgetlar[id].widget), metin);
    free(metin);
    return 0;
}
