#ifndef PWM_OUTPUT_H_
#define PWM_OUTPUT_H_
#include <stdbool.h>
#include <stdint.h>
#include "app/control/three_level_modulation.h"
extern volatile uint16_t pwmOutputEnabled;
void PwmOutput_Init(void);
void PwmOutput_Disable(void);
void PwmOutput_Update(bool enable, bool closeBreaker, const ThreeLevelDuty duty[3]);
#endif
