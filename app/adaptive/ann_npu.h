#ifndef ANN_NPU_H_
#define ANN_NPU_H_
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint16_t enabled, available, busy, fault;
    uint16_t status; /* 0 idle, 1 busy, 2 complete, 3 input reject, 4 timeout, 5 API error */
    int32_t api_result;
    uint32_t started, completed, rejected, failures, elapsed_ticks;
    float Q, angle, normalized_output[2], L_mH, R_ohm;
    int16_t quantized_input[16];
} ANN_NPU_MONITOR;

extern volatile ANN_NPU_MONITOR ANN_NPU_CTRL;
/* Init once at boot, after Board_init. Never reset a live hardware inference. */
void AnnNpu_Init(void);
/* Mirrors the TI ONNX scalar input quantizer: floor(x*64+0.5), saturate. */
int16_t AnnNpu_Quantize(float normalized);
bool AnnNpu_Start(float Q, float angle);
/* Background only: 0 still running, 1 complete, -1 failed. */
int AnnNpu_Poll(float *L_mH, float *R_ohm);
#endif
