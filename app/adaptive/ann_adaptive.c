#include "ann_adaptive.h"
#include "ann_models.h"
#include "app/control/reactive_control.h"
#include "driverlib.h"
#include <math.h>

typedef struct { float Q, angle, V; uint32_t session; uint16_t mode; } ANN_INPUT;

volatile ANN_ADAPTIVE_DEF ANN_CTRL = {0};
static volatile ANN_INPUT pending;
static volatile uint32_t session;
static volatile bool active;
static float qSum, angleSum, vSum;
static uint16_t sampleMode;
static uint32_t appliedResult;

volatile uint16_t annJdApplyEnable = 0U; // 0：禁止ANN更新J/D；1：允许更新

void AnnAdaptive_Init(void)
{
    const ANN_ADAPTIVE_DEF zero = {0};
    ANN_CTRL = zero;
    session = 0U;
    active = false;
    qSum = angleSum = vSum = 0.0f;
    sampleMode = Q_MODE_INACTIVE;
    appliedResult = 0U;
}

void AnnAdaptive_Sample(bool enabled, float Q, float angle)
{
    AnnAdaptive_SampleInputs(enabled, Q, angle, 0.0f); // ConstV compatibility API.
}

static uint16_t CurrentMode(void)
{
    return REACTIVE_CTRL.active_mode <= Q_MODE_DROOP ? REACTIVE_CTRL.active_mode : INV_CMD.q_mode;
}

static bool InputFinite(const ANN_INPUT *input)
{
    return input->mode <= Q_MODE_DROOP && isfinite(input->angle) &&
           (input->mode == Q_MODE_CONST_V ? isfinite(input->Q) : isfinite(input->V));
}

void AnnAdaptive_SampleInputs(bool enabled, float Q, float angle, float voltageCorrection)
{
    uint16_t mode = CurrentMode();
    enabled = enabled && mode <= Q_MODE_DROOP;
    if(!enabled || (active && sampleMode != mode))
    {
        if(active) session++; // Cancel inference started in the previous session.
        active = false;
        ANN_CTRL.ann_cnt = ANN_CTRL.ann_run_flag = ANN_CTRL.result_valid = 0U;
        ANN_CTRL.result_status = ANN_RESULT_DISABLED;
        qSum = angleSum = vSum = 0.0f;
        if(!enabled) return;
    }
    if(!active) ANN_CTRL.result_status = ANN_RESULT_WAITING;
    active = true;
    sampleMode = mode;
    qSum += Q;
    angleSum += angle;
    vSum += voltageCorrection;
    ANN_CTRL.ann_cnt++;
    if(ANN_CTRL.ann_cnt >= ANN_AVERAGE_SAMPLES)
    {
        // Latest complete window wins if the background has not taken the old one.
        pending.Q = qSum / (float)ANN_AVERAGE_SAMPLES;
        pending.angle = angleSum / (float)ANN_AVERAGE_SAMPLES;
        pending.V = vSum / (float)ANN_AVERAGE_SAMPLES;
        pending.mode = mode;
        pending.session = session;
        ANN_CTRL.ann_run_flag = 1U;
        ANN_CTRL.ann_cnt = 0U;
        qSum = angleSum = vSum = 0.0f;
    }
}

static void PublishResult(const ANN_INPUT *input, float L, float R)
{
    float J, D;
    bool wasDisabled, inputFinite, impedanceOk, valid;
    uint16_t resultStatus;
    J = D = NAN;
    inputFinite = InputFinite(input);
    if(inputFinite && isfinite(L) && isfinite(R)) VSG_JD_ANN2_Predict(R, L / 1000.0f, &J, &D);
    impedanceOk = isfinite(L) && isfinite(R) && L >= 0.0f && R >= 0.0f;
    valid = inputFinite && impedanceOk && isfinite(J) && isfinite(D) && J > 0.0f && D > 0.0f;
    if(!inputFinite) resultStatus = ANN_RESULT_INPUT_RANGE; // Legacy status code: nonfinite input only.
    else if(!isfinite(L) || !isfinite(R)) resultStatus = ANN_RESULT_INFERENCE_FAILED;
    else if(!impedanceOk) resultStatus = ANN_RESULT_IMPEDANCE_RANGE;
    else if(!valid) resultStatus = ANN_RESULT_JD_INVALID;
    else resultStatus = ANN_RESULT_VALID;

    wasDisabled = Interrupt_disableGlobal();
    if(active && input->session == session && input->mode == CurrentMode())
    {
        ANN_CTRL.ann_Q_input = input->Q;
        ANN_CTRL.ann_Angle_input = input->angle;
        ANN_CTRL.ann_V_input = input->V;
        ANN_CTRL.model_mode = input->mode;
        // Always publish this inference, including negative/NaN/Inf results.
        // CAN diagnostics must not hide failures behind previous valid values.
        // result_valid still prevents invalid predictions from entering control.
        ANN_CTRL.L_est = L;
        ANN_CTRL.R_est = R;
        ANN_CTRL.J_apt = J;
        ANN_CTRL.D_apt = D;
        if(valid) ANN_CTRL.has_valid_result = 1U;
        // Training-range indicators are read-only diagnostics, NEVER rejection gates.
        ANN_CTRL.input_in_range = ANN_ModeInputInRange(input->mode, input->Q, input->angle, input->V) ? 1U : 0U;
        ANN_CTRL.impedance_in_range = ANN_ImpedanceInRange(L, R) ? 1U : 0U;
        ANN_CTRL.result_valid = valid ? 1U : 0U;
        ANN_CTRL.result_status = resultStatus;
        ANN_CTRL.inference_count++;
    }
    if(!wasDisabled) Interrupt_enableGlobal();
}

void AnnAdaptive_Service(void)
{
    ANN_INPUT input;
    float L = NAN, R = NAN;
    bool wasDisabled;
    if(ANN_CTRL.ann_run_flag == 0U) return;
    wasDisabled = Interrupt_disableGlobal();
    if(wasDisabled) return;
    if(ANN_CTRL.ann_run_flag == 0U)
    {
        Interrupt_enableGlobal();
        return;
    }
    input.Q = pending.Q;
    input.angle = pending.angle;
    input.V = pending.V;
    input.mode = pending.mode;
    input.session = pending.session;
    ANN_CTRL.ann_run_flag = 0U;
    Interrupt_enableGlobal();

    // Original F28377D float32 networks, CPU only. No input clipping or quantization.
    if(InputFinite(&input)) ANN_ImpedancePredict(input.mode, input.Q, input.angle, input.V, &L, &R);
    PublishResult(&input, L, R);
}

bool AnnAdaptive_Apply(volatile VSG_PARAM *cfg, bool automatic, bool manual)
{
    uint16_t div = cfg->power_loop_div > 0U ? cfg->power_loop_div : 1U;
    float J = ANN_CTRL.J_apt, D = ANN_CTRL.D_apt;

    if(annJdApplyEnable == 0U || !ReactiveControl_AnnAllowed()) return false;

    if(!active || ANN_CTRL.model_mode != CurrentMode() || ANN_CTRL.result_valid != 1U || (!automatic && !manual)) return false;
    if(!manual && appliedResult == ANN_CTRL.inference_count) return false;
    // Minimum Euler damping condition: 0 < D*Ts/J < 2. Not a plant stability proof.
    if(!(J > 0.0f && D > 0.0f && D * CONTROL_TS * (float)div < 2.0f * J)) return false;
    cfg->h = J * 0.5f; // Old J_apt is m_pu=2H, NOT H or unscaled kg*m^2.
    cfg->dp_pu = D;
    appliedResult = ANN_CTRL.inference_count;
    ANN_CTRL.apply_count++;
    return true;
}
