#include "feedback_dq.h"
#include "adc_measurement.h"
#include "app/PLL/ABC_DQ0_POS_F.h"
#include <math.h>

static ABC_DQ0_POS_F DQ0_POS_VCAP, DQ0_POS_IINV, DQ0_POS_IFILTER;
volatile DQ_FEEDBACK_DEF INV_FEEDBACK = {0};

void FeedbackDq_Init(void)
{
    INV_FEEDBACK.Vd_cap = INV_FEEDBACK.Vq_cap = INV_FEEDBACK.Id_inv = INV_FEEDBACK.Iq_inv = INV_FEEDBACK.Id_filter = INV_FEEDBACK.Iq_filter = 0.0f;
    INV_FEEDBACK.Pe = INV_FEEDBACK.Qe = INV_FEEDBACK.Pe_pu = INV_FEEDBACK.Qe_pu = 0.0f;
    ABC_DQ0_POS_F_init(&DQ0_POS_VCAP);
    ABC_DQ0_POS_F_init(&DQ0_POS_IINV);
    ABC_DQ0_POS_F_init(&DQ0_POS_IFILTER);
}

static void Transform(ABC_DQ0_POS_F *v, float a, float b, float c, float sine, float cosine)
{
    v->a = a;
    v->b = b;
    v->c = c;
    v->sin_val = sine;
    v->cos_val = cosine;
    ABC_DQ0_POS_F_FUNC(v);
}

void FeedbackDq_Update(float theta)
{
    float sine = sinf(theta), cosine = cosf(theta);
    Transform(&DQ0_POS_VCAP, Va_cap, Vb_cap, Vc_cap, sine, cosine);
    Transform(&DQ0_POS_IINV, Ia_inv, Ib_inv, Ic_inv, sine, cosine);
    Transform(&DQ0_POS_IFILTER, Ia_filter, Ib_filter, Ic_filter, sine, cosine);
    INV_FEEDBACK.Vd_cap = DQ0_POS_VCAP.d;
    INV_FEEDBACK.Vq_cap = DQ0_POS_VCAP.q;
    INV_FEEDBACK.Id_inv = DQ0_POS_IINV.d;
    INV_FEEDBACK.Iq_inv = DQ0_POS_IINV.q;
    INV_FEEDBACK.Id_filter = DQ0_POS_IFILTER.d;
    INV_FEEDBACK.Iq_filter = DQ0_POS_IFILTER.q;
}

void FeedbackDq_CalcPower(void)
{
    INV_FEEDBACK.Pe = 1.5f * (INV_FEEDBACK.Vd_cap * INV_FEEDBACK.Id_filter + INV_FEEDBACK.Vq_cap * INV_FEEDBACK.Iq_filter);
    INV_FEEDBACK.Qe = 1.5f * (INV_FEEDBACK.Vq_cap * INV_FEEDBACK.Id_filter - INV_FEEDBACK.Vd_cap * INV_FEEDBACK.Iq_filter);
    INV_FEEDBACK.Pe_pu = INV_FEEDBACK.Pe / SYS_S_BASE_VA;
    INV_FEEDBACK.Qe_pu = INV_FEEDBACK.Qe / SYS_S_BASE_VA;
}
