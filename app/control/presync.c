#include "presync.h"
#include <math.h>

volatile PRESYNC_DEF PRESYNC_CTRL = {0};

static float WrapPi(float x)
{
    while(x > 3.14159265359f) x -= TWO_PI;
    while(x < -3.14159265359f) x += TWO_PI;
    return x;
}

void Presync_Reset(volatile PRESYNC_DEF *p)
{
    p->sync_ready = p->sync_ok_cnt = p->pll_wait_cnt = p->pll_ready = 0U;
    p->theta_err = p->freq_err_hz = p->volt_err = 0.0f;
    p->omega_sync = p->omega_sync_ui = 0.0f;
}

void Presync_Init(volatile PRESYNC_DEF *p)
{
    p->v_grid_mag = p->v_inv_mag = 0.0f;
    Presync_Reset(p);
}

float Presync_Calc(volatile PRESYNC_DEF *p, const volatile PRESYNC_PARAM *cfg, const PRESYNC_INPUT *in, bool enabled, float theta, float fRef, float vdRef, float voltageMax, float Ts)
{
    float output, phaseError;
    p->v_grid_mag = (in->grid_valid) ? sqrtf(in->grid_d * in->grid_d + in->grid_q * in->grid_q) : 0.0f;
    p->v_inv_mag = sqrtf(in->vd * in->vd + in->vq * in->vq);
    if(!enabled || !in->grid_valid || !(p->v_grid_mag > GRID_MIN_PEAK_V))
    {
        Presync_Reset(p);
        return vdRef;
    }
    if(p->pll_wait_cnt < cfg->wait_samples)
    {
        p->pll_wait_cnt++;
        p->pll_ready = p->sync_ready = 0U;
        return vdRef;
    }
    p->pll_ready = 1U;
    p->theta_err = WrapPi(in->grid_theta - theta);
    p->omega_sync_ui += cfg->ki * p->theta_err * Ts;
    output = cfg->kp * p->theta_err + p->omega_sync_ui;
    if(output > cfg->omega_limit) output = cfg->omega_limit;
    if(output < -cfg->omega_limit) output = -cfg->omega_limit;
    p->omega_sync = output;
    p->omega_sync_ui = output - cfg->kp * p->theta_err;
    vdRef += cfg->v_track_k * (p->v_grid_mag - vdRef);
    if(vdRef > voltageMax) vdRef = voltageMax;
    if(vdRef < 0.0f) vdRef = 0.0f;
    p->freq_err_hz = in->grid_frequency - (TWO_PI * fRef + p->omega_sync) / TWO_PI;
    p->volt_err = p->v_grid_mag - p->v_inv_mag;
    phaseError = WrapPi(p->theta_err - atan2f(in->vq, in->vd));
    if(fabsf(p->theta_err) < cfg->theta_ok && fabsf(phaseError) < cfg->theta_ok
       && fabsf(p->freq_err_hz) < cfg->freq_ok && fabsf(p->volt_err) < cfg->volt_ok
       && in->grid_d > GRID_MIN_PEAK_V
       && fabsf(in->grid_q) < GRID_PLL_Q_RATIO * p->v_grid_mag)
    {
        if(p->sync_ok_cnt < cfg->ready_samples) p->sync_ok_cnt++;
        p->sync_ready = (p->sync_ok_cnt >= cfg->ready_samples) ? 1U : 0U;
    }
    else p->sync_ok_cnt = p->sync_ready = 0U;
    return vdRef;
}
