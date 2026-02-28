/*
 * uretici_xtensa.h — Xtensa (ESP32/ESP8266) kod üreteci header
 *
 * Tonyukuk Türkçe programlama dili derleyicisi için Xtensa backend.
 * ESP-IDF ile uyumlu C kodu üretir.
 */

#ifndef URETICI_XTENSA_H
#define URETICI_XTENSA_H

#include "agac.h"
#include "bellek.h"
#include "uretici.h"

/* AST'den ESP32 uyumlu C kodu üret */
void kod_uret_xtensa(Üretici *u, Düğüm *program, Arena *arena);

#endif /* URETICI_XTENSA_H */
