#ifndef FEEDBACK_DQ_H_
#define FEEDBACK_DQ_H_
#include "app/system_config.h"
typedef struct
{
    float Vd_cap, Vq_cap, Id_inv, Iq_inv, Id_filter, Iq_filter;
    float Pe, Qe, Pe_pu, Qe_pu;
} DQ_FEEDBACK_DEF;
extern volatile DQ_FEEDBACK_DEF INV_FEEDBACK;
void FeedbackDq_Init(void);
void FeedbackDq_Update(float theta);
void FeedbackDq_CalcPower(void);
#endif
