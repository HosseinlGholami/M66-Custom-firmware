/**
 * @file    uart.c
 * @brief   UART communication implementation
 * @author  Hossein Gholami
 * @date    2025-11-01
 */

#include "uart.h"
#include "ril.h"
#include "ril_util.h"
#include "ql_stdlib.h"
#include "ql_trace.h"

/*============================================================================
 * Private Data
 *===========================================================================*/
static Enum_SerialPort m_uart_port = UART_PORT1;
static u8 m_rx_buffer[UART_RX_BUFFER_LEN];
static UartRxCallback_t m_rx_callback = NULL;  /* Registered RX callback */

/* Debug buffer (extern in header) */
char uart_debug_buffer[UART_DEBUG_BUFFER_LEN];

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Read data from serial port
 * @param port Serial port to read from
 * @param buffer Buffer to store received data
 * @param buf_len Buffer length
 * @return Number of bytes read, negative on error
 */
static s32 read_serial_port(Enum_SerialPort port, u8* buffer, u32 buf_len)
{
    s32 read_len = 0;
    s32 total_len = 0;
    
    if (buffer == NULL || buf_len == 0) {
        return -1;
    }
    
    Ql_memset(buffer, 0x0, buf_len);
    
    while (1) {
        read_len = Ql_UART_Read(port, buffer + total_len, buf_len - total_len);
        if (read_len <= 0) {
            break;  /* All data read or error */
        }
        total_len += read_len;
    }
    
    if (read_len < 0) {
        APP_DEBUG("ERROR: Failed to read from UART port[%d]\r\n", port);
        return -99;
    }
    
    return total_len;
}

/**
 * @brief AT command response handler
 * @param line Response line
 * @param len Line length
 * @param user_data User data (unused)
 * @return RIL response status
 */
static s32 at_response_handler(char* line, u32 len, void* user_data)
{
    /* Echo response back to UART */
    Ql_UART_Write(m_uart_port, (u8*)line, len);
    
    /* Check for response patterns */
    if (Ql_RIL_FindLine(line, len, "OK")) {
        return RIL_ATRSP_SUCCESS;
    }
    else if (Ql_RIL_FindLine(line, len, "ERROR")) {
        return RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CME ERROR")) {
        return RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CMS ERROR:")) {
        return RIL_ATRSP_FAILED;
    }
    
    return RIL_ATRSP_CONTINUE;  /* Continue waiting */
}

/**
 * @brief UART event callback handler
 * @param port UART port
 * @param event Event type
 * @param level Event level
 * @param customized_para Custom parameter
 */
static void uart_callback_handler(Enum_SerialPort port, Enum_UARTEventType event, 
                                  bool level, void* customized_para)
{
    switch (event)
    {
    case EVENT_UART_READY_TO_READ:
        if (m_uart_port == port) {
            s32 total_bytes = read_serial_port(port, m_rx_buffer, sizeof(m_rx_buffer));
            
            if (total_bytes <= 0) {
                APP_DEBUG("WARNING: No data in UART buffer\r\n");
                return;
            }
            
            /* If callback is registered, use it instead of AT handler */
            if (m_rx_callback != NULL) {
                m_rx_callback(m_rx_buffer, total_bytes);
                return;
            }
            
            /* Default behavior: Echo and send as AT command */
            Ql_UART_Write(m_uart_port, m_rx_buffer, total_bytes);
            
            /* Remove trailing CR/LF */
            char* p_crlf = Ql_strstr((char*)m_rx_buffer, "\r\n");
            if (p_crlf) {
                *p_crlf = '\0';
            }
            
            /* Ignore empty lines */
            if (Ql_strlen((char*)m_rx_buffer) == 0) {
                return;
            }
            
            /* Send as AT command */
            Ql_RIL_SendATCmd((char*)m_rx_buffer, total_bytes, 
                            at_response_handler, NULL, 0);
        }
        break;
        
    case EVENT_UART_READY_TO_WRITE:
        /* UART ready to write more data */
        break;
        
    default:
        break;
    }
}

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

s32 uart_init(Enum_SerialPort port, u32 baudrate)
{
    s32 ret;
    
    m_uart_port = port;
    
    /* Register UART callback */
    ret = Ql_UART_Register(port, uart_callback_handler, NULL);
    if (ret < 0) {
        Ql_Debug_Trace("ERROR: Failed to register UART port[%d], ret=%d\r\n", 
                      port, ret);
        return ret;
    }
    
    /* Open UART port */
    ret = Ql_UART_Open(port, baudrate, FC_NONE);
    if (ret < 0) {
        Ql_Debug_Trace("ERROR: Failed to open UART port[%d], ret=%d\r\n", 
                      port, ret);
        return ret;
    }
    
    APP_DEBUG("✅ UART initialized: port=%d, baudrate=%d\r\n", port, baudrate);
    
    return 0;
}

s32 uart_write(Enum_SerialPort port, const u8 *data, u32 len)
{
    if (data == NULL || len == 0) {
        return -1;
    }
    
    return Ql_UART_Write(port, (u8*)data, len);
}

s32 uart_write_string(Enum_SerialPort port, const char *str)
{
    if (str == NULL) {
        return -1;
    }
    
    return uart_write(port, (const u8*)str, Ql_strlen(str));
}

Enum_SerialPort uart_get_port(void)
{
    return m_uart_port;
}

void uart_register_callback(UartRxCallback_t callback)
{
    m_rx_callback = callback;
    
    if (callback != NULL) {
        APP_DEBUG("✅ UART RX callback registered\r\n");
    } else {
        APP_DEBUG("ℹ️  UART RX callback removed (back to AT mode)\r\n");
    }
}

