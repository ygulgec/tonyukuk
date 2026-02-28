#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#include "agac.h"

/* AST optimizasyonları uygula:
 *  - Sabit katlama (constant folding)
 *  - Ölü kod eleme (dead code elimination)
 *  - Güç azaltma (strength reduction)
 */
void optimize_et(Düğüm *program);

#endif
