#include "pi_controller.h"
// Generic math module: independent of inverter state and hardware.

void PI_Pos_Reset(PI_POS_DEF *v)
{
    v->ek = 0.0f;
    v->uk = 0.0f;
    v->ui = 0.0f;
}

void PI_Pos_Init(PI_POS_DEF *v, float kp, float ki, float upper, float lower)
{
    PI_Pos_Reset(v);
    v->kp = kp;
    v->ki = ki;
    v->uplimit = upper;
    v->lolimit = lower;
}

float PI_Pos_Cal(PI_POS_DEF *v, float error, float sampleTime)
{
    v->ek = error;
    v->ui += v->ki * sampleTime * v->ek;

    if(v->ui > 2.0f * v->uplimit)
    {
        v->ui = 2.0f * v->uplimit;
    }
    if(v->ui < 2.0f * v->lolimit)
    {
        v->ui = 2.0f * v->lolimit;
    }

    v->uk = v->kp * v->ek + v->ui;
    if(v->uk > v->uplimit)
    {
        v->uk = v->uplimit;
    }
    if(v->uk < v->lolimit)
    {
        v->uk = v->lolimit;
    }
    return v->uk;
}
