#include "can_port.h"
#include "app/communication/can_control.h"
#include "driverlib.h"
#include "device.h"
#include "mcan.h"

#define CAN_BASE MCANB_DRIVER_BASE
#define CAN_ERROR_IRQS (MCAN_INTR_SRC_BUS_OFF_STATUS | MCAN_INTR_SRC_PROTOCOL_ERR_ARB | MCAN_INTR_SRC_PROTOCOL_ERR_DATA | MCAN_INTR_SRC_MSG_RAM_ACCESS_FAILURE | MCAN_INTR_SRC_WARNING_STATUS | MCAN_INTR_SRC_ERR_PASSIVE)
#define CAN_IRQS (CAN_ERROR_IRQS | MCAN_INTR_SRC_DEDICATED_RX_BUFF_MSG)
#define CAN_WAIT_LIMIT 100000UL

volatile uint16_t cana_port_ready, cana_bus_off;
volatile uint32_t cana_init_error, cana_isr_status;
static uint32_t inFlight;
static MCAN_RxBufElement rx; // ISR-only; avoid the 64-byte driver element on the ISR stack.
static MCAN_TxBufElement tx; // Main-loop-only.
static __interrupt void MCANB_ISR(void);

static bool WaitMode(uint32_t mode)
{
    uint32_t i;
    for(i = 0U; i < CAN_WAIT_LIMIT; i++) if(MCAN_getOpMode(CAN_BASE) == mode) return true;
    return false;
}

bool CanPort_Init(void)
{
    MCAN_InitParams init = {0};
    MCAN_ConfigParams config = {0};
    MCAN_MsgRAMConfigParams ram = {0};
    MCAN_StdMsgIDFilterElement filter = {0};
    MCAN_BitTimingParams timing = {0};
    uint32_t i;
    cana_port_ready = cana_bus_off = 0U;
    cana_init_error = inFlight = 0U;
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_MCANB);
    SysCtl_setMCANClk(SYSCTL_MCANB, SYSCTL_MCANCLK_DIV_5);
    GPIO_setAnalogMode(CAN_PORT_RX_GPIO, GPIO_ANALOG_DISABLED); // GPIO33 also shares ADC B24/D1/E1.
    GPIO_setPinConfig(GPIO_58_MCANB_TX);
    GPIO_setPinConfig(GPIO_33_MCANB_RX);
    GPIO_setPadConfig(CAN_PORT_TX_GPIO, GPIO_PIN_TYPE_STD);
    GPIO_setPadConfig(CAN_PORT_RX_GPIO, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(CAN_PORT_RX_GPIO, GPIO_QUAL_ASYNC);
    for(i = 0U; i < CAN_WAIT_LIMIT; i++) if(MCAN_isMemInitDone(CAN_BASE)) break;
    if(i == CAN_WAIT_LIMIT) { cana_init_error = 1U; return false; }
    MCAN_setOpMode(CAN_BASE, MCAN_OPERATION_MODE_SW_INIT);
    if(!WaitMode(MCAN_OPERATION_MODE_SW_INIT)) { cana_init_error = 2U; return false; }
    init.wdcPreload = 0xFFU;
    init.darEnable = 0U; // Automatic retransmission, as on 28377D.
    // fdMode=brsEnable=tdcEnable=0: classic CAN only, no loopback.
    config.filterConfig.rrfe = config.filterConfig.rrfs = 1U;
    config.filterConfig.anfe = config.filterConfig.anfs = 2U; // Reject nonmatching IDs.
    // SYSCLK / 5 / 30 TQ = 1 Mbps. NBTP fields encode actual lengths minus one.
#if DEVICE_SYSCLK_FREQ != 150000000UL
#error Recalculate MCANB timing if the 150 MHz device clock changes.
#endif
    timing.nomRatePrescalar = 0U;
    timing.nomTimeSeg1 = 22U; // 23 TQ; sample point (1+23)/30 = 80%.
    timing.nomTimeSeg2 = 5U; // 6 TQ.
    timing.nomSynchJumpWidth = 3U; // 4 TQ.
    ram.flssa = 0U; ram.lss = 2U; // Two 4-byte exact-ID filters.
    ram.rxBufStartAddr = 0x20U; ram.rxBufElemSize = MCAN_ELEM_SIZE_8BYTES;
    ram.txStartAddr = 0x40U; ram.txBufNum = CAN_PORT_TX_SLOTS; ram.txBufElemSize = MCAN_ELEM_SIZE_8BYTES;
    if(MCAN_init(CAN_BASE, &init) != 0 || MCAN_config(CAN_BASE, &config) != 0 ||
       MCAN_setBitTime(CAN_BASE, &timing) != 0 || MCAN_msgRAMConfig(CAN_BASE, &ram) != 0)
    { cana_init_error = 3U; return false; }
    filter.sfec = MCAN_STDFILTEC_RXBUFF;
    filter.sfid1 = CANA_CONTROL_MSG_ID; filter.sfid2 = 0U;
    MCAN_addStdMsgIDFilter(CAN_BASE, 0U, &filter);
    filter.sfid1 = CANA_ESTOP_MSG_ID; filter.sfid2 = 1U;
    MCAN_addStdMsgIDFilter(CAN_BASE, 1U, &filter);
    MCAN_clearIntrStatus(CAN_BASE, MCAN_INTR_MASK_ALL);
    MCAN_selectIntrLine(CAN_BASE, CAN_IRQS, MCAN_INTR_LINE_NUM_0);
    MCAN_enableIntr(CAN_BASE, CAN_IRQS, 1U);
    MCAN_enableIntrLine(CAN_BASE, MCAN_INTR_LINE_NUM_0, 1U);
    Interrupt_register(INT_MCANB_0, &MCANB_ISR);
    Interrupt_enable(INT_MCANB_0);
    MCAN_setOpMode(CAN_BASE, MCAN_OPERATION_MODE_NORMAL);
    if(!WaitMode(MCAN_OPERATION_MODE_NORMAL)) { cana_init_error = 4U; return false; }
    cana_port_ready = 1U;
    return true;
}

void CanPort_Service(void)
{
    uint32_t completed;
    uint16_t i;
    if(cana_port_ready != 1U) return;
    completed = MCAN_getTxBufTransmissionStatus(CAN_BASE) & inFlight;
    for(i = 0U; i < CAN_PORT_TX_SLOTS; i++) if((completed & (1UL << i)) != 0U) cana_tx_count++;
    inFlight &= ~completed;
}

bool CanPort_TrySend(uint16_t slot, uint32_t id, const uint16_t data[8])
{
    uint16_t i;
    uint32_t mask;
    if(cana_port_ready != 1U || cana_bus_off != 0U || slot >= CAN_PORT_TX_SLOTS || id > 0x7FFU) return false;
    mask = 1UL << slot;
    if((inFlight & mask) != 0U || (MCAN_getTxBufReqPend(CAN_BASE) & mask) != 0U)
    { cana_tx_busy_count++; return false; }
    tx.id = id << 18U;
    tx.rtr = tx.xtd = tx.esi = tx.brs = tx.fdf = tx.efc = tx.mm = 0U;
    tx.dlc = 8U;
    for(i = 0U; i < 8U; i++) tx.data[i] = data[i] & 0xFFU;
    MCAN_writeMsgRam(CAN_BASE, MCAN_MEM_TYPE_BUF, slot, &tx);
    if(MCAN_txBufAddReq(CAN_BASE, slot) != 0) return false;
    inFlight |= mask;
    return true;
}

static __interrupt void MCANB_ISR(void)
{
    MCAN_RxNewDataStatus pending, clear = {0};
    MCAN_ProtocolStatus status;
    uint16_t pass, buffer;
    uint32_t flags = MCAN_getIntrStatus(CAN_BASE);
    cana_isr_status = flags;
    MCAN_clearIntrStatus(CAN_BASE, flags); // Clear first so a later arrival retriggers.
    if((flags & CAN_ERROR_IRQS) != 0U)
    {
        cana_error_flag++;
        cana_err_status = flags;
        MCAN_getProtocolStatus(CAN_BASE, &status);
        cana_bus_off = status.busOffStatus != 0U ? 1U : 0U;
    }
    MCAN_getNewDataStatus(CAN_BASE, &pending);
    for(pass = 0U; pass < 2U; pass++)
    {
        buffer = 1U - pass; // Emergency buffer before ordinary commands.
        if((pending.statusLow & (1UL << buffer)) == 0U) continue;
        MCAN_readMsgRam(CAN_BASE, MCAN_MEM_TYPE_BUF, buffer, MCAN_RX_FIFO_NUM_0, &rx);
        clear.statusLow = 1UL << buffer;
        MCAN_clearNewDataStatus(CAN_BASE, &clear);
        CanControl_Receive(rx.id >> 18U, rx.data, (uint16_t)rx.dlc, rx.xtd == 0U && rx.rtr == 0U && rx.fdf == 0U);
    }
    MCAN_clearInterrupt(CAN_BASE, 1U); // MCANSS line 0 W1C bit.
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}
