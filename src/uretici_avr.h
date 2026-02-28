/*
 * uretici_avr.h — AVR (Arduino) kod üreteci header
 *
 * Tonyukuk Türkçe programlama dili derleyicisi için AVR backend.
 */

#ifndef URETICI_AVR_H
#define URETICI_AVR_H

#include "agac.h"
#include "bellek.h"
#include "uretici.h"

/* AST'den AVR assembly üret */
void kod_uret_avr(Üretici *u, Düğüm *program, Arena *arena);

#endif /* URETICI_AVR_H */
