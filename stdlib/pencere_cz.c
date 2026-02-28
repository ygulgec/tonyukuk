/* Pencere modülü — çalışma zamanı implementasyonu
 * Tonyukuk Programlama Dili
 * GTK3 pencere yönetimi
 */

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

/* ========== DAHİLİ DURUM ========== */

#define MAKS_PENCERE 64

typedef struct {
    GtkWidget *pencere;
    int kapatildi;
} PencereBilgi;

static PencereBilgi pencereler[MAKS_PENCERE];
static int pencere_sayisi = 0;
static int gtk_baslatildi = 0;

/* ========== YARDIMCI FONKSİYONLAR ========== */

static char *metin_to_cstr_p(const char *ptr, long long uzunluk) {
    char *cstr = (char *)malloc(uzunluk + 1);
    if (!cstr) return NULL;
    memcpy(cstr, ptr, uzunluk);
    cstr[uzunluk] = '\0';
    return cstr;
}

/* Pencere kapatma olayı */
static gboolean pencere_kapat_olayi(GtkWidget *widget, GdkEvent *event, gpointer data) {
    (void)widget;
    (void)event;
    long long id = (long long)(intptr_t)data;
    if (id >= 0 && id < MAKS_PENCERE) {
        pencereler[id].kapatildi = 1;
    }
    return FALSE;
}

/* ========== BAŞLATMA ========== */

/* pencere.baslat() -> tam */
long long _tr_pencere_baslat(void) {
    if (!gtk_baslatildi) {
        gtk_init(NULL, NULL);
        memset(pencereler, 0, sizeof(pencereler));
        gtk_baslatildi = 1;
    }
    return 0;
}

/* ========== PENCERE OLUŞTURMA ========== */

/* pencere.olustur(baslik: metin, genislik: tam, yukseklik: tam) -> tam */
long long _tr_pencere_olustur(const char *baslik_ptr, long long baslik_uzunluk,
                               long long genislik, long long yukseklik) {
    if (!gtk_baslatildi) _tr_pencere_baslat();

    if (pencere_sayisi >= MAKS_PENCERE) return -1;

    char *baslik = metin_to_cstr_p(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;

    long long id = pencere_sayisi;

    GtkWidget *pencere = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pencere), baslik);
    gtk_window_set_default_size(GTK_WINDOW(pencere), (gint)genislik, (gint)yukseklik);
    gtk_window_set_position(GTK_WINDOW(pencere), GTK_WIN_POS_CENTER);

    /* Kapatma olayını bağla */
    g_signal_connect(pencere, "delete-event",
                     G_CALLBACK(pencere_kapat_olayi), (gpointer)(intptr_t)id);

    pencereler[id].pencere = pencere;
    pencereler[id].kapatildi = 0;
    pencere_sayisi++;

    free(baslik);
    return id;
}

/* ========== PENCERE ÖZELLİKLERİ ========== */

/* pencere.baslik_ayarla(pencere_id: tam, baslik: metin) -> tam */
long long _tr_pencere_baslik_ayarla(long long id,
                                     const char *baslik_ptr, long long baslik_uzunluk) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].pencere) return -1;

    char *baslik = metin_to_cstr_p(baslik_ptr, baslik_uzunluk);
    if (!baslik) return -1;

    gtk_window_set_title(GTK_WINDOW(pencereler[id].pencere), baslik);
    free(baslik);
    return 0;
}

/* pencere.boyut_ayarla(pencere_id: tam, genislik: tam, yukseklik: tam) -> tam */
long long _tr_pencere_boyut_ayarla(long long id, long long genislik, long long yukseklik) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].pencere) return -1;

    gtk_window_resize(GTK_WINDOW(pencereler[id].pencere), (gint)genislik, (gint)yukseklik);
    return 0;
}

/* pencere.simge_ayarla(pencere_id: tam, dosya_yolu: metin) -> tam */
long long _tr_pencere_simge_ayarla(long long id,
                                    const char *yol_ptr, long long yol_uzunluk) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].pencere) return -1;

    char *yol = metin_to_cstr_p(yol_ptr, yol_uzunluk);
    if (!yol) return -1;

    gtk_window_set_icon_from_file(GTK_WINDOW(pencereler[id].pencere), yol, NULL);
    free(yol);
    return 0;
}

/* ========== GÖRÜNÜRLÜK ========== */

/* pencere.goster(pencere_id: tam) -> tam */
long long _tr_pencere_goster(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].pencere) return -1;

    gtk_widget_show_all(pencereler[id].pencere);
    return 0;
}

/* pencere.gizle(pencere_id: tam) -> tam */
long long _tr_pencere_gizle(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].pencere) return -1;

    gtk_widget_hide(pencereler[id].pencere);
    return 0;
}

/* pencere.kapat(pencere_id: tam) -> tam */
long long _tr_pencere_kapat(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].pencere) return -1;

    gtk_widget_destroy(pencereler[id].pencere);
    pencereler[id].pencere = NULL;
    pencereler[id].kapatildi = 1;
    return 0;
}

/* ========== TAM EKRAN ========== */

/* pencere.tam_ekran(pencere_id: tam) -> tam */
long long _tr_pencere_tam_ekran(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].pencere) return -1;

    gtk_window_fullscreen(GTK_WINDOW(pencereler[id].pencere));
    return 0;
}

/* pencere.tam_ekrandan_cik(pencere_id: tam) -> tam */
long long _tr_pencere_tam_ekrandan_cik(long long id) {
    if (id < 0 || id >= pencere_sayisi || !pencereler[id].pencere) return -1;

    gtk_window_unfullscreen(GTK_WINDOW(pencereler[id].pencere));
    return 0;
}

/* ========== OLAY DÖNGÜSÜ ========== */

/* pencere.olaylari_isle() -> tam */
long long _tr_pencere_olaylari_isle(void) {
    if (!gtk_baslatildi) return 0;

    /* Bekleyen tüm GTK olaylarını işle (non-blocking) */
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    /* Tüm pencereler kapatıldı mı kontrol et */
    int acik_var = 0;
    for (int i = 0; i < pencere_sayisi; i++) {
        if (!pencereler[i].kapatildi) {
            acik_var = 1;
            break;
        }
    }

    return acik_var ? 1 : 0;
}

/* pencere.calistir() -> tam */
long long _tr_pencere_calistir(void) {
    if (!gtk_baslatildi) return -1;

    gtk_main();
    return 0;
}

/* ========== DURUM SORGULAMA ========== */

/* pencere.kapatildi_mi(pencere_id: tam) -> tam */
long long _tr_pencere_kapatildi_mi(long long id) {
    if (id < 0 || id >= pencere_sayisi) return 1;
    return pencereler[id].kapatildi ? 1 : 0;
}

/* pencere.icerik_ayarla(pencere_id: tam, widget_id: tam) -> tam */
long long _tr_pencere_icerik_ayarla(long long pencere_id, long long widget_id) {
    if (pencere_id < 0 || pencere_id >= pencere_sayisi || !pencereler[pencere_id].pencere)
        return -1;

    /* Widget id'den GtkWidget* al (arayuz modülünden) */
    extern GtkWidget *_tr_arayuz_widget_al(long long id);
    GtkWidget *widget = _tr_arayuz_widget_al(widget_id);
    if (!widget) return -1;

    gtk_container_add(GTK_CONTAINER(pencereler[pencere_id].pencere), widget);
    return 0;
}
