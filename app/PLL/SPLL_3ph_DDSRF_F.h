#ifndef SPLL_3ph_DDSRF_F_H_
#define SPLL_3ph_DDSRF_F_H_

#include "pll_types.h"   //定义数据类型

//*********** Structure Definition ********//
typedef struct{
	float32	B1_lf;
	float32	B0_lf;
	float32	A1_lf;
}SPLL_3ph_DDSRF_F_LPF_COEFF;

typedef struct{
	float32 d_p;
	float32 d_n;
	float32 q_p;
	float32 q_n;

	float32 d_p_decoupl;
	float32 d_n_decoupl;
	float32 q_p_decoupl;
	float32 q_n_decoupl;

	float32 cos_2theta;
	float32 sin_2theta;

	float32 y[2];
	float32 x[2];
	float32 w[2];
	float32 z[2];
	float32 k1;
	float32 k2;
	float32 d_p_decoupl_lpf;
	float32 d_n_decoupl_lpf;
	float32 q_p_decoupl_lpf;
	float32 q_n_decoupl_lpf;

	float32 v_q[2];
	float32 theta[2];
	float32 ylf[2];
	float32 fo;
	float32 fn;
	float32 delta_T;
	SPLL_3ph_DDSRF_F_LPF_COEFF lpf_coeff;
}SPLL_3ph_DDSRF_F;
//*********** Function Declarations *******//
void SPLL_3ph_DDSRF_F_init(int Grid_freq, float32 DELTA_T, float32 k1, float32 k2, SPLL_3ph_DDSRF_F *spll);
void SPLL_3ph_DDSRF_F_FUNC(SPLL_3ph_DDSRF_F *spll_obj);

// Keep the legacy entry point, with identical normalization and anti-windup.
#define SPLL_3ph_DDSRF_F_MACRO(spll_obj) SPLL_3ph_DDSRF_F_FUNC(&(spll_obj))


#endif /* SPLL_3ph_DDSRF_F_H_ */
