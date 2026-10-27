#include "three_level_modulation.h"
#include "app/system_config.h"
#include <math.h>
#include <stdint.h>

MODULATION_DEF MODULATION_OUT = {0};

static float Limit(float x)
{
    if(x > MODULATION_LIMIT) x = MODULATION_LIMIT;
    if(x < -MODULATION_LIMIT) x = -MODULATION_LIMIT;
    return x;
}

void Modulation_Reset(void)
{
    uint16_t i;
    MODULATION_OUT.VaRef = MODULATION_OUT.VbRef = MODULATION_OUT.VcRef = 0.0f;
    for(i = 0U; i < 3U; i++) MODULATION_OUT.duty[i].upper = MODULATION_OUT.duty[i].lower = 0.0f;
}

void Modulation_Update(float ud, float uq, float thetaPwm)
{
    float sine = sinf(thetaPwm), cosine = cosf(thetaPwm);
    float alpha = ud * cosine - uq * sine;
    float beta = ud * sine + uq * cosine;
    MODULATION_OUT.VaRef = Limit(alpha);
    MODULATION_OUT.VbRef = Limit(-0.5f * alpha + 0.86602540378f * beta);
    MODULATION_OUT.VcRef = Limit(-0.5f * alpha - 0.86602540378f * beta);
    ThreeLevelModulation_Map(MODULATION_OUT.VaRef, &MODULATION_OUT.duty[0]);
    ThreeLevelModulation_Map(MODULATION_OUT.VbRef, &MODULATION_OUT.duty[1]);
    ThreeLevelModulation_Map(MODULATION_OUT.VcRef, &MODULATION_OUT.duty[2]);
}

void ThreeLevelModulation_Map(float modulation, ThreeLevelDuty *duty)
{
    // Existing three-level carrier disposition: upper=max(m,0), lower=min(m+1,1).
    if(modulation > 1.0f) modulation = 1.0f;
    if(modulation < -1.0f) modulation = -1.0f;
    duty->upper = (modulation > 0.0f) ? modulation : 0.0f;
    duty->lower = (modulation < 0.0f) ? modulation + 1.0f : 1.0f;
}
