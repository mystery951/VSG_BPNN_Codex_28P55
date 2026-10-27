#ifndef INNER_LOOP_H_
#define INNER_LOOP_H_
#include "app/inverter_settings.h"
#include "app/measurement/feedback_dq.h"

typedef struct { float id_ref, iq_ref, ud_ref, uq_ref; } INNER_LOOP_DEF;
extern volatile INNER_LOOP_DEF INNER_CTRL;
void InnerLoop_Init(const volatile INNER_PARAM *p);
void InnerLoop_Reset(void);
void InnerLoop_Calc(const volatile INNER_PARAM *p, const volatile DQ_FEEDBACK_DEF *f, float vdRef, float vqRef, float omega, float Vdc, float Ts);
#endif
