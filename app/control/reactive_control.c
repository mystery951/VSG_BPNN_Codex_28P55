#include "reactive_control.h"
#include "app/inverter.h"

volatile REACTIVE_CTRL_DEF REACTIVE_CTRL = {Q_MODE_INACTIVE,0U,0U,Q_MODE_INACTIVE};
// 28377D gains produce volts from pu Q error. This implementation produces pu voltage.
#define QV_LEGACY_DROOP_KP 10.0f
#define QV_LEGACY_CONSTQ_KI 40.0f
static void ApplyGains(uint16_t mode)
{
    INV_PARAM.vsg.Kp_QV = mode == Q_MODE_DROOP ? QV_LEGACY_DROOP_KP / SYS_V_BASE_V : 0.0f;
    INV_PARAM.vsg.Ki_QV = mode == Q_MODE_CONST_Q ? QV_LEGACY_CONSTQ_KI / SYS_V_BASE_V : 0.0f;
}
void ReactiveControl_Init(void)
{
    REACTIVE_CTRL.active_mode = Q_MODE_INACTIVE;
    REACTIVE_CTRL.revision = REACTIVE_CTRL.last_result = 0U;
    REACTIVE_CTRL.last_requested = Q_MODE_INACTIVE;
    if(INV_CMD.q_mode > Q_MODE_DROOP) INV_CMD.q_mode = Q_MODE_CONST_V;
    ApplyGains(INV_CMD.q_mode);
}
bool ReactiveControl_Select(uint16_t mode)
{
    REACTIVE_CTRL.last_requested = mode;
    if(mode > Q_MODE_DROOP || INV_CTRL.state == INV_GRID_CONNECTED || INV_CMD.grid_connect_cmd == 1U)
    { REACTIVE_CTRL.last_result = 2U; return false; }
    INV_CMD.q_mode = mode; ApplyGains(mode);
    REACTIVE_CTRL.revision++; REACTIVE_CTRL.last_result = 1U;
    return true;
}
void ReactiveControl_EnterGrid(void)
{
    uint16_t mode = INV_CMD.q_mode <= Q_MODE_DROOP ? INV_CMD.q_mode : Q_MODE_CONST_V;
    REACTIVE_CTRL.active_mode = mode; ApplyGains(mode);
}
void ReactiveControl_LeaveGrid(void) { REACTIVE_CTRL.active_mode = Q_MODE_INACTIVE; }
bool ReactiveControl_AnnAllowed(void)
{
    uint16_t mode = REACTIVE_CTRL.active_mode <= Q_MODE_DROOP ? REACTIVE_CTRL.active_mode : INV_CMD.q_mode;
    return mode <= Q_MODE_DROOP;
}
void ReactiveControl_BuildStatus(uint16_t data[8])
{
    data[0] = 1U; data[1] = INV_CMD.q_mode; data[2] = REACTIVE_CTRL.active_mode;
    data[3] = (INV_CTRL.state == INV_GRID_CONNECTED || INV_CMD.grid_connect_cmd == 1U) ? 1U : 0U;
    data[4] = REACTIVE_CTRL.revision & 0xFFU; data[5] = (REACTIVE_CTRL.revision >> 8U) & 0xFFU;
    data[6] = REACTIVE_CTRL.last_result; data[7] = REACTIVE_CTRL.last_requested;
}
