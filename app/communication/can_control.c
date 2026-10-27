#include "can_control.h"
#include "app/hardware/can_port.h"
#include "app/inverter.h"
#include "app/control/presync.h"
#include "app/hardware/pwm_output.h"
#include "app/adaptive/ann_adaptive.h"
#include "app/measurement/feedback_dq.h"
#include "app/measurement/adc_measurement.h"
#include "app/control/vsg_auto_test.h"
#include "app/control/reactive_control.h"
#include <math.h>
#include "driverlib.h"
#include "board.h"

volatile uint16_t cana_telemetry_enable = 1U, cana_status_enable = 1U;
volatile uint16_t cana_last_rx_command, cana_pending_command, cana_command_pending;
volatile uint16_t cana_emergency_stop_pending, cana_emergency_stop_latched;
volatile uint16_t cana_last_frame_valid, cana_last_processed_command, cana_command_result;
volatile uint32_t cana_rx_count, cana_tx_count, cana_error_flag, cana_err_status;
volatile uint32_t cana_control_frame_count, cana_estop_frame_count;
volatile uint32_t cana_command_valid_count, cana_command_invalid_count;
volatile uint32_t cana_command_accepted_count, cana_command_rejected_count;
volatile uint32_t cana_telemetry_cycle_count, cana_status_tx_count, cana_tx_busy_count;
volatile uint32_t cana_output_tx_count;
uint16_t cana_status_tx_data[8];
static uint32_t lastTxTick;
// Single ISR producer / main-loop consumer. 63 usable pairs = 63 ms backlog.
#define POWER_QUEUE_SIZE 64U
typedef struct { uint32_t tick; float reference, measured; } POWER_SAMPLE;
static volatile POWER_SAMPLE powerQueue[POWER_QUEUE_SIZE];
static volatile uint16_t powerWrite, powerRead;
static uint16_t powerDivider, powerRefSent;
volatile uint16_t cana_power_enable = 1U, cana_power_queue_peak;
volatile uint32_t cana_power_sample_count, cana_power_tx_count, cana_power_drop_count;

void CanControl_Init(void)
{
    uint16_t i;
    cana_telemetry_enable = cana_status_enable = 1U;
    cana_last_rx_command = cana_pending_command = cana_command_pending = 0U;
    cana_emergency_stop_pending = cana_emergency_stop_latched = 0U;
    cana_last_frame_valid = cana_last_processed_command = cana_command_result = 0U;
    cana_rx_count = cana_tx_count = cana_error_flag = cana_err_status = 0U;
    cana_control_frame_count = cana_estop_frame_count = 0U;
    cana_command_valid_count = cana_command_invalid_count = 0U;
    cana_command_accepted_count = cana_command_rejected_count = 0U;
    cana_telemetry_cycle_count = cana_status_tx_count = cana_tx_busy_count = 0U;
    cana_output_tx_count = 0U;
    lastTxTick = 0U;
    powerWrite = powerRead = powerDivider = powerRefSent = 0U;
    cana_power_enable = 1U;
    cana_power_sample_count = cana_power_tx_count = cana_power_drop_count = 0U;
    cana_power_queue_peak = 0U;
    for(i = 0U; i < 8U; i++) cana_status_tx_data[i] = 0U;
}

void CanControl_Receive(uint32_t id, const uint16_t *data, uint16_t dlc, bool standardData)
{
    uint16_t command;
    if(id != CANA_CONTROL_MSG_ID && id != CANA_ESTOP_MSG_ID) return;
    cana_rx_count++;
    if(id == CANA_CONTROL_MSG_ID) cana_control_frame_count++;
    else cana_estop_frame_count++;
    cana_last_frame_valid = 0U;
    if(!standardData || dlc != 4U || data == 0) { cana_command_invalid_count++; return; }
    command = data[0] & 0xFFU;
    cana_last_rx_command = command;
    if((data[1] & 0xFFU) != 0xA5U || (data[2] & 0xFFU) != 0x5AU || (data[3] & 0xFFU) != 0U ||
       (id == CANA_CONTROL_MSG_ID && !((command >= 1U && command <= 6U) || (command >= CANA_COMMAND_CONST_V && command <= CANA_COMMAND_DROOP))) ||
       (id == CANA_ESTOP_MSG_ID && command != CANA_COMMAND_EMERGENCY_STOP))
    { cana_command_invalid_count++; return; }
    cana_last_frame_valid = 1U;
    cana_command_valid_count++;
    cana_pending_command = command;
    cana_command_pending = 1U; // Latest normal command wins, as in the legacy code.
    if(id == CANA_ESTOP_MSG_ID) cana_emergency_stop_pending = 1U;
}

static void StopCommands(void)
{
    INV_CMD.run_enable = INV_CMD.presync_request = INV_CMD.grid_connect_cmd = 0U;
    INV_CMD.jd_apply_cmd = INV_CMD.jd_adapt_enable = 0U;
    INV_CMD.p_ref = 0.0f;
    INV_CMD.ann_enable = 0U;
    INV_CTRL.grid_zero_cross_ok = 0U;
}

static void Result(uint16_t result)
{
    cana_command_result = result;
    if(result == CANA_COMMAND_RESULT_ACCEPTED) cana_command_accepted_count++;
    else cana_command_rejected_count++;
}

void CanControl_ProcessPending(void)
{
    uint16_t command;
    if(cana_emergency_stop_pending != 0U)
    {
        cana_emergency_stop_pending = cana_command_pending = 0U;
        cana_pending_command = CANA_COMMAND_NONE;
        cana_emergency_stop_latched = 1U;
        cana_last_processed_command = CANA_COMMAND_EMERGENCY_STOP;
        StopCommands();
        Result(CANA_COMMAND_RESULT_ACCEPTED);
        return;
    }
    if(cana_emergency_stop_latched != 0U) StopCommands();
    if(cana_command_pending == 0U) return;
    command = cana_pending_command;
    cana_command_pending = 0U;
    cana_pending_command = CANA_COMMAND_NONE;
    cana_last_processed_command = command;
    if(command == CANA_COMMAND_CLEAR_ESTOP)
    {
        if(cana_emergency_stop_latched != 0U && INV_CTRL.state == INV_STANDBY)
        {
            StopCommands();
            cana_emergency_stop_latched = 0U;
            Result(CANA_COMMAND_RESULT_ACCEPTED);
        }
        else Result(CANA_COMMAND_RESULT_INVALID_STATE);
        return;
    }
    if(cana_emergency_stop_latched != 0U) { Result(CANA_COMMAND_RESULT_FAULT_LATCHED); return; }
    switch(command)
    {
        case CANA_COMMAND_CONST_V:
        case CANA_COMMAND_CONST_Q:
        case CANA_COMMAND_DROOP:
            if(ReactiveControl_Select(command - CANA_COMMAND_CONST_V))
            { Result(CANA_COMMAND_RESULT_ACCEPTED); return; }
            break;
        case CANA_COMMAND_ISLAND_START:
            if(INV_CTRL.state != INV_STANDBY && INV_CTRL.state != INV_OFFGRID_VF) break;
            if(INV_CTRL.state == INV_STANDBY) VsgAutoTest_Prepare();
            INV_CMD.run_enable = 1U;
            INV_CMD.presync_request = INV_CMD.grid_connect_cmd = 0U;
            Result(CANA_COMMAND_RESULT_ACCEPTED);
            return;
        case CANA_COMMAND_PRESYNC_START:
            if(INV_CTRL.state != INV_OFFGRID_VF && INV_CTRL.state != INV_PRESYNC && INV_CTRL.state != INV_GRID_READY) break;
            INV_CMD.run_enable = INV_CMD.presync_request = 1U;
            INV_CMD.grid_connect_cmd = 0U;
            Result(CANA_COMMAND_RESULT_ACCEPTED);
            return;
        case CANA_COMMAND_GRID_CONNECT:
            if(INV_CTRL.state != INV_GRID_CONNECTED && !(INV_CTRL.state == INV_GRID_READY && PRESYNC_CTRL.sync_ready == 1U)) break;
            INV_CMD.run_enable = INV_CMD.grid_connect_cmd = 1U;
            if(INV_CTRL.state != INV_GRID_CONNECTED)
            {
                INV_CMD.ann_enable = 0U;
                INV_CMD.p_ref = 0.0f; // Always wait for explicit A/B start; never launch on grid connection.
            }
            // Already connected: do not restart the presync loop.
            INV_CMD.presync_request = INV_CTRL.state == INV_GRID_CONNECTED ? 0U : 1U;
            Result(CANA_COMMAND_RESULT_ACCEPTED);
            return;
        case CANA_COMMAND_TEST_FIXED:
        case CANA_COMMAND_TEST_ADAPTIVE:
            if(VsgAutoTest_Request(command == CANA_COMMAND_TEST_FIXED ? 1U : 2U))
            { Result(CANA_COMMAND_RESULT_ACCEPTED); return; }
            break;
        default: break;
    }
    Result(CANA_COMMAND_RESULT_INVALID_STATE);
}

static void PackFloat(float value, uint16_t *data)
{
    union { float f; uint32_t bits; } v;
    uint16_t i;
    v.f = value;
    for(i = 0U; i < 4U; i++) data[i] = (uint16_t)((v.bits >> (8U * i)) & 0xFFU);
}

void CanControl_CapturePower(uint32_t controlTick)
{
    uint16_t next, used;
    bool startSample = VsgAutoTest_Stamp(controlTick);
    if(!startSample && ++powerDivider < 10U) return;
    powerDivider = 0U;
    if(cana_power_enable != 1U) return;
    cana_power_sample_count++;
    next = (powerWrite + 1U) % POWER_QUEUE_SIZE;
    if(next == powerRead) { cana_power_drop_count++; return; }
    powerQueue[powerWrite].tick = controlTick; // Units = 100 us, not foreground arrival time.
    powerQueue[powerWrite].reference = INV_CMD.p_ref * SYS_S_BASE_VA;
    powerQueue[powerWrite].measured = P_filter; // HIL D12 power in W; control-loop Pe is NOT changed.
    powerWrite = next; // Publish ONLY after all sample fields have been written.
    used = (powerWrite + POWER_QUEUE_SIZE - powerRead) % POWER_QUEUE_SIZE;
    if(used > cana_power_queue_peak) cana_power_queue_peak = used;
}

static void ServicePower(void)
{
    uint16_t data[8], i, pass;
    uint32_t tick;
    // Bounded draining; never wait for an ACK or for bus space. A half-pair is retried,
    // never replaced by a newer sample. Matching timestamps prevent false associations.
    for(pass = 0U; pass < 4U && powerRead != powerWrite; pass++)
    {
        tick = powerQueue[powerRead].tick;
        for(i = 0U; i < 4U; i++) data[i] = (uint16_t)((tick >> (8U * i)) & 0xFFU);
        if(powerRefSent == 0U)
        {
            PackFloat(powerQueue[powerRead].reference, data + 4);
            if(!CanPort_TrySend(5U, CANA_POWER_REF_MSG_ID, data)) return;
            powerRefSent = 1U;
        }
        PackFloat(powerQueue[powerRead].measured, data + 4);
        if(!CanPort_TrySend(6U, CANA_POWER_MSG_ID, data)) return;
        powerRefSent = 0U;
        powerRead = (powerRead + 1U) % POWER_QUEUE_SIZE;
        cana_power_tx_count++;
    }
}

void CanControl_BuildTelemetry(uint16_t frames[3][8])
{
    uint16_t flags = 0U, i;
    PackFloat(ANN_CTRL.R_est, frames[0]);
    PackFloat(ANN_CTRL.L_est, frames[0] + 4); // mH, never convert to H on the wire.
    PackFloat(ANN_CTRL.J_apt, frames[1]);
    PackFloat(ANN_CTRL.D_apt, frames[1] + 4);
    // Preserve legacy wire state 5 without adding generic faults to the VF/VSG controller.
    frames[2][0] = cana_emergency_stop_latched != 0U ? 5U : (uint16_t)INV_CTRL.state;
    frames[2][1] = cana_last_processed_command;
    frames[2][2] = cana_command_result;
    if(pwmOutputEnabled == 1U) flags |= 0x01U;
    if(GPIO_readPin(Breaker) != 0U) flags |= 0x02U;
    if(PRESYNC_CTRL.sync_ready == 1U) flags |= 0x04U;
    if(cana_emergency_stop_latched != 0U) flags |= 0x08U;
    if(cana_error_flag != 0U) flags |= 0x10U;
    if(INV_CMD.run_enable == 1U) flags |= 0x20U;
    if(INV_CMD.presync_request == 1U) flags |= 0x40U;
    if(INV_CMD.grid_connect_cmd == 1U) flags |= 0x80U;
    frames[2][3] = flags;
    frames[2][4] = (uint16_t)(cana_rx_count & 0xFFU);
    frames[2][5] = (uint16_t)((cana_rx_count >> 8U) & 0xFFU);
    frames[2][6] = (uint16_t)(cana_command_accepted_count & 0xFFU);
    frames[2][7] = (uint16_t)(cana_command_rejected_count & 0xFFU);
    for(i = 0U; i < 8U; i++) cana_status_tx_data[i] = frames[2][i];
}

void CanControl_BuildAnnStatus(uint16_t data[8])
{
    uint16_t i;
    data[0] = 1U; // Diagnostic format version; legacy 0x201/202/203 are unchanged.
    data[1] = ANN_CTRL.result_status;
    data[2] = (ANN_CTRL.has_valid_result ? 1U : 0U) | (INV_CMD.ann_enable ? 2U : 0U);
    data[3] = 0U; // Legacy NPU status byte: always idle/unavailable in CPU-only firmware.
    for(i = 0U; i < 4U; i++) data[4U + i] = (uint16_t)((ANN_CTRL.inference_count >> (8U * i)) & 0xFFU);
}

void CanControl_BuildOutputTelemetry(uint16_t data[8], float vd, float vq, float omega)
{
    PackFloat(sqrtf(vd * vd + vq * vq), data);
    PackFloat(omega / TWO_PI, data + 4);
}

void CanControl_Service(uint32_t controlTick)
{
    uint16_t frames[3][8], annStatus[8], output[8], actualJd[8], experiment[8], qMode[8], qPower[8], telemetry, status;
    float vd, vq, omega;
    bool wasDisabled, rlSent, jdSent;
    wasDisabled = Interrupt_disableGlobal();
    if(wasDisabled) return; // Never touch MCAN from an already-masked caller.
    Interrupt_enableGlobal();
    CanPort_Service();
    ServicePower();
    if((uint32_t)(controlTick - lastTxTick) < 1000U) return; // 100 ms at 10 kHz.
    lastTxTick = controlTick;
    wasDisabled = Interrupt_disableGlobal();
    if(wasDisabled) return; // Defer sends and preserve an already-masked caller.
    telemetry = cana_telemetry_enable;
    status = cana_status_enable;
    CanControl_BuildTelemetry(frames);
    CanControl_BuildAnnStatus(annStatus);
    PackFloat(2.0f * INV_PARAM.vsg.h, actualJd);
    PackFloat(INV_PARAM.vsg.dp_pu, actualJd + 4);
    VsgAutoTest_BuildStatus(experiment);
    ReactiveControl_BuildStatus(qMode);
    PackFloat(INV_FEEDBACK.Qe, qPower); PackFloat(INV_CMD.q_ref * SYS_S_BASE_VA, qPower + 4);
    vd = INV_FEEDBACK.Vd_cap; vq = INV_FEEDBACK.Vq_cap; omega = INV_CTRL.omega;
    Interrupt_enableGlobal();
    if(telemetry != 0U)
    {
        rlSent = CanPort_TrySend(0U, CANA_TELEMETRY_RL_MSG_ID, frames[0]);
        jdSent = CanPort_TrySend(1U, CANA_TELEMETRY_JD_MSG_ID, frames[1]);
        if(rlSent && jdSent) cana_telemetry_cycle_count++;
    }
    if(status != 0U && CanPort_TrySend(2U, CANA_STATUS_MSG_ID, frames[2])) cana_status_tx_count++;
    if(status != 0U) CanPort_TrySend(3U, CANA_ANN_STATUS_MSG_ID, annStatus);
    if(status != 0U) CanPort_TrySend(8U, CANA_EXPERIMENT_MSG_ID, experiment);
    if(telemetry != 0U)
    {
        // Math runs outside the snapshot critical section; no extra control-ISR work.
        CanControl_BuildOutputTelemetry(output, vd, vq, omega);
        if(CanPort_TrySend(4U, CANA_OUTPUT_MSG_ID, output)) cana_output_tx_count++;
        CanPort_TrySend(7U, CANA_ACTUAL_JD_MSG_ID, actualJd); // 100 ms, actual adopted J/D (not ANN candidates).
        CanPort_TrySend(10U, CANA_REACTIVE_POWER_MSG_ID, qPower);
    }
    if(status != 0U) CanPort_TrySend(9U, CANA_REACTIVE_STATUS_MSG_ID, qMode);
}
