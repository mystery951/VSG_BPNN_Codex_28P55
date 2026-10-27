#include "inverter_settings.h"

// All editable defaults live here. Runtime commands: INV_CMD; tuning: INV_PARAM.
static const INV_CMD_DEF commandDefaults = {
    .run_enable = 0U, .presync_request = 0U, .grid_connect_cmd = 0U,
    .pll_enable = 1U,
    .ann_enable = 0U, .jd_apply_cmd = 0U, .jd_adapt_enable = 0U,
    .grid_impedance_step = 0U,
    .auto_test_enable = 1U,
    .virtual_angle_mode = 0U, // Legacy-only rollback; retained field is no longer a mode switch.
    .f_ref = 50.0f, .Vd_ref = 155.56f, .Vq_ref = 0.0f,
    .p_ref = 0.0f, .q_ref = 0.0f, .q_mode = 0U
};
static const INV_PARAM_DEF parameterDefaults = {
    .Vdc = 400.0f,
    .grid_diff_max = 0.05f * SYS_V_BASE_V,
    .inner = {
        .Lf = 0.002f, .Cf = 0.000012f,
        .Kpu = 0.7f, .Kiu = 0.5f, .Kpi = 1.5f, .Kii = 0.0f,
        .U_UpLimit = 300.0f, .I_UpLimit = 100.0f
    },
    .presync = {
        .kp = 4.0f, .ki = 10.0f, .omega_limit = 12.5663706f,
        .v_track_k = 0.001f, .theta_ok = 0.0872665f,
        .freq_ok = 0.5f, .volt_ok = 10.0f,
        .wait_samples = 2000U, .ready_samples = 10U
    },
    .vsg = {
        .h = 0.3f, .dp_pu = 20.0f, .Kp_QV = 0.0f, .Ki_QV = 0.0f,
        .omega_limit = 12.5663706f, .v_min = 0.0f, .v_max = 250.0f,
        .power_loop_div = 1U
    },
    .virtual_angle = {
        .establish_s = 2.0f, .stable_s = 0.5f, .window_s = 0.1f,
        .power_change_pu = 0.01f, .angle_change_deg = 0.25f, .frequency_error_hz = 0.05f
    },
    .test = { .p_base = 1.0f, .p_step = 1.1f }
};
volatile INV_CMD_DEF INV_CMD = {0};
volatile INV_PARAM_DEF INV_PARAM = {0};

void InverterSettings_Init(void)
{
    INV_CMD = commandDefaults;
    INV_PARAM = parameterDefaults;
}
