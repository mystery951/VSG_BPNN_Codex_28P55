#ifndef PI_CONTROLLER_H_
#define PI_CONTROLLER_H_

// Shared positional PI; preserves the validated integral/output limiting.
typedef struct
{
    float ek;
    float uk;
    float ui;
    float ki;
    float kp;
    float uplimit;
    float lolimit;
} PI_POS_DEF;

void PI_Pos_Init(PI_POS_DEF *v, float kp, float ki, float upper, float lower);
void PI_Pos_Reset(PI_POS_DEF *v);
float PI_Pos_Cal(PI_POS_DEF *v, float error, float sampleTime);

#endif
