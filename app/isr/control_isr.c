#include "control_isr.h"
#include "board.h"
#include "app/inverter.h"
#include "app/communication/can_control.h"

volatile uint32_t timer1Count = 0U, pwm1IntCount = 0U;

void ControlIsr_Init(void)
{
    Interrupt_register(INT_TIMER1, &cpu_timer1_isr);
    Interrupt_enable(INT_TIMER1);
    Interrupt_register(INT_EPWM1, &epwm1_isr);
    Interrupt_enable(INT_EPWM1);
}

__interrupt void cpu_timer1_isr(void)
{
    timer1Count++;
}

__interrupt void epwm1_isr(void)
{
    Inverter_Step();
    pwm1IntCount++;
    CanControl_CapturePower(pwm1IntCount);
    EPWM_clearEventTriggerInterruptFlag(PWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}
