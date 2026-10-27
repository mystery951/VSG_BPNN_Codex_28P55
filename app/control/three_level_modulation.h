#ifndef THREE_LEVEL_MODULATION_H_
#define THREE_LEVEL_MODULATION_H_

typedef struct { float upper; float lower; } ThreeLevelDuty;
typedef struct
{
    float VaRef, VbRef, VcRef;
    ThreeLevelDuty duty[3]; // A=PWM1/2, B=PWM3/4, C=PWM5/6.
} MODULATION_DEF;
extern MODULATION_DEF MODULATION_OUT;
void Modulation_Reset(void);
void Modulation_Update(float ud, float uq, float thetaPwm);
void ThreeLevelModulation_Map(float modulation, ThreeLevelDuty *duty);

#endif
