#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "app/inverter.h"
#include "app/control/reactive_control.h"
#include "app/control/vsg.h"
#include "app/control/vsg_auto_test.h"
#include "app/communication/can_control.h"
#include "app/adaptive/ann_adaptive.h"
static void NearQ(float a,float b){assert(fabsf(a-b)<0.0002f);}
void TestAnnReactiveModels(void);
void TestReactiveControl(void)
{
    unsigned mode,i;
    uint16_t data[8],command[4]={0U,0xA5U,0x5AU,0U},revision;
    VSG_DEF v={0}; float voltage;
    for(mode=0U;mode<3U;mode++)
    {
        Inverter_Init();
        INV_CMD.Vd_ref=143.0f;
        command[0]=(uint16_t)(0x10U+mode);
        CanControl_Receive(0x200U,command,4U,true);CanControl_ProcessPending();
        assert(cana_command_result==CANA_COMMAND_RESULT_ACCEPTED&&INV_CMD.q_mode==mode);
        NearQ(INV_PARAM.vsg.Kp_QV,mode==2U?10.0f/SYS_V_BASE_V:0.0f);
        NearQ(INV_PARAM.vsg.Ki_QV,mode==1U?40.0f/SYS_V_BASE_V:0.0f);
        assert(INV_CMD.Vd_ref==143.0f&&REACTIVE_CTRL.active_mode==255U); // Presync/VF not touched.
        revision=REACTIVE_CTRL.revision;
        INV_CMD.grid_connect_cmd=1U;
        assert(!ReactiveControl_Select((uint16_t)((mode+1U)%3U)));
        assert(REACTIVE_CTRL.revision==revision&&INV_CMD.q_mode==mode);
        INV_CTRL.state=INV_GRID_CONNECTED;ReactiveControl_EnterGrid();
        assert(REACTIVE_CTRL.active_mode==mode);
        assert(ReactiveControl_AnnAllowed());
        assert(!ReactiveControl_Select(0U));
        ReactiveControl_BuildStatus(data);
        assert(data[0]==1U&&data[1]==mode&&data[2]==mode&&data[3]==1U&&data[6]==2U);
        VSG_EnterGrid(&v,&INV_PARAM.vsg,TWO_PI*50.0f,0.0f,150.0f,0.03f);
        voltage=VSG_PowerLoop(&v,&INV_PARAM.vsg,0,0.03f,0,0,190.0f,0.001f);
        if(mode==0U)NearQ(voltage,150.0f);
        if(mode==1U)NearQ(voltage,150.0f+40.0f*0.001f*0.03f); // Same physical V increment as 28377D.
        if(mode==2U)
        {
            NearQ(voltage,150.0f); // Proportional branch starts at existing voltage, not a jump.
            voltage=VSG_PowerLoop(&v,&INV_PARAM.vsg,0,0.04f,0,0,190.0f,0.001f);
            NearQ(voltage,150.0f+10.0f*(0.04f-0.03f)); // 50var change => 0.1V, not 15.556V.
            for(i=0;i<1000U;i++)NearQ(VSG_PowerLoop(&v,&INV_PARAM.vsg,0,0.04f,0,0,190.0f,0.001f),voltage);
            VSG_EnterGrid(&v,&INV_PARAM.vsg,TWO_PI*50.0f,0.0f,150.0f,0.2f);
            NearQ(VSG_PowerLoop(&v,&INV_PARAM.vsg,0,0.2f,0,0,190,0.001f),150.0f); // Large anchor must not be clipped as PI windup.
        }
        ReactiveControl_LeaveGrid();INV_CTRL.state=INV_STANDBY;INV_CMD.grid_connect_cmd=0U;
        VsgAutoTest_Prepare();
        assert(INV_CMD.q_mode==mode&&REACTIVE_CTRL.active_mode==255U); // A/B restart preserves selection.
    }
    Inverter_Init();assert(ReactiveControl_Select(1U));ReactiveControl_EnterGrid();
    VSG_EnterGrid(&v,&INV_PARAM.vsg,TWO_PI*50.0f,0.0f,150,0);
    for(i=0;i<10000U;i++)
    {
        voltage=VSG_PowerLoop(&v,&INV_PARAM.vsg,0,0.5f,0,0,170,0.001f);
        assert(voltage>=150.0f&&voltage<=170.0f);
    }
    NearQ(voltage,170.0f);
    voltage=VSG_PowerLoop(&v,&INV_PARAM.vsg,0,-0.05f,0,0,170,0.001f);
    assert(voltage<170.0f&&voltage>169.0f); // No stored windup delays recovery.
    VSG_EnterGrid(&v,&INV_PARAM.vsg,TWO_PI*50.0f,0.0f,150.0f,0.0f);
    for(i=0;i<10000U;i++)voltage=VSG_PowerLoop(&v,&INV_PARAM.vsg,0,0.00001f,0,0,190,0.0001f);
    assert(voltage>150.0003f&&voltage<150.0005f); // Same 0.0004V/s as old Ki=40; tiny increments still accumulate.
    VSG_EnterGrid(&v,&INV_PARAM.vsg,TWO_PI*50.0f,0.0f,150.0f,0.0f);
    for(i=0;i<100U;i++)voltage=VSG_PowerLoop(&v,&INV_PARAM.vsg,0,0.1f,0,0,190,0.0001f);
    NearQ(voltage,150.04f); // 500var for 10ms => 0.04V, not 6.2224V.
    annJdApplyEnable=1U;assert(!AnnAdaptive_Apply(&INV_PARAM.vsg,true,true));
    Inverter_Init(); // Do not leave another fixture in ConstQ.
    TestAnnReactiveModels();
    puts("PASS reactive: V-to-pu gain conversion, old-equivalent P/I physical response, CAN/mode guards, bumpless entry, anti-windup and three-mode ANN");
}
