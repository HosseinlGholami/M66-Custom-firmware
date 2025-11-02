/**
 * @file    com.c
 * @brief   Command interface implementation
 * @author  Hossein Gholami
 * @date    2025-11-01
 */

#include "com.h"
#include "param/param.h"
#include "uart/uart.h"
#include "ql_stdlib.h"
#include "ql_system.h"  /* For mutex functions */
/* Note: <stdarg.h> removed - Ql_vsnprintf is broken, using Ql_sprintf instead */

/*============================================================================
 * Private Data
 *===========================================================================*/
static ComResponseCallback_t response_callback = NULL;
static bool com_initialized = FALSE;

/* Global response buffer (in data segment, not stack) */
static char g_response_buffer[COM_RESPONSE_MAX];

/* Mutex for thread-safe access to response buffer */
static u32 g_response_mutex = 0;

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Simple string tokenizer (replacement for strtok)
 * @param str String to tokenize (NULL for subsequent calls)
 * @param delim Delimiter character
 * @return Pointer to next token, or NULL if no more tokens
 */
static char* simple_strtok(char* str, char delim)
{
    static char* next_token = NULL;
    char* token_start;
    
    /* Use provided string or continue with previous */
    if (str != NULL) {
        next_token = str;
    }
    
    /* No more tokens */
    if (next_token == NULL || *next_token == '\0') {
        return NULL;
    }
    
    /* Find start of token (skip leading delimiters) */
    while (*next_token == delim) {
        next_token++;
    }
    
    if (*next_token == '\0') {
        return NULL;
    }
    
    token_start = next_token;
    
    /* Find end of token */
    while (*next_token != '\0' && *next_token != delim) {
        next_token++;
    }
    
    /* Null-terminate token if not at end of string */
    if (*next_token != '\0') {
        *next_token = '\0';
        next_token++;
    }
    
    return token_start;
}

/**
 * @brief Send a pre-formatted response string (THREAD-SAFE)
 * 
 * NOTE: This function does NOT use Ql_vsnprintf (it's broken in this SDK).
 *       Callers must format their strings using Ql_sprintf before calling.
 * 
 * @param str The pre-formatted null-terminated string to send
 */
static void send_response(const char* str)
{
    u32 len;

    if (str == NULL) {
        return;
    }

    /* Lock mutex for thread-safe UART access */
    if (g_response_mutex != 0) {
        Ql_OS_TakeMutex(g_response_mutex);
    }

    len = Ql_strlen(str);

    /* Send response */
    if (response_callback != NULL) {
        response_callback(str, len);
    } else {
        APP_DEBUG("%s", str);  /* Direct output, no formatting */
    }

    /* Unlock mutex */
    if (g_response_mutex != 0) {
        Ql_OS_GiveMutex(g_response_mutex);
    }
}

/**
 * @brief Find parameter key by name
 * @return ParamKey_e on success, PARAM_MAX_COUNT on error
 */
static ParamKey_e find_param_by_name(const char* name)
{
    ParamKey_e key;
    
    for (key = 0; key < PARAM_MAX_COUNT; key++) {
        if (Ql_strcmp(param_get_name(key), name) == 0) {
            return key;
        }
    }
    
    return PARAM_MAX_COUNT;
}

/**
 * @brief Parse parameter key from string (number or name)
 * @return ParamKey_e on success, PARAM_MAX_COUNT on error
 */
static ParamKey_e parse_param_key(const char* key_str)
{
    s32 key_num;
    
    /* Try to parse as number first */
    key_num = Ql_atoi(key_str);
    if (key_num >= 0 && key_num < PARAM_MAX_COUNT) {
        return (ParamKey_e)key_num;
    }
    
    /* Try to find by name */
    return find_param_by_name(key_str);
}

/**
 * @brief Command: Set parameter
 * Format: S,<key>,<value>!
 */
static ComResult_e cmd_set_parameter(const char* key_str, const char* value_str)
{
    ParamKey_e key;
    s32 result;
    
    /* Parse key */
    key = parse_param_key(key_str);
    if (key >= PARAM_MAX_COUNT) {
        Ql_sprintf(g_response_buffer, "ERROR: Invalid parameter key: %s\r\n", key_str);
        send_response(g_response_buffer);
        return COM_ERR_INVALID_KEY;
    }
    
    /* Get parameter type and set value */
    switch (param_get_name(key)[0]) { /* Hacky way to get type - improve later */
        case 'i': /* int type parameters */
        case 'n': /* net_rssi */
        case 't': /* task_counter */
        case 's': /* sensor_temp */
        case 'g': /* gps_lat, gps_lon */
        {
            /* Determine type by trying param_config lookup */
            /* For now, try int32 first, then int16, then int8 */
            s32 value = Ql_atoi(value_str);
            
            /* Try int8 first (most common) */
            result = param_set_int8(key, (s8)value);
            if (result == -3) { /* Type mismatch */
                /* Try int16 */
                result = param_set_int16(key, (s16)value);
                if (result == -3) {
                    /* Try int32 */
                    result = param_set_int32(key, value);
                }
            }
            
            if (result == 0) {
                Ql_sprintf(g_response_buffer, "OK: %s = %d\r\n", param_get_name(key), value);
                send_response(g_response_buffer);
                return COM_OK;
            }
            break;
        }
        
        default: /* Assume string type */
            result = param_set_string(key, value_str);
            if (result == 0) {
                Ql_sprintf(g_response_buffer, "OK: %s = %s\r\n", param_get_name(key), value_str);
                send_response(g_response_buffer);
                return COM_OK;
            }
            break;
    }
    
    send_response("ERROR: Failed to set parameter\r\n");
    return COM_ERR_PARAM_ERROR;
}

/**
 * @brief Command: Get parameter
 * Format: G,<key>!
 */
static ComResult_e cmd_get_parameter(const char* key_str)
{
    ParamKey_e key;
    s8 i8_val;
    s16 i16_val;
    s32 i32_val;
    char str_val[256];
    s32 result;
    
    /* Parse key */
    key = parse_param_key(key_str);
    if (key >= PARAM_MAX_COUNT) {
        Ql_sprintf(g_response_buffer, "ERROR: Invalid parameter key: %s\r\n", key_str);
        send_response(g_response_buffer);
        return COM_ERR_INVALID_KEY;
    }
    
    /* Try to get value by type */
    result = param_get_int8(key, &i8_val);
    if (result == 0) {
        Ql_sprintf(g_response_buffer, "%s = %d\r\n", param_get_name(key), i8_val);
        send_response(g_response_buffer);
        return COM_OK;
    }
    
    result = param_get_int16(key, &i16_val);
    if (result == 0) {
        Ql_sprintf(g_response_buffer, "%s = %d\r\n", param_get_name(key), i16_val);
        send_response(g_response_buffer);
        return COM_OK;
    }
    
    result = param_get_int32(key, &i32_val);
    if (result == 0) {
        Ql_sprintf(g_response_buffer, "%s = %d\r\n", param_get_name(key), i32_val);
        send_response(g_response_buffer);
        return COM_OK;
    }
    
    result = param_get_string(key, str_val, sizeof(str_val));
    if (result == 0) {
        Ql_sprintf(g_response_buffer, "%s = %s\r\n", param_get_name(key), str_val);
        send_response(g_response_buffer);
        return COM_OK;
    }
    
    send_response("ERROR: Failed to get parameter\r\n");
    return COM_ERR_PARAM_ERROR;
}

/**
 * @brief Command: Commit parameters to NVRAM
 * Format: C!
 */
static ComResult_e cmd_commit(void)
{
    s32 saved = param_commit();
    
    if (saved >= 0) {
        Ql_sprintf(g_response_buffer, "OK: Saved %d parameters to NVRAM\r\n", saved);
        send_response(g_response_buffer);
        return COM_OK;
    }
    
    send_response("ERROR: Failed to commit parameters\r\n");
    return COM_ERR_PARAM_ERROR;
}

/**
 * @brief Command: List all parameters
 * Format: L!
 */
static ComResult_e cmd_list_parameters(void)
{
    u32 i;
    s8 i8_val;
    s16 i16_val;
    s32 i32_val;
    char str_val[256];
    
    Ql_sprintf(g_response_buffer, "\r\n=== Parameters (%d total) ===\r\n", PARAM_MAX_COUNT);
    send_response(g_response_buffer);
    
    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        /* Try each type */
        if (param_get_int8((ParamKey_e)i, &i8_val) == 0) {
            Ql_sprintf(g_response_buffer, "[%d] %s = %d\r\n", i, param_get_name((ParamKey_e)i), i8_val);
            send_response(g_response_buffer);
        } else if (param_get_int16((ParamKey_e)i, &i16_val) == 0) {
            Ql_sprintf(g_response_buffer, "[%d] %s = %d\r\n", i, param_get_name((ParamKey_e)i), i16_val);
            send_response(g_response_buffer);
        } else if (param_get_int32((ParamKey_e)i, &i32_val) == 0) {
            Ql_sprintf(g_response_buffer, "[%d] %s = %d\r\n", i, param_get_name((ParamKey_e)i), i32_val);
            send_response(g_response_buffer);
        } else if (param_get_string((ParamKey_e)i, str_val, sizeof(str_val)) == 0) {
            Ql_sprintf(g_response_buffer, "[%d] %s = %s\r\n", i, param_get_name((ParamKey_e)i), str_val);
            send_response(g_response_buffer);
        }
    }
    
    send_response("===========================\r\n\r\n");
    return COM_OK;
}

/**
 * @brief Command: Show help
 * Format: ?!
 */
static ComResult_e cmd_help(void)
{
    /* Constant strings don't need Ql_sprintf formatting */
    send_response("\r\n=== Command Help ===\r\n");
    send_response("S,<key>,<value>!  - Set parameter\r\n");
    send_response("G,<key>!          - Get parameter\r\n");
    send_response("C!                - Commit to NVRAM\r\n");
    send_response("L!                - List all parameters\r\n");
    send_response("?!                - Show this help\r\n");
    send_response("\r\nExamples:\r\n");
    send_response("  S,4,1!          - Set param 4 to 1\r\n");
    send_response("  S,mqtt_port,1883! - Set by name\r\n");
    send_response("  G,4!            - Get param 4\r\n");
    send_response("  C!              - Save to NVRAM\r\n");
    send_response("====================\r\n\r\n");
    
    return COM_OK;
}

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

s32 com_init(ComResponseCallback_t callback)
{
    if (com_initialized) {
        APP_DEBUG("WARNING: COM already initialized\r\n");
        return 0;
    }
    
    /* Create mutex for thread-safe response buffer access */
    g_response_mutex = Ql_OS_CreateMutex("com_resp");
    if (g_response_mutex == 0) {
        APP_DEBUG("ERROR: Failed to create COM response mutex\r\n");
        return -1;
    }
    
    /* Clear response buffer */
    Ql_memset(g_response_buffer, 0, COM_RESPONSE_MAX);
    
    response_callback = callback;
    com_initialized = TRUE;
    
    APP_DEBUG("✅ Command interface initialized\r\n");
    APP_DEBUG("   Protocol: CMD,KEY,VALUE!\r\n");
    APP_DEBUG("   Mutex: 0x%08X (thread-safe)\r\n", g_response_mutex);
    APP_DEBUG("   Type ?! for help\r\n");
    
    return 0;
}

s32 com_process_command(const char* cmd, u32 len)
{
    char buffer[COM_CMD_MAX_LEN];
    char* cmd_type;
    char* key_str;
    char* value_str;
    ComResult_e result = COM_ERR_PARSE_ERROR;

    if (!com_initialized) {
        APP_DEBUG("ERROR: COM not initialized\r\n");
        return -1;
    }
    
    if (len >= COM_CMD_MAX_LEN) {
        Ql_sprintf(g_response_buffer, "ERROR: Command too long (max %d chars)\r\n", COM_CMD_MAX_LEN);
        send_response(g_response_buffer);
        return COM_ERR_TOO_LONG;
    }
    
    /* Copy command to buffer (for tokenization) */
    Ql_memcpy(buffer, cmd, len);
    buffer[len] = '\0';
    
    /* Remove terminator */
    if (buffer[len-1] == COM_CMD_TERMINATOR) {
        buffer[len-1] = '\0';
    }
    
    /* Parse command type */
    cmd_type = simple_strtok(buffer, ',');
    if (cmd_type == NULL) {
        send_response("ERROR: Invalid command format\r\n");
        return COM_ERR_PARSE_ERROR;
    }
    
    /* Process command */
    switch (cmd_type[0]) {
        case 'S': /* Set parameter */
        case 's':
            key_str = simple_strtok(NULL, ',');
            value_str = simple_strtok(NULL, ',');
            
            if (key_str == NULL || value_str == NULL) {
                send_response("ERROR: SET requires key and value (S,key,value!)\r\n");
                result = COM_ERR_PARSE_ERROR;
            } else {
                result = cmd_set_parameter(key_str, value_str);
            }
            break;
            
        case 'G': /* Get parameter */
        case 'g':
            key_str = simple_strtok(NULL, ',');
            
            if (key_str == NULL) {
                send_response("ERROR: GET requires key (G,key!)\r\n");
                result = COM_ERR_PARSE_ERROR;
            } else {
                result = cmd_get_parameter(key_str);
            }
            break;
            
        case 'C': /* Commit */
        case 'c':
            result = cmd_commit();
            break;
            
        case 'L': /* List */
        case 'l':
            result = cmd_list_parameters();
            break;
            
        case '?': /* Help */
            result = cmd_help();
            break;
            
        default:
            Ql_sprintf(g_response_buffer, "ERROR: Unknown command '%s'\r\n", cmd_type);
            send_response(g_response_buffer);
            result = COM_ERR_INVALID_CMD;
            break;
    }
    
    return result;
}

/* com_send_response removed - Ql_vsnprintf is broken in this SDK
 * Use Ql_sprintf directly in command handlers instead */

const char* com_result_string(ComResult_e result)
{
    switch (result) {
        case COM_OK:                return "OK";
        case COM_ERR_INVALID_CMD:   return "Invalid command";
        case COM_ERR_INVALID_KEY:   return "Invalid key";
        case COM_ERR_INVALID_VALUE: return "Invalid value";
        case COM_ERR_PARAM_ERROR:   return "Parameter error";
        case COM_ERR_PARSE_ERROR:   return "Parse error";
        case COM_ERR_TOO_LONG:      return "Command too long";
        default:                    return "Unknown error";
    }
}

