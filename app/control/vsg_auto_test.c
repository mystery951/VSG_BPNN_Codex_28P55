#include "vsg_auto_test.h"
#include "app/inverter.h"
#include "app/adaptive/ann_adaptive.h"
#include "reactive_control.h"

#define TICKS_PER_SECOND 10000UL
static const uint16_t eventSeconds[] = {9U,10U,15U,20U,24U,25U,30U,40U};
static uint16_t nextEvent, requested, stampPending;
static uint32_t inferenceAtReset, lastInference, lastInferenceTick;
static float baseH, baseD;
static bool stopHandled;
volatile VSG_TEST_DEF VSG_TEST = {0};

static void RestartPrediction(void)
{
    AnnAdaptive_Sample(false, 0.0f, 0.0f);
    inferenceAtReset = lastInference = ANN_CTRL.inference_count;
    lastInferenceTick = VSG_TEST.elapsed_ticks;
}

void VsgAutoTest_Init(void)
{
    const VSG_TEST_DEF empty = {0};
    VSG_TEST = empty;
    nextEvent = requested = stampPending = 0U;
    stopHandled = false;
    baseH = INV_PARAM.vsg.h; baseD = INV_PARAM.vsg.dp_pu;
}

void VsgAutoTest_Prepare(void)
{
    if(INV_CTRL.state != INV_STANDBY) return;
    VSG_TEST.state = VSG_TEST_IDLE; VSG_TEST.mode = VSG_TEST.flags = 0U;
    VSG_TEST.seconds = VSG_TEST.apply_count = 0U;
    VSG_TEST.J_applied = VSG_TEST.D_applied = 0.0f;
    VSG_TEST.elapsed_ticks = VSG_TEST.start_tick = 0U;
    nextEvent = requested = stampPending = 0U;
    stopHandled = false;
    INV_PARAM.vsg.h = baseH; INV_PARAM.vsg.dp_pu = baseD;
    INV_CMD.p_ref = 0.0f;
    INV_CMD.grid_impedance_step = INV_CMD.ann_enable = INV_CMD.jd_apply_cmd = INV_CMD.jd_adapt_enable = 0U;
    annJdApplyEnable = 0U;
    RestartPrediction();
}

bool VsgAutoTest_Request(uint16_t mode)
{
    if((mode != 1U && mode != 2U) || INV_CTRL.state != INV_GRID_CONNECTED ||
       INV_CMD.auto_test_enable != 1U || VSG_TEST.mode != 0U || requested != 0U) return false;
    if(mode == 2U && !ReactiveControl_AnnAllowed()) return false;
    requested = mode;
    return true;
}

bool VsgAutoTest_Stamp(uint32_t tick)
{
    if(stampPending == 0U) return false;
    VSG_TEST.start_tick = tick;
    VSG_TEST.flags |= VSG_TEST_STAMPED;
    stampPending = 0U;
    return true;
}

void VsgAutoTest_BuildStatus(uint16_t data[8])
{
    uint16_t i;
    for(i=0U; i<4U; i++) data[i] = (uint16_t)((VSG_TEST.start_tick >> (8U*i)) & 0xFFU);
    data[4] = VSG_TEST.run_id & 0xFFU;
    data[5] = (VSG_TEST.run_id >> 8U) & 0xFFU;
    data[6] = VSG_TEST.mode;
    data[7] = VSG_TEST.flags;
}

static void ApplyParameters(bool gridValid, uint16_t success, uint16_t failure)
{
    if(VSG_TEST.mode != 2U) return; // A predicts with identical load but NEVER writes H/D.
    if(gridValid && ANN_CTRL.inference_count != inferenceAtReset &&
       (uint32_t)(VSG_TEST.elapsed_ticks - lastInferenceTick) <= 1000UL &&
       AnnAdaptive_Apply(&INV_PARAM.vsg, false, true))
    {
        VSG_TEST.apply_count++;
        VSG_TEST.J_applied = 2.0f * INV_PARAM.vsg.h;
        VSG_TEST.D_applied = INV_PARAM.vsg.dp_pu;
        VSG_TEST.flags |= success;
    }
    else VSG_TEST.flags |= failure; // Never postpone the agreed Pref/GPIO schedule.
}

bool VsgAutoTest_Step(bool connected, bool gridValid)
{
    bool restart = false;
    if(!connected || INV_CMD.auto_test_enable != 1U)
    {
        if(!stopHandled && (VSG_TEST.mode != 0U || requested != 0U))
        {
            if((VSG_TEST.flags & VSG_TEST_RUNNING) != 0U)
                VSG_TEST.flags = (VSG_TEST.flags & ~VSG_TEST_RUNNING) | VSG_TEST_ABORTED;
            requested = stampPending = 0U;
            INV_CMD.p_ref = 0.0f;
            INV_CMD.grid_impedance_step = INV_CMD.ann_enable = INV_CMD.jd_apply_cmd = INV_CMD.jd_adapt_enable = 0U;
            annJdApplyEnable = 0U;
            RestartPrediction();
            stopHandled = true;
        }
        return false;
    }
    if(requested != 0U)
    {
        VSG_TEST.mode = requested; requested = 0U;
        stopHandled = false;
        VSG_TEST.run_id++; if(VSG_TEST.run_id == 0U) VSG_TEST.run_id = 1U;
        VSG_TEST.elapsed_ticks = VSG_TEST.start_tick = 0U;
        VSG_TEST.seconds = VSG_TEST.apply_count = nextEvent = 0U;
        VSG_TEST.flags = VSG_TEST_RUNNING; stampPending = 1U;
        VSG_TEST.state = VSG_TEST_IDENTIFY_FIRST;
        INV_CMD.p_ref = INV_PARAM.test.p_base;
        INV_CMD.grid_impedance_step = 0U;
        RestartPrediction(); restart = true;
    }
    else if((VSG_TEST.flags & VSG_TEST_RUNNING) != 0U)
    {
        VSG_TEST.elapsed_ticks++;
        VSG_TEST.seconds = (uint16_t)(VSG_TEST.elapsed_ticks / TICKS_PER_SECOND);
        if(ANN_CTRL.inference_count != lastInference)
        {
            lastInference = ANN_CTRL.inference_count;
            lastInferenceTick = VSG_TEST.elapsed_ticks;
        }
        if(nextEvent < sizeof(eventSeconds)/sizeof(eventSeconds[0]) &&
           VSG_TEST.elapsed_ticks >= (uint32_t)eventSeconds[nextEvent] * TICKS_PER_SECOND)
        {
            switch(nextEvent)
            {
                case 0U:
                    ApplyParameters(gridValid, VSG_TEST_APPLIED_FIRST, VSG_TEST_FAILED_FIRST);
                    VSG_TEST.state = VSG_TEST_BASE_FIRST;
                    break;
                case 1U: INV_CMD.p_ref = INV_PARAM.test.p_step; VSG_TEST.state = VSG_TEST_STEP_FIRST; break;
                case 2U: INV_CMD.p_ref = INV_PARAM.test.p_base; VSG_TEST.state = VSG_TEST_BASE_FIRST; break;
                case 3U:
                    INV_CMD.grid_impedance_step = 1U;
                    RestartPrediction(); restart = true; // Skip the ADC window acquired BEFORE GPIO12 changes.
                    VSG_TEST.state = VSG_TEST_IDENTIFY_SECOND;
                    break;
                case 4U:
                    ApplyParameters(gridValid, VSG_TEST_APPLIED_SECOND, VSG_TEST_FAILED_SECOND);
                    VSG_TEST.state = VSG_TEST_BASE_SECOND;
                    break;
                case 5U: INV_CMD.p_ref = INV_PARAM.test.p_step; VSG_TEST.state = VSG_TEST_STEP_SECOND; break;
                case 6U: INV_CMD.p_ref = INV_PARAM.test.p_base; VSG_TEST.state = VSG_TEST_BASE_SECOND; break;
                default:
                    VSG_TEST.flags = (VSG_TEST.flags & ~VSG_TEST_RUNNING) | VSG_TEST_DONE;
                    VSG_TEST.state = VSG_TEST_COMPLETE; // Hold Pref=1 and GPIO12=1 until operator stop.
                    break;
            }
            nextEvent++;
        }
    }
    if(VSG_TEST.mode != 0U)
    {
        INV_CMD.ann_enable = ReactiveControl_AnnAllowed() ? 1U : 0U;
        INV_CMD.jd_apply_cmd = INV_CMD.jd_adapt_enable = 0U;
        annJdApplyEnable = VSG_TEST.mode == 2U ? 1U : 0U;
    }
    return restart;
}
