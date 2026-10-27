#ifndef ANN_MODELS_H_
#define ANN_MODELS_H_

#include <stdbool.h>
#include <stdint.h>

// Historical training/normalization domain, NOT runtime input restrictions.
#define ANN_Q_MIN       (-9746.9f)
#define ANN_Q_MAX       (969.2444f)
#define ANN_ANGLE_MIN   (2.6946f)
#define ANN_ANGLE_MAX   (26.2217f)

// Exact F28377D model contracts: L is mH; R is ohm.
void ANN_ConstV_Predict(float Q, float angle, float *L, float *R);
void ANN_ConstQ_Predict(float angle, float V, float *L, float *R);
void ANN_Droop_Predict(float angle, float V, float *L, float *R);
void ANN_ImpedancePredict(uint16_t mode, float Q, float angle, float V, float *L, float *R);
bool ANN_ModeInputInRange(uint16_t mode, float Q, float angle, float V);
// Rg is ohm, Lg is H. Outputs are already normalized m_pu=2H and dp_pu.
void VSG_JD_ANN2_Predict(float Rg, float Lg, float *J_est, float *D_est);
bool ANN_InputInRange(float Q, float angle);
bool ANN_ImpedanceInRange(float L, float R);

#endif
