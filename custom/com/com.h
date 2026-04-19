/**
 * @file    com.h
 * @brief   Command interface for UART/SMS parameter control
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * Simple text-based command protocol for parameter control over UART/SMS.
 * Commands are compact, easy to parse, and SMS-friendly.
 * 
 * Command Format: CMD,KEY,VALUE!
 * 
 * Commands:
 *   S,<key>,<value>!  - Set parameter (key = number or name)
 *   G,<key>!          - Get parameter
 *   L!                - List all parameters
 *   ?!                - Help (show command list)
 * 
 * Examples:
 *   S,4,1!            - Set PARAM_IO_STATE (enum value 4) to 1
 *   S,mqtt_port,1883! - Set parameter by name to 1883
 *   G,4!              - Get PARAM_IO_STATE value
 *   L!                - Print all parameters
 */

#ifndef COM_H
#define COM_H

#include "ql_type.h"

/*============================================================================
 * Constants
 *===========================================================================*/
#define COM_CMD_MAX_LEN     128     /* Maximum command length */
#define COM_RESPONSE_MAX    256     /* Maximum response length */
#define COM_CMD_DELIMITER   ','     /* Field separator */
#define COM_CMD_TERMINATOR  '!'     /* Command terminator */

/*============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Command result codes
 */
typedef enum {
    COM_OK = 0,                 /* Command executed successfully */
    COM_ERR_INVALID_CMD,        /* Unknown command */
    COM_ERR_INVALID_KEY,        /* Invalid parameter key */
    COM_ERR_INVALID_VALUE,      /* Invalid value for parameter type */
    COM_ERR_PARAM_ERROR,        /* Parameter system error */
    COM_ERR_PARSE_ERROR,        /* Command parsing error */
    COM_ERR_TOO_LONG            /* Command too long */
} ComResult_e;

/**
 * @brief Command response callback
 * Called when a command generates a response (for UART output or SMS reply)
 * 
 * @param response Response string
 * @param len Length of response
 */
typedef void (*ComResponseCallback_t)(const char* response, u32 len);

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize command interface
 * Sets up the command parser and registers with UART
 * 
 * @param response_callback Optional callback for command responses (NULL = use APP_DEBUG)
 * @return 0 on success, negative on error
 */
s32 com_init(ComResponseCallback_t response_callback);

/**
 * @brief Process a command string
 * Parses and executes the command, generates response
 * 
 * @param cmd Command string (null-terminated)
 * @param len Length of command string
 * @return COM_OK on success, error code on failure
 * 
 * @example
 *   com_process_command("S,4,1!", 6);        // Set PARAM_IO_STATE to 1
 *   com_process_command("G,mqtt_port!", 13); // Get MQTT port
 */
s32 com_process_command(const char* cmd, u32 len);
s32 com_process_command_with_callback(const char* cmd, u32 len, ComResponseCallback_t callback);

/* Note: com_send_response removed due to Ql_vsnprintf bug in SDK.
 * Use Ql_sprintf + callback or APP_DEBUG directly instead. */

/**
 * @brief Get command result string
 * Converts result code to human-readable string
 * 
 * @param result Result code
 * @return Result string
 */
const char* com_result_string(ComResult_e result);

#endif /* COM_H */

