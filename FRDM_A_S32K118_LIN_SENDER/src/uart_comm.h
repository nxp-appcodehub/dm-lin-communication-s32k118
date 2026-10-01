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

#ifndef UART_COMM_H
#define UART_COMM_H

/**
 *   @file uart_comm.h
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
#include "Std_Types.h"

/*==================================================================================================
 *                                      DEFINES AND MACROS
 ==================================================================================================*/

/* UART channel used for console output */
#define UART_LPUART_INTERNAL_CHANNEL  0

/*==================================================================================================
 *                                   FUNCTION PROTOTYPES
 ==================================================================================================*/

/**
 * @brief Send a null-terminated string over the console UART (blocking).
 * @param str String to transmit.
 */
void SendString(const char *str);

/**
 * @brief Start a one-byte asynchronous receive on the console channel.
 * @param pByte Destination for the received byte.
 * @return E_OK if the receive was started successfully.
 */
Std_ReturnType Uart_StartReceiveByte(uint8 *pByte);

/**
 * @brief Check whether the pending one-byte receive has completed.
 * @return TRUE when a byte has been received since the last start.
 */
boolean Uart_ReceiveByteReady(void);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* UART_COMM_H */
