#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stubs/driverlib.h"
#include "app/control/inner_loop.h"
#include "app/inverter.h"
#include "app/control/presync.h"
#include "app/PLL/grid_pll.h"
#include "app/control/vsg.h"
#include "app/control/inverter_state.h"

#include "app/measurement/feedback_dq.h"
#include "app/control/three_level_modulation.h"
#include "app/hardware/pwm_output.h"
#include "app/measurement/adc_measurement.h"
#include "app/communication/can_control.h"

// Real measurement/controller code, with ADC and PWM registers simulated below.
static bool adcFlags[5], adcOverflows[5];
static uint16_t adcResults[5][16];
static unsigned adcResultReads, adcFlagClears;
bool ADC_getInterruptStatus(uint32_t base, unsigned interrupt) { (void)interrupt; return adcFlags[base - 10U]; }
bool ADC_getInterruptOverflowStatus(uint32_t base, unsigned interrupt) { (void)interrupt; return adcOverflows[base - 10U]; }
void ADC_clearInterruptStatus(uint32_t base, unsigned interrupt)
{
    (void)interrupt;
    assert(adcResultReads == 0U || adcResultReads == 52U);
    adcFlags[base - 10U] = false;
    adcFlagClears++;
}
void ADC_clearInterruptOverflowStatus(uint32_t base, unsigned interrupt) { (void)interrupt; adcOverflows[base - 10U] = false; }
uint16_t ADC_readResult(uint32_t base, unsigned soc)
{
    assert(adcFlagClears == 0U); // Do not clear flags before the full result copy.
    assert(base != ADCARESULT_BASE || soc % 4U != 1U); // Faulty A2 remains dummy-only.
    assert(base != ADCBRESULT_BASE || soc % 4U != 0U); // Faulty B0 remains dummy-only.
    adcResultReads++;
    return adcResults[base - 20U][soc];
}

typedef struct { uint16_t period, compare[2], force, flags, counter, direction; } FakePwm;
static FakePwm hw[8];
static uint32_t gpioValue[64];
uint32_t GPIO_readPin(uint32_t pin) { return gpioValue[pin]; }
void GPIO_writePin(uint32_t pin, uint32_t value)
{
    unsigned i;
    assert(pin < 64U);
    if(pin == 21U && value == 1U)
    {
        assert(INV_CTRL.state == INV_GRID_CONNECTED && INV_CMD.grid_connect_cmd == 1U);
        assert(INV_CMD.run_enable == 1U && pwmOutputEnabled == 1U && pwmOutputEnabled == 1U);
    }
    if((pin == 47U || pin == 48U) && value == 0U)
    {
        // Both buffers may only be enabled after every used bridge is released.
        for(i = 1U; i <= 6U; i++) assert(hw[i].flags == 0U);
        assert(hw[7].flags & EPWM_TZ_FLAG_OST);
    }
    gpioValue[pin] = value;
}
void EPWM_setTripZoneAction(uint32_t b, unsigned e, unsigned a) { (void)b; (void)e; assert(a == EPWM_TZ_ACTION_LOW); }
void EPWM_forceTripZoneEvent(uint32_t b, uint16_t e) { assert(gpioValue[47] == 1U && gpioValue[48] == 1U); hw[b].flags |= e; }
uint16_t EPWM_getTimeBasePeriod(uint32_t b) { return hw[b].period; }
void EPWM_setCounterCompareShadowLoadMode(uint32_t b, unsigned c, unsigned m) { (void)b; (void)c; assert(m == 0U); }
void EPWM_setActionQualifierContSWForceShadowMode(uint32_t b, unsigned m) { (void)b; assert(m == 0U); }
void EPWM_setRisingEdgeDeadBandDelayInput(uint32_t b, uint16_t i) { (void)b; assert(i == 0U); }
void EPWM_setFallingEdgeDeadBandDelayInput(uint32_t b, uint16_t i) { (void)b; assert(i == 0U); }
void EPWM_setCounterCompareValue(uint32_t b, unsigned c, uint16_t v) { hw[b].compare[c] = v; }
void EPWM_setActionQualifierContSWForceAction(uint32_t b, unsigned o, EPWM_ActionQualifierSWOutput f) { assert(o == 0U); hw[b].force = (uint16_t)f; }
uint16_t EPWM_getTripZoneFlagStatus(uint32_t b) { return hw[b].flags; }
uint16_t EPWM_getTimeBaseCounterValue(uint32_t b) { return hw[b].counter; }
uint16_t EPWM_getTimeBaseCounterDirection(uint32_t b) { return hw[b].direction; }
void EPWM_clearTripZoneFlag(uint32_t b, uint16_t f) { assert(gpioValue[47] == 1U && gpioValue[48] == 1U); hw[b].flags &= (uint16_t)~f; }

static void Near(float actual, float expected, float tolerance)
{
    if(!(fabsf(actual - expected) <= tolerance))
    {
        fprintf(stderr, "Expected %.8g, got %.8g (tolerance %.8g)\n", expected, actual, tolerance);
        exit(1);
    }
}

static void SetAbc(float alpha, float beta, volatile float *a, volatile float *b, volatile float *c)
{
    *a = alpha;
    *b = -0.5f * alpha + 0.86602540378f * beta;
    *c = -0.5f * alpha - 0.86602540378f * beta;
}

#include "app/measurement/adc_calibration.h"


static void TestInnerInit(void)
{
    InnerLoop_Init(&INV_PARAM.inner);
    INV_CMD.run_enable = 0U;
}
static void TestPllUpdate(float a, float b, float c) { GridPll_Update(a, b, c, INV_CMD.pll_enable == 1U); }
static void TestPwmUpdate(bool enable, float a, float b, float c)
{
    ThreeLevelDuty duty[3];
    ThreeLevelModulation_Map(a, &duty[0]);
    ThreeLevelModulation_Map(b, &duty[1]);
    ThreeLevelModulation_Map(c, &duty[2]);
    PwmOutput_Update(enable, INV_CTRL.state == INV_GRID_CONNECTED, duty);
}
static void TestPresyncCalc(volatile PRESYNC_DEF *p, float Ts)
{
    PRESYNC_INPUT sync = { gridPllValid == 1U, grid_theta, SPLL_GRID.fo, SPLL_GRID.d_p_decoupl_lpf, SPLL_GRID.q_p_decoupl_lpf, INV_FEEDBACK.Vd_cap, INV_FEEDBACK.Vq_cap };
    INV_CMD.Vd_ref = Presync_Calc(p, &INV_PARAM.presync, &sync, INV_CMD.presync_request == 1U && INV_CMD.run_enable == 1U, INV_CTRL.theta, INV_CMD.f_ref, INV_CMD.Vd_ref, 0.475f * INV_PARAM.Vdc, Ts);
}
static void TestPowerLoop(volatile VSG_DEF *v, float pe, float qe, float Ts)
{
    INV_CMD.Vd_ref = VSG_PowerLoop(v, &INV_PARAM.vsg, INV_CMD.p_ref, INV_CMD.q_ref, pe, qe, 0.475f * INV_PARAM.Vdc, Ts);
}
static void TestEnterGrid(volatile VSG_DEF *v, float theta, float correction, float voltage)
{
    INV_CTRL.theta = theta;
    VSG_EnterGrid(v, &INV_PARAM.vsg, TWO_PI * INV_CMD.f_ref, correction, voltage, INV_CMD.q_ref - INV_FEEDBACK.Qe_pu);
}
static void TestStateStep(volatile INV_STATE_CTRL *s)
{
    float diff[3] = { PCC_ua - Va_cap, PCC_ub - Vb_cap, PCC_uc - Vc_cap };
    InvState_Step(s, &INV_CMD, PRESYNC_CTRL.sync_ready == 1U, gridPllValid == 1U, diff, INV_PARAM.grid_diff_max);
}

static void Reset(void)
{
    CanControl_Init();
    InverterSettings_Init();
    // Frozen controller fixture for the original numerical regression scenarios.
    INV_CMD.auto_test_enable = 0U;
    // Keep runtime user defaults (currently D=200 / Kp_QV=0) unchanged in production.
    INV_PARAM.vsg.dp_pu = 50.0f;
    INV_PARAM.vsg.Kp_QV = 0.05f;
    memset(hw, 0, sizeof(hw));
    memset(gpioValue, 0, sizeof(gpioValue));
    Va_cap = Vb_cap = Vc_cap = 0.0f;
    Ia_inv = Ib_inv = Ic_inv = 0.0f;
    Ia_filter = Ib_filter = Ic_filter = 0.0f;
    INV_CTRL.theta = 0.0f;
    INV_CMD.Vd_ref = 20.0f; INV_CMD.Vq_ref = 0.0f;
    INV_CMD.f_ref = 50.0f; INV_PARAM.Vdc = 400.0f; INV_PARAM.inner.Lf = 0.002f; INV_PARAM.inner.Cf = 0.000012f;
    INV_PARAM.inner.Kpu = 0.0053314595f; INV_PARAM.inner.Kiu = 1.1843525f;
    INV_PARAM.inner.Kpi = 6.2831853f; INV_PARAM.inner.Kii = 0.31415927f;
    INV_PARAM.inner.U_UpLimit = 30.0f; INV_PARAM.inner.I_UpLimit = 100.0f;
    FeedbackDq_Init();
    TestInnerInit();
    GridPll_Init();
    INV_CMD.pll_enable = 1U;
    Presync_Init(&PRESYNC_CTRL);
    VSG_Init(&VSG_CTRL);
    VSG_CTRL.omega0 = TWO_PI * INV_CMD.f_ref;
    InvState_Init();
    PwmOutput_Init();
}

static void TestAdc(void)
{
    unsigned i, j;
    volatile float *physical[12] = { &Va_cap, &PCC_ua, &Ia_inv, &Vb_cap, &PCC_ub, &Ib_inv, &Vc_cap, &PCC_uc, &Ic_inv, &Ia_filter, &Ib_filter, &Ic_filter };
    volatile uint16_t *raw[12] = { &adcA0Raw, &adcE15Raw, &adcA5Raw, &adcE12Raw, &adcB1Raw, &adcB8Raw, &adcC4Raw, &adcC8Raw, &adcC14Raw, &adcA12Raw, &adcB10Raw, &adcD11Raw };
    const unsigned banks[12] = {0,4,0,4,1,1,2,2,2,0,1,3};
    const unsigned socs[12] = {0,1,2,0,1,2,0,1,2,3,3,0};
    const unsigned strides[12] = {4,2,4,2,4,4,3,3,3,4,4,2};
    Reset();
    AdcMeasurement_Init();
    for(i = 0; i < 5; i++)
    {
        adcFlags[i] = false;
        adcOverflows[i] = true; // Housekeeping must not suppress sampling.
        for(j = 0; j < 16; j++) adcResults[i][j] = (uint16_t)(2000 + i * 20 + j);
    }
    adcResultReads = adcFlagClears = 0U;
    AdcMeasurement_Acquire();
    assert(adcResultReads == 52U && adcFlagClears == 5U);
    for(i = 0; i < 12; i++)
    {
        float scale = (i == 0 || i == 1 || i == 3 || i == 4 || i == 6 || i == 7) ? VOL_SCALE : CUR_SCALE;
        uint32_t sum = 0U;
        for(j = 0; j < 4; j++) sum += adcResults[banks[i]][socs[i] + j * strides[i]];
        assert(*raw[i] == (uint16_t)((sum + 2U) >> 2U));
        Near(*physical[i], ((float)((sum + 2U) >> 2U) - adcZeroOffset[i]) * scale, 0.0001f);
    }
    for(i = 0; i < 5; i++) assert(!adcFlags[i] && !adcOverflows[i]);
    assert(adcD12Raw == (adcResults[3][1] + adcResults[3][3] + adcResults[3][5] + adcResults[3][7] + 2U) / 4U);
    Near(POWER_SCALE, 5.37109375f, 0.00001f);
    Near(P_filter_raw, ((float)adcD12Raw - 1858.0f) * 5.37109375f, 0.001f);
    assert(P_filter > 0.0f && P_filter < P_filter_raw * 0.04f); // No unfiltered first-sample jump.
    // Check screenshot zero codes and both signs of the power conversion.
    {
        const uint16_t zeroCodes[12] = {1855,1855,1855,1858,1855,1853,1858,1855,1859,1855,1859,1857};
        int delta;
        for(i = 0; i < 12; i++)
        {
            Near(adcZeroOffset[i], (float)zeroCodes[i], 0.0f);
            for(j = 0; j < 4; j++) adcResults[banks[i]][socs[i] + j * strides[i]] = zeroCodes[i];
        }
        for(delta = -100; delta <= 100; delta += 100)
        {
            for(j = 0; j < 4; j++) adcResults[3][1 + j * 2] = (uint16_t)(1858 + delta);
            adcResultReads = adcFlagClears = 0U;
            AdcMeasurement_Acquire();
            for(i = 0; i < 12; i++) Near(*physical[i], 0.0f, 0.0f);
            Near(P_filter_raw, (float)delta * 5.37109375f, 0.001f);
            for(i = 0U; i < 1000U; i++)
            {
                adcResultReads = adcFlagClears = 0U;
                AdcMeasurement_Acquire();
            }
            Near(P_filter, (float)delta * 5.37109375f, 0.002f); // DC gain and both signs retained.
        }
    }
    // Measure actual frequency response through quantized ADC samples, not just the recurrence.
    for(i = 0U; i < 2U; i++)
    {
        unsigned n;
        double inputEnergy = 0.0, outputEnergy = 0.0;
        float frequency = i == 0U ? 50.0f : 500.0f, gain;
        AdcMeasurement_Init();
        for(n = 0U; n < 4000U; n++)
        {
            uint16_t code = (uint16_t)(1858.0f + 500.0f * sinf(TWO_PI * frequency * CONTROL_TS * (float)n) + 0.5f);
            for(j = 0U; j < 4U; j++) adcResults[3][1U + j * 2U] = code;
            adcResultReads = adcFlagClears = 0U;
            AdcMeasurement_Acquire();
            if(n >= 2000U)
            {
                inputEnergy += (double)P_filter_raw * P_filter_raw;
                outputEnergy += (double)P_filter * P_filter;
            }
        }
        gain = (float)sqrt(outputEnergy / inputEnergy);
        if(i == 0U) assert(gain > 0.70f && gain < 0.715f); // Approx -3dB at 50Hz.
        else assert(gain > 0.09f && gain < 0.11f); // Approx -20dB at 500Hz.
    }
    AdcMeasurement_Init();
    assert(P_filter_raw == 0.0f && P_filter == 0.0f);
    puts("PASS thirteen fixed offsets, V/I mapping, signed D12 power/DC gain, 50Hz LPF response and reset");
}

static void TestControl(void)
{
    Reset();
    INNER_CTRL.id_ref = 2.0f;
    Inverter_ControlStep();
    assert(INNER_CTRL.id_ref == 0.0f && MODULATION_OUT.VaRef == 0.0f);
    INV_CMD.run_enable = 1U;
    INV_PARAM.inner.Kpu = INV_PARAM.inner.Kiu = INV_PARAM.inner.Kpi = INV_PARAM.inner.Kii = 0.0f;
    SetAbc(10.0f, 2.0f, &Va_cap, &Vb_cap, &Vc_cap);
    SetAbc(3.0f, 4.0f, &Ia_inv, &Ib_inv, &Ic_inv);
    SetAbc(1.0f, 0.5f, &Ia_filter, &Ib_filter, &Ic_filter);
    Inverter_ControlStep();
    Near(INV_FEEDBACK.Vd_cap, 10.0f, 0.0001f);
    Near(INV_FEEDBACK.Vq_cap, 2.0f, 0.0001f);
    Near(INNER_CTRL.id_ref, 1.0f - INV_CTRL.omega * INV_PARAM.inner.Cf * 2.0f, 0.0001f);
    Near(INNER_CTRL.iq_ref, 0.5f + INV_CTRL.omega * INV_PARAM.inner.Cf * 10.0f, 0.0001f);
    Near(INNER_CTRL.ud_ref, (10.0f - INV_CTRL.omega * INV_PARAM.inner.Lf * 4.0f) * 2.0f / INV_PARAM.Vdc, 0.0001f);
    Near(INNER_CTRL.uq_ref, (2.0f + INV_CTRL.omega * INV_PARAM.inner.Lf * 3.0f) * 2.0f / INV_PARAM.Vdc, 0.0001f);
    Ia_inv = 50.0f; // Requested basic controller has no overcurrent latch.
    Inverter_ControlStep();
    assert(INV_CMD.run_enable == 1U && isfinite(MODULATION_OUT.VaRef));
    INV_CMD.run_enable = 0U;
    Inverter_ControlStep();
    assert(INV_CTRL.theta == 0.0f && INNER_CTRL.id_ref == 0.0f && MODULATION_OUT.VaRef == 0.0f);
    puts("PASS dq, PI reset, cross coupling, normalization and stop/restart");
}

static void TestPwm(void)
{
    unsigned i;
    ThreeLevelDuty duty;
    Reset();
    for(i = 0; i <= 200; i++)
    {
        float m = (float)i * 0.01f - 1.0f;
        ThreeLevelModulation_Map(m, &duty);
        Near(duty.upper - (1.0f - duty.lower), m, 0.0001f);
    }
    TestPwmUpdate(true, 0.2f, -0.3f, 0.4f);
    assert(pwmOutputEnabled == 0U);
    TestPwmUpdate(true, 0.2f, -0.3f, 0.4f);
    assert(pwmOutputEnabled == 1U && gpioValue[47] == 0U && gpioValue[48] == 0U);
    assert(hw[1].compare[0] == 1500U && hw[5].compare[0] == 3000U);
    assert(hw[2].force == EPWM_AQ_SW_OUTPUT_HIGH && hw[3].force == EPWM_AQ_SW_OUTPUT_LOW);
    for(i = 1; i <= 6; i++) assert(hw[i].flags == 0U);
    assert(hw[7].flags & EPWM_TZ_FLAG_OST);
    assert(gpioValue[21] == 0U);
    TestPwmUpdate(false, 0.0f, 0.0f, 0.0f);
    assert(gpioValue[47] == 1U && gpioValue[48] == 1U && pwmOutputEnabled == 0U);
    TestPwmUpdate(true, 0.0f, 0.0f, 0.0f);
    assert(pwmOutputEnabled == 0U);
    TestPwmUpdate(true, 0.0f, 0.0f, 0.0f);
    assert(pwmOutputEnabled == 1U);
    puts("PASS three-level duties, EPWM5/6 C phase, shadow priming, buffer ordering, stop and unused EPWM7");
}

static void PlantStep(float *current, float *voltage, float drive, float resistance, float h)
{
    float i = *current, v = *voltage;
    float a1 = (drive - v - 0.0001f * i) / 0.002f;
    float b1 = (i - v / resistance) / 0.000012f;
    float a2 = (drive - (v + h * b1 * 0.5f) - 0.0001f * (i + h * a1 * 0.5f)) / 0.002f;
    float b2 = (i + h * a1 * 0.5f - (v + h * b1 * 0.5f) / resistance) / 0.000012f;
    float a3 = (drive - (v + h * b2 * 0.5f) - 0.0001f * (i + h * a2 * 0.5f)) / 0.002f;
    float b3 = (i + h * a2 * 0.5f - (v + h * b2 * 0.5f) / resistance) / 0.000012f;
    float a4 = (drive - (v + h * b3) - 0.0001f * (i + h * a3)) / 0.002f;
    float b4 = (i + h * a3 - (v + h * b3) / resistance) / 0.000012f;
    *current += h * (a1 + 2.0f * a2 + 2.0f * a3 + a4) / 6.0f;
    *voltage += h * (b1 + 2.0f * b2 + 2.0f * b3 + b4) / 6.0f;
}

static void TestClosedLoopPlant(float reference)
{
    unsigned k, sub, samples = 0;
    float ia = 0.0f, ib = 0.0f, va = 0.0f, vb = 0.0f;
    float driveA = 0.0f, driveB = 0.0f;
    float peakError = 0.0f;
    double sumSquares = 0.0;
    Reset();
    INV_CMD.Vd_ref = reference;
    INV_CMD.run_enable = 1U;
    for(k = 0; k < 35000; k++)
    {
        float resistance = (k < 20000) ? 50.0f : 12.0f;
        float nextDriveA, nextDriveB;
        SetAbc(va, vb, &Va_cap, &Vb_cap, &Vc_cap);
        SetAbc(ia, ib, &Ia_inv, &Ib_inv, &Ic_inv);
        SetAbc(va / resistance, vb / resistance, &Ia_filter, &Ib_filter, &Ic_filter);
        Inverter_ControlStep();
        nextDriveA = MODULATION_OUT.VaRef * INV_PARAM.Vdc * 0.5f;
        nextDriveB = (MODULATION_OUT.VbRef - MODULATION_OUT.VcRef) * INV_PARAM.Vdc * 0.28867513459f;
        // One sample delay: the present command loads at the next PWM zero.
        for(sub = 0; sub < 20; sub++)
        {
            PlantStep(&ia, &va, driveA, resistance, CONTROL_TS / 20.0f);
            PlantStep(&ib, &vb, driveB, resistance, CONTROL_TS / 20.0f);
        }
        driveA = nextDriveA;
        driveB = nextDriveB;
        if(k >= 30000)
        {
            float error = fabsf(hypotf(va, vb) - INV_CMD.Vd_ref);
            if(error > peakError) peakError = error;
            sumSquares += (double)va * va;
            samples++;
        }
    }
    printf("Plant: startup + 50-to-12 ohm step, phase RMS %.4f V, worst final amplitude error %.4f V\n", sqrt(sumSquares / samples), peakError);
    assert(peakError < INV_CMD.Vd_ref * 0.03f);
    Near((float)sqrt(sumSquares / samples), INV_CMD.Vd_ref * 0.70710678f, 3.0f);
}

static float AngleError(float a, float b)
{
    float x = a - b;
    while(x > 3.14159265359f) x -= TWO_PI;
    while(x < -3.14159265359f) x += TWO_PI;
    return x;
}

static void TestPresyncRules(void)
{
    unsigned i;
    float before;
    Reset();
    INV_CMD.run_enable = 1U;
    INV_CMD.presync_request = 1U;
    // Missing PCC must not interrupt island control or change the reference.
    TestPllUpdate(0.0f, 0.0f, 0.0f);
    before = INV_CMD.Vd_ref;
    Inverter_ControlStep();
    assert(PRESYNC_CTRL.pll_ready == 0U && PRESYNC_CTRL.sync_ready == 0U);
    assert(PRESYNC_CTRL.omega_sync == 0.0f && INV_CMD.Vd_ref == before);
    assert(INV_CMD.run_enable == 1U && INV_CTRL.state != INV_STANDBY);

    // Phase-wrap direction, bounded frequency correction, bumpless reference.
    gridPllValid = gridPllLocked = 1U;
    SPLL_GRID.d_p_decoupl_lpf = 155.56f;
    SPLL_GRID.q_p_decoupl_lpf = 0.0f;
    SPLL_GRID.fo = 50.0f;
    INV_PARAM.presync.wait_samples = 0U;
    grid_theta = TWO_PI - 0.02f;
    INV_CTRL.theta = 0.02f;
    INV_FEEDBACK.Vd_cap = 20.0f; INV_FEEDBACK.Vq_cap = 0.0f;
    before = INV_CTRL.theta;
    TestPresyncCalc(&PRESYNC_CTRL, CONTROL_TS);
    Near(PRESYNC_CTRL.theta_err, -0.04f, 0.00001f);
    assert(PRESYNC_CTRL.omega_sync < 0.0f && INV_CTRL.theta == before);
    Near(INV_CMD.Vd_ref, 20.0f + 0.001f * (155.56f - 20.0f), 0.00001f);
    grid_theta = 3.0f;
    INV_PARAM.presync.kp = 100.0f;
    TestPresyncCalc(&PRESYNC_CTRL, CONTROL_TS);
    Near(PRESYNC_CTRL.omega_sync, INV_PARAM.presync.omega_limit, 0.00001f);

    // Readiness is consecutive and revalidated, not a latched close command.
    Presync_Init(&PRESYNC_CTRL);
    INV_CMD.presync_request = 1U;
    INV_PARAM.presync.wait_samples = 0U;
    grid_theta = INV_CTRL.theta = 0.0f;
    INV_FEEDBACK.Vd_cap = INV_CMD.Vd_ref = 155.56f; INV_FEEDBACK.Vq_cap = 0.0f;
    for(i = 0; i < 9U; i++) TestPresyncCalc(&PRESYNC_CTRL, CONTROL_TS);
    assert(PRESYNC_CTRL.sync_ready == 0U);
    TestPresyncCalc(&PRESYNC_CTRL, CONTROL_TS);
    assert(PRESYNC_CTRL.sync_ready == 1U);
    INV_FEEDBACK.Vq_cap = 30.0f; // Oscillator alone is not sufficient if measured phase lags.
    TestPresyncCalc(&PRESYNC_CTRL, CONTROL_TS);
    assert(PRESYNC_CTRL.sync_ready == 0U);
    before = INV_CTRL.theta;
    gridPllValid = 0U;
    TestPresyncCalc(&PRESYNC_CTRL, CONTROL_TS);
    assert(PRESYNC_CTRL.omega_sync == 0.0f && PRESYNC_CTRL.pll_wait_cnt == 0U);
    assert(INV_CTRL.theta == before && INV_CMD.run_enable == 1U);
    INV_CMD.presync_request = 0U;
    before = INV_CMD.Vd_ref;
    TestPresyncCalc(&PRESYNC_CTRL, CONTROL_TS);
    assert(INV_CMD.Vd_ref == before);
    // Every entry waits again, and stopping clears compensation without a trip.
    INV_CMD.presync_request = 1U;
    gridPllValid = gridPllLocked = 1U;
    INV_PARAM.presync.wait_samples = 3U;
    for(i = 0; i < 3U; i++) TestPresyncCalc(&PRESYNC_CTRL, CONTROL_TS);
    assert(PRESYNC_CTRL.pll_ready == 0U);
    TestPresyncCalc(&PRESYNC_CTRL, CONTROL_TS);
    assert(PRESYNC_CTRL.pll_ready == 1U);
    INV_CMD.run_enable = 0U;
    Inverter_ControlStep();
    assert(PRESYNC_CTRL.pll_ready == 0U && PRESYNC_CTRL.omega_sync == 0.0f);
    puts("PASS presync no-PCC fallback, phase wrap, PI bounds, measured phase and readiness reset");
}

static void TestPresyncTracking(float frequency)
{
    unsigned k, readyCount = 0U;
    float theta = 1.2f;
    Reset();
    INV_CMD.run_enable = 1U;
    INV_CMD.Vd_ref = 120.0f;
    INV_CMD.presync_request = 1U;
    // Actual migrated PLL and presync, ideal settled voltage inner loop.
    // This isolates synchronization from an unvalidated HIL power-stage model.
    for(k = 0; k < 120000U; k++)
    {
        float oldTheta = INV_CTRL.theta;
        float sine = sinf(theta), cosine = cosf(theta);
        SetAbc(155.56f * cosine, 155.56f * sine, &PCC_ua, &PCC_ub, &PCC_uc);
        SetAbc(INV_CMD.Vd_ref * cosf(oldTheta), INV_CMD.Vd_ref * sinf(oldTheta), &Va_cap, &Vb_cap, &Vc_cap);
        TestPllUpdate(PCC_ua, PCC_ub, PCC_uc);
        Inverter_ControlStep();
        TestPwmUpdate(true, MODULATION_OUT.VaRef, MODULATION_OUT.VbRef, MODULATION_OUT.VcRef);
        Near(AngleError(INV_CTRL.theta, oldTheta), INV_CTRL.omega * CONTROL_TS, 0.00001f);
        assert(fabsf(PRESYNC_CTRL.omega_sync) <= INV_PARAM.presync.omega_limit + 0.0001f);
        theta += TWO_PI * frequency * CONTROL_TS;
        if(theta >= TWO_PI) theta -= TWO_PI;
        if(k > 110000U && PRESYNC_CTRL.sync_ready) readyCount++;
    }
    printf("Presync %.1f Hz: PLL %.5f Hz, output %.5f Hz, phase %.6f rad, voltage %.4f V, ready %u/9999\n", frequency, SPLL_GRID.fo, INV_CTRL.omega / TWO_PI, PRESYNC_CTRL.theta_err, INV_CMD.Vd_ref, readyCount);
    Near(SPLL_GRID.fo, frequency, 0.02f);
    Near(INV_CTRL.omega / TWO_PI, frequency, 0.02f);
    assert(fabsf(PRESYNC_CTRL.theta_err) < 0.01f && readyCount > 9900U);
    Near(INV_CMD.Vd_ref, 155.56f, 0.02f);
    assert(gpioValue[21] == 0U);
    // Remove PCC without stopping PWM; then restore it and reacquire.
    TestPllUpdate(0.0f, 0.0f, 0.0f);
    Inverter_ControlStep();
    TestPwmUpdate(true, MODULATION_OUT.VaRef, MODULATION_OUT.VbRef, MODULATION_OUT.VcRef);
    assert(PRESYNC_CTRL.sync_ready == 0U && PRESYNC_CTRL.omega_sync == 0.0f);
    assert(pwmOutputEnabled == 1U && gpioValue[21] == 0U);
    // Continue the grid angle through a 400 ms dropout and reacquire.
    for(k = 0; k < 100000U; k++)
    {
        float oldTheta = INV_CTRL.theta;
        SetAbc(INV_CMD.Vd_ref * cosf(oldTheta), INV_CMD.Vd_ref * sinf(oldTheta), &Va_cap, &Vb_cap, &Vc_cap);
        if(k < 4000U) TestPllUpdate(0.0f, 0.0f, 0.0f);
        else
        {
            SetAbc(155.56f * cosf(theta), 155.56f * sinf(theta), &PCC_ua, &PCC_ub, &PCC_uc);
            TestPllUpdate(PCC_ua, PCC_ub, PCC_uc);
        }
        Inverter_ControlStep();
        TestPwmUpdate(true, MODULATION_OUT.VaRef, MODULATION_OUT.VbRef, MODULATION_OUT.VcRef);
        if(k < 4000U) assert(PRESYNC_CTRL.sync_ready == 0U);
        theta += TWO_PI * frequency * CONTROL_TS;
        if(theta >= TWO_PI) theta -= TWO_PI;
    }
    printf("Recovery %.1f: PLL=%g Hz, output=%g Hz, phase=%g rad, ready=%u\n", frequency, SPLL_GRID.fo, INV_CTRL.omega / TWO_PI, PRESYNC_CTRL.theta_err, PRESYNC_CTRL.sync_ready);
    assert(PRESYNC_CTRL.sync_ready == 1U && gpioValue[21] == 0U);
    Near(INV_CTRL.omega / TWO_PI, frequency, 0.02f);
    printf("PASS %.1f Hz PCC dropout/reacquisition; breaker remains open\n", frequency);
    INV_CMD.presync_request = 0U;
    Inverter_ControlStep();
    Near(INV_CTRL.omega, TWO_PI * INV_CMD.f_ref, 0.0001f);
}

static void TestVsgPowerUnits(void)
{
    float expected;
    Reset();
    Near(SYS_S_BASE_VA, 5000.0f, 0.0f);
    INV_FEEDBACK.Vd_cap = 100.0f; INV_FEEDBACK.Vq_cap = 20.0f;
    INV_FEEDBACK.Id_filter = 10.0f; INV_FEEDBACK.Iq_filter = -2.0f;
    FeedbackDq_CalcPower();
    Near(INV_FEEDBACK.Pe, 1440.0f, 0.001f);
    Near(INV_FEEDBACK.Qe, 600.0f, 0.001f);
    Near(INV_FEEDBACK.Pe_pu, 0.288f, 0.000001f);
    Near(INV_FEEDBACK.Qe_pu, 0.12f, 0.000001f);
    INV_CMD.p_ref = 0.1f;
    expected = 0.1f * CONTROL_TS / 5.0f;
    TestPowerLoop(&VSG_CTRL, 0.0f, 0.0f, CONTROL_TS);
    Near(VSG_CTRL.omega_m_pu, expected, 0.00000001f);
    Near(VSG_CTRL.omega_m, expected * VSG_CTRL.omega0, 0.000001f);
    INV_CMD.p_ref = 1000.0f;
    TestPowerLoop(&VSG_CTRL, 0.0f, 0.0f, 1.0f);
    Near(VSG_CTRL.omega_m_pu, INV_PARAM.vsg.omega_limit / VSG_CTRL.omega0, 0.000001f);
    INV_CMD.p_ref = -1000.0f;
    TestPowerLoop(&VSG_CTRL, 0.0f, 0.0f, 1.0f);
    Near(VSG_CTRL.omega_m_pu, (-INV_PARAM.vsg.omega_limit) / VSG_CTRL.omega0, 0.000001f);
    INV_CMD.q_ref = 0.2f;
    INV_FEEDBACK.Qe_pu = 0.1f;
    TestEnterGrid(&VSG_CTRL, 2.0f, -TWO_PI, 150.0f);
    INV_CMD.p_ref = 0.0f;
    TestPowerLoop(&VSG_CTRL, 0.0f, 0.1f, CONTROL_TS);
    Near(INV_CMD.Vd_ref, 150.0f, 0.00001f);
    TestPowerLoop(&VSG_CTRL, 0.0f, 0.0f, CONTROL_TS);
    Near(VSG_CTRL.v_delta_pu, 0.005f, 0.000001f);
    Near((VSG_CTRL.v_delta_pu * SYS_V_BASE_V), 0.005f * 155.56f, 0.00001f);
    puts("PASS 5000 W power normalization, pu swing equation/limits and bumpless normalized Q-V PI");
}

static void TestGridCommandInterlock(void)
{
    unsigned i;
    Reset();
    INV_CMD.run_enable = 1U;
    INV_CMD.grid_connect_cmd = 1U;
    Inverter_ControlStep();
    assert(INV_CMD.grid_connect_cmd == 0U && INV_CTRL.grid_connect_rejected == 1U);
    assert(INV_CTRL.state != INV_GRID_CONNECTED && gpioValue[21] == 0U);
    // Stale sync_ready is recalculated and cannot close after PCC disappears.
    INV_CMD.presync_request = PRESYNC_CTRL.sync_ready = 1U;
    gridPllValid = 0U;
    INV_CMD.grid_connect_cmd = 1U;
    Inverter_ControlStep();
    assert(INV_CMD.grid_connect_cmd == 0U && INV_CTRL.state != INV_GRID_CONNECTED);
    // A lone phase-A match does not bypass B/C voltage mismatch.
    gridPllValid = gridPllLocked = 1U;
    INV_CMD.presync_request = PRESYNC_CTRL.sync_ready = 1U;
    PCC_ua = Va_cap = 100.0f;
    PCC_ub = 30.0f; Vb_cap = -50.0f;
    PCC_uc = Vc_cap = -50.0f;
    INV_CMD.grid_connect_cmd = 1U;
    TestStateStep(&INV_CTRL);
    assert(INV_CTRL.grid_zero_cross_ok == 0U && INV_CMD.grid_connect_cmd == 0U);
    for(i = 0; i < 20U; i++) TestStateStep(&INV_CTRL);
    assert(INV_CTRL.state != INV_GRID_CONNECTED);
    puts("PASS premature/stale commands rejected, all-three-phase mismatch check and no delayed automatic closure");
}

static void SetGridModel(float thetaGrid, float reactance, bool connected)
{
    float a = INV_CMD.Vd_ref * cosf(INV_CTRL.theta);
    float b = INV_CMD.Vd_ref * sinf(INV_CTRL.theta);
    float ga = 155.56f * cosf(thetaGrid), gb = 155.56f * sinf(thetaGrid);
    float ia = connected ? (b - gb) / reactance : 0.0f;
    float ib = connected ? -(a - ga) / reactance : 0.0f;
    SetAbc(a, b, &Va_cap, &Vb_cap, &Vc_cap);
    SetAbc(ga, gb, &PCC_ua, &PCC_ub, &PCC_uc);
    SetAbc(ia, ib, &Ia_filter, &Ib_filter, &Ic_filter);
    SetAbc(ia - INV_CTRL.omega * INV_PARAM.inner.Cf * b, ib + INV_CTRL.omega * INV_PARAM.inner.Cf * a, &Ia_inv, &Ib_inv, &Ic_inv);
}

static void TestGridVsgSwitch(float gridFrequency)
{
    unsigned k;
    float thetaGrid = 1.0f;
    float thetaBefore, voltageBefore, heldCorrection, frequencyBefore;
    Reset();
    INV_CMD.run_enable = 1U;
    INV_CMD.Vd_ref = 120.0f;
    INV_CMD.presync_request = 1U;
    for(k = 0; k < 120000U; k++)
    {
        SetGridModel(thetaGrid, 1.0f, false);
        TestPllUpdate(PCC_ua, PCC_ub, PCC_uc);
        Inverter_ControlStep();
        TestPwmUpdate(true, MODULATION_OUT.VaRef, MODULATION_OUT.VbRef, MODULATION_OUT.VcRef);
        assert(gpioValue[21] == 0U); // Readiness alone never closes the breaker.
        thetaGrid += TWO_PI * gridFrequency * CONTROL_TS;
        if(thetaGrid >= TWO_PI) thetaGrid -= TWO_PI;
    }
    assert(INV_CTRL.state == INV_GRID_READY);
    assert(PRESYNC_CTRL.sync_ready == 1U);
    thetaBefore = INV_CTRL.theta;
    voltageBefore = INV_CMD.Vd_ref;
    frequencyBefore = INV_CTRL.omega / TWO_PI;
    INV_CMD.grid_connect_cmd = 1U;
    SetGridModel(thetaGrid, 1.0f, false);
    TestPllUpdate(PCC_ua, PCC_ub, PCC_uc);
    Inverter_ControlStep();
    heldCorrection = VSG_CTRL.omega_sync;
    assert(INV_CTRL.state == INV_GRID_CONNECTED && INV_CTRL.grid_connect_rejected == 0U);
    assert(INV_CMD.presync_request == 0U && PRESYNC_CTRL.omega_sync == 0.0f);
    Near(VSG_CTRL.omega_m, 0.0f, 0.0f);
    Near(AngleError(INV_CTRL.theta, thetaBefore), INV_CTRL.omega * CONTROL_TS, 0.00001f);
    Near(INV_CTRL.omega / TWO_PI, frequencyBefore, 0.001f);
    Near(INV_CMD.Vd_ref, voltageBefore, 0.0001f);
    TestPwmUpdate(true, MODULATION_OUT.VaRef, MODULATION_OUT.VbRef, MODULATION_OUT.VcRef);
    assert(gpioValue[21] == 1U);
    thetaGrid += TWO_PI * gridFrequency * CONTROL_TS;
    if(thetaGrid >= TWO_PI) thetaGrid -= TWO_PI;

    // Ideal voltage-source / reactance grid tests actual migrated VSG dynamics.
    for(k = 0; k < 60000U; k++)
    {
        INV_CMD.p_ref = (k < 30000U) ? 0.1f : 0.2f;
        SetGridModel(thetaGrid, 1.0f, true);
        TestPllUpdate(PCC_ua, PCC_ub, PCC_uc);
        thetaBefore = INV_CTRL.theta;
        Inverter_ControlStep();
        TestPwmUpdate(true, MODULATION_OUT.VaRef, MODULATION_OUT.VbRef, MODULATION_OUT.VcRef);
        Near(AngleError(INV_CTRL.theta, thetaBefore), INV_CTRL.omega * CONTROL_TS, 0.00001f);
        Near(VSG_CTRL.omega_sync, heldCorrection, 0.0f); // Frozen, no PLL steering.
        assert(INV_CTRL.state == INV_GRID_CONNECTED && gpioValue[21] == 1U);
        if(k == 29999U) Near(INV_FEEDBACK.Pe_pu, 0.1f, 0.003f);
        thetaGrid += TWO_PI * gridFrequency * CONTROL_TS;
        if(thetaGrid >= TWO_PI) thetaGrid -= TWO_PI;
    }
    Near(INV_FEEDBACK.Pe_pu, 0.2f, 0.003f);
    Near(INV_FEEDBACK.Pe, 1000.0f, 15.0f);
    Near(INV_CTRL.omega / TWO_PI, gridFrequency, 0.02f);
    printf("Grid %.1f Hz: continuous VF->VSG, captured correction %.6f rad/s, P %.3f W, Q %.3f var\n", gridFrequency, heldCorrection, INV_FEEDBACK.Pe, INV_FEEDBACK.Qe);

    // Manual open retains angle/frequency/voltage and does not auto-presync.
    thetaBefore = INV_CTRL.theta;
    frequencyBefore = INV_CTRL.omega / TWO_PI;
    voltageBefore = INV_CMD.Vd_ref;
    INV_CMD.grid_connect_cmd = 0U;
    Inverter_ControlStep();
    TestPwmUpdate(true, MODULATION_OUT.VaRef, MODULATION_OUT.VbRef, MODULATION_OUT.VcRef);
    assert(INV_CTRL.state == INV_OFFGRID_VF && gpioValue[21] == 0U);
    Near(INV_CTRL.omega / TWO_PI, frequencyBefore, 0.0001f);
    Near(AngleError(INV_CTRL.theta, thetaBefore), INV_CTRL.omega * CONTROL_TS, 0.00001f);
    Near(INV_CMD.Vd_ref, voltageBefore, 0.0001f);
    assert(INV_CMD.presync_request == 0U);
    INV_CMD.run_enable = 0U;
    INV_CMD.grid_connect_cmd = 1U;
    Inverter_ControlStep();
    TestPwmUpdate(false, MODULATION_OUT.VaRef, MODULATION_OUT.VbRef, MODULATION_OUT.VcRef);
    assert(INV_CTRL.state == INV_STANDBY && INV_CMD.grid_connect_cmd == 0U);
    assert(gpioValue[21] == 0U && pwmOutputEnabled == 0U);
    puts("PASS manual-open VF continuity, master stop opens breaker, no automatic reconnection");
}

static void TestUnifiedApi(void)
{
    unsigned i, j;
    memset(hw, 0, sizeof(hw));
    memset(gpioValue, 0, sizeof(gpioValue));
    Inverter_Init();
    assert(INV_CMD.run_enable == 0U && INV_CMD.presync_request == 0U && INV_CMD.grid_connect_cmd == 0U);
    assert(INV_CMD.pll_enable == 1U && INV_CTRL.state == INV_STANDBY);
    assert(INV_CMD.grid_impedance_step == 0U && gpioValue[12] == 0U);
    Near(INV_PARAM.inner.Kpu, 0.7f, 0.0f);
    Near(INV_PARAM.inner.Kiu, 0.5f, 0.0f);
    Near(INV_PARAM.inner.Kpi, 1.5f, 0.0f);
    Near(INV_PARAM.inner.U_UpLimit, 300.0f, 0.0f);
    // Operator-tunable H/D defaults must not pin communication tests to old tuning.
    assert(isfinite(INV_PARAM.vsg.h) && INV_PARAM.vsg.h > 0.0f);
    assert(isfinite(INV_PARAM.vsg.dp_pu) && INV_PARAM.vsg.dp_pu > 0.0f);
    assert(gpioValue[21] == 0U && gpioValue[47] == 1U && gpioValue[48] == 1U);
    for(i = 0; i < 5; i++) for(j = 0; j < 16; j++) adcResults[i][j] = 1855U;
    INV_CMD.run_enable = 1U;
    for(i = 0; i < 3; i++)
    {
        adcResultReads = adcFlagClears = 0U;
        Inverter_Step();
        assert(adcResultReads == 52U && adcFlagClears == 5U);
    }
    assert(INV_CTRL.state == INV_OFFGRID_VF && pwmOutputEnabled == 1U && gpioValue[21] == 0U);
    assert(gpioValue[47] == 0U && gpioValue[48] == 0U);
    INV_CMD.grid_impedance_step = 1U;
    adcResultReads = adcFlagClears = 0U;
    Inverter_Step();
    assert(gpioValue[12] == 1U && pwmOutputEnabled == 1U && gpioValue[21] == 0U);
    INV_CMD.grid_connect_cmd = 1U;
    adcResultReads = adcFlagClears = 0U;
    Inverter_Step();
    assert(INV_CMD.grid_connect_cmd == 0U && INV_CTRL.grid_connect_rejected == 1U);
    INV_CMD.run_enable = 0U;
    adcResultReads = adcFlagClears = 0U;
    Inverter_Step();
    assert(INV_CTRL.state == INV_STANDBY && pwmOutputEnabled == 0U && gpioValue[21] == 0U);
    assert(gpioValue[47] == 1U && gpioValue[48] == 1U);
    assert(INV_CTRL.theta == 0.0f && MODULATION_OUT.VaRef == 0.0f);
    assert(gpioValue[12] == 1U); // HIL condition stays independent of inverter stop.
    INV_CMD.grid_impedance_step = 0U;
    adcResultReads = adcFlagClears = 0U;
    Inverter_Step();
    assert(gpioValue[12] == 0U && pwmOutputEnabled == 0U && gpioValue[21] == 0U);
    INV_CMD.grid_impedance_step = 2U;
    adcResultReads = adcFlagClears = 0U;
    Inverter_Step();
    assert(gpioValue[12] == 0U); // Only explicit 1 drives HIGH.
    INV_CMD.grid_impedance_step = 1U;
    adcResultReads = adcFlagClears = 0U;
    Inverter_Step();
    assert(gpioValue[12] == 1U);
    Inverter_Init();
    assert(INV_CMD.grid_impedance_step == 0U && gpioValue[12] == 0U);
    puts("PASS GPIO12 HIL condition: default LOW, manual HIGH/LOW, stop independence and initialization reset");
    puts("PASS unified Init/Step API, centralized defaults/commands, full ADC->control->PWM and master stop");
}

static void TestInnerStateAtTransfer(void)
{
    unsigned k;
    float integralOutput;
    Reset();
    INV_CMD.run_enable = 1U;
    INV_PARAM.inner.Kpu = 0.0f;
    INV_PARAM.inner.Kiu = 10.0f;
    INV_CMD.Vd_ref = 160.56f;
    for(k = 0; k < 2000U; k++)
    {
        SetGridModel(INV_CTRL.theta, 1.0f, false);
        // Keep measured amplitude at 155.56 V to build +5 V voltage error.
        SetAbc(155.56f * cosf(INV_CTRL.theta), 155.56f * sinf(INV_CTRL.theta), &Va_cap, &Vb_cap, &Vc_cap);
        Inverter_ControlStep();
    }
    integralOutput = INNER_CTRL.id_ref;
    assert(integralOutput > 9.0f);
    INV_CMD.Vd_ref = 155.56f;
    INV_CMD.presync_request = 1U;
    INV_PARAM.presync.wait_samples = 0U;
    INV_PARAM.presync.ready_samples = 1U;
    gridPllValid = gridPllLocked = 1U;
    grid_theta = INV_CTRL.theta;
    SPLL_GRID.fo = 50.0f;
    SPLL_GRID.d_p_decoupl_lpf = 155.56f;
    SPLL_GRID.q_p_decoupl_lpf = 0.0f;
    SetGridModel(INV_CTRL.theta, 1.0f, false);
    INV_CMD.grid_connect_cmd = 1U;
    Inverter_ControlStep();
    assert(INV_CTRL.state == INV_GRID_CONNECTED);
    Near(INNER_CTRL.id_ref, integralOutput, 0.001f);
    puts("PASS black-box inner PI integral retention at VF->VSG transfer");
}

void TestAnnMigration(void);
void TestCanMigration(void);

int main(int argc, char **argv)
{
    void TestVsgAutoTest(void);
    void TestPllOptimization(void);
    void TestVirtualAngleModes(void);
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    TestAdc();
    if(argc == 2 && strcmp(argv[1], "--virtual-angle-only") == 0)
    {
        TestVirtualAngleModes();
        return 0;
    }
    if(argc == 2 && strcmp(argv[1], "--pll-only") == 0)
    {
        TestPllOptimization();
        TestPresyncRules();
        TestPresyncTracking(49.0f);
        TestPresyncTracking(50.0f);
        TestPresyncTracking(51.0f);
        TestInnerStateAtTransfer();
        return 0;
    }
    if(argc == 2 && strcmp(argv[1], "--auto-test-only") == 0)
    {
        TestVsgAutoTest();
        return 0;
    }
    if(argc == 2 && strcmp(argv[1], "--ann-only") == 0)
    {
        TestAnnMigration();
        return 0;
    }
    if(argc == 2 && strcmp(argv[1], "--can-only") == 0)
    {
        TestControl();
        TestPwm();
        TestUnifiedApi();
        TestGridCommandInterlock();
        TestCanMigration();
        puts("ALL CAN MIGRATION AND ADC/INVERTER INTEGRATION TESTS PASSED");
        return 0;
    }
    if(argc == 2 && strcmp(argv[1], "--adc-only") == 0)
    {
        TestUnifiedApi();
        puts("ALL ADC REMAP AND UNIFIED ACQUISITION TESTS PASSED");
        return 0;
    }
    TestControl();
    TestPwm();
    TestUnifiedApi();
    TestInnerStateAtTransfer();
    TestVsgPowerUnits();
    TestGridCommandInterlock();
    TestGridVsgSwitch(49.0f);
    TestGridVsgSwitch(50.0f);
    TestGridVsgSwitch(51.0f);
    TestPresyncRules();
    TestPresyncTracking(49.0f);
    TestPresyncTracking(50.0f);
    TestPresyncTracking(51.0f);
    TestClosedLoopPlant(20.0f);
    TestClosedLoopPlant(155.56f);
    TestAnnMigration();
    TestCanMigration();
    TestVsgAutoTest();
    TestPllOptimization();
    TestVirtualAngleModes();
    puts("ALL MODULAR INVERTER HOST TESTS PASSED");
    return 0;
}
