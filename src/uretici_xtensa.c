/*
 * uretici_xtensa.c — Xtensa (ESP32/ESP8266) kod üreteci
 *
 * Tonyukuk Türkçe programlama dili derleyicisi için Xtensa backend.
 * ESP-IDF framework ile uyumlu C kodu üretir.
 *
 * ESP32 özellikleri:
 *   - Dual-core Xtensa LX6 (240 MHz)
 *   - 520 KB SRAM
 *   - WiFi 802.11 b/g/n
 *   - Bluetooth 4.2 + BLE
 *   - 34 GPIO pin
 *   - 12-bit ADC, 8-bit DAC
 *   - FreeRTOS tabanlı
 *
 * Bu üretici doğrudan C kodu üretir çünkü ESP-IDF framework
 * ile entegrasyon için en pratik yöntem budur.
 */

#include "uretici_xtensa.h"
#include "uretici.h"
#include "hata.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* ---- Yardımcı: C kodu satırı yaz ---- */

static void yaz(Üretici *u, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->cikti, buf);
}

static void veri_yaz(Üretici *u, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    metin_satir_ekle(&u->veri_bolumu, buf);
}

static int yeni_etiket(Üretici *u) {
    return u->etiket_sayac++;
}

/* İleri bildirimler */
static void ifade_üret(Üretici *u, Düğüm *d);
static void bildirim_uret(Üretici *u, Düğüm *d);
static void blok_uret(Üretici *u, Düğüm *blok, int indent);

/* Girinti oluştur */
static void girintili_yaz(Üretici *u, int indent, const char *fmt, ...) {
    char buf[1024];
    char indent_str[64] = "";
    for (int i = 0; i < indent; i++) {
        strcat(indent_str, "    ");
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char full[1088];
    snprintf(full, sizeof(full), "%s%s", indent_str, buf);
    metin_satir_ekle(&u->cikti, full);
}

/* ---- String literal ---- */

static int metin_literal_ekle(Üretici *u, const char *metin, int uzunluk) {
    int idx = u->metin_sayac++;

    veri_yaz(u, "static const char _str_%d[] = \"", idx);

    /* Escape karakterleri işle */
    Metin buf;
    metin_baslat(&buf);

    for (int i = 0; i < uzunluk; i++) {
        unsigned char c = (unsigned char)metin[i];

        if (c == '\\' && i + 1 < uzunluk) {
            char sonraki = metin[i + 1];
            switch (sonraki) {
                case 'n':  metin_ekle(&buf, "\\n"); i++; continue;
                case 't':  metin_ekle(&buf, "\\t"); i++; continue;
                case 'r':  metin_ekle(&buf, "\\r"); i++; continue;
                case '\\': metin_ekle(&buf, "\\\\"); i++; continue;
                case '"':  metin_ekle(&buf, "\\\""); i++; continue;
            }
        }

        if (c == '"') {
            metin_ekle(&buf, "\\\"");
        } else if (c == '\\') {
            metin_ekle(&buf, "\\\\");
        } else if (c >= 32 && c < 127) {
            char tmp[2] = {(char)c, 0};
            metin_ekle(&buf, tmp);
        } else {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "\\x%02x", c);
            metin_ekle(&buf, tmp);
        }
    }

    veri_yaz(u, "%s\";", buf.veri);
    metin_serbest(&buf);

    return idx;
}

/* ---- İfade üretimi ---- */

static void ifade_str_uret(Üretici *u, Düğüm *d, Metin *out);

static void tam_sayi_str(Üretici *u, Düğüm *d, Metin *out) {
    (void)u;
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)d->veri.tam_deger);
    metin_ekle(out, buf);
}

static void ondalik_str(Üretici *u, Düğüm *d, Metin *out) {
    (void)u;
    char buf[64];
    snprintf(buf, sizeof(buf), "%f", d->veri.ondalık_değer);
    metin_ekle(out, buf);
}

static void metin_str(Üretici *u, Düğüm *d, Metin *out) {
    int idx = metin_literal_ekle(u, d->veri.metin_değer,
                                  (int)strlen(d->veri.metin_değer));
    char buf[32];
    snprintf(buf, sizeof(buf), "_str_%d", idx);
    metin_ekle(out, buf);
}

static void mantik_str(Üretici *u, Düğüm *d, Metin *out) {
    (void)u;
    metin_ekle(out, d->veri.mantık_değer ? "1" : "0");
}

static void tanimlayici_str(Üretici *u, Düğüm *d, Metin *out) {
    (void)u;
    metin_ekle(out, d->veri.tanimlayici.isim);
}

static void ikili_islem_str(Üretici *u, Düğüm *d, Metin *out) {
    metin_ekle(out, "(");
    ifade_str_uret(u, d->çocuklar[0], out);

    SözcükTürü op = d->veri.islem.islem;
    const char *op_str = " ? ";

    switch (op) {
        case TOK_ARTI:        op_str = " + "; break;
        case TOK_EKSI:        op_str = " - "; break;
        case TOK_ÇARPIM:      op_str = " * "; break;
        case TOK_BÖLME:       op_str = " / "; break;
        case TOK_YÜZDE:       op_str = " % "; break;
        case TOK_EŞİT_EŞİT:   op_str = " == "; break;
        case TOK_EŞİT_DEĞİL:  op_str = " != "; break;
        case TOK_KÜÇÜK:       op_str = " < "; break;
        case TOK_KÜÇÜK_EŞİT:  op_str = " <= "; break;
        case TOK_BÜYÜK:       op_str = " > "; break;
        case TOK_BÜYÜK_EŞİT:  op_str = " >= "; break;
        case TOK_VE:          op_str = " && "; break;
        case TOK_VEYA:        op_str = " || "; break;
        default: break;
    }

    metin_ekle(out, op_str);
    ifade_str_uret(u, d->çocuklar[1], out);
    metin_ekle(out, ")");
}

static void tekli_islem_str(Üretici *u, Düğüm *d, Metin *out) {
    SözcükTürü op = d->veri.islem.islem;

    if (op == TOK_EKSI) {
        metin_ekle(out, "(-");
        ifade_str_uret(u, d->çocuklar[0], out);
        metin_ekle(out, ")");
    } else if (op == TOK_DEĞİL) {
        metin_ekle(out, "(!");
        ifade_str_uret(u, d->çocuklar[0], out);
        metin_ekle(out, ")");
    }
}

static void cagri_str(Üretici *u, Düğüm *d, Metin *out) {
    const char *isim = d->veri.tanimlayici.isim;
    if (!isim) return;

    /* Fonksiyon adını yaz */
    metin_ekle(out, isim);
    metin_ekle(out, "(");

    /* Argümanları yaz */
    for (int i = 0; i < d->çocuk_sayısı; i++) {
        if (i > 0) metin_ekle(out, ", ");
        ifade_str_uret(u, d->çocuklar[i], out);
    }

    metin_ekle(out, ")");
}

static void ifade_str_uret(Üretici *u, Düğüm *d, Metin *out) {
    if (!d) return;

    switch (d->tur) {
        case DÜĞÜM_TAM_SAYI:
            tam_sayi_str(u, d, out);
            break;
        case DÜĞÜM_ONDALIK_SAYI:
            ondalik_str(u, d, out);
            break;
        case DÜĞÜM_METİN_DEĞERİ:
            metin_str(u, d, out);
            break;
        case DÜĞÜM_MANTIK_DEĞERİ:
            mantik_str(u, d, out);
            break;
        case DÜĞÜM_TANIMLAYICI:
            tanimlayici_str(u, d, out);
            break;
        case DÜĞÜM_İKİLİ_İŞLEM:
            ikili_islem_str(u, d, out);
            break;
        case DÜĞÜM_TEKLİ_İŞLEM:
            tekli_islem_str(u, d, out);
            break;
        case DÜĞÜM_ÇAĞRI:
            cagri_str(u, d, out);
            break;
        default:
            metin_ekle(out, "0 /* unsupported */");
            break;
    }
}

static void ifade_üret(Üretici *u, Düğüm *d) {
    Metin out;
    metin_baslat(&out);
    ifade_str_uret(u, d, &out);
    yaz(u, "%s", out.veri);
    metin_serbest(&out);
}

/* ---- Bildirim üretimi ---- */

static void degisken_uret(Üretici *u, Düğüm *d, int indent) {
    const char *isim = d->veri.değişken.isim;
    const char *tip = d->veri.değişken.tip;

    /* Tip dönüşümü */
    const char *c_tip = "int32_t";
    if (tip) {
        if (strcmp(tip, "tam") == 0) c_tip = "int32_t";
        else if (strcmp(tip, "ondalık") == 0 || strcmp(tip, "ondalik") == 0) c_tip = "double";
        else if (strcmp(tip, "metin") == 0) c_tip = "const char*";
        else if (strcmp(tip, "mantık") == 0 || strcmp(tip, "mantik") == 0) c_tip = "int";
    }

    if (d->çocuk_sayısı > 0) {
        Metin val;
        metin_baslat(&val);
        ifade_str_uret(u, d->çocuklar[0], &val);
        girintili_yaz(u, indent, "%s %s = %s;", c_tip, isim, val.veri);
        metin_serbest(&val);
    } else {
        girintili_yaz(u, indent, "%s %s = 0;", c_tip, isim);
    }
}

static void atama_uret(Üretici *u, Düğüm *d, int indent) {
    Düğüm *sol = d->çocuklar[0];
    Düğüm *sag = d->çocuklar[1];

    Metin sol_str, sag_str;
    metin_baslat(&sol_str);
    metin_baslat(&sag_str);

    ifade_str_uret(u, sol, &sol_str);
    ifade_str_uret(u, sag, &sag_str);

    girintili_yaz(u, indent, "%s = %s;", sol_str.veri, sag_str.veri);

    metin_serbest(&sol_str);
    metin_serbest(&sag_str);
}

static void eger_uret(Üretici *u, Düğüm *d, int indent) {
    Metin kosul;
    metin_baslat(&kosul);
    ifade_str_uret(u, d->çocuklar[0], &kosul);

    girintili_yaz(u, indent, "if (%s) {", kosul.veri);
    metin_serbest(&kosul);

    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1], indent + 1);
    }

    if (d->çocuk_sayısı > 2) {
        girintili_yaz(u, indent, "} else {");
        blok_uret(u, d->çocuklar[2], indent + 1);
    }

    girintili_yaz(u, indent, "}");
}

static void iken_uret(Üretici *u, Düğüm *d, int indent) {
    Metin kosul;
    metin_baslat(&kosul);
    ifade_str_uret(u, d->çocuklar[0], &kosul);

    girintili_yaz(u, indent, "while (%s) {", kosul.veri);
    metin_serbest(&kosul);

    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1], indent + 1);
    }

    girintili_yaz(u, indent, "}");
}

static void dongu_uret(Üretici *u, Düğüm *d, int indent) {
    const char *sayac = d->veri.dongu.isim;

    Metin başlangıç, bitis;
    metin_baslat(&başlangıç);
    metin_baslat(&bitis);

    ifade_str_uret(u, d->çocuklar[0], &başlangıç);
    ifade_str_uret(u, d->çocuklar[1], &bitis);

    girintili_yaz(u, indent, "for (int32_t %s = %s; %s < %s; %s++) {",
                  sayac, başlangıç.veri, sayac, bitis.veri, sayac);

    metin_serbest(&başlangıç);
    metin_serbest(&bitis);

    if (d->çocuk_sayısı > 2) {
        blok_uret(u, d->çocuklar[2], indent + 1);
    }

    girintili_yaz(u, indent, "}");
}

static void dondur_uret(Üretici *u, Düğüm *d, int indent) {
    if (d->çocuk_sayısı > 0) {
        Metin val;
        metin_baslat(&val);
        ifade_str_uret(u, d->çocuklar[0], &val);
        girintili_yaz(u, indent, "return %s;", val.veri);
        metin_serbest(&val);
    } else {
        girintili_yaz(u, indent, "return;");
    }
}

static void bildirim_uret_indent(Üretici *u, Düğüm *d, int indent) {
    if (!d) return;

    switch (d->tur) {
        case DÜĞÜM_DEĞİŞKEN:
            degisken_uret(u, d, indent);
            break;
        case DÜĞÜM_ATAMA:
            atama_uret(u, d, indent);
            break;
        case DÜĞÜM_EĞER:
            eger_uret(u, d, indent);
            break;
        case DÜĞÜM_İKEN:
            iken_uret(u, d, indent);
            break;
        case DÜĞÜM_DÖNGÜ:
            dongu_uret(u, d, indent);
            break;
        case DÜĞÜM_DÖNDÜR:
            dondur_uret(u, d, indent);
            break;
        case DÜĞÜM_KIR:
            girintili_yaz(u, indent, "break;");
            break;
        case DÜĞÜM_DEVAM:
            girintili_yaz(u, indent, "continue;");
            break;
        case DÜĞÜM_İFADE_BİLDİRİMİ:
            if (d->çocuk_sayısı > 0) {
                Metin expr;
                metin_baslat(&expr);
                ifade_str_uret(u, d->çocuklar[0], &expr);
                girintili_yaz(u, indent, "%s;", expr.veri);
                metin_serbest(&expr);
            }
            break;
        case DÜĞÜM_ÇAĞRI: {
            Metin expr;
            metin_baslat(&expr);
            cagri_str(u, d, &expr);
            girintili_yaz(u, indent, "%s;", expr.veri);
            metin_serbest(&expr);
            break;
        }
        default:
            break;
    }
}

static void bildirim_uret(Üretici *u, Düğüm *d) {
    bildirim_uret_indent(u, d, 1);
}

static void blok_uret(Üretici *u, Düğüm *blok, int indent) {
    if (!blok) return;

    for (int i = 0; i < blok->çocuk_sayısı; i++) {
        bildirim_uret_indent(u, blok->çocuklar[i], indent);
    }
}

/* ---- Fonksiyon üretimi ---- */

static void islev_uret(Üretici *u, Düğüm *d) {
    const char *isim = d->veri.islev.isim;
    const char *dönüş_tipi = d->veri.islev.dönüş_tipi;

    /* Dönüş tipi */
    const char *c_donus = "void";
    if (dönüş_tipi) {
        if (strcmp(dönüş_tipi, "tam") == 0) c_donus = "int32_t";
        else if (strcmp(dönüş_tipi, "ondalık") == 0 || strcmp(dönüş_tipi, "ondalik") == 0) c_donus = "double";
        else if (strcmp(dönüş_tipi, "metin") == 0) c_donus = "const char*";
        else if (strcmp(dönüş_tipi, "mantık") == 0 || strcmp(dönüş_tipi, "mantik") == 0) c_donus = "int";
    }

    /* Fonksiyon imzası */
    yaz(u, "");
    yaz(u, "%s %s(", c_donus, isim);

    /* Parametreler */
    Düğüm *params = d->çocuklar[0];
    if (params->çocuk_sayısı == 0) {
        yaz(u, "void) {");
    } else {
        for (int i = 0; i < params->çocuk_sayısı; i++) {
            const char *param_isim = params->çocuklar[i]->veri.değişken.isim;
            const char *param_tip = params->çocuklar[i]->veri.değişken.tip;

            const char *c_tip = "int32_t";
            if (param_tip) {
                if (strcmp(param_tip, "tam") == 0) c_tip = "int32_t";
                else if (strcmp(param_tip, "ondalık") == 0 || strcmp(param_tip, "ondalik") == 0) c_tip = "double";
                else if (strcmp(param_tip, "metin") == 0) c_tip = "const char*";
            }

            if (i > 0) yaz(u, ", ");
            yaz(u, "%s %s", c_tip, param_isim);
        }
        yaz(u, ") {");
    }

    /* Fonksiyon gövdesi */
    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1], 1);
    }

    yaz(u, "}");
}

/* ---- Ana kod üretim fonksiyonu ---- */

void kod_uret_xtensa(Üretici *u, Düğüm *program, Arena *arena) {
    u->arena = arena;
    u->etiket_sayac = 0;
    u->metin_sayac = 0;
    u->dongu_baslangic_etiket = 0;
    u->dongu_bitis_etiket = 0;
    u->mevcut_islev_donus_tipi = TİP_BOŞLUK;
    u->mevcut_sinif = NULL;

    metin_baslat(&u->cikti);
    metin_baslat(&u->veri_bolumu);
    metin_baslat(&u->bss_bolumu);
    metin_baslat(&u->yardimcilar);

    u->kapsam = kapsam_oluştur(arena, NULL);

    /* ESP-IDF başlık */
    yaz(u, "/*");
    yaz(u, " * Tonyukuk Derleyici tarafından üretildi");
    yaz(u, " * Hedef: ESP32 (Xtensa LX6)");
    yaz(u, " * ESP-IDF ile derleyin");
    yaz(u, " */");
    yaz(u, "");
    yaz(u, "#include <stdio.h>");
    yaz(u, "#include <stdint.h>");
    yaz(u, "#include <string.h>");
    yaz(u, "#include \"freertos/FreeRTOS.h\"");
    yaz(u, "#include \"freertos/task.h\"");
    yaz(u, "#include \"driver/gpio.h\"");
    yaz(u, "#include \"driver/ledc.h\"");
    yaz(u, "#include \"driver/adc.h\"");
    yaz(u, "#include \"driver/i2c.h\"");
    yaz(u, "#include \"driver/spi_master.h\"");
    yaz(u, "#include \"driver/uart.h\"");
    yaz(u, "#include \"esp_log.h\"");
    yaz(u, "#include \"esp_wifi.h\"");
    yaz(u, "#include \"esp_event.h\"");
    yaz(u, "#include \"nvs_flash.h\"");
    yaz(u, "");
    yaz(u, "static const char *TAG = \"tonyukuk\";");
    yaz(u, "");

    /* Donanım fonksiyon makroları */
    yaz(u, "/* Donanım Fonksiyonları */");
    yaz(u, "#define pin_modu(pin, mod) _tr_pin_modu(pin, mod)");
    yaz(u, "#define dijital_yaz(pin, val) _tr_dijital_yaz(pin, val)");
    yaz(u, "#define dijital_oku(pin) _tr_dijital_oku(pin)");
    yaz(u, "#define pwm_baslat(pin) _tr_pwm_baslat(pin)");
    yaz(u, "#define pwm_yaz(pin, val) _tr_pwm_yaz(pin, val)");
    yaz(u, "#define analog_oku(pin) _tr_analog_oku(pin)");
    yaz(u, "#define bekle_ms(ms) vTaskDelay(pdMS_TO_TICKS(ms))");
    yaz(u, "#define milis() (xTaskGetTickCount() * portTICK_PERIOD_MS)");
    yaz(u, "#define seri_baslat(baud) _tr_seri_baslat(baud)");
    yaz(u, "#define seri_yaz(str) printf(\"%%s\", str)");
    yaz(u, "#define yazdır(x) _Generic((x), \\");
    yaz(u, "    int: printf(\"%%d\\n\", x), \\");
    yaz(u, "    int32_t: printf(\"%%d\\n\", x), \\");
    yaz(u, "    long long: printf(\"%%lld\\n\", x), \\");
    yaz(u, "    double: printf(\"%%f\\n\", x), \\");
    yaz(u, "    char*: printf(\"%%s\\n\", x), \\");
    yaz(u, "    const char*: printf(\"%%s\\n\", x))");
    yaz(u, "");

    /* Donanım fonksiyon implementasyonları */
    yaz(u, "/* GPIO */");
    yaz(u, "static void _tr_pin_modu(int pin, int mod) {");
    yaz(u, "    gpio_config_t io_conf = {");
    yaz(u, "        .pin_bit_mask = (1ULL << pin),");
    yaz(u, "        .mode = mod ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,");
    yaz(u, "        .pull_up_en = GPIO_PULLUP_DISABLE,");
    yaz(u, "        .pull_down_en = GPIO_PULLDOWN_DISABLE,");
    yaz(u, "        .intr_type = GPIO_INTR_DISABLE");
    yaz(u, "    };");
    yaz(u, "    gpio_config(&io_conf);");
    yaz(u, "}");
    yaz(u, "");
    yaz(u, "static void _tr_dijital_yaz(int pin, int val) {");
    yaz(u, "    gpio_set_level(pin, val ? 1 : 0);");
    yaz(u, "}");
    yaz(u, "");
    yaz(u, "static int _tr_dijital_oku(int pin) {");
    yaz(u, "    return gpio_get_level(pin);");
    yaz(u, "}");
    yaz(u, "");
    yaz(u, "/* PWM (LEDC) */");
    yaz(u, "static void _tr_pwm_baslat(int pin) {");
    yaz(u, "    ledc_timer_config_t timer_conf = {");
    yaz(u, "        .speed_mode = LEDC_LOW_SPEED_MODE,");
    yaz(u, "        .timer_num = LEDC_TIMER_0,");
    yaz(u, "        .duty_resolution = LEDC_TIMER_8_BIT,");
    yaz(u, "        .freq_hz = 5000,");
    yaz(u, "        .clk_cfg = LEDC_AUTO_CLK");
    yaz(u, "    };");
    yaz(u, "    ledc_timer_config(&timer_conf);");
    yaz(u, "    ledc_channel_config_t ch_conf = {");
    yaz(u, "        .gpio_num = pin,");
    yaz(u, "        .speed_mode = LEDC_LOW_SPEED_MODE,");
    yaz(u, "        .channel = LEDC_CHANNEL_0,");
    yaz(u, "        .timer_sel = LEDC_TIMER_0,");
    yaz(u, "        .duty = 0,");
    yaz(u, "        .hpoint = 0");
    yaz(u, "    };");
    yaz(u, "    ledc_channel_config(&ch_conf);");
    yaz(u, "}");
    yaz(u, "");
    yaz(u, "static void _tr_pwm_yaz(int pin, int val) {");
    yaz(u, "    (void)pin;");
    yaz(u, "    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, val);");
    yaz(u, "    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);");
    yaz(u, "}");
    yaz(u, "");
    yaz(u, "/* ADC */");
    yaz(u, "static int _tr_analog_oku(int pin) {");
    yaz(u, "    adc1_config_width(ADC_WIDTH_BIT_12);");
    yaz(u, "    adc1_config_channel_atten(pin, ADC_ATTEN_DB_11);");
    yaz(u, "    return adc1_get_raw(pin);");
    yaz(u, "}");
    yaz(u, "");
    yaz(u, "/* UART */");
    yaz(u, "static void _tr_seri_baslat(int baud) {");
    yaz(u, "    uart_config_t uart_config = {");
    yaz(u, "        .baud_rate = baud,");
    yaz(u, "        .data_bits = UART_DATA_8_BITS,");
    yaz(u, "        .parity = UART_PARITY_DISABLE,");
    yaz(u, "        .stop_bits = UART_STOP_BITS_1,");
    yaz(u, "        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE");
    yaz(u, "    };");
    yaz(u, "    uart_param_config(UART_NUM_0, &uart_config);");
    yaz(u, "}");
    yaz(u, "");

    /* String literalleri */
    if (u->veri_bolumu.uzunluk > 0) {
        yaz(u, "/* String Literalleri */");
        metin_satir_ekle(&u->cikti, u->veri_bolumu.veri);
        yaz(u, "");
    }

    /* Fonksiyonları üret */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur == DÜĞÜM_İŞLEV) {
            islev_uret(u, d);
        }
    }

    /* Global değişkenler ve üst düzey kod için app_main */
    yaz(u, "");
    yaz(u, "void app_main(void) {");
    yaz(u, "    ESP_LOGI(TAG, \"Tonyukuk programı başlatılıyor...\");");
    yaz(u, "");

    /* Üst düzey bildirimleri üret */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur != DÜĞÜM_İŞLEV && d->tur != DÜĞÜM_KULLAN) {
            bildirim_uret_indent(u, d, 1);
        }
    }

    /* ana() fonksiyonu varsa çağır */
    yaz(u, "");
    yaz(u, "    /* ana() fonksiyonunu çağır */");
    yaz(u, "    ana();");
    yaz(u, "}");
}
