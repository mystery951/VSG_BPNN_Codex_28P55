#ifndef VSG_H_
#define VSG_H_
#include "app/inverter_settings.h"
#include <stdbool.h>

typedef struct
{
    uint16_t ready, have_window;
    float elapsed_s, stable_s, window_elapsed_s;
    float p_sum, q_sum, angle_sum, frequency_sum;
    uint32_t samples;
    float p_previous, q_previous, angle_previous, p_ref, q_ref;
    float angle_rad, pcc_rad; // Latched virtual-angle and PCC-increment baselines.
} VSG_ANGLE_BASE;

typedef struct
{
    float omega0, omega_m, omega_sync, omega_m_pu;
    float v_nom, v_delta_pu;
    // Identification feature only; never added to the modulation angle.
    float virtual_angle_int, virtual_angle_rad, virtual_angle_deg;
    float virtual_angle_legacy_int, virtual_angle_legacy_deg; // Original algorithm, parallel diagnostic.
    float virtual_angle_pcc_int, virtual_angle_pcc_deg;
    float virtual_angle_phase_prev, virtual_angle_offset;
    uint16_t virtual_angle_mode, virtual_angle_phase_valid, virtual_angle_valid;
    VSG_ANGLE_BASE angle_base; // Reserved compatibility diagnostics, stays zero after rollback.
    uint16_t power_loop_cnt;
    float q_bias_pu; // Fixed entry anchor for Ki=0 droop; not an integrating PI state.
} VSG_DEF;
extern volatile VSG_DEF VSG_CTRL;
void VSG_Init(volatile VSG_DEF *v);
void VSG_Reset(volatile VSG_DEF *v);
void VSG_EnterGrid(volatile VSG_DEF *v, const volatile VSG_PARAM *cfg, float omega0, float omegaSync, float voltage, float qError);
// pu P/Q in; physical phase-peak voltage reference out.
float VSG_PowerLoop(volatile VSG_DEF *v, const volatile VSG_PARAM *cfg, float pRef, float qRef, float pe, float qe, float voltageMax, float Ts);
// Legacy API retained unchanged: integral of omega_m alone, deadband +/-0.015 rad/s.
void VSG_VirtualAngleStep(volatile VSG_DEF *v, float omegaP, float Ts, bool enabled);
// Compatibility wrapper: always calls original Step; ignores PCC and mode arguments.
void VSG_VirtualAngleUpdate(volatile VSG_DEF *v, float thetaInv, float thetaPcc, float Ts, bool enabled, bool pccValid, uint16_t mode);
// Compatibility no-op after rollback: returns false, does not latch a baseline.
bool VSG_VirtualAngleEstablish(volatile VSG_DEF *v, const volatile VIRTUAL_ANGLE_PARAM *cfg, float Ts, bool pllLocked, float frequency, float p, float q, float pRef, float qRef);
#endif
