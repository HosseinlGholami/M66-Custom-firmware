/**
 * @file    uart.h
 * @brief   UART communication module
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * Handles UART initialization, data transmission, and reception.
 * Provides debug output and AT command interface.
 */

#ifndef UART_H
#define UART_H

#include "ql_type.h"
#include "ql_uart.h"

/*============================================================================
 * Constants
 *===========================================================================*/
#define UART_RX_BUFFER_LEN      2048
#define UART_DEBUG_BUFFER_LEN   512

/*============================================================================
 * Debug Macros
 *===========================================================================*/
#define DEBUG_ENABLE 1

#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT1
extern char uart_debug_buffer[UART_DEBUG_BUFFER_LEN];

#define APP_DEBUG(FORMAT,...) {\
    Ql_memset(uart_debug_buffer, 0, UART_DEBUG_BUFFER_LEN);\
    Ql_sprintf(uart_debug_buffer, FORMAT, ##__VA_ARGS__); \
    if (UART_PORT2 == (DEBUG_PORT)) \
    {\
        Ql_Debug_Trace(uart_debug_buffer);\
    } else {\
        Ql_UART_Write((Enum_SerialPort)(DEBUG_PORT), (u8*)(uart_debug_buffer), \
                     Ql_strlen((const char *)(uart_debug_buffer)));\
    }\
}
#else
#define APP_DEBUG(FORMAT,...) 
#endif

/*============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief UART RX callback function type
 * Called when data is received on UART
 * @param data Pointer to received data
 * @param len Length of received data
 */
typedef void (*UartRxCallback_t)(u8* data, u32 len);

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize UART module
 * Registers and opens UART port for communication
 * @param port UART port to use (UART_PORT1, UART_PORT2, etc.)
 * @param baudrate Baud rate (e.g., 115200)
 * @return 0 on success, negative on error
 */
s32 uart_init(Enum_SerialPort port, u32 baudrate);

/**
 * @brief Register callback for UART RX data
 * When callback is registered, received data is passed to callback instead of AT handler
 * @param callback Callback function (NULL to remove callback)
 */
void uart_register_callback(UartRxCallback_t callback);

/**
 * @brief Write data to UART
 * @param port UART port
 * @param data Data buffer to send
 * @param len Length of data
 * @return Number of bytes written, negative on error
 */
s32 uart_write(Enum_SerialPort port, const u8 *data, u32 len);

/**
 * @brief Write string to UART
 * @param port UART port
 * @param str Null-terminated string to send
 * @return Number of bytes written, negative on error
 */
s32 uart_write_string(Enum_SerialPort port, const char *str);

/**
 * @brief Get configured UART port
 * @return Current UART port
 */
Enum_SerialPort uart_get_port(void);

#endif /* UART_H */

