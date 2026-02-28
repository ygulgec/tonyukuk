/* Paralel modülü — çalışma zamanı implementasyonu */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "runtime.h"

/* ========== MODUL 5: Paralel (pthreads) ========== */

typedef struct {
    long long (*fn)(void);
    long long sonuç;
} _TrIsVeri;

/* Parametreli async için genişletilmiş yapı */
typedef struct {
    void *fn_ptr;
    long long args[8];  /* max 8 parametre */
    int arg_count;
    long long sonuç;
} _TrAsyncVeri;

static void *_tr_is_fonksiyonu(void *arg) {
    _TrIsVeri *veri = (_TrIsVeri *)arg;
    veri->sonuç = veri->fn();
    return NULL;
}

/* Parametreli async thread fonksiyonu */
static void *_tr_async_fonksiyonu(void *arg) {
    _TrAsyncVeri *veri = (_TrAsyncVeri *)arg;
    long long (*fn0)(void) = (long long (*)(void))veri->fn_ptr;
    long long (*fn1)(long long) = (long long (*)(long long))veri->fn_ptr;
    long long (*fn2)(long long, long long) = (long long (*)(long long, long long))veri->fn_ptr;
    long long (*fn3)(long long, long long, long long) = (long long (*)(long long, long long, long long))veri->fn_ptr;

    switch (veri->arg_count) {
        case 0: veri->sonuç = fn0(); break;
        case 1: veri->sonuç = fn1(veri->args[0]); break;
        case 2: veri->sonuç = fn2(veri->args[0], veri->args[1]); break;
        case 3: veri->sonuç = fn3(veri->args[0], veri->args[1], veri->args[2]); break;
        default: veri->sonuç = 0; break;
    }
    return NULL;
}

/* is_olustur(fonk_ptr) -> tam (thread id / handle) */
long long _tr_is_olustur(long long fn_ptr) {
    _TrIsVeri *veri = (_TrIsVeri *)malloc(sizeof(_TrIsVeri));
    if (!veri) return 0;
    veri->fn = (long long (*)(void))fn_ptr;
    veri->sonuç = 0;

    pthread_t *tid = (pthread_t *)malloc(sizeof(pthread_t));
    if (!tid) { free(veri); return 0; }

    if (pthread_create(tid, NULL, _tr_is_fonksiyonu, veri) != 0) {
        free(veri); free(tid);
        return 0;
    }

    /* tid ve veri'yi bir struct'ta paketleyelim */
    /* Basitlik icin: 16 byte = [pthread_t, veri_ptr] */
    long long *handle = (long long *)malloc(2 * sizeof(long long));
    if (!handle) return 0;
    handle[0] = (long long)*tid;
    handle[1] = (long long)veri;
    return (long long)handle;
}

/* is_bekle(is: tam) -> tam (sonuç) */
long long _tr_is_bekle(long long handle_ptr) {
    if (handle_ptr == 0) return 0;
    long long *handle = (long long *)handle_ptr;
    pthread_t tid = (pthread_t)handle[0];
    _TrIsVeri *veri = (_TrIsVeri *)handle[1];

    pthread_join(tid, NULL);
    long long sonuç = veri->sonuç;
    free(veri);
    free(handle);
    return sonuç;
}

/* Parametreli async oluştur: _tr_async_olustur(fn_ptr, arg_count, arg0, arg1, ...) */
long long _tr_async_olustur(long long fn_ptr, long long arg_count,
                            long long a0, long long a1, long long a2, long long a3) {
    _TrAsyncVeri *veri = (_TrAsyncVeri *)malloc(sizeof(_TrAsyncVeri));
    if (!veri) return 0;
    veri->fn_ptr = (void *)fn_ptr;
    veri->arg_count = (int)arg_count;
    veri->args[0] = a0;
    veri->args[1] = a1;
    veri->args[2] = a2;
    veri->args[3] = a3;
    veri->sonuç = 0;

    pthread_t *tid = (pthread_t *)malloc(sizeof(pthread_t));
    if (!tid) { free(veri); return 0; }

    if (pthread_create(tid, NULL, _tr_async_fonksiyonu, veri) != 0) {
        free(veri); free(tid);
        return 0;
    }

    /* Handle: [pthread_t, veri_ptr] */
    long long *handle = (long long *)malloc(2 * sizeof(long long));
    if (!handle) return 0;
    handle[0] = (long long)*tid;
    handle[1] = (long long)veri;
    return (long long)handle;
}

/* Parametreli async bekle */
long long _tr_async_bekle(long long handle_ptr) {
    if (handle_ptr == 0) return 0;
    long long *handle = (long long *)handle_ptr;
    pthread_t tid = (pthread_t)handle[0];
    _TrAsyncVeri *veri = (_TrAsyncVeri *)handle[1];

    pthread_join(tid, NULL);
    long long sonuç = veri->sonuç;
    free(veri);
    free(handle);
    return sonuç;
}

/* kilit_olustur() -> tam (mutex pointer) */
long long _tr_kilit_olustur(void) {
    pthread_mutex_t *mtx = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (!mtx) return 0;
    pthread_mutex_init(mtx, NULL);
    return (long long)mtx;
}

/* kilitle(kilit: tam) */
void _tr_kilitle(long long kilit_ptr) {
    if (kilit_ptr == 0) return;
    pthread_mutex_lock((pthread_mutex_t *)kilit_ptr);
}

/* kilit_birak(kilit: tam) */
void _tr_kilit_birak(long long kilit_ptr) {
    if (kilit_ptr == 0) return;
    pthread_mutex_unlock((pthread_mutex_t *)kilit_ptr);
}
