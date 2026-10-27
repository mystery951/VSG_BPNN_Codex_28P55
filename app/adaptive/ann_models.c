#include "ann_models.h"
#include "ann_weights.h"
#include <math.h>
#include <stdint.h>

// The model's training base is fixed, independent of operating frequency.
#define ANN_OMEGA_BASE 314.15926f
#define ANN_POWER_BASE 5000.0f

typedef struct
{
    const float (*w1)[2];
    const float *b1;
    const float (*w2)[30];
    const float *b2;
    const float (*w3)[20];
    const float *b3;
} ANN_MODEL;

static const ANN_MODEL constV = { ANN_W1, ANN_B1, ANN_W2, ANN_B2, ANN_W3, ANN_B3 };
static const ANN_MODEL constQ = { ANN_W1_Q, ANN_B1_Q, ANN_W2_Q, ANN_B2_Q, ANN_W3_Q, ANN_B3_Q };
static const ANN_MODEL droop = { ANN_DROOP_W1, ANN_DROOP_B1, ANN_DROOP_W2, ANN_DROOP_B2, ANN_DROOP_W3, ANN_DROOP_B3 };
static const ANN_MODEL jd = { ANN2_W1, ANN2_B1, ANN2_W2, ANN2_B2, ANN2_W3, ANN2_B3 };

static float Tansig(float x, bool jdModel)
{
    float e;
    // Preserve both legacy activation expressions and JD saturation exactly.
    if(!jdModel) return 2.0f / (1.0f + expf(-2.0f * x)) - 1.0f;
    if(x > 20.0f) return 1.0f;
    if(x < -20.0f) return -1.0f;
    e = expf(-2.0f * x);
    return (1.0f - e) / (1.0f + e);
}

static void Forward(const ANN_MODEL *model, float x0, float x1, bool jdModel, float y[2])
{
    float a1[30], a2[20], sum;
    uint16_t i, j;
    for(i = 0U; i < 30U; i++)
    {
        sum = model->b1[i] + model->w1[i][0] * x0 + model->w1[i][1] * x1;
        a1[i] = Tansig(sum, jdModel);
    }
    for(i = 0U; i < 20U; i++)
    {
        sum = model->b2[i];
        for(j = 0U; j < 30U; j++) sum += model->w2[i][j] * a1[j];
        a2[i] = Tansig(sum, jdModel);
    }
    for(i = 0U; i < 2U; i++)
    {
        sum = model->b3[i];
        for(j = 0U; j < 20U; j++) sum += model->w3[i][j] * a2[j];
        y[i] = sum;
    }
}

void ANN_ConstV_Predict(float Q, float angle, float *L, float *R)
{
    float y[2];
    float x0 = (Q - ANN_Q_MIN) * 0.0001866342898f - 1.0f;
    float x1 = (angle - ANN_ANGLE_MIN) * 0.08500835207f - 1.0f;
    Forward(&constV, x0, x1, false, y);
    if(L != 0) *L = (y[0] + 1.0f) * 4.5f + 1.0f;
    if(R != 0) *R = (y[1] + 1.0f) * 0.435f + 0.1f;
}

static void VoltagePredict(const ANN_MODEL *model, float x0, float x1, float *L, float *R)
{
    float y[2];
    Forward(model, x0, x1, false, y);
    if(L != 0) *L = (y[0] + 1.0f) * 4.5f + 1.0f;
    if(R != 0) *R = (y[1] + 1.0f) * 0.435f + 0.1f;
}
void ANN_ConstQ_Predict(float angle, float V, float *L, float *R)
{
    VoltagePredict(&constQ, (angle - 2.2058f) * 0.0745f - 1.0f, (V - (-17.2162f)) * 0.0561f - 1.0f, L, R);
}
void ANN_Droop_Predict(float angle, float V, float *L, float *R)
{
    VoltagePredict(&droop, (angle - 2.5438f) * 0.0850879f - 1.0f, (V - (-1.7165f)) * 0.1646f - 1.0f, L, R);
}
void ANN_ImpedancePredict(uint16_t mode, float Q, float angle, float V, float *L, float *R)
{
    if(mode == 0U) ANN_ConstV_Predict(Q, angle, L, R);
    else if(mode == 1U) ANN_ConstQ_Predict(angle, V, L, R);
    else if(mode == 2U) ANN_Droop_Predict(angle, V, L, R);
    else { if(L != 0) *L = NAN; if(R != 0) *R = NAN; }
}
bool ANN_ModeInputInRange(uint16_t mode, float Q, float angle, float V)
{
    if(mode == 0U) return ANN_InputInRange(Q, angle);
    if(mode == 1U) return angle >= 2.2058f && angle <= 29.048f && V >= -17.2162f && V <= 18.4114f;
    if(mode == 2U) return angle >= 2.5438f && angle <= 26.0489f && V >= -1.7165f && V <= 10.4307f;
    return false;
}

void VSG_JD_ANN2_Predict(float Rg, float Lg, float *J_est, float *D_est)
{
    float y[2];
    float x0 = (Rg - ANN2_INPUT_XOFFSET[0]) * ANN2_INPUT_GAIN[0] + ANN2_INPUT_YMIN[0];
    float x1 = (Lg - ANN2_INPUT_XOFFSET[1]) * ANN2_INPUT_GAIN[1] + ANN2_INPUT_YMIN[0];
    Forward(&jd, x0, x1, true, y);
    if(J_est != 0)
    {
        *J_est = (y[0] - ANN2_OUTPUT_YMIN[0]) / ANN2_OUTPUT_GAIN[0] + ANN2_OUTPUT_XOFFSET[0];
        *J_est = *J_est * ANN_OMEGA_BASE * ANN_OMEGA_BASE / ANN_POWER_BASE;
    }
    if(D_est != 0)
    {
        *D_est = (y[1] - ANN2_OUTPUT_YMIN[0]) / ANN2_OUTPUT_GAIN[1] + ANN2_OUTPUT_XOFFSET[1];
        *D_est = *D_est * ANN_OMEGA_BASE * ANN_OMEGA_BASE / ANN_POWER_BASE;
    }
}

bool ANN_InputInRange(float Q, float angle)
{
    return Q >= ANN_Q_MIN && Q <= ANN_Q_MAX && angle >= ANN_ANGLE_MIN && angle <= ANN_ANGLE_MAX;
}

bool ANN_ImpedanceInRange(float L, float R)
{
    return L >= 1.0f && L <= 10.0f && R >= 0.1f && R <= 0.97f;
}
