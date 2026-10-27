#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "app/adaptive/ann_npu.h"
#include "app/adaptive/ann_adaptive.h"
#include "app/system_config.h"
#include "ann_npu_vectors.inc"

static uint32_t ticks;
static bool finished, masked;
static int32_t api_result;
static float *hw_output;
static const int16_t *hw_input;
uint32_t AnnNpuTest_ClockTick(void) { return ticks; }
bool AnnNpuTest_Finished(void) { return finished; }
int32_t AnnNpuTest_Launch(const int16_t *input, float *output)
{
    hw_input = input;
    hw_output = output;
    finished = false;
    return api_result;
}
bool Interrupt_disableGlobal(void) { bool previous = masked; masked = true; return previous; }
void Interrupt_enableGlobal(void) { masked = false; }
static void Complete(float a, float b) { hw_output[0] = a; hw_output[1] = b; finished = true; }
static void Window(float q, float angle)
{
    unsigned i;
    for(i = 0; i < ANN_AVERAGE_SAMPLES; ++i) AnnAdaptive_Sample(true, q, angle);
}

int main(void)
{
    unsigned i, j;
    float L, R;
    for(i = 0; i < sizeof(vectors)/sizeof(vectors[0]); ++i)
    {
        AnnNpu_Init();
        assert(ANN_NPU_CTRL.enabled && ANN_NPU_CTRL.available);
        assert(AnnNpu_Start(vectors[i].q, vectors[i].angle));
        assert(hw_input[0] == vectors[i].x0 && hw_input[1] == vectors[i].x1);
        for(j = 2; j < 16; ++j) assert(hw_input[j] == 0);
        assert(!AnnNpu_Start(0.0f, 10.0f));
        assert(AnnNpu_Poll(&L, &R) == 0);
        Complete(vectors[i].y0, vectors[i].y1);
        assert(AnnNpu_Poll(&L, &R) == 1);
        assert(fabsf(L - (vectors[i].y0 * 2.368518667782116f + 3.2359364762792078f)) < 1e-6f);
        assert(fabsf(R - (vectors[i].y1 * 0.46936755216588216f + 0.8041675440903024f)) < 1e-6f);
    }
    assert(AnnNpu_Quantize(-0.01f) == -1); /* floor, not truncation toward zero */
    assert(AnnNpu_Quantize(2.0f) == 127 && AnnNpu_Quantize(-3.0f) == -128);
    AnnNpu_Init();
    assert(!AnnNpu_Start(NAN, 10.0f) && !AnnNpu_Start(0.0f, INFINITY));
    assert(!AnnNpu_Start(2000.0f, 10.0f));
    assert(ANN_NPU_CTRL.rejected == 3U);
    ticks = 0xfffffff0UL;
    assert(AnnNpu_Start(0.0f, 10.0f));
    ticks += (uint32_t)(0.1f / CONTROL_TS + 0.5f);
    assert(AnnNpu_Poll(&L, &R) == -1 && ANN_NPU_CTRL.fault && ANN_NPU_CTRL.status == 4U);
    assert(!AnnNpu_Start(0.0f, 10.0f));
    AnnNpu_Init();
    api_result = -7;
    assert(!AnnNpu_Start(0.0f, 10.0f) && ANN_NPU_CTRL.status == 5U);
    api_result = 0;

    /* Background start/poll is nonblocking; new windows cannot overwrite DMA input. */
    AnnAdaptive_Init();
    Window(-4000.0f, 15.0f);
    AnnAdaptive_Service();
    assert(ANN_NPU_CTRL.busy && ANN_CTRL.inference_count == 0U);
    Window(-3000.0f, 16.0f);
    AnnAdaptive_Service();
    assert(ANN_NPU_CTRL.busy && ANN_CTRL.ann_run_flag == 1U);
    Complete(0.0f, 0.0f);
    AnnAdaptive_Service();
    assert(ANN_CTRL.inference_count == 1U && fabsf(ANN_CTRL.ann_Q_input + 4000.0f) < 0.01f);
    AnnAdaptive_Service();
    assert(ANN_NPU_CTRL.busy && ANN_CTRL.ann_run_flag == 0U);
    /* Disable/re-enable cancels publication of the old session's result. */
    AnnAdaptive_Sample(false, 0.0f, 0.0f);
    Window(0.0f, 10.0f);
    Complete(0.0f, 0.0f);
    AnnAdaptive_Service();
    assert(ANN_CTRL.inference_count == 1U && ANN_CTRL.result_valid == 0U);
    AnnAdaptive_Service();
    Complete(0.0f, 0.0f);
    AnnAdaptive_Service();
    assert(ANN_CTRL.inference_count == 2U && ANN_CTRL.ann_Q_input == 0.0f);
    assert(!masked);

    // Regression: input rejection must not publish NaN or enable stale J/D application.
    AnnAdaptive_Init();
    Window(0.0f, 10.0f); AnnAdaptive_Service();
    Complete((5.0f - 3.2359364762792078f) / 2.368518667782116f, (0.5f - 0.8041675440903024f) / 0.46936755216588216f);
    AnnAdaptive_Service();
    assert(ANN_CTRL.result_valid && ANN_CTRL.has_valid_result && ANN_CTRL.result_status == ANN_RESULT_VALID);
    L = ANN_CTRL.L_est; R = ANN_CTRL.R_est;
    Window(2000.0f, 40.0f); AnnAdaptive_Service();
    assert(ANN_NPU_CTRL.status == 3U && ANN_NPU_CTRL.fault == 0U);
    assert(ANN_CTRL.result_valid == 0U && ANN_CTRL.result_status == ANN_RESULT_INPUT_RANGE);
    assert(ANN_CTRL.L_est == L && ANN_CTRL.R_est == R && isfinite(ANN_CTRL.J_apt) && isfinite(ANN_CTRL.D_apt));
    {
        VSG_PARAM cfg = {0};
        cfg.h = 2.5f; cfg.dp_pu = 200.0f; cfg.power_loop_div = 1U;
        assert(!AnnAdaptive_Apply(&cfg, true, true) && cfg.h == 2.5f && cfg.dp_pu == 200.0f);
    }
    Window(0.0f, 10.0f); AnnAdaptive_Service();
    Complete((2.0f - 3.2359364762792078f) / 2.368518667782116f, (0.3f - 0.8041675440903024f) / 0.46936755216588216f);
    AnnAdaptive_Service();
    assert(ANN_CTRL.result_valid && ANN_CTRL.result_status == ANN_RESULT_VALID && ANN_CTRL.L_est != L);
    L = ANN_CTRL.L_est; R = ANN_CTRL.R_est;
    Window(0.0f, 10.0f); AnnAdaptive_Service();
    ticks += 1000U; AnnAdaptive_Service();
    assert(ANN_CTRL.result_status == ANN_RESULT_INFERENCE_FAILED && ANN_CTRL.result_valid == 0U);
    assert(ANN_CTRL.L_est == L && ANN_CTRL.R_est == R);
    puts("PASS NPU: valid->rejected->recovered->timeout keeps finite last-valid estimates, explicit status, no stale J/D apply");
    puts("PASS NPU: 8 board-golden input quantizers/restoration, async mailbox, session cancellation, invalid input, timeout wrap, API fault");
    return 0;
}
