#include "inverter_state.h"
#include <math.h>

volatile INV_STATE_CTRL INV_CTRL = { INV_STANDBY, INV_STANDBY, 0U, 0U, 0.0f, 0.0f, {0.0f,0.0f,0.0f} };

void InvState_Init(void)
{
    INV_CTRL.state = INV_CTRL.last_state = INV_STANDBY;
    INV_CTRL.grid_zero_cross_ok = INV_CTRL.grid_connect_rejected = 0U;
    INV_CTRL.theta = 0.0f;
    INV_CTRL.omega = 0.0f;
    INV_CTRL.sync_diff[0] = INV_CTRL.sync_diff[1] = INV_CTRL.sync_diff[2] = 0.0f;
}

void InvState_Step(volatile INV_STATE_CTRL *s, volatile INV_CMD_DEF *cmd, bool syncReady, bool gridValid, const float diff[3], float maxDiff)
{
    uint16_t i;
    s->last_state = s->state;
    s->grid_zero_cross_ok = 1U;
    for(i = 0U; i < 3U; i++)
    {
        s->sync_diff[i] = diff[i];
        if(!(fabsf(diff[i]) < maxDiff)) s->grid_zero_cross_ok = 0U;
    }
    if(cmd->run_enable != 1U)
    {
        s->state = INV_STANDBY;
        cmd->grid_connect_cmd = cmd->presync_request = 0U;
        s->grid_zero_cross_ok = s->grid_connect_rejected = 0U;
        return;
    }
    if(s->state == INV_GRID_CONNECTED)
    {
        cmd->presync_request = 0U;
        if(cmd->grid_connect_cmd != 1U) s->state = INV_OFFGRID_VF;
        return;
    }
    s->state = (cmd->presync_request == 1U) ? INV_PRESYNC : INV_OFFGRID_VF;
    if(cmd->presync_request == 1U && syncReady && gridValid && s->grid_zero_cross_ok == 1U)
        s->state = INV_GRID_READY;
    if(cmd->grid_connect_cmd == 1U)
    {
        if(s->state == INV_GRID_READY)
        {
            cmd->presync_request = 0U;
            s->grid_connect_rejected = 0U;
            s->state = INV_GRID_CONNECTED;
        }
        else
        {
            cmd->grid_connect_cmd = 0U;
            s->grid_connect_rejected = 1U;
        }
    }
}
