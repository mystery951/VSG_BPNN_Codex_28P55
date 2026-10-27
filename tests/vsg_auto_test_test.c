#include <assert.h>
#include <stdio.h>
#include "app/control/vsg_auto_test.h"
#include "app/control/vsg.h"
#include "app/adaptive/ann_adaptive.h"
#include "app/inverter.h"
#include "app/PLL/grid_pll.h"
#include "app/measurement/adc_measurement.h"
#include "driverlib.h"
#define TICKS 10000U
static void RunTicks(unsigned n){while(n--)VsgAutoTest_Step(true,true);}
static void Predict(float q,float angle)
{
    unsigned i;
    for(i=0;i<ANN_AVERAGE_SAMPLES;i++)AnnAdaptive_Sample(true,q,angle);
    AnnAdaptive_Service();
}
static void Start(unsigned mode)
{
    Inverter_Init();
    INV_CTRL.state=INV_GRID_CONNECTED;
    INV_CMD.run_enable=INV_CMD.grid_connect_cmd=1U;
    RunTicks(2U*TICKS);
    assert(INV_CMD.p_ref==0.0f&&INV_CMD.ann_enable==0U&&VSG_TEST.mode==0U); // NEVER auto-start.
    assert(VsgAutoTest_Request((uint16_t)mode));
    assert(!VsgAutoTest_Request((uint16_t)mode));
    assert(VsgAutoTest_Step(true,true));
    assert(INV_CMD.p_ref==1.0f&&INV_CMD.ann_enable==1U&&VSG_TEST.elapsed_ticks==0U);
    assert(VsgAutoTest_Stamp(123456U));
    assert(VSG_TEST.start_tick==123456U&&!VsgAutoTest_Stamp(123457U));
    assert(!VsgAutoTest_Request(1U));
}
static void TestSequence(unsigned mode)
{
    float h,D,J1,D1,J2,D2;
    uint16_t status[8],id;
    Start(mode); h=INV_PARAM.vsg.h;D=INV_PARAM.vsg.dp_pu;
    RunTicks(89900U);Predict(0.0f,10.0f);
    assert(ANN_CTRL.result_valid==1U);
    J1=ANN_CTRL.J_apt;D1=ANN_CTRL.D_apt;
    RunTicks(100U);
    assert(VSG_TEST.seconds==9U&&INV_CMD.p_ref==1.0f);
    assert(VSG_TEST.apply_count==(mode==2U?1U:0U));
    assert(INV_PARAM.vsg.h==(mode==2U?J1*0.5f:h));
    assert(INV_PARAM.vsg.dp_pu==(mode==2U?D1:D));
    RunTicks(TICKS);assert(INV_CMD.p_ref==1.1f&&INV_CMD.grid_impedance_step==0U);
    RunTicks(5U*TICKS);assert(INV_CMD.p_ref==1.0f&&INV_CMD.grid_impedance_step==0U);
    RunTicks(5U*TICKS);
    assert(VSG_TEST.seconds==20U&&INV_CMD.p_ref==1.0f&&INV_CMD.grid_impedance_step==1U);
    assert(ANN_CTRL.result_valid==0U&&ANN_CTRL.ann_run_flag==0U);
    RunTicks(39900U);Predict(-1000.0f,15.0f);
    assert(ANN_CTRL.result_valid==1U);
    J2=ANN_CTRL.J_apt;D2=ANN_CTRL.D_apt;RunTicks(100U);
    assert(VSG_TEST.seconds==24U&&VSG_TEST.apply_count==(mode==2U?2U:0U));
    assert(INV_PARAM.vsg.h==(mode==2U?J2*0.5f:h));
    assert(INV_PARAM.vsg.dp_pu==(mode==2U?D2:D));
    RunTicks(TICKS);assert(INV_CMD.p_ref==1.1f);
    RunTicks(5U*TICKS);assert(INV_CMD.p_ref==1.0f);
    RunTicks(10U*TICKS);
    assert(VSG_TEST.seconds==40U&&VSG_TEST.state==VSG_TEST_COMPLETE);
    assert(INV_CMD.grid_impedance_step==1U&&INV_CMD.p_ref==1.0f);
    assert((VSG_TEST.flags&VSG_TEST_DONE)!=0U&&(VSG_TEST.flags&VSG_TEST_RUNNING)==0U);
    if(mode==2U)assert((VSG_TEST.flags&0x30U)==0x30U);
    else assert((VSG_TEST.flags&0xF0U)==0U);
    id=VSG_TEST.run_id;
    VsgAutoTest_BuildStatus(status);
    assert(status[0]==0x40U&&status[1]==0xE2U&&status[2]==1U&&status[4]==id&&status[6]==mode);
    RunTicks(50U*TICKS);assert(VSG_TEST.elapsed_ticks==400000UL&&!VsgAutoTest_Request(2U));
    VsgAutoTest_Step(false,true);
    assert(INV_CMD.p_ref==0.0f&&INV_CMD.grid_impedance_step==0U&&annJdApplyEnable==0U);
    INV_CTRL.state=INV_STANDBY;VsgAutoTest_Prepare();
    assert(INV_PARAM.vsg.h==h&&INV_PARAM.vsg.dp_pu==D&&VSG_TEST.mode==0U&&VSG_TEST.run_id==id);
    assert(!VsgAutoTest_Request(1U));
    INV_CTRL.state=INV_GRID_CONNECTED;assert(VsgAutoTest_Request(1U));VsgAutoTest_Step(true,true);
    assert(VSG_TEST.run_id==id+1U); // Prepare retains identity, no accidental repeat.
    printf("PASS 40s mode%u: wait/button/t0, 9/24s J/D, 10/25s up,15/30s down,20s GPIO,40s hold, reset baseline\n",mode);
}
static void TestInvalidAndStop(void)
{
    float h,D;
    Start(2);h=INV_PARAM.vsg.h;D=INV_PARAM.vsg.dp_pu;
    Predict(0,10);RunTicks(9U*TICKS); // Valid but stale by 9 s.
    assert(VSG_TEST.apply_count==0U&&(VSG_TEST.flags&VSG_TEST_FAILED_FIRST)!=0U);
    RunTicks(TICKS);assert(INV_CMD.p_ref==1.1f);
    RunTicks(10U*TICKS);assert(INV_CMD.grid_impedance_step==1U&&ANN_CTRL.result_valid==0U);
    RunTicks(4U*TICKS);
    assert((VSG_TEST.flags&VSG_TEST_FAILED_SECOND)!=0U&&VSG_TEST.apply_count==0U);
    RunTicks(TICKS);assert(INV_CMD.p_ref==1.1f);
    assert(INV_PARAM.vsg.h==h&&INV_PARAM.vsg.dp_pu==D);
    VsgAutoTest_Step(false,true);
    assert((VSG_TEST.flags&VSG_TEST_ABORTED)!=0U&&INV_CMD.p_ref==0.0f);
    Start(2);RunTicks(89999U);Predict(0,10);VsgAutoTest_Step(true,false);
    assert((VSG_TEST.flags&VSG_TEST_FAILED_FIRST)!=0U); // Invalid PLL, not an unscheduled retry.
    puts("PASS missing/stale/unlocked prediction keeps actual J/D, flags failed attempts, no schedule delay, stop aborts");
}
static void TestControlIntegration(void)
{
    unsigned i;
    Inverter_Init();INV_CTRL.state=INV_GRID_CONNECTED;
    INV_CMD.run_enable=INV_CMD.grid_connect_cmd=1U;gridPllValid=gridPllLocked=1U;
    VSG_EnterGrid(&VSG_CTRL,&INV_PARAM.vsg,TWO_PI*50.0f,0.0f,155.56f,0.0f);
    Va_cap=Vb_cap=Vc_cap=PCC_ua=PCC_ub=PCC_uc=0.0f;
    Ia_inv=Ib_inv=Ic_inv=Ia_filter=Ib_filter=Ic_filter=0.0f;
    for(i=0;i<TICKS;i++)Inverter_ControlStep();
    assert(INV_CMD.p_ref==0.0f&&VSG_TEST.mode==0U);
    assert(VsgAutoTest_Request(1U));Inverter_ControlStep();
    assert(INV_CMD.p_ref==1.0f&&ANN_CTRL.ann_cnt==0U);
    Inverter_ControlStep();assert(ANN_CTRL.ann_cnt==1U);
    INV_CMD.run_enable=0U;Inverter_ControlStep();
    assert(INV_CTRL.state==INV_STANDBY&&(VSG_TEST.flags&VSG_TEST_ABORTED)!=0U);
    puts("PASS real controller waits for button, starts fresh ANN window, master stop cancels trial");
}
extern void TestReactiveControl(void);
void TestVsgAutoTest(void){TestSequence(1);TestSequence(2);TestInvalidAndStop();TestControlIntegration();TestReactiveControl();}
