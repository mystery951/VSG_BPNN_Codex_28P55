#include "ann_npu.h"
#include "ann_models.h"
#include "app/system_config.h"
#include <math.h>

#if defined(__TMS320C28XX__)
#include "device.h"
#include "driverlib.h"
/* Unmodified TI SDK NPU support, excluded from independent CCS compilation. */
#include "npu_model/f28p55x_npu_impl.inc"
#endif

#if defined(_FLASH) && defined(__TMS320C28XX__)
#define ANN_NPU_TARGET 1
#include "npu_model/tvmgen_default.h"
#ifndef TVMGEN_DEFAULT_TI_NPU
#error Expected a hardware NPU regression model
#endif
static int8_t input_buffer[16];
static float output_buffer[2];
extern volatile uint32_t pwm1IntCount;
static uint32_t ClockTick(void) { return pwm1IntCount; }
static bool Finished(void) { return tvmgen_default_finished != 0; }
static int32_t Launch(void)
{
    struct tvmgen_default_inputs inputs = {(void *)input_buffer};
    struct tvmgen_default_outputs outputs = {(void *)output_buffer};
    tvmgen_default_finished = 0;
    return tvmgen_default_run(&inputs, &outputs);
}
#elif defined(ANN_NPU_TEST_BACKEND)
#define ANN_NPU_TARGET 1
static int16_t input_buffer[16];
static float output_buffer[2];
extern uint32_t AnnNpuTest_ClockTick(void);
extern bool AnnNpuTest_Finished(void);
extern int32_t AnnNpuTest_Launch(const int16_t *, float *);
static uint32_t ClockTick(void) { return AnnNpuTest_ClockTick(); }
static bool Finished(void) { return AnnNpuTest_Finished(); }
static int32_t Launch(void) { return AnnNpuTest_Launch(input_buffer, output_buffer); }
#else
#define ANN_NPU_TARGET 0
#endif

volatile ANN_NPU_MONITOR ANN_NPU_CTRL = {0};
#if ANN_NPU_TARGET
static uint32_t start_tick;
#endif

void AnnNpu_Init(void)
{
    const ANN_NPU_MONITOR zero = {0};
    ANN_NPU_CTRL = zero;
    ANN_NPU_CTRL.available = ANN_NPU_TARGET;
    ANN_NPU_CTRL.enabled = ANN_NPU_TARGET;
}

int16_t AnnNpu_Quantize(float normalized)
{
    float value = floorf(normalized * 64.0f + 0.5f);
    if(value > 127.0f) value = 127.0f;
    if(value < -128.0f) value = -128.0f;
    return (int16_t)value;
}

bool AnnNpu_Start(float Q, float angle)
{
#if ANN_NPU_TARGET
    uint16_t i;
    float x0, x1;
    if(!ANN_NPU_CTRL.enabled || ANN_NPU_CTRL.busy || ANN_NPU_CTRL.fault) return false;
    ANN_NPU_CTRL.Q = Q;
    ANN_NPU_CTRL.angle = angle;
    if(!(Q >= ANN_Q_MIN && Q <= ANN_Q_MAX && angle >= ANN_ANGLE_MIN && angle <= ANN_ANGLE_MAX))
    {
        ANN_NPU_CTRL.rejected++;
        ANN_NPU_CTRL.status = 3U;
        return false;
    }
    x0 = (Q - ANN_Q_MIN) * 0.0001866342898f - 1.0f;
    x1 = (angle - ANN_ANGLE_MIN) * 0.08500835207f - 1.0f;
    for(i = 0U; i < 16U; ++i) input_buffer[i] = 0;
    input_buffer[0] = AnnNpu_Quantize(x0);
    input_buffer[1] = AnnNpu_Quantize(x1);
    for(i = 0U; i < 16U; ++i) ANN_NPU_CTRL.quantized_input[i] = input_buffer[i];
    output_buffer[0] = output_buffer[1] = NAN;
    start_tick = ClockTick();
    ANN_NPU_CTRL.busy = 1U;
    ANN_NPU_CTRL.status = 1U;
    ANN_NPU_CTRL.started++;
    ANN_NPU_CTRL.api_result = Launch();
    if(ANN_NPU_CTRL.api_result != 0)
    {
        ANN_NPU_CTRL.busy = 0U;
        ANN_NPU_CTRL.fault = 1U;
        ANN_NPU_CTRL.status = 5U;
        ANN_NPU_CTRL.failures++;
        return false;
    }
    return true;
#else
    (void)Q; (void)angle;
    return false;
#endif
}

int AnnNpu_Poll(float *L_mH, float *R_ohm)
{
#if ANN_NPU_TARGET
    if(!ANN_NPU_CTRL.busy) return -1;
    ANN_NPU_CTRL.elapsed_ticks = ClockTick() - start_tick;
    if(!Finished())
    {
        /* 100 ms, derived from the project's actual control sampling period. */
        if(ANN_NPU_CTRL.elapsed_ticks < (uint32_t)(0.1f / CONTROL_TS + 0.5f)) return 0;
        ANN_NPU_CTRL.busy = 0U;
        ANN_NPU_CTRL.fault = 1U; /* Do not reuse buffers while hardware could still run. */
        ANN_NPU_CTRL.status = 4U;
        ANN_NPU_CTRL.failures++;
        return -1;
    }
    ANN_NPU_CTRL.busy = 0U;
    ANN_NPU_CTRL.normalized_output[0] = output_buffer[0];
    ANN_NPU_CTRL.normalized_output[1] = output_buffer[1];
    ANN_NPU_CTRL.L_mH = output_buffer[0] * 2.368518667782116f + 3.2359364762792078f;
    ANN_NPU_CTRL.R_ohm = output_buffer[1] * 0.46936755216588216f + 0.8041675440903024f;
    if(!isfinite(ANN_NPU_CTRL.L_mH) || !isfinite(ANN_NPU_CTRL.R_ohm))
    {
        ANN_NPU_CTRL.fault = 1U;
        ANN_NPU_CTRL.status = 5U;
        ANN_NPU_CTRL.failures++;
        return -1;
    }
    *L_mH = ANN_NPU_CTRL.L_mH;
    *R_ohm = ANN_NPU_CTRL.R_ohm;
    ANN_NPU_CTRL.completed++;
    ANN_NPU_CTRL.status = 2U;
    return 1;
#else
    (void)L_mH; (void)R_ohm;
    return -1;
#endif
}
