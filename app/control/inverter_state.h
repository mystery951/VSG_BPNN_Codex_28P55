#ifndef INVERTER_STATE_H_
#define INVERTER_STATE_H_
#include <stdbool.h>
#include "app/inverter_settings.h"

typedef enum
{
    INV_STANDBY = 0, INV_OFFGRID_VF = 1, INV_PRESYNC = 2,
    INV_GRID_READY = 3, INV_GRID_CONNECTED = 4
} INV_STATE;
typedef struct
{
    INV_STATE state, last_state;
    uint16_t grid_zero_cross_ok, grid_connect_rejected;
    float theta, omega; // ONE shared angle / angular frequency for VF and VSG.
    float sync_diff[3];
} INV_STATE_CTRL;

extern volatile INV_STATE_CTRL INV_CTRL;
void InvState_Init(void);
// Select mode only. No PI, PLL, VSG or GPIO operations in the state module.
void InvState_Step(volatile INV_STATE_CTRL *s, volatile INV_CMD_DEF *cmd, bool syncReady, bool gridValid, const float diff[3], float maxDiff);
#endif
