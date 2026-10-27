#ifndef GRID_PLL_H_
#define GRID_PLL_H_

#include <stdint.h>
#include <stdbool.h>
#include "SPLL_3ph_DDSRF_F.h"

extern SPLL_3ph_DDSRF_F SPLL_GRID;
extern volatile uint16_t gridPllValid;
extern volatile uint16_t gridPllLocked; // Separate from input presence; qualified for 100 ms.
extern volatile float grid_theta; // Sample-time angle; theta[0] advances one Ts.

void GridPll_Init(void);
// Inputs are physical volts. First valid PCC sample seeds PLL angle, never INV_CTRL.theta.
void GridPll_Update(float ua, float ub, float uc, bool enabled);

#endif
