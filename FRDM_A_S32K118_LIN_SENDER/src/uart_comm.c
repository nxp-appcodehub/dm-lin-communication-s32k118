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

/**
 *   @file uart_comm.c
 *
 *   @addtogroup uart_comm uart communication documentation
 *   @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
 *                                        INCLUDE FILES
 ==================================================================================================*/
#include "uart_comm.h"
#include "CDD_Uart.h"
#include <string.h>

/*==================================================================================================
 *                                       GLOBAL FUNCTIONS
 ==================================================================================================*/

/**
 * @brief Send a null terminated string over the console UART (blocking).
 * @param str String to transmit.
 */
void SendString(const char *str)
{
    Std_ReturnType status;
    Uart_StatusType transmitStatus;
    uint32 bytesRemaining;

    status = Uart_AsyncSend(UART_LPUART_INTERNAL_CHANNEL,
                           (const uint8 *)str,
                           strlen(str));

    if (E_OK == status)
    {
        /* Wait until transmission completes */
        do
        {
            transmitStatus = Uart_GetStatus(UART_LPUART_INTERNAL_CHANNEL,
                                           &bytesRemaining,
                                           UART_SEND);
        } while (UART_STATUS_NO_ERROR != transmitStatus);
    }
}

/**
 * @brief Start a one byte asynchronous receive on the console channel.
 * @param pByte Destination for the received byte.
 * @return E_OK if the receive was started successfully.
 */
Std_ReturnType Uart_StartReceiveByte(uint8 *pByte)
{
    return Uart_AsyncReceive(UART_LPUART_INTERNAL_CHANNEL, pByte, 1U);
}

/**
 * @brief Check whether the pending one byte receive has completed.
 * @return TRUE when a byte has been received since the last start.
 */
boolean Uart_ReceiveByteReady(void)
{
    Uart_StatusType receiveStatus;
    uint32 bytesRemaining;

    receiveStatus = Uart_GetStatus(UART_LPUART_INTERNAL_CHANNEL,
                                   &bytesRemaining,
                                   UART_RECEIVE);

    /* Complete when the driver is idle with no bytes still outstanding. */
    return ((UART_STATUS_NO_ERROR == receiveStatus) && (0U == bytesRemaining))
               ? TRUE : FALSE;
}

#ifdef __cplusplus
}
#endif

/** @} */

