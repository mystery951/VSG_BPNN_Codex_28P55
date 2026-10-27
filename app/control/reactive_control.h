#ifndef REACTIVE_CONTROL_H_
#define REACTIVE_CONTROL_H_
#include <stdbool.h>
#include <stdint.h>
#define Q_MODE_CONST_V 0U
#define Q_MODE_CONST_Q 1U
#define Q_MODE_DROOP 2U
#define Q_MODE_INACTIVE 255U
typedef struct { uint16_t active_mode, revision, last_result, last_requested; } REACTIVE_CTRL_DEF;
extern volatile REACTIVE_CTRL_DEF REACTIVE_CTRL;
void ReactiveControl_Init(void);
bool ReactiveControl_Select(uint16_t mode); // Before grid request only; no VF/presync voltage change.
void ReactiveControl_EnterGrid(void); // Latch gains before VSG PI initialization.
void ReactiveControl_LeaveGrid(void);
bool ReactiveControl_AnnAllowed(void); // All three modes have dedicated CPU impedance models.
void ReactiveControl_BuildStatus(uint16_t data[8]);
#endif
