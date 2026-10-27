#ifndef INVERTER_SETTINGS_H_
#define INVERTER_SETTINGS_H_
#include <stdint.h>
#include "system_config.h"

// The ONLY operator commands / setpoints. Voltage is phase peak V; P/Q are pu.
typedef struct
{
    uint16_t run_enable, presync_request, grid_connect_cmd, pll_enable;
    uint16_t ann_enable, jd_apply_cmd, jd_adapt_enable; // Predict / apply once / auto apply; all default 0.
    uint16_t grid_impedance_step; // HIL GPIO12: 0=original condition, 1=changed grid impedance.
    uint16_t auto_test_enable; // 1: allow button-triggered 40s A/B experiment; never auto-start on connection.
    uint16_t virtual_angle_mode; // Reserved compatibility field, default 0. Always uses legacy omega_m integration.
    float f_ref, Vd_ref, Vq_ref, p_ref, q_ref;
    uint16_t q_mode; // 0 ConstV, 1 ConstQ(Ki=40/Vbase), 2 Droop(Kp=10/Vbase); pu voltage output.
} INV_CMD_DEF;

typedef struct
{
    float Lf, Cf, Kpu, Kiu, Kpi, Kii, U_UpLimit, I_UpLimit;
} INNER_PARAM;

typedef struct
{
    float kp, ki, omega_limit, v_track_k, theta_ok, freq_ok, volt_ok;
    uint16_t wait_samples, ready_samples;
} PRESYNC_PARAM;

typedef struct
{
    float h, dp_pu, Kp_QV, Ki_QV, omega_limit, v_min, v_max;
    uint16_t power_loop_div;
} VSG_PARAM;

typedef struct
{
    float establish_s, stable_s, window_s;
    float power_change_pu, angle_change_deg, frequency_error_hz;
} VIRTUAL_ANGLE_PARAM;

typedef struct
{
    float Vdc, grid_diff_max;
    INNER_PARAM inner;
    PRESYNC_PARAM presync;
    VSG_PARAM vsg;
    VIRTUAL_ANGLE_PARAM virtual_angle; // Reserved layout only; baseline compensation removed.
    struct { float p_base, p_step; } test; // pu; automatic experiment setpoints.
} INV_PARAM_DEF;

extern volatile INV_CMD_DEF INV_CMD;
extern volatile INV_PARAM_DEF INV_PARAM;
void InverterSettings_Init(void);
#endif
