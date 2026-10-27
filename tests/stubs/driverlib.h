#ifndef HOST_DRIVERLIB_H_
#define HOST_DRIVERLIB_H_
#include <stdbool.h>
#include <stdint.h>
typedef unsigned ADC_SOCNumber;
bool Interrupt_disableGlobal(void);
bool Interrupt_enableGlobal(void);
#define EPWM1_BASE 1U
#define EPWM2_BASE 2U
#define EPWM3_BASE 3U
#define EPWM4_BASE 4U
#define EPWM5_BASE 5U
#define EPWM6_BASE 6U
#define EPWM7_BASE 7U
#define EPWM_TZ_ACTION_EVENT_TZA 0U
#define EPWM_TZ_ACTION_EVENT_TZB 1U
#define EPWM_TZ_ACTION_LOW 2U
#define EPWM_TZ_FORCE_EVENT_OST 4U
#define EPWM_TZ_FLAG_OST 4U
#define EPWM_TZ_INTERRUPT 1U
#define EPWM_COUNTER_COMPARE_A 0U
#define EPWM_COUNTER_COMPARE_B 1U
#define EPWM_COMP_LOAD_ON_CNTR_ZERO 0U
#define EPWM_AQ_SW_SH_LOAD_ON_CNTR_ZERO 0U
#define EPWM_DB_INPUT_EPWMA 0U
#define EPWM_TIME_BASE_STATUS_COUNT_DOWN 0U
#define EPWM_AQ_OUTPUT_A 0U
typedef enum { EPWM_AQ_SW_DISABLED = 0, EPWM_AQ_SW_OUTPUT_LOW = 1, EPWM_AQ_SW_OUTPUT_HIGH = 2 } EPWM_ActionQualifierSWOutput;
void EPWM_setTripZoneAction(uint32_t b, unsigned e, unsigned a);
void EPWM_forceTripZoneEvent(uint32_t b, uint16_t e);
uint16_t EPWM_getTimeBasePeriod(uint32_t b);
void EPWM_setCounterCompareShadowLoadMode(uint32_t b, unsigned c, unsigned m);
void EPWM_setActionQualifierContSWForceShadowMode(uint32_t b, unsigned m);
void EPWM_setRisingEdgeDeadBandDelayInput(uint32_t b, uint16_t i);
void EPWM_setFallingEdgeDeadBandDelayInput(uint32_t b, uint16_t i);
void EPWM_setCounterCompareValue(uint32_t b, unsigned c, uint16_t v);
void EPWM_setActionQualifierContSWForceAction(uint32_t b, unsigned o, EPWM_ActionQualifierSWOutput f);
uint16_t EPWM_getTripZoneFlagStatus(uint32_t b);
uint16_t EPWM_getTimeBaseCounterValue(uint32_t b);
uint16_t EPWM_getTimeBaseCounterDirection(uint32_t b);
void EPWM_clearTripZoneFlag(uint32_t b, uint16_t f);
void GPIO_writePin(uint32_t pin, uint32_t value);
uint32_t GPIO_readPin(uint32_t pin);
#define ADCARESULT_BASE 20U
#define ADCBRESULT_BASE 21U
#define ADCCRESULT_BASE 22U
#define ADCDRESULT_BASE 23U
#define ADCERESULT_BASE 24U
#define ADC_INT_NUMBER1 0U
#define ADC_SOC_NUMBER0 0U
#define ADC_SOC_NUMBER1 1U
#define ADC_SOC_NUMBER2 2U
#define ADC_SOC_NUMBER3 3U
bool ADC_getInterruptStatus(uint32_t base, unsigned interrupt);
bool ADC_getInterruptOverflowStatus(uint32_t base, unsigned interrupt);
void ADC_clearInterruptStatus(uint32_t base, unsigned interrupt);
void ADC_clearInterruptOverflowStatus(uint32_t base, unsigned interrupt);
uint16_t ADC_readResult(uint32_t base, unsigned soc);
#endif
