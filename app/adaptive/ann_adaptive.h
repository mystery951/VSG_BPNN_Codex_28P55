#ifndef ANN_ADAPTIVE_H_
#define ANN_ADAPTIVE_H_

#include <stdbool.h>
#include <stdint.h>
#include "app/inverter_settings.h"

#define ANN_AVERAGE_SAMPLES 100U

typedef enum
{
    ANN_RESULT_DISABLED = 0, ANN_RESULT_WAITING, ANN_RESULT_VALID,
    ANN_RESULT_INPUT_RANGE, ANN_RESULT_IMPEDANCE_RANGE,
    ANN_RESULT_JD_INVALID, ANN_RESULT_INFERENCE_FAILED
} ANN_RESULT_STATUS;

typedef struct
{
    uint16_t ann_cnt, ann_run_flag, result_valid, input_in_range, impedance_in_range; // Range flags are diagnostic only.
    uint16_t result_status, has_valid_result;
    float ann_Q_input, ann_Angle_input, ann_V_input; // V is legacy Q-loop correction in volts, not pu or total voltage.
    uint16_t model_mode; // 0 ConstV, 1 ConstQ, 2 Droop; model used for latest published result.
    float L_est, R_est, J_apt, D_apt; // Latest raw inference, including invalid values; zero before first inference.
    uint32_t inference_count, apply_count;
} ANN_ADAPTIVE_DEF;

// Read-only monitoring. Operator inputs stay in INV_CMD.
extern volatile ANN_ADAPTIVE_DEF ANN_CTRL;
extern volatile uint16_t annJdApplyEnable; // 0 blocks all ANN J/D writes; prediction/CAN remain active.
void AnnAdaptive_Init(void);
// ISR: ConstV compatibility wrapper; voltage-feature modes must call SampleInputs.
void AnnAdaptive_Sample(bool enabled, float Q, float angle);
// All modes: average Q(var), angle(deg), legacy voltage correction(V) over the same window.
void AnnAdaptive_SampleInputs(bool enabled, float Q, float angle, float voltageCorrection);
// Main loop only: original CPU float32 ANN + J/D network, without training-range gates.
void AnnAdaptive_Service(void);
// ISR only. Manual can reapply a valid result; automatic applies each new one once.
bool AnnAdaptive_Apply(volatile VSG_PARAM *cfg, bool automatic, bool manual);

#endif
