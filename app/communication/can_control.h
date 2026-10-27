#ifndef CAN_CONTROL_H_
#define CAN_CONTROL_H_
#include <stdbool.h>
#include <stdint.h>

// Legacy 28377D wire protocol; the cana_ names are retained for CCS/host compatibility.
#define CANA_CONTROL_MSG_ID 0x200U
#define CANA_ESTOP_MSG_ID 0x100U
#define CANA_TELEMETRY_RL_MSG_ID 0x201U
#define CANA_TELEMETRY_JD_MSG_ID 0x202U
#define CANA_STATUS_MSG_ID 0x203U
#define CANA_ANN_STATUS_MSG_ID 0x204U
#define CANA_OUTPUT_MSG_ID 0x205U
#define CANA_POWER_REF_MSG_ID 0x206U
#define CANA_POWER_MSG_ID 0x207U
#define CANA_ACTUAL_JD_MSG_ID 0x208U
#define CANA_EXPERIMENT_MSG_ID 0x209U
#define CANA_REACTIVE_STATUS_MSG_ID 0x20AU
#define CANA_REACTIVE_POWER_MSG_ID 0x20BU
#define CANA_COMMAND_CONST_V 0x10U
#define CANA_COMMAND_CONST_Q 0x11U
#define CANA_COMMAND_DROOP 0x12U
#define CANA_COMMAND_NONE 0U
#define CANA_COMMAND_ISLAND_START 1U
#define CANA_COMMAND_PRESYNC_START 2U
#define CANA_COMMAND_GRID_CONNECT 3U
#define CANA_COMMAND_CLEAR_ESTOP 4U
#define CANA_COMMAND_TEST_FIXED 5U
#define CANA_COMMAND_TEST_ADAPTIVE 6U
#define CANA_COMMAND_EMERGENCY_STOP 255U
#define CANA_COMMAND_RESULT_NONE 0U
#define CANA_COMMAND_RESULT_ACCEPTED 1U
#define CANA_COMMAND_RESULT_INVALID_STATE 2U
#define CANA_COMMAND_RESULT_FAULT_LATCHED 3U

extern volatile uint16_t cana_telemetry_enable, cana_status_enable;
extern volatile uint16_t cana_last_rx_command, cana_pending_command, cana_command_pending;
extern volatile uint16_t cana_emergency_stop_pending, cana_emergency_stop_latched;
extern volatile uint16_t cana_last_frame_valid, cana_last_processed_command, cana_command_result;
extern volatile uint32_t cana_rx_count, cana_tx_count, cana_error_flag, cana_err_status;
extern volatile uint32_t cana_control_frame_count, cana_estop_frame_count;
extern volatile uint32_t cana_command_valid_count, cana_command_invalid_count;
extern volatile uint32_t cana_command_accepted_count, cana_command_rejected_count;
extern volatile uint32_t cana_telemetry_cycle_count, cana_status_tx_count, cana_tx_busy_count;
extern volatile uint32_t cana_output_tx_count;
extern volatile uint16_t cana_power_enable;
extern volatile uint32_t cana_power_sample_count, cana_power_tx_count, cana_power_drop_count;
extern volatile uint16_t cana_power_queue_peak;
extern uint16_t cana_status_tx_data[8];

void CanControl_Init(void);
// MCAN ISR: validate/copy only; one uint16_t slot holds one wire byte.
void CanControl_Receive(uint32_t id, const uint16_t *data, uint16_t dlc, bool standardData);
// Control ISR, before state selection: apply requests and enforce the stop latch.
void CanControl_ProcessPending(void);
// Main loop only; coherent snapshot, then nonblocking hardware sends with IRQs enabled.
void CanControl_Service(uint32_t controlTick);
// End of 10 kHz ISR: capture every 10th tick; no MCAN calls or waits in the ISR.
void CanControl_CapturePower(uint32_t controlTick);
// Caller owns synchronization; three legacy 8-byte payloads, in 201/202/203 order.
void CanControl_BuildTelemetry(uint16_t frames[3][8]);
void CanControl_BuildAnnStatus(uint16_t data[8]); // Version, result status, flags, NPU status, uint32 count.
// 0x205: measured capacitor phase-peak voltage (V), inverter running-angle frequency (Hz).
void CanControl_BuildOutputTelemetry(uint16_t data[8], float vd, float vq, float omega);
#endif
