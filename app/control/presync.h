#ifndef PRESYNC_H_
#define PRESYNC_H_
#include <stdbool.h>
#include "app/inverter_settings.h"

typedef struct
{
    uint16_t sync_ready, sync_ok_cnt, pll_wait_cnt, pll_ready;
    float theta_err, freq_err_hz, volt_err, omega_sync, omega_sync_ui;
    float v_grid_mag, v_inv_mag;
} PRESYNC_DEF;
typedef struct
{
    bool grid_valid;
    float grid_theta, grid_frequency, grid_d, grid_q, vd, vq;
} PRESYNC_INPUT;
extern volatile PRESYNC_DEF PRESYNC_CTRL;
void Presync_Init(volatile PRESYNC_DEF *p);
void Presync_Reset(volatile PRESYNC_DEF *p);
// Returns voltage reference; never reads/writes commands or operates hardware.
float Presync_Calc(volatile PRESYNC_DEF *p, const volatile PRESYNC_PARAM *cfg, const PRESYNC_INPUT *in, bool enabled, float theta, float fRef, float vdRef, float voltageMax, float Ts);
#endif
