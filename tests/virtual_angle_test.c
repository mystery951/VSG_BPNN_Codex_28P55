#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "app/control/vsg.h"
#include "app/inverter.h"
#include "app/PLL/grid_pll.h"
#include "app/measurement/adc_measurement.h"
#include "app/adaptive/ann_adaptive.h"

static void TestLegacyMath(void)
{
    VSG_DEF v={0};
    float integral=0.0f,omega,wrapped;
    const float inputs[]={0.0f,0.014f,-0.014f,0.015f,-0.015f,0.016f,-0.016f,1.0f,-2.0f};
    unsigned k;
    for(k=0;k<100000;k++)
    {
        omega=inputs[k%9U];
        if(omega>0.015f||omega< -0.015f)integral+=omega*CONTROL_TS;
        VSG_VirtualAngleStep(&v,omega,CONTROL_TS,true);
        wrapped=atan2f(sinf(integral),cosf(integral));
        assert(v.virtual_angle_int==integral);
        assert(v.virtual_angle_rad==wrapped);
        assert(v.virtual_angle_deg==wrapped*57.29577951308232f&&v.virtual_angle_valid==1U);
    }
    VSG_VirtualAngleStep(&v,100.0f,CONTROL_TS,false);
    assert(v.virtual_angle_int==0.0f&&v.virtual_angle_deg==0.0f&&v.virtual_angle_valid==0U);
    for(k=0;k<100000;k++)VSG_VirtualAngleStep(&v,-3.14159265359f,CONTROL_TS,true);
    assert(v.virtual_angle_int < -30.0f); // Deliberately NO frequency-step compensation or integral clipping.
    assert(v.virtual_angle_rad>=-3.141593f&&v.virtual_angle_rad<=3.141593f);
    puts("PASS legacy angle: exact +/-0.015 deadband, original accumulation/wrap, disabled reset, sustained frequency error accumulates");
}

static void TestCompatibilityHasNoCompensation(void)
{
    VSG_DEF v={0};
    unsigned k;float expected=0;
    for(k=0;k<10000;k++)
    {
        v.omega_m=0.2f;expected+=0.2f*CONTROL_TS;
        VSG_VirtualAngleUpdate(&v,(float)k,NAN,CONTROL_TS,true,false,(uint16_t)(k%3));
        assert(v.virtual_angle_int==expected&&v.virtual_angle_mode==0U);
        assert(!VSG_VirtualAngleEstablish(&v,&INV_PARAM.virtual_angle,CONTROL_TS,true,50,1,0,1,0));
        assert(v.angle_base.ready==0U&&v.virtual_angle_pcc_int==0.0f);
    }
    puts("PASS rollback compatibility: old modes/PCC values cannot reactivate compensation or baseline capture");
}

static void TestProductionControllerAndAnn(void)
{
    float expected,omegaBefore,thetaBefore;
    unsigned k;
    Inverter_Init();
    assert(INV_CMD.virtual_angle_mode==0U);
    INV_CMD.auto_test_enable=0U;
    INV_CMD.run_enable=INV_CMD.grid_connect_cmd=INV_CMD.ann_enable=1U;
    INV_CMD.p_ref=1.0f;
    INV_CTRL.state=INV_CTRL.last_state=INV_GRID_CONNECTED;
    INV_PARAM.vsg.Kp_QV=INV_PARAM.vsg.Ki_QV=0.0f;
    VSG_EnterGrid(&VSG_CTRL,&INV_PARAM.vsg,TWO_PI*50.0f,-0.0207f,155.56f,0.0f);
    gridPllValid=gridPllLocked=1U;
    Va_cap=Vb_cap=Vc_cap=PCC_ua=PCC_ub=PCC_uc=0.0f;
    Ia_inv=Ib_inv=Ic_inv=Ia_filter=Ib_filter=Ic_filter=0.0f;
    for(k=0;k<2000;k++)
    {
        expected=VSG_CTRL.virtual_angle_int;thetaBefore=INV_CTRL.theta;
        INV_CMD.virtual_angle_mode=(uint16_t)(k%3); // Retained field is intentionally ignored.
        grid_theta=(k&1U)?NAN:2.5f;
        Inverter_ControlStep();
        if(VSG_CTRL.omega_m>0.015f||VSG_CTRL.omega_m< -0.015f)expected+=VSG_CTRL.omega_m*CONTROL_TS;
        assert(VSG_CTRL.virtual_angle_int==expected&&VSG_CTRL.virtual_angle_mode==0U);
        assert(VSG_CTRL.virtual_angle_valid==1U&&VSG_CTRL.angle_base.ready==0U);
        omegaBefore=VSG_CTRL.omega0+VSG_CTRL.omega_sync+VSG_CTRL.omega_m;
        assert(INV_CTRL.omega==omegaBefore);
        assert(fabsf(atan2f(sinf(INV_CTRL.theta-thetaBefore),cosf(INV_CTRL.theta-thetaBefore))-omegaBefore*CONTROL_TS)<0.00001f);
        if((k+1U)%ANN_AVERAGE_SAMPLES==0U)
        {
            assert(ANN_CTRL.ann_run_flag==1U);Inverter_Background();
        }
    }
    assert(ANN_CTRL.inference_count>0U);
    INV_CMD.p_ref=0.0f;Inverter_ControlStep();
    assert(VSG_CTRL.virtual_angle_int==0.0f&&VSG_CTRL.virtual_angle_valid==0U);
    INV_CMD.run_enable=0U;Inverter_ControlStep();
    assert(INV_CTRL.state==INV_STANDBY&&VSG_CTRL.virtual_angle_int==0.0f);
    puts("PASS production legacy-only path, ANN sampling stays active, PCC/mode ignored, VSG frequency/PWM angle unchanged, stop reset");
}

void TestVirtualAngleModes(void)
{
    TestLegacyMath();TestCompatibilityHasNoCompensation();TestProductionControllerAndAnn();
}
