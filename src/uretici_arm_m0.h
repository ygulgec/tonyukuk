/*
 * uretici_arm_m0.h — ARM Cortex-M0+ (Raspberry Pi Pico) kod üreteci header
 *
 * Tonyukuk Türkçe programlama dili derleyicisi için ARM Cortex-M0+ backend.
 * Pico SDK ile uyumlu C kodu üretir.
 */

#ifndef URETICI_ARM_M0_H
#define URETICI_ARM_M0_H

#include "agac.h"
#include "bellek.h"
#include "uretici.h"

/* AST'den Raspberry Pi Pico uyumlu C kodu üret */
void kod_uret_arm_m0(Üretici *u, Düğüm *program, Arena *arena);

#endif /* URETICI_ARM_M0_H */
