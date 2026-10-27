#include "adc_measurement.h"
#include "adc_calibration.h"
#include "driverlib.h"
#include "board.h"

volatile uint16_t adcB1Raw = 0;
volatile uint16_t adcB8Raw = 0, adcB10Raw = 0;
volatile uint16_t adcC4Raw = 0, adcC8Raw = 0, adcC14Raw = 0;
volatile uint16_t adcD11Raw = 0, adcD12Raw = 0;
volatile uint16_t adcE12Raw = 0, adcE15Raw = 0;
volatile uint16_t adcA0Raw = 0, adcA5Raw = 0, adcA12Raw = 0;

volatile float Va_cap = 0, Vb_cap = 0, Vc_cap = 0;
volatile float PCC_ua = 0, PCC_ub = 0, PCC_uc = 0;
volatile float Ia_inv = 0, Ib_inv = 0, Ic_inv = 0;
volatile float Ia_filter = 0, Ib_filter = 0, Ic_filter = 0;
volatile float P_filter_raw = 0; // D12 power conversion before low-pass filtering, W.
volatile float P_filter = 0; // 50 Hz low-pass power, W; independent of computed Pe/Qe.

// First-order LPF at 10 kHz: alpha = 1-exp(-2*pi*50*100us).
// Recalculate this constant if the ADC acquisition period changes.
#define POWER_LPF_ALPHA 0.0309275737f

void AdcMeasurement_Init(void)
{
    P_filter_raw = P_filter = 0.0f;
}

// Read four independently converted samples and return the rounded average.
static inline uint16_t ADC_Average4(uint32_t resultBase, uint16_t firstSoc, uint16_t stride)
{
    uint16_t i;
    uint32_t sum = 0U;

    for(i = 0U; i < 4U; i++)
    {
        sum += (uint32_t)ADC_readResult(resultBase, (ADC_SOCNumber)(firstSoc + i * stride));
    }

    return (uint16_t)((sum + 2U) >> 2U);
}


// PWM1 SOCA starts conversion at zero; the period ISR reads the results.
void AdcMeasurement_Acquire(void)
{
    uint16_t i;
    const uint32_t bases[5] = { ADC_A_BASE, ADC_B_BASE, ADC_C_BASE, ADC_D_BASE, ADC_E_BASE };
    adcA0Raw  = ADC_Average4(ADCARESULT_BASE, 0U, 4U);
    // A2 is faulty: leave its SOCs configured as dummy conversions, do not read.
    adcA5Raw  = ADC_Average4(ADCARESULT_BASE, 2U, 4U);
    adcA12Raw = ADC_Average4(ADCARESULT_BASE, 3U, 4U);

    adcB1Raw  = ADC_Average4(ADCBRESULT_BASE, 1U, 4U);
    adcB8Raw  = ADC_Average4(ADCBRESULT_BASE, 2U, 4U);
    adcB10Raw = ADC_Average4(ADCBRESULT_BASE, 3U, 4U);

    adcC4Raw  = ADC_Average4(ADCCRESULT_BASE, 0U, 3U);
    adcC8Raw  = ADC_Average4(ADCCRESULT_BASE, 1U, 3U);
    adcC14Raw = ADC_Average4(ADCCRESULT_BASE, 2U, 3U);

    // SysConfig alternates D11/D12 six times; use the first four of each.
    adcD11Raw = ADC_Average4(ADCDRESULT_BASE, 0U, 2U);
    adcD12Raw = ADC_Average4(ADCDRESULT_BASE, 1U, 2U);

    adcE12Raw = ADC_Average4(ADCERESULT_BASE, 0U, 2U);
    adcE15Raw = ADC_Average4(ADCERESULT_BASE, 1U, 2U);

    // Clear housekeeping flags without rejecting samples or stopping control.
    for(i = 0U; i < 5U; i++)
    {
        ADC_clearInterruptOverflowStatus(bases[i], ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(bases[i], ADC_INT_NUMBER1);
    }

    Va_cap = ((float)adcA0Raw  - adcZeroOffset[0]) * VOL_SCALE;
    PCC_ua = ((float)adcE15Raw - adcZeroOffset[1]) * VOL_SCALE;
    Ia_inv = ((float)adcA5Raw  - adcZeroOffset[2]) * CUR_SCALE;
    
    Vb_cap = ((float)adcE12Raw - adcZeroOffset[3]) * VOL_SCALE;
    PCC_ub = ((float)adcB1Raw  - adcZeroOffset[4]) * VOL_SCALE;
    Ib_inv = ((float)adcB8Raw  - adcZeroOffset[5]) * CUR_SCALE;
    
    Vc_cap = ((float)adcC4Raw  - adcZeroOffset[6]) * VOL_SCALE;
    PCC_uc = ((float)adcC8Raw  - adcZeroOffset[7]) * VOL_SCALE;
    
    Ic_inv    = ((float)adcC14Raw - adcZeroOffset[8])  * CUR_SCALE;
    Ia_filter = ((float)adcA12Raw - adcZeroOffset[9])  * CUR_SCALE;
    Ib_filter = ((float)adcB10Raw - adcZeroOffset[10]) * CUR_SCALE;
    Ic_filter = ((float)adcD11Raw - adcZeroOffset[11]) * CUR_SCALE;
    P_filter_raw = ((float)adcD12Raw - adcZeroOffset[12]) * POWER_SCALE;
    P_filter += POWER_LPF_ALPHA * (P_filter_raw - P_filter);
}
