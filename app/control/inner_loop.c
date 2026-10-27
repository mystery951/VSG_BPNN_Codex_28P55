#include "inner_loop.h"
#include "app/measurement/feedback_dq.h"
#include "app/math/pi_controller.h"

static PI_POS_DEF PI_UD, PI_UQ, PI_ID, PI_IQ;
volatile INNER_LOOP_DEF INNER_CTRL = {0};

static float Limit(float value, float limit)
{
    if(value > limit) value = limit;
    if(value < -limit) value = -limit;
    return value;
}

void InnerLoop_Init(const volatile INNER_PARAM *p)
{
    PI_Pos_Init(&PI_UD, p->Kpu, p->Kiu, p->U_UpLimit, -p->U_UpLimit);
    PI_Pos_Init(&PI_UQ, p->Kpu, p->Kiu, p->U_UpLimit, -p->U_UpLimit);
    PI_Pos_Init(&PI_ID, p->Kpi, p->Kii, p->I_UpLimit, -p->I_UpLimit);
    PI_Pos_Init(&PI_IQ, p->Kpi, p->Kii, p->I_UpLimit, -p->I_UpLimit);
    InnerLoop_Reset();
}

void InnerLoop_Reset(void)
{
    PI_Pos_Reset(&PI_UD); PI_Pos_Reset(&PI_UQ);
    PI_Pos_Reset(&PI_ID); PI_Pos_Reset(&PI_IQ);
    INNER_CTRL.id_ref = INNER_CTRL.iq_ref = 0.0f;
    INNER_CTRL.ud_ref = INNER_CTRL.uq_ref = 0.0f;
}

void InnerLoop_Calc(const volatile INNER_PARAM *p, const volatile DQ_FEEDBACK_DEF *f, float vdRef, float vqRef, float omega, float Vdc, float Ts)
{
    PI_UD.kp = PI_UQ.kp = p->Kpu;
    PI_UD.ki = PI_UQ.ki = p->Kiu;
    PI_ID.kp = PI_IQ.kp = p->Kpi;
    PI_ID.ki = PI_IQ.ki = p->Kii;
    PI_UD.uplimit = PI_UQ.uplimit = p->U_UpLimit;
    PI_UD.lolimit = PI_UQ.lolimit = -p->U_UpLimit;
    PI_ID.uplimit = PI_IQ.uplimit = p->I_UpLimit;
    PI_ID.lolimit = PI_IQ.lolimit = -p->I_UpLimit;
    INNER_CTRL.id_ref = Limit(PI_Pos_Cal(&PI_UD, vdRef - f->Vd_cap, Ts) + f->Id_filter - omega * p->Cf * f->Vq_cap, p->U_UpLimit);
    INNER_CTRL.iq_ref = Limit(PI_Pos_Cal(&PI_UQ, vqRef - f->Vq_cap, Ts) + f->Iq_filter + omega * p->Cf * f->Vd_cap, p->U_UpLimit);
    INNER_CTRL.ud_ref = PI_Pos_Cal(&PI_ID, INNER_CTRL.id_ref - f->Id_inv, Ts) + f->Vd_cap - omega * p->Lf * f->Iq_inv;
    INNER_CTRL.uq_ref = PI_Pos_Cal(&PI_IQ, INNER_CTRL.iq_ref - f->Iq_inv, Ts) + f->Vq_cap + omega * p->Lf * f->Id_inv;
    INNER_CTRL.ud_ref = Limit(INNER_CTRL.ud_ref * 2.0f / Vdc, MODULATION_LIMIT);
    INNER_CTRL.uq_ref = Limit(INNER_CTRL.uq_ref * 2.0f / Vdc, MODULATION_LIMIT);
}
