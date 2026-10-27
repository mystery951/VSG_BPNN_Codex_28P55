#ifndef CONTROL_ISR_H_
#define CONTROL_ISR_H_
#include "driverlib.h"
extern volatile uint32_t timer1Count, pwm1IntCount;
void ControlIsr_Init(void);
__interrupt void cpu_timer1_isr(void);
__interrupt void epwm1_isr(void);
#endif
