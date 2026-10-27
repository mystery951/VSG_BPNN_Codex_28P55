#include "pwm_output.h"
#include "app/control/three_level_modulation.h"
#include "app/system_config.h"
#include "driverlib.h"
#include "board.h"

volatile uint16_t pwmOutputEnabled = 0U;
static const uint32_t allPwm[7] = { EPWM1_BASE, EPWM2_BASE, EPWM3_BASE, EPWM4_BASE, EPWM5_BASE, EPWM6_BASE, EPWM7_BASE };
static bool prepared = false;

static void SetBufferEnabled(bool enable)
{
    GPIO_writePin(enPWM48, enable ? 0U : 1U);
    GPIO_writePin(enPWM47, enable ? 0U : 1U);
}

void PwmOutput_Disable(void)
{
    uint16_t i;
    GPIO_writePin(Breaker, 0U); // 0=open, 1=closed on the HIL platform.
    SetBufferEnabled(false);
    // OST is only a manual output lock here, not a fault state machine.
    for(i = 0U; i < 7U; i++) EPWM_forceTripZoneEvent(allPwm[i], EPWM_TZ_FORCE_EVENT_OST);
    pwmOutputEnabled = 0U;
    prepared = false;
}

static void SetDuty(uint32_t base, float duty)
{
    uint16_t compare;
    EPWM_ActionQualifierSWOutput force;
    // Exact 0/100% before deadband avoids coincident compare/zero events.
    if(duty <= 0.0f) { compare = 1U; force = EPWM_AQ_SW_OUTPUT_LOW; }
    else if(duty >= 1.0f) { compare = PWM_PERIOD - 1U; force = EPWM_AQ_SW_OUTPUT_HIGH; }
    else
    {
        compare = (uint16_t)(duty * (float)PWM_PERIOD + 0.5f);
        if(compare < 1U) compare = 1U;
        if(compare >= PWM_PERIOD) compare = PWM_PERIOD - 1U;
        force = EPWM_AQ_SW_DISABLED;
    }
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, compare);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_B, compare);
    EPWM_setActionQualifierContSWForceAction(base, EPWM_AQ_OUTPUT_A, force);
}

void PwmOutput_Init(void)
{
    uint16_t i;
    PwmOutput_Disable();
    for(i = 0U; i < 7U; i++)
    {
        EPWM_setTripZoneAction(allPwm[i], EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_LOW);
        EPWM_setTripZoneAction(allPwm[i], EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_LOW);
        EPWM_setCounterCompareShadowLoadMode(allPwm[i], EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);
        EPWM_setCounterCompareShadowLoadMode(allPwm[i], EPWM_COUNTER_COMPARE_B, EPWM_COMP_LOAD_ON_CNTR_ZERO);
        EPWM_setActionQualifierContSWForceShadowMode(allPwm[i], EPWM_AQ_SW_SH_LOAD_ON_CNTR_ZERO);
        EPWM_setRisingEdgeDeadBandDelayInput(allPwm[i], EPWM_DB_INPUT_EPWMA);
        EPWM_setFallingEdgeDeadBandDelayInput(allPwm[i], EPWM_DB_INPUT_EPWMA);
    }
}

void PwmOutput_Update(bool enable, bool closeBreaker, const ThreeLevelDuty duty[3])
{
    uint16_t i;
    GPIO_writePin(Breaker, (enable && pwmOutputEnabled == 1U && closeBreaker) ? 1U : 0U);
    if(!enable) { PwmOutput_Disable(); return; }

    SetDuty(EPWM1_BASE, duty[0].upper);
    SetDuty(EPWM2_BASE, duty[0].lower);
    SetDuty(EPWM3_BASE, duty[1].upper);
    SetDuty(EPWM4_BASE, duty[1].lower);
    SetDuty(EPWM5_BASE, duty[2].upper);
    SetDuty(EPWM6_BASE, duty[2].lower);
    // Prime shadows for one full interrupt period before releasing outputs.
    if(prepared && pwmOutputEnabled == 0U)
    {
        for(i = 0U; i < 6U; i++) EPWM_clearTripZoneFlag(allPwm[i], EPWM_TZ_FLAG_OST | EPWM_TZ_INTERRUPT);
        pwmOutputEnabled = 1U;
        SetBufferEnabled(true);
    }
    prepared = true;
}
