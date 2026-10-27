#include "grid_pll.h"
#include "ABC_DQ0_POS_F.h"
#include "ABC_DQ0_NEG_F.h"
#include "app/system_config.h"
#include <math.h>

SPLL_3ph_DDSRF_F SPLL_GRID;
static ABC_DQ0_POS_F DQ0_POS_REF;
static ABC_DQ0_NEG_F DQ0_NEG_REF;

volatile uint16_t gridPllValid = 0U;
volatile uint16_t gridPllLocked = 0U;
volatile float grid_theta = 0.0f;
static bool hadPcc = false;
static uint16_t lockCount;
static float frequencyAverage;

static void ResetLoop(void)
{
    ABC_DQ0_POS_F_init(&DQ0_POS_REF);
    ABC_DQ0_NEG_F_init(&DQ0_NEG_REF);

    SPLL_3ph_DDSRF_F_init(50,CONTROL_TS,0.00933678f,-0.9813264f,&SPLL_GRID);

    SPLL_GRID.fo = 50.0f;
    SPLL_GRID.sin_2theta = 0.0f;
    SPLL_GRID.cos_2theta = 1.0f;
    lockCount = gridPllLocked = 0U;
    frequencyAverage = 50.0f;
}

static void StartFromPcc(float alpha, float beta)
{
    float theta = atan2f(beta, alpha);
    float magnitude = sqrtf(alpha * alpha + beta * beta);
    ResetLoop();
    if(theta < 0.0f) theta += TWO_PI;
    SPLL_GRID.theta[0] = SPLL_GRID.theta[1] = theta;
    // At the measured initial angle, positive-sequence d is approximately |V|.
    // Seed its LPF near steady state instead of creating a false amplitude ramp.
    SPLL_GRID.d_p_decoupl_lpf = magnitude;
    SPLL_GRID.y[0] = SPLL_GRID.y[1] = 0.5f * magnitude;
}

void GridPll_Init(void)
{
    gridPllValid = 0U;
    grid_theta = 0.0f;
    hadPcc = false;
    ResetLoop();
}

void GridPll_Update(float ua, float ub, float uc, bool enabled)
{
    float theta;
    float sinTheta;
    float cosTheta;
    float previousFrequency, magnitude;
    float alpha = 0.6666666667f * (ua - 0.5f * (ub + uc));
    float beta = 0.57735026913f * (ub - uc);

    gridPllValid = 0U;
    // Missing PCC is not a fault; reset stale PLL states once when it returns.
    if(!enabled || !isfinite(alpha) || !isfinite(beta) || !(alpha * alpha + beta * beta > GRID_MIN_PEAK_V * GRID_MIN_PEAK_V))
    {
        hadPcc = false;
        lockCount = gridPllLocked = 0U;
        return;
    }
    if(!hadPcc) StartFromPcc(alpha, beta);
    hadPcc = true;
    gridPllValid = 1U;
    theta = SPLL_GRID.theta[0];
    grid_theta = theta;
    sinTheta = sinf(theta);
    cosTheta = cosf(theta);

    // 正序坐标变换
    DQ0_POS_REF.a = ua;
    DQ0_POS_REF.b = ub;
    DQ0_POS_REF.c = uc;
    DQ0_POS_REF.sin_val = sinTheta;
    DQ0_POS_REF.cos_val = cosTheta;
    ABC_DQ0_POS_F_FUNC(&DQ0_POS_REF);

    // 负序坐标变换
    DQ0_NEG_REF.a = ua;
    DQ0_NEG_REF.b = ub;
    DQ0_NEG_REF.c = uc;
    DQ0_NEG_REF.sin_val = sinTheta;
    DQ0_NEG_REF.cos_val = cosTheta;
    ABC_DQ0_NEG_F_FUNC(&DQ0_NEG_REF);

    SPLL_GRID.d_p = DQ0_POS_REF.d;
    SPLL_GRID.q_p = DQ0_POS_REF.q;
    SPLL_GRID.d_n = DQ0_NEG_REF.d;
    SPLL_GRID.q_n = DQ0_NEG_REF.q;

    SPLL_GRID.sin_2theta = 2.0f * sinTheta * cosTheta;
    SPLL_GRID.cos_2theta = cosTheta * cosTheta - sinTheta * sinTheta;

    previousFrequency = frequencyAverage;
    SPLL_3ph_DDSRF_F_FUNC(&SPLL_GRID);
    // Qualify lock using averaged frequency; raw PI output contains ADC/harmonic jitter.
    // This filter is diagnostic only and does not alter PLL angle integration.
    frequencyAverage += 0.01f * (SPLL_GRID.fo - frequencyAverage);
    magnitude = sqrtf(SPLL_GRID.d_p_decoupl_lpf * SPLL_GRID.d_p_decoupl_lpf + SPLL_GRID.q_p_decoupl_lpf * SPLL_GRID.q_p_decoupl_lpf);
    if(SPLL_GRID.d_p_decoupl_lpf > GRID_MIN_PEAK_V
       && fabsf(SPLL_GRID.q_p_decoupl_lpf) < GRID_PLL_Q_RATIO * magnitude
       && SPLL_GRID.fo > GRID_PLL_MIN_HZ + 0.1f && SPLL_GRID.fo < GRID_PLL_MAX_HZ - 0.1f
       && fabsf(frequencyAverage - previousFrequency) < GRID_PLL_LOCK_DF_HZ)
    {
        if(lockCount < GRID_PLL_LOCK_SAMPLES) lockCount++;
    }
    else lockCount = 0U;
    gridPllLocked = lockCount >= GRID_PLL_LOCK_SAMPLES ? 1U : 0U;
}
