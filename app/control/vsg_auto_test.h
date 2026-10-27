#ifndef VSG_AUTO_TEST_H_
#define VSG_AUTO_TEST_H_
#include <stdbool.h>
#include <stdint.h>

typedef enum { VSG_TEST_IDLE=0, VSG_TEST_IDENTIFY_FIRST=2, VSG_TEST_BASE_FIRST=4, VSG_TEST_STEP_FIRST,
    VSG_TEST_IDENTIFY_SECOND, VSG_TEST_BASE_SECOND=8, VSG_TEST_STEP_SECOND, VSG_TEST_COMPLETE } VSG_TEST_STATE;
#define VSG_TEST_STAMPED 1U
#define VSG_TEST_RUNNING 2U
#define VSG_TEST_DONE 4U
#define VSG_TEST_ABORTED 8U
#define VSG_TEST_APPLIED_FIRST 16U
#define VSG_TEST_APPLIED_SECOND 32U
#define VSG_TEST_FAILED_FIRST 64U
#define VSG_TEST_FAILED_SECOND 128U
typedef struct
{
    uint16_t state, seconds, apply_count, mode, run_id, flags; // mode: 0 waiting, 1 fixed A, 2 adaptive B
    uint32_t elapsed_ticks, start_tick;
    float J_applied, D_applied;
} VSG_TEST_DEF;
extern volatile VSG_TEST_DEF VSG_TEST;
void VsgAutoTest_Init(void);
void VsgAutoTest_Prepare(void); // STANDBY only: restore baseline H/D, clear completed/aborted trial.
bool VsgAutoTest_Request(uint16_t mode); // Grid-connected waiting only; no duplicate restarts.
bool VsgAutoTest_Step(bool connected, bool gridValid);
bool VsgAutoTest_Stamp(uint32_t tick); // End of ISR, same tick as Pref=1; forces t=0 sample.
void VsgAutoTest_BuildStatus(uint16_t data[8]); // 0x209: startTick u32, runId u16, mode u8, flags u8.
#endif
