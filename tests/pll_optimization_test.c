#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "app/PLL/grid_pll.h"
#include "app/inverter.h"
#include "app/control/presync.h"
#include "app/measurement/adc_measurement.h"
#include "app/measurement/adc_calibration.h"

static float PhaseError(float a, float b)
{
    return atan2f(sinf(a - b), cosf(a - b));
}

static void PccSample(float theta, float magnitude)
{
    PCC_ua = magnitude * cosf(theta);
    PCC_ub = magnitude * cosf(theta - TWO_PI / 3.0f);
    PCC_uc = magnitude * cosf(theta + TWO_PI / 3.0f);
    GridPll_Update(PCC_ua, PCC_ub, PCC_uc, true);
}

static void Advance(float *theta, float frequency)
{
    *theta += TWO_PI * frequency * CONTROL_TS;
    if(*theta >= TWO_PI) *theta -= TWO_PI;
}

static void CheckBounded(void)
{
    assert(isfinite(SPLL_GRID.fo));
    assert(SPLL_GRID.fo >= GRID_PLL_MIN_HZ && SPLL_GRID.fo <= GRID_PLL_MAX_HZ);
    assert(SPLL_GRID.ylf[0] >= GRID_PLL_MIN_HZ - SPLL_GRID.fn && SPLL_GRID.ylf[0] <= GRID_PLL_MAX_HZ - SPLL_GRID.fn);
    assert(SPLL_GRID.ylf[1] == SPLL_GRID.ylf[0]);
}

static void CheckLocked(float frequency, float theta, float magnitude)
{
    if(!gridPllLocked || fabsf(SPLL_GRID.fo - frequency) > 0.03f)
        fprintf(stderr, "PLL unlock: f=%.5f expected=%.2f d=%.3f q=%.3f ylf=%.3f\n", SPLL_GRID.fo, frequency, SPLL_GRID.d_p_decoupl_lpf, SPLL_GRID.q_p_decoupl_lpf, SPLL_GRID.ylf[0]);
    assert(gridPllValid == 1U && gridPllLocked == 1U);
    assert(fabsf(SPLL_GRID.fo - frequency) < 0.03f);
    assert(fabsf(PhaseError(grid_theta, theta)) < 0.02f);
    assert(fabsf(SPLL_GRID.d_p_decoupl_lpf - magnitude) < 0.2f);
    assert(fabsf(SPLL_GRID.q_p_decoupl_lpf) < 0.2f);
}

static void TestStartupSweep(void)
{
    unsigned phase, f, k;
    const float frequencies[] = {49.0f, 50.0f, 51.0f};
    for(f = 0U; f < 3U; f++) for(phase = 0U; phase < 72U; phase++)
    {
        float theta = TWO_PI * (float)phase / 72.0f;
        GridPll_Init();
        for(k = 0U; k < 20000U; k++)
        {
            PccSample(theta, 155.56f);
            CheckBounded();
            if(k == 0U) assert(fabsf(PhaseError(grid_theta, theta)) < 0.00001f);
            if(k == 19999U) CheckLocked(frequencies[f], theta, 155.56f);
            Advance(&theta, frequencies[f]);
        }
    }
    puts("PASS PLL: 216 startup cases, 0..355deg at 49/50/51Hz; measured-angle seeding, bounded states, no persistent 45Hz capture");
}

static void TestDisturbanceRecovery(void)
{
    unsigned k;
    float theta = 1.0f;
    GridPll_Init();
    for(k = 0U; k < 5000U; k++) { PccSample(theta, 155.56f); Advance(&theta, 50.0f); }
    assert(gridPllLocked == 1U);
    theta += 3.14159265359f;
    if(theta >= TWO_PI) theta -= TWO_PI;
    for(k = 0U; k < 30000U; k++)
    {
        PccSample(theta, 155.56f);
        CheckBounded();
        if(k == 29999U) CheckLocked(50.0f, theta, 155.56f);
        Advance(&theta, 50.0f);
    }
    for(k = 0U; k < 20000U; k++)
    {
        PccSample(theta, 140.0f);
        if(k == 19999U) CheckLocked(51.0f, theta, 140.0f);
        Advance(&theta, 51.0f);
    }
    GridPll_Update(0.0f, 0.0f, 0.0f, true);
    assert(gridPllValid == 0U && gridPllLocked == 0U);
    theta = 3.3f;
    for(k = 0U; k < 20000U; k++)
    {
        PccSample(theta, 155.56f);
        if(k == 19999U) CheckLocked(49.0f, theta, 155.56f);
        Advance(&theta, 49.0f);
    }
    GridPll_Update(PCC_ua, PCC_ub, PCC_uc, false);
    assert(gridPllValid == 0U && gridPllLocked == 0U);
    GridPll_Update(NAN, 0.0f, 0.0f, true);
    assert(gridPllValid == 0U && gridPllLocked == 0U);
    puts("PASS PLL: 180deg phase jump, frequency/amplitude step, PCC dropout/re-entry, disable and invalid-input handling");
}

static void TestPresyncVoltageHold(void)
{
    unsigned k;
    float theta = 3.14159265359f;
    Inverter_Init();
    INV_CMD.auto_test_enable = 0U;
    INV_CMD.run_enable = INV_CMD.presync_request = 1U;
    INV_CMD.Vd_ref = 155.56f;
    Ia_inv = Ib_inv = Ic_inv = Ia_filter = Ib_filter = Ic_filter = 0.0f;
    for(k = 0U; k < GRID_PLL_LOCK_SAMPLES - 1U; k++)
    {
        PccSample(theta, 155.56f);
        Va_cap = INV_CMD.Vd_ref * cosf(INV_CTRL.theta);
        Vb_cap = INV_CMD.Vd_ref * cosf(INV_CTRL.theta - TWO_PI / 3.0f);
        Vc_cap = INV_CMD.Vd_ref * cosf(INV_CTRL.theta + TWO_PI / 3.0f);
        Inverter_ControlStep();
        assert(gridPllLocked == 0U && INV_CMD.Vd_ref == 155.56f);
        assert(PRESYNC_CTRL.pll_ready == 0U && PRESYNC_CTRL.omega_sync == 0.0f);
        Advance(&theta, 50.0f);
    }
    // Deliberately reproduce the screenshot estimate. Input presence is not lock.
    gridPllValid = 1U; gridPllLocked = 0U;
    SPLL_GRID.fo = 45.0f;
    SPLL_GRID.d_p_decoupl_lpf = -120.0f;
    SPLL_GRID.q_p_decoupl_lpf = 10.0f;
    for(k = 0U; k < 5000U; k++) Inverter_ControlStep();
    assert(INV_CMD.Vd_ref == 155.56f && PRESYNC_CTRL.sync_ready == 0U);
    assert(INV_CTRL.state == INV_PRESYNC);
    puts("PASS PLL/presync: unlocked estimates cannot lower VF voltage reference or claim synchronization");
}

static void TestMacroParity(void)
{
    SPLL_3ph_DDSRF_F a, b;
    SPLL_3ph_DDSRF_F_init(50, CONTROL_TS, 0.00933678f, -0.9813264f, &a);
    a.cos_2theta = 1.0f; a.sin_2theta = 0.0f;
    a.d_p = 155.56f; a.q_p = 100.0f;
    a.ylf[1] = 100.0f;
    b = a;
    SPLL_3ph_DDSRF_F_FUNC(&a);
    SPLL_3ph_DDSRF_F_MACRO(b);
    assert(a.fo == b.fo && a.ylf[1] == b.ylf[1] && a.theta[0] == b.theta[0]);
    assert(a.ylf[1] == GRID_PLL_MAX_HZ - a.fn);
    puts("PASS PLL: legacy macro/function share normalization and anti-windup");
}

static void TestQuantizedPcc(void)
{
    unsigned k, scenario, lockedSamples;
    uint32_t noise = 1U;
    float theta, maxDf, maxQ, lastF, minF, maxF;
    for(scenario = 0U; scenario < 2U; scenario++)
    {
    lockedSamples = 0U;
    theta = 3.3f;
    maxDf = maxQ = 0.0f; minF = 100.0f; maxF = 0.0f;
    GridPll_Init();
    for(k = 0U; k < 30000U; k++)
    {
        float v[3];
        unsigned phase;
        for(phase = 0U; phase < 3U; phase++)
        {
            float angle = theta - TWO_PI * (float)phase / 3.0f;
            float value = 155.56f * cosf(angle);
            if(scenario == 1U) value += 15.556f * cosf(-theta - TWO_PI * (float)phase / 3.0f) + 3.1112f * cosf(5.0f * angle);
            noise = noise * 1664525U + 1013904223U;
            value += ((float)(noise & 65535U) / 65535.0f - 0.5f) * 0.2f;
            v[phase] = floorf(value / VOL_SCALE + 0.5f) * VOL_SCALE;
        }
        lastF = SPLL_GRID.fo;
        GridPll_Update(v[0], v[1], v[2], true);
        CheckBounded();
        if(k >= 20000U)
        {
            if(gridPllLocked) lockedSamples++;
            if(fabsf(SPLL_GRID.fo - lastF) > maxDf) maxDf = fabsf(SPLL_GRID.fo - lastF);
            if(fabsf(SPLL_GRID.q_p_decoupl_lpf) > maxQ) maxQ = fabsf(SPLL_GRID.q_p_decoupl_lpf);
            if(SPLL_GRID.fo < minF) minF = SPLL_GRID.fo;
            if(SPLL_GRID.fo > maxF) maxF = SPLL_GRID.fo;
        }
        Advance(&theta, 50.0f);
    }
    printf("PLL quantized PCC scenario %u: lock=%u/10000 last-second samples, fo=%.5f\n", scenario, lockedSamples, SPLL_GRID.fo);
    printf("frequency range=%.3f..%.3f, maximum step=%.3f, maximum filtered q=%.3fV\n", minF, maxF, maxDf, maxQ);
    fflush(stdout);
    assert(lockedSamples > 9900U);
    }
    puts("PASS PLL: ADC-count quantization, +/-0.1V noise, 10% negative sequence and 2% fifth harmonic preserve lock");
}

void TestPllOptimization(void)
{
    TestStartupSweep();
    TestDisturbanceRecovery();
    TestPresyncVoltageHold();
    TestMacroParity();
    TestQuantizedPcc();
}
