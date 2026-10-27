#ifndef ADC_MEASUREMENT_H_
#define ADC_MEASUREMENT_H_

#include <stdbool.h>
#include <stdint.h>

extern volatile uint16_t adcA0Raw, adcA5Raw, adcA12Raw;
extern volatile uint16_t adcB1Raw, adcB8Raw, adcB10Raw;
extern volatile uint16_t adcC4Raw, adcC8Raw, adcC14Raw;
extern volatile uint16_t adcD11Raw, adcD12Raw; // D12 = P_filter.
extern volatile uint16_t adcE12Raw, adcE15Raw;

extern volatile float Va_cap, Vb_cap, Vc_cap;
extern volatile float PCC_ua, PCC_ub, PCC_uc;
extern volatile float Ia_inv, Ib_inv, Ic_inv;
extern volatile float Ia_filter, Ib_filter, Ic_filter;
extern volatile float P_filter_raw; // D12 power conversion before LPF, W.
extern volatile float P_filter; // 50 Hz first-order LPF output, W (not per-unit).

void AdcMeasurement_Init(void); // Reset power LPF to zero on firmware initialization.
// Read thirteen inputs, apply offsets/scales, and update the power LPF at 10 kHz.
void AdcMeasurement_Acquire(void);

#endif
