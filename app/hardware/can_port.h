#ifndef CAN_PORT_H_
#define CAN_PORT_H_
#include <stdbool.h>
#include <stdint.h>

// User-confirmed MCANB wiring. SysConfig reserves these two GPIOs.
#define CAN_PORT_TX_GPIO 58U
#define CAN_PORT_RX_GPIO 33U
#define CAN_PORT_TX_SLOTS 11U // Existing nine + reactive-mode status and Q/Qref.
extern volatile uint16_t cana_port_ready, cana_bus_off;
extern volatile uint32_t cana_init_error, cana_isr_status;
bool CanPort_Init(void); // Once after Board_init, before EINT; never initializes PIE globally.
void CanPort_Service(void); // Main loop: count acknowledged TX completions.
bool CanPort_TrySend(uint16_t slot, uint32_t id, const uint16_t data[8]);
#endif
