/*
 * Copyright (c) 2020 Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef BOARD_H
#define BOARD_H

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

//
// Included Files
//

#include "driverlib.h"
#include "device.h"

//*****************************************************************************
//
// PinMux Configurations
//
//*****************************************************************************

//
// ANALOG -> myANALOGPinMux0 Pinmux
//

//
// EPWM1 -> PWM1 Pinmux
//
//
// EPWM1_A - GPIO Settings
//
#define GPIO_PIN_EPWM1_A 0
#define PWM1_EPWMA_GPIO 0
#define PWM1_EPWMA_PIN_CONFIG GPIO_0_EPWM1_A
//
// EPWM1_B - GPIO Settings
//
#define GPIO_PIN_EPWM1_B 1
#define PWM1_EPWMB_GPIO 1
#define PWM1_EPWMB_PIN_CONFIG GPIO_1_EPWM1_B

//
// EPWM2 -> PWM2 Pinmux
//
//
// EPWM2_A - GPIO Settings
//
#define GPIO_PIN_EPWM2_A 2
#define PWM2_EPWMA_GPIO 2
#define PWM2_EPWMA_PIN_CONFIG GPIO_2_EPWM2_A
//
// EPWM2_B - GPIO Settings
//
#define GPIO_PIN_EPWM2_B 3
#define PWM2_EPWMB_GPIO 3
#define PWM2_EPWMB_PIN_CONFIG GPIO_3_EPWM2_B

//
// EPWM3 -> PWM3 Pinmux
//
//
// EPWM3_A - GPIO Settings
//
#define GPIO_PIN_EPWM3_A 4
#define PWM3_EPWMA_GPIO 4
#define PWM3_EPWMA_PIN_CONFIG GPIO_4_EPWM3_A
//
// EPWM3_B - GPIO Settings
//
#define GPIO_PIN_EPWM3_B 5
#define PWM3_EPWMB_GPIO 5
#define PWM3_EPWMB_PIN_CONFIG GPIO_5_EPWM3_B

//
// EPWM4 -> PWM4 Pinmux
//
//
// EPWM4_A - GPIO Settings
//
#define GPIO_PIN_EPWM4_A 6
#define PWM4_EPWMA_GPIO 6
#define PWM4_EPWMA_PIN_CONFIG GPIO_6_EPWM4_A
//
// EPWM4_B - GPIO Settings
//
#define GPIO_PIN_EPWM4_B 7
#define PWM4_EPWMB_GPIO 7
#define PWM4_EPWMB_PIN_CONFIG GPIO_7_EPWM4_B

//
// EPWM5 -> PWM5 Pinmux
//
//
// EPWM5_A - GPIO Settings
//
#define GPIO_PIN_EPWM5_A 8
#define PWM5_EPWMA_GPIO 8
#define PWM5_EPWMA_PIN_CONFIG GPIO_8_EPWM5_A
//
// EPWM5_B - GPIO Settings
//
#define GPIO_PIN_EPWM5_B 9
#define PWM5_EPWMB_GPIO 9
#define PWM5_EPWMB_PIN_CONFIG GPIO_9_EPWM5_B

//
// EPWM6 -> PWM6 Pinmux
//
//
// EPWM6_A - GPIO Settings
//
#define GPIO_PIN_EPWM6_A 10
#define PWM6_EPWMA_GPIO 10
#define PWM6_EPWMA_PIN_CONFIG GPIO_10_EPWM6_A
//
// EPWM6_B - GPIO Settings
//
#define GPIO_PIN_EPWM6_B 11
#define PWM6_EPWMB_GPIO 11
#define PWM6_EPWMB_PIN_CONFIG GPIO_11_EPWM6_B

//
// EPWM7 -> PMW7 Pinmux
//
//
// EPWM7_A - GPIO Settings
//
#define GPIO_PIN_EPWM7_A 12
#define PMW7_EPWMA_GPIO 12
#define PMW7_EPWMA_PIN_CONFIG GPIO_12_EPWM7_A
//
// EPWM7_B - GPIO Settings
//
#define GPIO_PIN_EPWM7_B 13
#define PMW7_EPWMB_GPIO 13
#define PMW7_EPWMB_PIN_CONFIG GPIO_13_EPWM7_B
//
// GPIO20 - GPIO Settings
//
#define LEDRED_GPIO_PIN_CONFIG GPIO_20_GPIO20

//*****************************************************************************
//
// ADC Configurations
//
//*****************************************************************************
#define ADC_A_BASE ADCA_BASE
#define ADC_A_RESULT_BASE ADCARESULT_BASE
#define ADC_A_SOC0 ADC_SOC_NUMBER0
#define ADC_A_FORCE_SOC0 ADC_FORCE_SOC0
#define ADC_A_SAMPLE_WINDOW_SOC0 200
#define ADC_A_TRIGGER_SOURCE_SOC0 ADC_TRIGGER_EPWM1_SOCA
#define ADC_A_CHANNEL_SOC0 ADC_CH_ADCIN0
#define ADC_A_SOC1 ADC_SOC_NUMBER1
#define ADC_A_FORCE_SOC1 ADC_FORCE_SOC1
#define ADC_A_SAMPLE_WINDOW_SOC1 200
#define ADC_A_TRIGGER_SOURCE_SOC1 ADC_TRIGGER_EPWM1_SOCA
#define ADC_A_CHANNEL_SOC1 ADC_CH_ADCIN5
#define ADC_A_SOC2 ADC_SOC_NUMBER2
#define ADC_A_FORCE_SOC2 ADC_FORCE_SOC2
#define ADC_A_SAMPLE_WINDOW_SOC2 200
#define ADC_A_TRIGGER_SOURCE_SOC2 ADC_TRIGGER_EPWM1_SOCA
#define ADC_A_CHANNEL_SOC2 ADC_CH_ADCIN9
#define ADC_A_SOC3 ADC_SOC_NUMBER3
#define ADC_A_FORCE_SOC3 ADC_FORCE_SOC3
#define ADC_A_SAMPLE_WINDOW_SOC3 200
#define ADC_A_TRIGGER_SOURCE_SOC3 ADC_TRIGGER_EPWM1_SOCA
#define ADC_A_CHANNEL_SOC3 ADC_CH_ADCIN12
void ADC_A_init();

#define ADC_B_BASE ADCB_BASE
#define ADC_B_RESULT_BASE ADCBRESULT_BASE
#define ADC_B_SOC0 ADC_SOC_NUMBER0
#define ADC_B_FORCE_SOC0 ADC_FORCE_SOC0
#define ADC_B_SAMPLE_WINDOW_SOC0 200
#define ADC_B_TRIGGER_SOURCE_SOC0 ADC_TRIGGER_EPWM1_SOCA
#define ADC_B_CHANNEL_SOC0 ADC_CH_ADCIN0
#define ADC_B_SOC1 ADC_SOC_NUMBER1
#define ADC_B_FORCE_SOC1 ADC_FORCE_SOC1
#define ADC_B_SAMPLE_WINDOW_SOC1 200
#define ADC_B_TRIGGER_SOURCE_SOC1 ADC_TRIGGER_EPWM1_SOCA
#define ADC_B_CHANNEL_SOC1 ADC_CH_ADCIN1
#define ADC_B_SOC2 ADC_SOC_NUMBER2
#define ADC_B_FORCE_SOC2 ADC_FORCE_SOC2
#define ADC_B_SAMPLE_WINDOW_SOC2 200
#define ADC_B_TRIGGER_SOURCE_SOC2 ADC_TRIGGER_EPWM1_SOCA
#define ADC_B_CHANNEL_SOC2 ADC_CH_ADCIN8
#define ADC_B_SOC3 ADC_SOC_NUMBER3
#define ADC_B_FORCE_SOC3 ADC_FORCE_SOC3
#define ADC_B_SAMPLE_WINDOW_SOC3 200
#define ADC_B_TRIGGER_SOURCE_SOC3 ADC_TRIGGER_EPWM1_SOCA
#define ADC_B_CHANNEL_SOC3 ADC_CH_ADCIN10
void ADC_B_init();

#define ADC_C_BASE ADCC_BASE
#define ADC_C_RESULT_BASE ADCCRESULT_BASE
#define ADC_C_SOC0 ADC_SOC_NUMBER0
#define ADC_C_FORCE_SOC0 ADC_FORCE_SOC0
#define ADC_C_SAMPLE_WINDOW_SOC0 200
#define ADC_C_TRIGGER_SOURCE_SOC0 ADC_TRIGGER_EPWM1_SOCA
#define ADC_C_CHANNEL_SOC0 ADC_CH_ADCIN4
#define ADC_C_SOC1 ADC_SOC_NUMBER1
#define ADC_C_FORCE_SOC1 ADC_FORCE_SOC1
#define ADC_C_SAMPLE_WINDOW_SOC1 200
#define ADC_C_TRIGGER_SOURCE_SOC1 ADC_TRIGGER_EPWM1_SOCA
#define ADC_C_CHANNEL_SOC1 ADC_CH_ADCIN8
#define ADC_C_SOC2 ADC_SOC_NUMBER2
#define ADC_C_FORCE_SOC2 ADC_FORCE_SOC2
#define ADC_C_SAMPLE_WINDOW_SOC2 200
#define ADC_C_TRIGGER_SOURCE_SOC2 ADC_TRIGGER_EPWM1_SOCA
#define ADC_C_CHANNEL_SOC2 ADC_CH_ADCIN14
void ADC_C_init();

#define ADC_D_BASE ADCD_BASE
#define ADC_D_RESULT_BASE ADCDRESULT_BASE
#define ADC_D_SOC0 ADC_SOC_NUMBER0
#define ADC_D_FORCE_SOC0 ADC_FORCE_SOC0
#define ADC_D_SAMPLE_WINDOW_SOC0 200
#define ADC_D_TRIGGER_SOURCE_SOC0 ADC_TRIGGER_EPWM1_SOCA
#define ADC_D_CHANNEL_SOC0 ADC_CH_ADCIN11
#define ADC_D_SOC1 ADC_SOC_NUMBER1
#define ADC_D_FORCE_SOC1 ADC_FORCE_SOC1
#define ADC_D_SAMPLE_WINDOW_SOC1 200
#define ADC_D_TRIGGER_SOURCE_SOC1 ADC_TRIGGER_EPWM1_SOCA
#define ADC_D_CHANNEL_SOC1 ADC_CH_ADCIN12
#define ADC_D_SOC2 ADC_SOC_NUMBER2
#define ADC_D_FORCE_SOC2 ADC_FORCE_SOC2
#define ADC_D_SAMPLE_WINDOW_SOC2 200
#define ADC_D_TRIGGER_SOURCE_SOC2 ADC_TRIGGER_EPWM1_SOCA
#define ADC_D_CHANNEL_SOC2 ADC_CH_ADCIN14
void ADC_D_init();

#define ADC_E_BASE ADCE_BASE
#define ADC_E_RESULT_BASE ADCERESULT_BASE
#define ADC_E_SOC0 ADC_SOC_NUMBER0
#define ADC_E_FORCE_SOC0 ADC_FORCE_SOC0
#define ADC_E_SAMPLE_WINDOW_SOC0 200
#define ADC_E_TRIGGER_SOURCE_SOC0 ADC_TRIGGER_EPWM1_SOCA
#define ADC_E_CHANNEL_SOC0 ADC_CH_ADCIN12
#define ADC_E_SOC1 ADC_SOC_NUMBER1
#define ADC_E_FORCE_SOC1 ADC_FORCE_SOC1
#define ADC_E_SAMPLE_WINDOW_SOC1 200
#define ADC_E_TRIGGER_SOURCE_SOC1 ADC_TRIGGER_EPWM1_SOCA
#define ADC_E_CHANNEL_SOC1 ADC_CH_ADCIN15
void ADC_E_init();


//*****************************************************************************
//
// ASYSCTL Configurations
//
//*****************************************************************************

//*****************************************************************************
//
// CPUTIMER Configurations
//
//*****************************************************************************
#define TIMER1_BASE CPUTIMER1_BASE
void TIMER1_init();

//*****************************************************************************
//
// EPWM Configurations
//
//*****************************************************************************
#define PWM1_BASE EPWM1_BASE
#define PWM1_TBPRD 7500
#define PWM1_COUNTER_MODE EPWM_COUNTER_MODE_UP_DOWN
#define PWM1_TBPHS 0
#define PWM1_CMPA 3750
#define PWM1_CMPB 3750
#define PWM1_CMPC 0
#define PWM1_CMPD 0
#define PWM1_DBRED 60
#define PWM1_DBFED 60
#define PWM1_TZA_ACTION EPWM_TZ_ACTION_LOW
#define PWM1_TZB_ACTION EPWM_TZ_ACTION_LOW
#define PWM1_INTERRUPT_SOURCE EPWM_INT_TBCTR_PERIOD
#define PWM2_BASE EPWM2_BASE
#define PWM2_TBPRD 7500
#define PWM2_COUNTER_MODE EPWM_COUNTER_MODE_UP_DOWN
#define PWM2_TBPHS 0
#define PWM2_CMPA 3750
#define PWM2_CMPB 3750
#define PWM2_CMPC 0
#define PWM2_CMPD 0
#define PWM2_DBRED 60
#define PWM2_DBFED 60
#define PWM2_TZA_ACTION EPWM_TZ_ACTION_LOW
#define PWM2_TZB_ACTION EPWM_TZ_ACTION_LOW
#define PWM2_INTERRUPT_SOURCE EPWM_INT_TBCTR_DISABLED
#define PWM3_BASE EPWM3_BASE
#define PWM3_TBPRD 7500
#define PWM3_COUNTER_MODE EPWM_COUNTER_MODE_UP_DOWN
#define PWM3_TBPHS 0
#define PWM3_CMPA 3750
#define PWM3_CMPB 3750
#define PWM3_CMPC 0
#define PWM3_CMPD 0
#define PWM3_DBRED 60
#define PWM3_DBFED 60
#define PWM3_TZA_ACTION EPWM_TZ_ACTION_LOW
#define PWM3_TZB_ACTION EPWM_TZ_ACTION_LOW
#define PWM3_INTERRUPT_SOURCE EPWM_INT_TBCTR_DISABLED
#define PWM4_BASE EPWM4_BASE
#define PWM4_TBPRD 7500
#define PWM4_COUNTER_MODE EPWM_COUNTER_MODE_UP_DOWN
#define PWM4_TBPHS 0
#define PWM4_CMPA 3750
#define PWM4_CMPB 3750
#define PWM4_CMPC 0
#define PWM4_CMPD 0
#define PWM4_DBRED 60
#define PWM4_DBFED 60
#define PWM4_TZA_ACTION EPWM_TZ_ACTION_LOW
#define PWM4_TZB_ACTION EPWM_TZ_ACTION_LOW
#define PWM4_INTERRUPT_SOURCE EPWM_INT_TBCTR_DISABLED
#define PWM5_BASE EPWM5_BASE
#define PWM5_TBPRD 7500
#define PWM5_COUNTER_MODE EPWM_COUNTER_MODE_UP_DOWN
#define PWM5_TBPHS 0
#define PWM5_CMPA 3750
#define PWM5_CMPB 3750
#define PWM5_CMPC 0
#define PWM5_CMPD 0
#define PWM5_DBRED 60
#define PWM5_DBFED 60
#define PWM5_TZA_ACTION EPWM_TZ_ACTION_LOW
#define PWM5_TZB_ACTION EPWM_TZ_ACTION_LOW
#define PWM5_INTERRUPT_SOURCE EPWM_INT_TBCTR_DISABLED
#define PWM6_BASE EPWM6_BASE
#define PWM6_TBPRD 7500
#define PWM6_COUNTER_MODE EPWM_COUNTER_MODE_UP_DOWN
#define PWM6_TBPHS 0
#define PWM6_CMPA 3750
#define PWM6_CMPB 3750
#define PWM6_CMPC 0
#define PWM6_CMPD 0
#define PWM6_DBRED 60
#define PWM6_DBFED 60
#define PWM6_TZA_ACTION EPWM_TZ_ACTION_LOW
#define PWM6_TZB_ACTION EPWM_TZ_ACTION_LOW
#define PWM6_INTERRUPT_SOURCE EPWM_INT_TBCTR_DISABLED
#define PMW7_BASE EPWM7_BASE
#define PMW7_TBPRD 7500
#define PMW7_COUNTER_MODE EPWM_COUNTER_MODE_UP_DOWN
#define PMW7_TBPHS 0
#define PMW7_CMPA 3750
#define PMW7_CMPB 3750
#define PMW7_CMPC 0
#define PMW7_CMPD 0
#define PMW7_DBRED 60
#define PMW7_DBFED 60
#define PMW7_TZA_ACTION EPWM_TZ_ACTION_LOW
#define PMW7_TZB_ACTION EPWM_TZ_ACTION_LOW
#define PMW7_INTERRUPT_SOURCE EPWM_INT_TBCTR_DISABLED

//*****************************************************************************
//
// GPIO Configurations
//
//*****************************************************************************
#define LEDRED 20
void LEDRED_init();

//*****************************************************************************
//
// SYNC Scheme Configurations
//
//*****************************************************************************

//*****************************************************************************
//
// Board Configurations
//
//*****************************************************************************
void	Board_init();
void	ADC_init();
void	ASYSCTL_init();
void	CPUTIMER_init();
void	EPWM_init();
void	GPIO_init();
void	SYNC_init();
void	PinMux_init();

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif  // end of BOARD_H definition
