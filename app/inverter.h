#ifndef INVERTER_H_
#define INVERTER_H_
#include "inverter_settings.h"
#include "control/inverter_state.h"

// Board_init() must run first. Init leaves PWM disabled and breaker open.
void Inverter_Init(void);
// Call once per 100 us: acquisition -> PLL -> control -> PWM/breaker.
void Inverter_Step(void);
// Pre-acquired measurement/PLL path, for simulation and numerical tests only.
void Inverter_ControlStep(void);
// Main loop, interrupts enabled: execute the pending ANN window, never from ISR.
void Inverter_Background(void);
#endif
