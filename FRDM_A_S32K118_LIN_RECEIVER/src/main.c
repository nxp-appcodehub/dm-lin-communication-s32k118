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
#include "Gpt.h"
#include "Lin_43_LPUART_FLEXIO.h"
#include "Platform.h"
#include "CDD_Uart.h"
#include "uart_comm.h"
#include "Lpuart_Uart_Ip.h"
#include "IntCtrl_Ip_Cfg.h"
#include "OsIf.h"
#include "stdbool.h"
#include "lin_stack_cfg.h"
#include "lin_common_api.h"

/*==================================================================================================
 *                                      DEFINES AND MACROS
 ==================================================================================================*/

/* LIN channel index */
#define T_LinChannel_0              ((uint8)0)

/* GPT timeout period - equivalent to 500 us at the configured clock */
#define PIT_PERIOD                  20000

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
static void print_char_msg(const char *prefix, uint8_t ch, const char *suffix);
void        TMR_ISR(void);
void        lin_slave_task(void);

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
    lin_slave_task();

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
    Mcu_Init(NULL_PTR);
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
    Lin_43_LPUART_FLEXIO_Init(NULL_PTR);
    Lin_43_LPUART_FLEXIO_WakeupInternal(T_LinChannel_0);
    Lin_43_LPUART_FLEXIO_Wakeup(T_LinChannel_0);
}

/**
 * @brief GPT notification callback - invoked every 500 us.
 * @details Services the LIN timeout counter. The slave does not drive a schedule
 *          table; it responds to headers sent by the master.
 */
void TMR_ISR(void)
{
    lin_dal_timeout_service(LI0);
}

/**
 * @brief Build and transmit a UART message of the form: prefix + raw ASCII byte + suffix.
 * @param prefix  Null-terminated string printed before the character.
 * @param ch      Raw ASCII byte to embed in the message.
 * @param suffix  Null-terminated string printed after the character.
 */
static void print_char_msg(const char *prefix, uint8_t ch, const char *suffix)
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
 * @brief LIN slave application loop - signal-based communication.
 * @details Initializes the LIN interface, pre-loads an initial SlaveStatus value,
 *          then loops forever:
 *            1. Waits for the CommMasterCmd frame-received flag. The LIN stack sets
 *               LI0_CommMasterCmd_flag each time the master transmits the CommMasterCmd
 *               frame (ID 0x30) and the slave successfully receives it.
 *            2. Reads the MasterCommand signal, prints it over UART.
 *            3. Computes a reply (received + 1, wrapping 127 -> 32) and writes it
 *               into the SlaveStatus signal of the CommSlaveStatus frame (ID 0x33).
 *               The stack will include this value in the response when the master
 *               next requests CommSlaveStatus.
 */
void lin_slave_task(void)
{
    uint8_t rx_char;
    uint8_t tx_char;

    l_sys_init();
    l_ifc_init(LI0);

    /* Pre-load a valid initial status so the first CommSlaveStatus request from the
     * master gets a defined value even before the first MasterCommand is processed. */
    l_u8_wr(LI0_SlaveStatus, 32U);
    l_bool_wr(LI0_SlaveCommError, 0);

    for (;;)
    {
        /* Wait for the master to transmit CommMasterCmd and the stack to receive it.
         * The stack sets LI0_CommMasterCmd_flag on successful frame reception. */
        if (0U != (l_u8)l_flg_tst(LI0_CommMasterCmd_flag))
        {
            l_flg_clr(LI0_CommMasterCmd_flag);

            rx_char = l_u8_rd(LI0_MasterCommand);
            print_char_msg("Receiver received [", rx_char, "] from sender\r\n");

            /* Compute reply: received character + 1, wrapping 127 -> 32 */
            tx_char = (uint8_t)(rx_char + 1U);
            if (tx_char > 127U)
            {
                tx_char = 32U;
            }

            /* Write the reply into the SlaveStatus signal.  The stack will transmit
             * this value when the master next issues a CommSlaveStatus header (ID 0x33). */
            l_u8_wr(LI0_SlaveStatus, tx_char);
            l_bool_wr(LI0_SlaveCommError, 0);

            print_char_msg("Receiver sent [", tx_char, "] to sender\r\n");
        }
    }
}

#ifdef __cplusplus
}
#endif

/** @} */
