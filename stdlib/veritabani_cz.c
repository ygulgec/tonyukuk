/* Veritabanı modülü — çalışma zamanı implementasyonu */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "runtime.h"

/* ========== MODUL 6: Veritabani (SQLite) ========== */

/* Weak-linked sqlite3 - kullanılmazsa bağlama hatası vermez */
/* Fonksiyonlar dlopen ile yüklenir veya doğrudan bağlanır */
#ifdef __has_include
#if __has_include(<sqlite3.h>)
#include <sqlite3.h>
#define HAS_SQLITE3 1
#endif
#endif

#ifdef HAS_SQLITE3

/* ========== SQL Escape Fonksiyonu (SQL Injection Koruması) ========== */
/* Tehlikeli karakterleri escape eder: ' -> '' */
static char *_sql_escape(const char *input, size_t len) {
    /* En kötü durumda her karakter 2x olabilir */
    char *escaped = (char *)malloc(len * 2 + 1);
    if (!escaped) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\'') {
            escaped[j++] = '\'';
            escaped[j++] = '\'';
        } else if (input[i] == '\0') {
            break; /* NULL byte'da dur */
        } else {
            escaped[j++] = input[i];
        }
    }
    escaped[j] = '\0';
    return escaped;
}

/* SQL escape fonksiyonu - Tonyukuk'tan çağrılabilir */
TrMetin _tr_vt_escape(const char *metin_ptr, long long metin_len) {
    TrMetin m = {NULL, 0};
    if (!metin_ptr || metin_len <= 0) return m;

    char *escaped = _sql_escape(metin_ptr, (size_t)metin_len);
    if (!escaped) return m;

    m.ptr = escaped;
    m.len = (long long)strlen(escaped);
    return m;
}
long long _tr_vt_ac(const char *yol_ptr, long long yol_len) {
    char yol[512];
    int n = (int)yol_len;
    if (n >= (int)sizeof(yol)) n = (int)sizeof(yol) - 1;
    memcpy(yol, yol_ptr, n);
    yol[n] = '\0';

    sqlite3 *db = NULL;
    int rc = sqlite3_open(yol, &db);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }
    return (long long)(intptr_t)db;
}

long long _tr_vt_calistir(long long db_handle, const char *sql_ptr, long long sql_len) {
    if (db_handle == 0) return -1;
    sqlite3 *db = (sqlite3 *)(intptr_t)db_handle;
    char *sql = (char *)malloc(sql_len + 1);
    if (!sql) return -1;
    memcpy(sql, sql_ptr, sql_len);
    sql[sql_len] = '\0';

    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    free(sql);
    if (err) sqlite3_free(err);
    if (rc != SQLITE_OK) return -1;
    return (long long)sqlite3_changes(db);
}

/* Callback for _tr_vt_sorgula */
typedef struct {
    char *buf;
    long long len;
    long long cap;
} _VtSorguBuf;

static int _vt_sorgu_cb(void *data, int argc, char **argv, char **col_names) {
    _VtSorguBuf *sb = (_VtSorguBuf *)data;
    for (int i = 0; i < argc; i++) {
        const char *val = argv[i] ? argv[i] : "NULL";
        int vlen = (int)strlen(val);
        const char *cname = col_names[i] ? col_names[i] : "";
        int clen = (int)strlen(cname);
        /* Need: cname=val[,|\n] */
        int need = clen + 1 + vlen + 2;
        while (sb->len + need >= sb->cap) {
            sb->cap *= 2;
            sb->buf = (char *)realloc(sb->buf, sb->cap);
            if (!sb->buf) return 1;
        }
        if (i > 0) { sb->buf[sb->len++] = ','; }
        memcpy(sb->buf + sb->len, cname, clen); sb->len += clen;
        sb->buf[sb->len++] = '=';
        memcpy(sb->buf + sb->len, val, vlen); sb->len += vlen;
    }
    if (sb->len + 1 < sb->cap) {
        sb->buf[sb->len++] = '\n';
    }
    return 0;
}

TrMetin _tr_vt_sorgula(long long db_handle, const char *sql_ptr, long long sql_len) {
    TrMetin m = {NULL, 0};
    if (db_handle == 0) return m;
    sqlite3 *db = (sqlite3 *)(intptr_t)db_handle;
    char *sql = (char *)malloc(sql_len + 1);
    if (!sql) return m;
    memcpy(sql, sql_ptr, sql_len);
    sql[sql_len] = '\0';

    _VtSorguBuf sb;
    sb.cap = 1024;
    sb.len = 0;
    sb.buf = (char *)malloc(sb.cap);
    if (!sb.buf) { free(sql); return m; }

    char *err = NULL;
    sqlite3_exec(db, sql, _vt_sorgu_cb, &sb, &err);
    free(sql);
    if (err) sqlite3_free(err);

    m.ptr = sb.buf;
    m.len = sb.len;
    return m;
}

void _tr_vt_kapat(long long db_handle) {
    if (db_handle == 0) return;
    sqlite3 *db = (sqlite3 *)(intptr_t)db_handle;
    sqlite3_close(db);
}

/* ========== GÜVENLİ PREPARED STATEMENT API ========== */
/* SQL Injection'a karşı güvenli parameterized query desteği */

/* Prepared statement hazırla */
long long _tr_vt_hazirla(long long db_handle, const char *sql_ptr, long long sql_len) {
    if (db_handle == 0) return 0;
    sqlite3 *db = (sqlite3 *)(intptr_t)db_handle;

    char *sql = (char *)malloc(sql_len + 1);
    if (!sql) return 0;
    memcpy(sql, sql_ptr, sql_len);
    sql[sql_len] = '\0';

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    free(sql);

    if (rc != SQLITE_OK || !stmt) {
        return 0;
    }
    return (long long)(intptr_t)stmt;
}

/* Prepared statement'a metin parametresi bağla (1-indexed) */
long long _tr_vt_bagla_metin(long long stmt_handle, long long index,
                              const char *val_ptr, long long val_len) {
    if (stmt_handle == 0) return -1;
    sqlite3_stmt *stmt = (sqlite3_stmt *)(intptr_t)stmt_handle;

    /* Değeri kopyala çünkü SQLITE_TRANSIENT kullanacağız */
    char *val = (char *)malloc(val_len + 1);
    if (!val) return -1;
    memcpy(val, val_ptr, val_len);
    val[val_len] = '\0';

    int rc = sqlite3_bind_text(stmt, (int)index, val, (int)val_len, free);
    return rc == SQLITE_OK ? 0 : -1;
}

/* Prepared statement'a tam sayı parametresi bağla (1-indexed) */
long long _tr_vt_bagla_tam(long long stmt_handle, long long index, long long deger) {
    if (stmt_handle == 0) return -1;
    sqlite3_stmt *stmt = (sqlite3_stmt *)(intptr_t)stmt_handle;

    int rc = sqlite3_bind_int64(stmt, (int)index, deger);
    return rc == SQLITE_OK ? 0 : -1;
}

/* Prepared statement'a ondalık parametresi bağla (1-indexed) */
long long _tr_vt_bagla_ondalik(long long stmt_handle, long long index, double deger) {
    if (stmt_handle == 0) return -1;
    sqlite3_stmt *stmt = (sqlite3_stmt *)(intptr_t)stmt_handle;

    int rc = sqlite3_bind_double(stmt, (int)index, deger);
    return rc == SQLITE_OK ? 0 : -1;
}

/* Prepared statement çalıştır (INSERT/UPDATE/DELETE için) */
long long _tr_vt_adimla(long long stmt_handle) {
    if (stmt_handle == 0) return -1;
    sqlite3_stmt *stmt = (sqlite3_stmt *)(intptr_t)stmt_handle;

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) return 0;
    if (rc == SQLITE_ROW) return 1; /* Veri var */
    return -1; /* Hata */
}

/* Prepared statement'tan sütun değeri oku (SELECT için, 0-indexed) */
TrMetin _tr_vt_sutun_metin(long long stmt_handle, long long col_index) {
    TrMetin m = {NULL, 0};
    if (stmt_handle == 0) return m;
    sqlite3_stmt *stmt = (sqlite3_stmt *)(intptr_t)stmt_handle;

    const unsigned char *text = sqlite3_column_text(stmt, (int)col_index);
    if (!text) return m;

    int len = sqlite3_column_bytes(stmt, (int)col_index);
    m.ptr = (char *)malloc(len + 1);
    if (!m.ptr) return m;
    memcpy(m.ptr, text, len);
    m.ptr[len] = '\0';
    m.len = len;
    return m;
}

long long _tr_vt_sutun_tam(long long stmt_handle, long long col_index) {
    if (stmt_handle == 0) return 0;
    sqlite3_stmt *stmt = (sqlite3_stmt *)(intptr_t)stmt_handle;
    return sqlite3_column_int64(stmt, (int)col_index);
}

double _tr_vt_sutun_ondalik(long long stmt_handle, long long col_index) {
    if (stmt_handle == 0) return 0.0;
    sqlite3_stmt *stmt = (sqlite3_stmt *)(intptr_t)stmt_handle;
    return sqlite3_column_double(stmt, (int)col_index);
}

/* Prepared statement'ı sıfırla (tekrar kullanmak için) */
long long _tr_vt_sifirla(long long stmt_handle) {
    if (stmt_handle == 0) return -1;
    sqlite3_stmt *stmt = (sqlite3_stmt *)(intptr_t)stmt_handle;

    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    return 0;
}

/* Prepared statement'ı bitir ve belleği serbest bırak */
void _tr_vt_bitir(long long stmt_handle) {
    if (stmt_handle == 0) return;
    sqlite3_stmt *stmt = (sqlite3_stmt *)(intptr_t)stmt_handle;
    sqlite3_finalize(stmt);
}

#else
/* Stub implementations when sqlite3 is not available */
static void _sqlite_uyari(void) {
    fprintf(stderr, "Hata: SQLite3 desteği mevcut değil\n");
}
long long _tr_vt_ac(const char *yol_ptr, long long yol_len) {
    (void)yol_ptr; (void)yol_len; _sqlite_uyari(); return 0;
}
long long _tr_vt_calistir(long long db, const char *sql_ptr, long long sql_len) {
    (void)db; (void)sql_ptr; (void)sql_len; return -1;
}
TrMetin _tr_vt_sorgula(long long db, const char *sql_ptr, long long sql_len) {
    (void)db; (void)sql_ptr; (void)sql_len;
    TrMetin m = {NULL, 0}; return m;
}
TrMetin _tr_vt_escape(const char *metin_ptr, long long metin_len) {
    (void)metin_ptr; (void)metin_len;
    TrMetin m = {NULL, 0}; return m;
}
void _tr_vt_kapat(long long db) { (void)db; }
/* Prepared statement stubs */
long long _tr_vt_hazirla(long long db, const char *sql_ptr, long long sql_len) {
    (void)db; (void)sql_ptr; (void)sql_len; return 0;
}
long long _tr_vt_bagla_metin(long long stmt, long long idx, const char *val, long long len) {
    (void)stmt; (void)idx; (void)val; (void)len; return -1;
}
long long _tr_vt_bagla_tam(long long stmt, long long idx, long long val) {
    (void)stmt; (void)idx; (void)val; return -1;
}
long long _tr_vt_bagla_ondalik(long long stmt, long long idx, double val) {
    (void)stmt; (void)idx; (void)val; return -1;
}
long long _tr_vt_adimla(long long stmt) { (void)stmt; return -1; }
TrMetin _tr_vt_sutun_metin(long long stmt, long long col) {
    (void)stmt; (void)col; TrMetin m = {NULL, 0}; return m;
}
long long _tr_vt_sutun_tam(long long stmt, long long col) { (void)stmt; (void)col; return 0; }
double _tr_vt_sutun_ondalik(long long stmt, long long col) { (void)stmt; (void)col; return 0.0; }
long long _tr_vt_sifirla(long long stmt) { (void)stmt; return -1; }
void _tr_vt_bitir(long long stmt) { (void)stmt; }
#endif
