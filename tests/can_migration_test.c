#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "driverlib.h"
#include "board.h"
#include "app/communication/can_control.h"
#include "app/hardware/can_port.h"
#include "app/hardware/pwm_output.h"
#include "app/control/three_level_modulation.h"
#include "app/control/presync.h"
#include "app/adaptive/ann_adaptive.h"
#include "app/measurement/adc_measurement.h"
#include "app/measurement/feedback_dq.h"
#include "app/inverter.h"
#include "app/control/vsg_auto_test.h"

volatile uint16_t cana_port_ready, cana_bus_off;
volatile uint32_t cana_init_error, cana_isr_status;
static uint16_t sent[32][8];
static uint32_t sentId[32];
static unsigned sentCount;
static bool fakeBusy;
static int fakeBusySlot = -1;
bool CanPort_Init(void) { cana_port_ready = 1U; return true; }
void CanPort_Service(void) { }
bool CanPort_TrySend(uint16_t slot, uint32_t id, const uint16_t data[8])
{
    bool wasDisabled = Interrupt_disableGlobal();
    assert(!wasDisabled); // No MCAN operations inside the telemetry critical section.
    Interrupt_enableGlobal();
    assert(slot < CAN_PORT_TX_SLOTS && id == 0x201U + slot);
    if(fakeBusy || (int)slot == fakeBusySlot) { cana_tx_busy_count++; return false; }
    assert(sentCount < 32);
    memcpy(sent[sentCount], data, sizeof(sent[0]));
    sentId[sentCount++] = id;
    return true;
}

static void Command(uint32_t id, uint16_t command)
{
    uint16_t data[4] = {command, 0xA5U, 0x5AU, 0U};
    CanControl_Receive(id, data, 4U, true);
}

static float ReadFloat(const uint16_t data[4])
{
    union { float f; uint32_t bits; } decoded;
    decoded.bits = (uint32_t)data[0] | ((uint32_t)data[1] << 8U) | ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
    return decoded.f;
}

static void Tick(void)
{
    Inverter_ControlStep();
    PwmOutput_Update(INV_CMD.run_enable == 1U, INV_CTRL.state == INV_GRID_CONNECTED, MODULATION_OUT.duty);
}

void TestCanMigration(void)
{
    unsigned i;
    uint16_t frames[3][8], annStatus[8], output[8], data[8] = {1U, 0xA5U, 0x5AU, 0U};
    {
        union { float f; uint32_t bits; } decoded;
        unsigned n;
        ANN_CTRL.R_est = NAN; ANN_CTRL.L_est = INFINITY;
        ANN_CTRL.J_apt = -0.2f; ANN_CTRL.D_apt = -INFINITY;
        ANN_CTRL.result_valid = 0U;
        CanControl_BuildTelemetry(frames);
        for(n = 0U; n < 4U; n++)
        {
            uint16_t *bytes = &frames[n / 2U][(n % 2U) * 4U];
            decoded.bits = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
            if(n == 0U) assert(isnan(decoded.f));
            else if(n == 1U) assert(isinf(decoded.f) && decoded.f > 0.0f);
            else if(n == 2U) assert(decoded.f == -0.2f);
            else assert(isinf(decoded.f) && decoded.f < 0.0f);
        }
        puts("PASS CAN: invalid raw negative/NaN/Infinity values remain intact in 0x201/0x202");
    }
    Inverter_Init();
    Va_cap = Vb_cap = Vc_cap = PCC_ua = PCC_ub = PCC_uc = 0.0f;
    Ia_inv = Ib_inv = Ic_inv = Ia_filter = Ib_filter = Ic_filter = 0.0f;
    assert(INV_CMD.run_enable == 0U);
    CanControl_Receive(0x200U, data, 3U, true);
    CanControl_Receive(0x200U, data, 8U, true);
    CanControl_Receive(0x200U, data, 4U, false);
    data[1] = 0U;
    CanControl_Receive(0x200U, data, 4U, true);
    assert(cana_command_invalid_count == 4U && cana_command_pending == 0U);
    Command(0x100U, 1U); Command(0x200U, 255U);
    CanControl_Receive(0x200U, 0, 4U, true);
    assert(cana_command_invalid_count == 7U);
    CanControl_Receive(0x299U, data, 4U, true);
    assert(cana_rx_count == 7U);
    for(i = 2U; i <= 4U; i++)
    {
        Command(0x200U, (uint16_t)i); Tick();
        assert(cana_command_result == CANA_COMMAND_RESULT_INVALID_STATE);
        assert(INV_CTRL.state == INV_STANDBY && pwmOutputEnabled == 0U);
    }
    Command(0x200U, 1U);
    assert(INV_CMD.run_enable == 0U); // The receive path cannot switch GPIO/state.
    Tick();
    assert(pwmOutputEnabled == 0U); // Existing one-cycle PWM shadow priming is retained.
    Tick();
    assert(INV_CTRL.state == INV_OFFGRID_VF && pwmOutputEnabled == 1U && GPIO_readPin(Breaker) == 0U);
    Command(0x200U, 3U); Tick();
    assert(cana_command_result == CANA_COMMAND_RESULT_INVALID_STATE && GPIO_readPin(Breaker) == 0U);
    assert(INV_CMD.ann_enable == 0U && INV_CMD.p_ref == 0.0f); // Rejected connect cannot enable inference/power.
    Command(0x200U, 2U); Tick();
    assert(cana_command_result == CANA_COMMAND_RESULT_ACCEPTED && INV_CTRL.state == INV_PRESYNC);
    Command(0x100U, 255U);
    Command(0x200U, 1U); // A following normal frame cannot erase the emergency stop.
    Tick();
    assert(cana_emergency_stop_latched == 1U && INV_CTRL.state == INV_STANDBY);
    assert(cana_last_processed_command == 255U && pwmOutputEnabled == 0U);
    assert(GPIO_readPin(enPWM47) == 1U && GPIO_readPin(enPWM48) == 1U && GPIO_readPin(Breaker) == 0U);
    INV_CMD.run_enable = 1U; Tick();
    assert(INV_CMD.run_enable == 0U && pwmOutputEnabled == 0U);
    Command(0x200U, 1U); Tick();
    assert(cana_command_result == CANA_COMMAND_RESULT_FAULT_LATCHED);
    CanControl_BuildTelemetry(frames);
    assert(frames[2][0] == 5U && (frames[2][3] & 0x08U) != 0U);
    Command(0x200U, 4U); Tick();
    assert(cana_emergency_stop_latched == 0U && INV_CTRL.state == INV_STANDBY && INV_CMD.run_enable == 0U);
    Command(0x200U, 1U); Tick();
    assert(INV_CTRL.state == INV_OFFGRID_VF);
    Tick();
    INV_CTRL.state = INV_GRID_READY; PRESYNC_CTRL.sync_ready = 1U;
    INV_CMD.p_ref = 0.25f;
    assert(INV_CMD.ann_enable == 0U);
    Command(0x200U, 3U); CanControl_ProcessPending();
    assert(INV_CMD.grid_connect_cmd == 1U && cana_command_result == CANA_COMMAND_RESULT_ACCEPTED);
    assert(INV_CMD.ann_enable == 0U && INV_CMD.p_ref == 0.0f); // Timer starts on actual connection, not command receipt.
    assert(INV_CMD.jd_adapt_enable == 0U && INV_CMD.jd_apply_cmd == 0U);
    INV_CTRL.state = INV_GRID_CONNECTED;
    Command(0x200U, 1U); CanControl_ProcessPending();
    assert(cana_command_result == CANA_COMMAND_RESULT_INVALID_STATE);
    INV_CMD.ann_enable = 0U; INV_CMD.p_ref = 0.5f;
    Command(0x200U, 3U); CanControl_ProcessPending();
    assert(INV_CMD.grid_connect_cmd == 1U && INV_CMD.presync_request == 0U);
    assert(INV_CMD.ann_enable == 0U && INV_CMD.p_ref == 0.5f); // Duplicate frame does not disturb an automatic experiment.
    INV_CMD.auto_test_enable = 0U;
    Command(0x200U, 3U); CanControl_ProcessPending();
    assert(INV_CMD.ann_enable == 0U && INV_CMD.p_ref == 0.5f);
    puts("PASS CAN connect waits for explicit A/B in every configuration; duplicate connect never resets Pref");
    INV_CMD.auto_test_enable = 1U;
    Command(0x200U, 5U); CanControl_ProcessPending();
    assert(cana_command_result == CANA_COMMAND_RESULT_ACCEPTED);
    VsgAutoTest_Step(true, true);
    assert(INV_CMD.p_ref == 1.0f && VSG_TEST.mode == 1U);
    assert(VsgAutoTest_Stamp(1234U));
    Command(0x200U, 6U); CanControl_ProcessPending();
    assert(cana_command_result == CANA_COMMAND_RESULT_INVALID_STATE && VSG_TEST.mode == 1U);
    VsgAutoTest_Step(false, true);
    INV_CTRL.state = INV_STANDBY; VsgAutoTest_Prepare(); INV_CTRL.state = INV_GRID_CONNECTED;
    puts("PASS legacy commands, exact DLC/magic/ID validation, state guards, prioritized latched stop and explicit recovery");

    ANN_CTRL.R_est = 1.0f; ANN_CTRL.L_est = -2.5f; ANN_CTRL.J_apt = 3.0f; ANN_CTRL.D_apt = 4.0f;
    INV_CMD.presync_request = 1U;
    GPIO_writePin(Breaker, 1U); pwmOutputEnabled = 1U; cana_error_flag = 1U;
    cana_rx_count = 0x12345U; cana_command_accepted_count = 0x102U; cana_command_rejected_count = 0x103U;
    CanControl_BuildTelemetry(frames);
    assert(frames[0][0] == 0 && frames[0][1] == 0 && frames[0][2] == 0x80 && frames[0][3] == 0x3F);
    assert(frames[0][4] == 0 && frames[0][5] == 0 && frames[0][6] == 0x20 && frames[0][7] == 0xC0);
    assert(frames[1][2] == 0x40 && frames[1][3] == 0x40 && frames[1][6] == 0x80 && frames[1][7] == 0x40);
    assert(frames[2][0] == 4U && frames[2][3] == 0xF7U);
    assert(frames[2][4] == 0x45U && frames[2][5] == 0x23U && frames[2][6] == 2U && frames[2][7] == 3U);
    assert(memcmp(frames[2], cana_status_tx_data, sizeof(frames[2])) == 0);
    ANN_CTRL.result_status = ANN_RESULT_INPUT_RANGE;
    ANN_CTRL.has_valid_result = 1U;
    ANN_CTRL.inference_count = 0x12345678U;
    CanControl_BuildAnnStatus(annStatus);
    assert(annStatus[0] == 1U && annStatus[1] == ANN_RESULT_INPUT_RANGE && (annStatus[2] & 1U) == 1U);
    assert((annStatus[2] & 0x0CU) == 0U && annStatus[3] == 0U); // CPU-only, never reports NPU activity.
    assert(annStatus[4] == 0x78U && annStatus[5] == 0x56U && annStatus[6] == 0x34U && annStatus[7] == 0x12U);
    CanControl_BuildOutputTelemetry(output, 3.0f, 4.0f, TWO_PI * 49.5f);
    assert(ReadFloat(output) == 5.0f && fabsf(ReadFloat(output + 4) - 49.5f) < 0.00001f);
    assert(output[2] == 0xA0U && output[3] == 0x40U); // 5 V peak, not RMS or a setpoint.
    CanControl_BuildOutputTelemetry(output, 0.0f, 0.0f, 0.0f);
    assert(ReadFloat(output) == 0.0f && ReadFloat(output + 4) == 0.0f);
    CanControl_BuildOutputTelemetry(output, NAN, 4.0f, INFINITY);
    assert(isnan(ReadFloat(output)) && isinf(ReadFloat(output + 4)));
    INV_FEEDBACK.Vd_cap = 3.0f; INV_FEEDBACK.Vq_cap = 4.0f;
    INV_CTRL.omega = TWO_PI * 49.5f;
    INV_FEEDBACK.Qe = 250.0f; INV_CMD.q_ref = 0.1f;
    sentCount = 0; fakeBusy = false;
    CanControl_Service(999U); assert(sentCount == 0U);
    CanControl_Service(1000U); assert(sentCount == 9U && sentId[7] == 0x20BU && sentId[8] == 0x20AU && sentId[0] == 0x201U && sentId[2] == 0x203U && sentId[3] == 0x204U && sentId[4] == 0x209U && sentId[5] == 0x205U && sentId[6] == 0x208U);
    assert(ReadFloat(sent[6]) == 2.0f * INV_PARAM.vsg.h && ReadFloat(sent[6] + 4) == INV_PARAM.vsg.dp_pu);
    assert(ReadFloat(sent[5]) == 5.0f && fabsf(ReadFloat(sent[5] + 4) - 49.5f) < 0.00001f);
    assert(cana_output_tx_count == 1U); // Output telemetry remains live even with invalid ANN.
    assert(ReadFloat(sent[7])==250.0f && ReadFloat(sent[7]+4)==500.0f); // var, not pu/kvar on wire.
    assert(sent[8][0]==1U && sent[8][1]==INV_CMD.q_mode); // Mode protocol version/readback.
    assert(memcmp(sent[3], annStatus, sizeof(annStatus)) == 0);
    assert(memcmp(sent[0], frames[0], sizeof(frames[0])) == 0);
    cana_status_enable = 0U;
    CanControl_Service(2000U); assert(sentCount == 14U && sentId[11] == 0x205U && sentId[12] == 0x208U && sentId[13] == 0x20BU && cana_output_tx_count == 2U);
    cana_telemetry_enable = 0U;
    CanControl_Service(3000U); assert(sentCount == 14U);
    cana_status_enable = 1U;
    CanControl_Service(4000U); assert(sentCount == 18U && sentId[14] == 0x203U && sentId[15] == 0x204U && sentId[16] == 0x209U && sentId[17] == 0x20AU);
    fakeBusy = true;
    CanControl_Service(5000U); assert(sentCount == 18U && cana_tx_busy_count == 4U);
    fakeBusy = false;
    CanControl_Service(0xFFFFFFF0U);
    i = sentCount;
    CanControl_Service(984U); assert(sentCount == i + 4U); // 32-bit tick rollover, status + ANN status.
    Interrupt_disableGlobal();
    CanControl_Service(1984U);
    assert(Interrupt_enableGlobal()); // Service must preserve an already-masked caller.
    Inverter_Init();
    assert(cana_output_tx_count == 0U);
    INV_CMD.p_ref = 1.1f; INV_FEEDBACK.Pe = -999.0f; P_filter = 5123.5f;
    for(i = 1U; i <= 9U; i++) CanControl_CapturePower(i);
    assert(cana_power_sample_count == 0U);
    CanControl_CapturePower(10U);
    INV_CMD.p_ref = 0.0f; P_filter = 0.0f; // Capture must not use foreground-time values.
    sentCount = 0U; CanControl_Service(10U);
    assert(sentCount == 2U && sentId[0] == 0x206U && sentId[1] == 0x207U);
    assert(sent[0][0] == 10U && sent[1][0] == 10U);
    assert(fabsf(ReadFloat(sent[0] + 4) - 5500.0f) < 0.001f && ReadFloat(sent[1] + 4) == 5123.5f);
    assert(cana_power_tx_count == 1U);
    Inverter_Init();
    for(i = 1U; i <= 10U; i++) CanControl_CapturePower(i);
    sentCount = 0U; fakeBusySlot = 6;
    CanControl_Service(10U); CanControl_Service(10U);
    assert(sentCount == 1U && sentId[0] == 0x206U && cana_power_tx_count == 0U);
    fakeBusySlot = -1; CanControl_Service(10U);
    assert(sentCount == 2U && sentId[1] == 0x207U && cana_power_tx_count == 1U); // No duplicate Pref on half-pair retry.
    Inverter_Init();
    for(i = 1U; i <= 10000U; i++)
    {
        CanControl_CapturePower(i);
        if(i % 10U == 0U) { sentCount = 0U; CanControl_Service(i); }
    }
    assert(cana_power_sample_count == 1000U && cana_power_tx_count == 1000U && cana_power_drop_count == 0U);
    assert(cana_telemetry_cycle_count == 10U && cana_status_tx_count == 10U && cana_output_tx_count == 10U);
    Inverter_Init(); fakeBusy = true;
    for(i = 1U; i <= 800U; i++) CanControl_CapturePower(i);
    assert(cana_power_sample_count == 80U && cana_power_drop_count == 17U && cana_power_queue_peak == 63U);
    sentCount = 0U; CanControl_Service(800U); assert(sentCount == 0U);
    fakeBusy = false;
    for(i = 0U; i < 16U; i++) { sentCount = 0U; CanControl_Service(800U); }
    assert(cana_power_tx_count == 63U); // Bounded main-loop drain; no ISR hardware send.
    Inverter_Init();
    INV_CTRL.state = INV_GRID_CONNECTED; INV_CMD.run_enable = INV_CMD.grid_connect_cmd = 1U;
    assert(VsgAutoTest_Request(1U)); VsgAutoTest_Step(true,true);
    P_filter=123.0f;
    CanControl_CapturePower(77U); // Not a multiple of ten: t=0 still MUST be captured.
    sentCount=0U; CanControl_Service(77U);
    assert(sentCount==2U&&sent[0][0]==77U&&sent[1][0]==77U&&ReadFloat(sent[1]+4)==123.0f);
    assert(VSG_TEST.start_tick==77U&&(VSG_TEST.flags&VSG_TEST_STAMPED)!=0U);
    Inverter_Init(); cana_power_enable = 0U;
    for(i = 1U; i <= 100U; i++) CanControl_CapturePower(i);
    assert(cana_power_sample_count == 0U);
    puts("PASS 1ms power: ISR every10 ticks, coherent Pref/P_filter snapshot, forced t0 sample, timestamp pair, bounded queue/drop, actual J/D and experiment ACK at100ms");
    puts("PASS 0x205: measured dq phase peak, running-angle Hz, zero/nonfinite values, independent ANN status, fifth slot and telemetry enable");
    puts("PASS legacy float32 little-endian bytes, status flags/counter truncation, 100ms pacing, busy skip and tick rollover");
}
