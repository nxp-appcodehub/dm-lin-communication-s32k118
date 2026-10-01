/*==================================================================================================
* Project : RTD AUTOSAR 4.9
* Platform : CORTEXM
* Peripheral : S32K3XX
* Dependencies : none
*
* Autosar Version : 4.9.0
* Autosar Revision : ASR_REL_4_9_REV_0000
* Autosar Conf.Variant :
* SW Version : 7.0.1
* Build Version : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
* Copyright 2026 NXP
*
*   NXP Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
 *                                        INCLUDE FILES
 ==================================================================================================*/
#include "Mcal.h"
#include "Mcu.h"
#include "Port.h"
#include "CDD_Uart.h"
#include "Lpuart_Uart_Ip_Irq.h"
#include "uart_comm.h"
#include "Gpt.h"
#include "Lin_43_LPUART_FLEXIO.h"
#include "Platform.h"
#include "IntCtrl_Ip_Cfg.h"
#include "Lpuart_Uart_Ip.h"
#include "stdbool.h"
#include "lin_stack_cfg.h"
#include "lin_common_api.h"
#include "OsIf.h"

/*==================================================================================================
 *                                      DEFINES AND MACROS
 ==================================================================================================*/

/* LIN channel index */
#define T_LinChannel_0  ((uint8)0)

/* GPT timeout period - equivalent to 500 us at the configured clock */
#define PIT_PERIOD      15000U

/*==================================================================================================
 *                                      GLOBAL VARIABLES
 ==================================================================================================*/

volatile int exit_code = 0;

/*==================================================================================================
 *                                   LOCAL FUNCTION PROTOTYPES
 ==================================================================================================*/

static void MCU_Setup(void);
static void TMR_Init(void);
static void Lin_PHY_Init(void);
static void DelayMs(uint32 ms);
static void print_char_msg(const char *prefix, l_u8 ch, const char *suffix);
void        TMR_ISR(void);
void        lin_master_task(void);

/*==================================================================================================
 *                                       MAIN FUNCTION
 ==================================================================================================*/

/*!
 * \brief The main function for the project.
 */
int main(void)
{
    MCU_Setup();
    Uart_Init(NULL_PTR);
    TMR_Init();
    Lin_PHY_Init();
    lin_master_task();

    for (;;)
    {
        if (exit_code != 0)
        {
            break;
        }
    }
    return exit_code;
}

/*==================================================================================================
 *                                       LOCAL FUNCTIONS
 ==================================================================================================*/

/**
 * @brief Initialize the system clock and install platform interrupt handlers.
 */
static void MCU_Setup(void)
{
    Mcu_Init(&Mcu_PreCompileConfig);
    Mcu_InitClock(McuClockSettingConfig_0);
    Mcu_SetMode(McuModeSettingConf_0);
    Port_Init(NULL_PTR);
    Platform_Init(NULL_PTR);
    Platform_InstallIrqHandler(LPUART0_RxTx_IRQn, &LPUART0_LIN_IP_RxTx_IRQHandler, NULL_PTR);
    Platform_InstallIrqHandler(LPUART1_RxTx_IRQn, &LPUART_UART_IP_1_IRQHandler, NULL_PTR);
    Platform_SetIrqPriority(LPUART1_RxTx_IRQn, 3U);
    Platform_SetIrq(LPUART1_RxTx_IRQn, TRUE);
    Platform_InstallIrqHandler(LPIT_IRQn, &LPIT_0_ISR, NULL_PTR);
    OsIf_Init(NULL_PTR);
}

/**
 * @brief Initialize the GPT timer and enable the periodic LIN timebase notification.
 */
static void TMR_Init(void)
{
    Gpt_Init(&Gpt_Config);
    Gpt_StartTimer(GptConf_GptChannelConfiguration_GptChannelConfiguration_0, PIT_PERIOD);
    Gpt_EnableNotification(GptConf_GptChannelConfiguration_GptChannelConfiguration_0);
}

/**
 * @brief Initialize the LIN physical interface and issue a wakeup pulse on the bus.
 */
static void Lin_PHY_Init(void)
{
    Lin_43_LPUART_FLEXIO_Init(&Lin_43_LPUART_FLEXIO_xConfig);
    Lin_43_LPUART_FLEXIO_WakeupInternal(T_LinChannel_0);
    Lin_43_LPUART_FLEXIO_Wakeup(T_LinChannel_0);
}

/**
 * @brief GPT notification callback - invoked every 500 us.
 * @details Services the LIN timeout counter. Every 5 ms (10 ticks) the master
 *          schedule tick l_sch_tick() is called to advance the LIN schedule table,
 *          transmitting CommMasterCmd then requesting CommSlaveStatus alternately.
 */
void TMR_ISR(void)
{
    static uint32_t interruptCount = 0UL;

    lin_dal_timeout_service(LI0);

    if (++interruptCount > 9UL)
    {
        l_sch_tick(LI0);
        interruptCount = 0UL;
    }
}

/**
 * @brief Build and transmit a UART message of the form: prefix + raw ASCII byte + suffix.
 * @param prefix  Null-terminated string printed before the character.
 * @param ch      Raw ASCII byte to embed in the message.
 * @param suffix  Null-terminated string printed after the character.
 */
static void print_char_msg(const char *prefix, l_u8 ch, const char *suffix)
{
    char buf[64];
    uint32_t i = 0U;
    const char *p;

    for (p = prefix; *p != '\0'; p++)
    {
        buf[i++] = *p;
    }
    buf[i++] = (char)ch;
    for (p = suffix; *p != '\0'; p++)
    {
        buf[i++] = *p;
    }
    buf[i] = '\0';

    SendString(buf);
}

/**
 * @brief LIN master application loop - signal-based communication.
 * @details Initializes the LIN interface and activates the NormalTable schedule, then
 *          loops forever:
 *            1. Writes a cycling ASCII character (0x20..0x7F, wrapping) into the
 *               MasterCommand signal of the CommMasterCmd frame (ID 0x30).
 *            2. l_sch_tick() (driven by TMR_ISR every 5 ms) transmits CommMasterCmd
 *               and then issues the CommSlaveStatus header (ID 0x33) to the slave.
 *            3. Waits for the CommSlaveStatus frame-received flag, then reads the
 *               SlaveStatus signal (the slave's reply) and prints both values over UART.
 */
void lin_master_task(void)
{
    l_u8 tx_char = 32U;
    l_u8 rx_val;

    l_sys_init();
    l_ifc_init(LI0);
    l_sch_set(LI0, LI0_NormalTable, 0u);

    for (;;)
    {
        /* Clear any stale CommSlaveStatus received flag from the previous cycle,
         * then publish the new MasterCommand value into the frame buffer.
         * The LIN scheduler (l_sch_tick in TMR_ISR) will transmit CommMasterCmd
         * on its next slot and request CommSlaveStatus immediately after. */
        l_flg_clr(LI0_CommSlaveStatus_flag);
        l_u8_wr(LI0_MasterCommand, tx_char);

        print_char_msg("Sender sent [", tx_char, "] to receiver\r\n");

        /* Spin until the CommSlaveStatus frame has been received from the slave.
         * The stack sets LI0_CommSlaveStatus_flag on successful reception. */
        while (0U == (l_u8)l_flg_tst(LI0_CommSlaveStatus_flag))
        {
            /* driven by l_sch_tick() in TMR_ISR */
        }
        l_flg_clr(LI0_CommSlaveStatus_flag);

        rx_val = l_u8_rd(LI0_SlaveStatus);
        print_char_msg("Sender received [", rx_val, "] from receiver\r\n");

        tx_char++;
        if (tx_char > 127U)
        {
            tx_char = 32U;
        }

        DelayMs(200);
    }
}

/**
 * @brief Millisecond delay function using OsIf timer.
 * @param ui32TimeoutMs  Delay time in milliseconds.
 */
static void DelayMs(uint32 ui32TimeoutMs)
{
    uint32 ui32CurTime     = OsIf_GetCounter(OSIF_COUNTER_SYSTEM);
    uint32 ui32ElapsedTicks = 0U;
    uint32 ui32TimeoutTicks = OsIf_MicrosToTicks(ui32TimeoutMs * 1000U, OSIF_COUNTER_SYSTEM);

    while (ui32ElapsedTicks < ui32TimeoutTicks)
    {
        ui32ElapsedTicks += OsIf_GetElapsed(&ui32CurTime, OSIF_COUNTER_SYSTEM);
    }
}

#ifdef __cplusplus
}
#endif

/** @} */
