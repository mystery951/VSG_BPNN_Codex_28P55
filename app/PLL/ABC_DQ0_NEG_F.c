// ABC_DQ0_NEG_F.c
#include "ABC_DQ0_NEG_F.h"

//*********** Structure Init Function ****//
void ABC_DQ0_NEG_F_init(ABC_DQ0_NEG_F *v){
	v->a=0;
	v->b=0;
	v->c=0;
	v->alpha=0;
	v->beta=0;
	v->z=0;
	v->d=0;
	v->q=0;
}

//*********** Function Definition ********//
void ABC_DQ0_NEG_F_FUNC(ABC_DQ0_NEG_F *v)
{
	v->alpha = 0.6666666667f * (v->a - 0.5f * (v->b + v->c));
	v->beta = 0.57735026913f * (v->b - v->c);
	v->z = 0.3333333333f * (v->a + v->b + v->c);
	v->d=v->alpha*v->cos_val-v->beta*v->sin_val;
	v->q=v->alpha*v->sin_val+v->beta*v->cos_val;
}
