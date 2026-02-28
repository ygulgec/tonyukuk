/*
 * uretici_arm_m0.c — ARM Cortex-M0+ (Raspberry Pi Pico) kod üreteci
 *
 * Tonyukuk Türkçe programlama dili derleyicisi için ARM Cortex-M0+ backend.
 * Pico SDK ile uyumlu C kodu üretir.
 *
 * RP2040 özellikleri:
 *   - Dual-core ARM Cortex-M0+ (133 MHz)
 *   - 264 KB SRAM
 *   - 2 MB Flash (harici)
 *   - 30 GPIO pin
 *   - 2x UART, 2x SPI, 2x I2C
 *   - 16x PWM kanalı
 *   - 3x 12-bit ADC
 *   - 8x PIO state machine
 *
 * Bu üretici Pico SDK ile derlenecek C kodu üretir.
 */

#include "uretici_arm_m0.h"
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
static void ifade_str_uret(Üretici *u, Düğüm *d, Metin *out);
static void bildirim_uret_indent(Üretici *u, Düğüm *d, int indent);
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
    metin_ekle(out, d->veri.mantık_değer ? "true" : "false");
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

    metin_ekle(out, isim);
    metin_ekle(out, "(");

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
            metin_ekle(out, "0");
            break;
    }
}

/* ---- Bildirim üretimi ---- */

static void degisken_uret(Üretici *u, Düğüm *d, int indent) {
    const char *isim = d->veri.değişken.isim;
    const char *tip = d->veri.değişken.tip;

    const char *c_tip = "int32_t";
    if (tip) {
        if (strcmp(tip, "tam") == 0) c_tip = "int32_t";
        else if (strcmp(tip, "ondalık") == 0 || strcmp(tip, "ondalik") == 0) c_tip = "float";
        else if (strcmp(tip, "metin") == 0) c_tip = "const char*";
        else if (strcmp(tip, "mantık") == 0 || strcmp(tip, "mantik") == 0) c_tip = "bool";
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
    Metin sol_str, sag_str;
    metin_baslat(&sol_str);
    metin_baslat(&sag_str);

    ifade_str_uret(u, d->çocuklar[0], &sol_str);
    ifade_str_uret(u, d->çocuklar[1], &sag_str);

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

    const char *c_donus = "void";
    if (dönüş_tipi) {
        if (strcmp(dönüş_tipi, "tam") == 0) c_donus = "int32_t";
        else if (strcmp(dönüş_tipi, "ondalık") == 0 || strcmp(dönüş_tipi, "ondalik") == 0) c_donus = "float";
        else if (strcmp(dönüş_tipi, "metin") == 0) c_donus = "const char*";
        else if (strcmp(dönüş_tipi, "mantık") == 0 || strcmp(dönüş_tipi, "mantik") == 0) c_donus = "bool";
    }

    yaz(u, "");
    yaz(u, "%s %s(", c_donus, isim);

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
                else if (strcmp(param_tip, "ondalık") == 0 || strcmp(param_tip, "ondalik") == 0) c_tip = "float";
                else if (strcmp(param_tip, "metin") == 0) c_tip = "const char*";
            }

            if (i > 0) yaz(u, ", ");
            yaz(u, "%s %s", c_tip, param_isim);
        }
        yaz(u, ") {");
    }

    if (d->çocuk_sayısı > 1) {
        blok_uret(u, d->çocuklar[1], 1);
    }

    yaz(u, "}");
}

/* ---- Ana kod üretim fonksiyonu ---- */

void kod_uret_arm_m0(Üretici *u, Düğüm *program, Arena *arena) {
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

    /* Pico SDK başlık */
    yaz(u, "/*");
    yaz(u, " * Tonyukuk Derleyici tarafından üretildi");
    yaz(u, " * Hedef: Raspberry Pi Pico (RP2040)");
    yaz(u, " * Pico SDK ile derleyin");
    yaz(u, " */");
    yaz(u, "");
    yaz(u, "#include <stdio.h>");
    yaz(u, "#include <stdint.h>");
    yaz(u, "#include <stdbool.h>");
    yaz(u, "#include <string.h>");
    yaz(u, "#include \"pico/stdlib.h\"");
    yaz(u, "#include \"hardware/gpio.h\"");
    yaz(u, "#include \"hardware/pwm.h\"");
    yaz(u, "#include \"hardware/adc.h\"");
    yaz(u, "#include \"hardware/i2c.h\"");
    yaz(u, "#include \"hardware/spi.h\"");
    yaz(u, "#include \"hardware/uart.h\"");
    yaz(u, "");

    /* Donanım fonksiyon makroları */
    yaz(u, "/* Donanım Fonksiyonları */");
    yaz(u, "#define pin_modu(pin, mod) _tr_pin_modu(pin, mod)");
    yaz(u, "#define dijital_yaz(pin, val) gpio_put(pin, val)");
    yaz(u, "#define dijital_oku(pin) gpio_get(pin)");
    yaz(u, "#define pwm_baslat(pin) _tr_pwm_baslat(pin)");
    yaz(u, "#define pwm_yaz(pin, val) _tr_pwm_yaz(pin, val)");
    yaz(u, "#define analog_oku(pin) _tr_analog_oku(pin)");
    yaz(u, "#define bekle_ms(ms) sleep_ms(ms)");
    yaz(u, "#define bekle_us(us) sleep_us(us)");
    yaz(u, "#define milis() to_ms_since_boot(get_absolute_time())");
    yaz(u, "#define mikros() to_us_since_boot(get_absolute_time())");
    yaz(u, "#define seri_baslat(baud) stdio_init_all()");
    yaz(u, "#define seri_yaz(str) printf(\"%%s\", str)");
    yaz(u, "#define yazdır(x) _Generic((x), \\");
    yaz(u, "    int: printf(\"%%d\\n\", x), \\");
    yaz(u, "    int32_t: printf(\"%%d\\n\", x), \\");
    yaz(u, "    long long: printf(\"%%lld\\n\", x), \\");
    yaz(u, "    float: printf(\"%%f\\n\", x), \\");
    yaz(u, "    double: printf(\"%%f\\n\", x), \\");
    yaz(u, "    char*: printf(\"%%s\\n\", x), \\");
    yaz(u, "    const char*: printf(\"%%s\\n\", x))");
    yaz(u, "");

    /* Sabitler */
    yaz(u, "/* Sabitler */");
    yaz(u, "#define GIRIS 0");
    yaz(u, "#define CIKIS 1");
    yaz(u, "#define YUKSEK 1");
    yaz(u, "#define DUSUK 0");
    yaz(u, "#define DAHILI_LED PICO_DEFAULT_LED_PIN");
    yaz(u, "");

    /* Donanım fonksiyon implementasyonları */
    yaz(u, "/* GPIO */");
    yaz(u, "static void _tr_pin_modu(uint pin, int mod) {");
    yaz(u, "    gpio_init(pin);");
    yaz(u, "    gpio_set_dir(pin, mod ? GPIO_OUT : GPIO_IN);");
    yaz(u, "}");
    yaz(u, "");
    yaz(u, "/* PWM */");
    yaz(u, "static uint _pwm_slice[30] = {0};");
    yaz(u, "static void _tr_pwm_baslat(uint pin) {");
    yaz(u, "    gpio_set_function(pin, GPIO_FUNC_PWM);");
    yaz(u, "    uint slice = pwm_gpio_to_slice_num(pin);");
    yaz(u, "    _pwm_slice[pin] = slice;");
    yaz(u, "    pwm_set_wrap(slice, 255);");
    yaz(u, "    pwm_set_enabled(slice, true);");
    yaz(u, "}");
    yaz(u, "");
    yaz(u, "static void _tr_pwm_yaz(uint pin, int val) {");
    yaz(u, "    uint channel = pwm_gpio_to_channel(pin);");
    yaz(u, "    pwm_set_chan_level(_pwm_slice[pin], channel, val);");
    yaz(u, "}");
    yaz(u, "");
    yaz(u, "/* ADC */");
    yaz(u, "static int _adc_initialized = 0;");
    yaz(u, "static int _tr_analog_oku(uint pin) {");
    yaz(u, "    if (!_adc_initialized) {");
    yaz(u, "        adc_init();");
    yaz(u, "        _adc_initialized = 1;");
    yaz(u, "    }");
    yaz(u, "    /* GPIO26-29 -> ADC0-3 */");
    yaz(u, "    uint channel = pin - 26;");
    yaz(u, "    if (channel > 3) return 0;");
    yaz(u, "    adc_gpio_init(pin);");
    yaz(u, "    adc_select_input(channel);");
    yaz(u, "    return adc_read();");
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

    /* main() fonksiyonu */
    yaz(u, "");
    yaz(u, "int main(void) {");
    yaz(u, "    stdio_init_all();");
    yaz(u, "");

    /* Üst düzey bildirimleri üret */
    for (int i = 0; i < program->çocuk_sayısı; i++) {
        Düğüm *d = program->çocuklar[i];
        if (d->tur != DÜĞÜM_İŞLEV && d->tur != DÜĞÜM_KULLAN) {
            bildirim_uret_indent(u, d, 1);
        }
    }

    yaz(u, "");
    yaz(u, "    /* ana() fonksiyonunu çağır */");
    yaz(u, "    ana();");
    yaz(u, "");
    yaz(u, "    return 0;");
    yaz(u, "}");
}
