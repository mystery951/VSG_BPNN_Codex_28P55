#ifndef ABC_DQ0_NEG_F_H
#define ABC_DQ0_NEG_F_H

#include "pll_types.h"   //定义数据类型

//*********** Structure Definition ********//
typedef struct{
	float32 a;
	float32 b;
	float32 c;
	float32 alpha;
	float32 beta;
	float32 sin_val;
	float32 cos_val;
	float32 d;
	float32 q;
	float32 z;
}ABC_DQ0_NEG_F;

//*********** Function Declarations *******//
void ABC_DQ0_NEG_F_init(ABC_DQ0_NEG_F *v);
void ABC_DQ0_NEG_F_FUNC(ABC_DQ0_NEG_F *v);

//*********** Macro Definition ***********//
#define ABC_DQ0_NEG_F_MACRO(v) 						\
	v.alpha=(0.6666666667)*(v.a - 0.5*(v.b + v.c));	\
	v.beta=(0.57735026913)*(v.b - v.c);				\
	v.z =0.57735026913*(v.a + v.b + v.c);			\
	v.d=v.alpha*v.cos_val - v.beta*v.sin_val;				\
	v.q=v.alpha*v.sin_val + v.beta*v.cos_val;


#endif /* ABC_DQ0_NEG_F_H_ */
