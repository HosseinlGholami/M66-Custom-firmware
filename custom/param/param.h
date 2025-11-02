/**
 * @file    param.h
 * @brief   Fast RAM-based parameter system with optional persistence
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * Features:
 * - RAM-optimized storage (strings separate from integers)
 * - Optional NVRAM persistence per parameter
 * - Type-safe access (int8, int16, int32, string)
 * - Enum-based keys for type safety and speed
 * - Suitable for inter-task synchronization
 */

#ifndef PARAM_H
#define PARAM_H

#include "ql_type.h"
#include "ql_stdlib.h"

/*============================================================================
 * Constants
 *===========================================================================*/
#define PARAM_STRING_MAX_LEN    256     /* Max string value length */

/*============================================================================
 * Parameter Key Enumeration
 *===========================================================================*/

/**
 * @brief Parameter key identifiers
 * Add your parameters here - no need to maintain a list elsewhere!
 */
typedef enum {
    /* Network Configuration (persistent, string type) */
    PARAM_APN = 0,
    PARAM_MQTT_HOST,
    PARAM_DEVICE_ID,
    
    /* Network Configuration (persistent, numeric) */
    PARAM_MQTT_PORT,
    
    /* Runtime State (RAM-only, fast sync between tasks) */
    PARAM_IO_STATE,
    PARAM_SENSOR_TEMP,
    PARAM_NET_RSSI,
    PARAM_TASK_COUNTER,
    PARAM_GPS_LAT,
    PARAM_GPS_LON,
    
    /* Add your parameters above this line */
    PARAM_MAX_COUNT         /* Must be last - auto count */
} ParamKey_e;

/*============================================================================
 * Parameter Types
 *===========================================================================*/

/**
 * @brief Parameter data types
 */
typedef enum {
    PARAM_TYPE_INT8 = 0,    /* 8-bit signed integer */
    PARAM_TYPE_INT16,       /* 16-bit signed integer */
    PARAM_TYPE_INT32,       /* 32-bit signed integer */
    PARAM_TYPE_STRING       /* String (stored separately) */
} ParamType_e;

/**
 * @brief Parameter value union (optimized - no large strings!)
 * Strings are stored in separate array to save RAM
 */
typedef union {
    s8  i8;                 /* 8-bit integer value */
    s16 i16;                /* 16-bit integer value */
    s32 i32;                /* 32-bit integer value */
    u8  str_idx;            /* Index into string storage array */
} ParamValue_u;

/**
 * @brief Parameter change callback function type
 * Called whenever a parameter value changes (after param_set_*() completes)
 * 
 * @param key The parameter that changed
 * @param old_value Pointer to the old value (read-only)
 * @param new_value Pointer to the new value (read-only)
 * @param type The parameter type
 * 
 * @note Callback is called with mutex LOCKED - keep it fast!
 * @note For strings, old_value/new_value are char* pointers
 * @note For integers, cast to appropriate type based on 'type' parameter
 */
typedef void (*ParamChangeCallback_t)(ParamKey_e key, 
                                      const void* old_value, 
                                      const void* new_value, 
                                      ParamType_e type);

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize parameter system
 * Automatically registers all parameters and loads persistent ones from NVRAM
 * @return 0 on success, negative on error
 */
s32 param_init(void);

/**
 * @brief Set int8 parameter value
 * @param key Parameter enum key
 * @param value Value to set
 * @return 0 on success, negative on error
 */
s32 param_set_int8(ParamKey_e key, s8 value);

/**
 * @brief Set int16 parameter value
 * @param key Parameter enum key
 * @param value Value to set
 * @return 0 on success, negative on error
 */
s32 param_set_int16(ParamKey_e key, s16 value);

/**
 * @brief Set int32 parameter value
 * @param key Parameter enum key
 * @param value Value to set
 * @return 0 on success, negative on error
 */
s32 param_set_int32(ParamKey_e key, s32 value);

/**
 * @brief Set string parameter value
 * @param key Parameter enum key
 * @param value String value (null-terminated)
 * @return 0 on success, negative on error
 */
s32 param_set_string(ParamKey_e key, const char* value);

/**
 * @brief Get int8 parameter value
 * @param key Parameter enum key
 * @param value Pointer to store value
 * @return 0 on success, negative on error
 */
s32 param_get_int8(ParamKey_e key, s8* value);

/**
 * @brief Get int16 parameter value
 * @param key Parameter enum key
 * @param value Pointer to store value
 * @return 0 on success, negative on error
 */
s32 param_get_int16(ParamKey_e key, s16* value);

/**
 * @brief Get int32 parameter value
 * @param key Parameter enum key
 * @param value Pointer to store value
 * @return 0 on success, negative on error
 */
s32 param_get_int32(ParamKey_e key, s32* value);

/**
 * @brief Get string parameter value
 * @param key Parameter enum key
 * @param value Buffer to store string
 * @param max_len Maximum buffer length
 * @return 0 on success, negative on error
 */
s32 param_get_string(ParamKey_e key, char* value, u32 max_len);

/**
 * @brief Save all dirty persistent parameters to NVRAM
 * Only writes parameters that have persist=TRUE and dirty=TRUE
 * @return Number of parameters saved, negative on error
 */
s32 param_commit(void);

/**
 * @brief Get number of registered parameters
 * @return Parameter count
 */
u32 param_count(void);

/**
 * @brief Print all parameters (debug)
 */
void param_print_all(void);

/**
 * @brief Get parameter type
 * @param key Parameter enum key
 * @param type Pointer to store type
 * @return 0 on success, negative on error
 */
s32 param_get_type(ParamKey_e key, ParamType_e* type);

/**
 * @brief Reset parameter to default value (zeros)
 * @param key Parameter enum key
 * @return 0 on success, negative on error
 */
s32 param_reset(ParamKey_e key);

/**
 * @brief Reset all parameters
 */
void param_reset_all(void);

/**
 * @brief Get parameter name string (for debug)
 * @param key Parameter enum key
 * @return Parameter name string
 */
const char* param_get_name(ParamKey_e key);

/**
 * @brief Set parameter persistence flag
 * @param key Parameter enum key
 * @param persist TRUE to save to NVRAM, FALSE for RAM-only
 * @return 0 on success, negative on error
 */
s32 param_set_persist(ParamKey_e key, bool persist);

/**
 * @brief Get parameter persistence flag
 * @param key Parameter enum key
 * @param persist Pointer to store persistence flag
 * @return 0 on success, negative on error
 */
s32 param_get_persist(ParamKey_e key, bool* persist);

/**
 * @brief Get RAM usage statistics
 * @param total_bytes Total RAM used
 * @param int_params Number of integer parameters
 * @param str_params Number of string parameters
 */
void param_get_ram_usage(u32* total_bytes, u32* int_params, u32* str_params);

/**
 * @brief Register a callback for parameter changes
 * The callback will be invoked whenever the parameter value changes.
 * Only ONE callback per parameter is supported.
 * 
 * @param key Parameter enum key
 * @param callback Callback function (or NULL to remove callback)
 * @return 0 on success, negative on error
 * 
 * @note Callback is called AFTER the value is updated
 * @note Callback runs with mutex LOCKED - keep it fast!
 * @note Use this for GPIO control, state machines, etc.
 * 
 * @example
 * void on_led_change(ParamKey_e key, const void* old_val, const void* new_val, ParamType_e type) {
 *     s8 new_state = *(s8*)new_val;
 *     Ql_GPIO_SetLevel(LED_PIN, new_state ? PINLEVEL_HIGH : PINLEVEL_LOW);
 * }
 * param_set_callback(PARAM_LED_STATE, on_led_change);
 */
s32 param_set_callback(ParamKey_e key, ParamChangeCallback_t callback);

/**
 * @brief Get the registered callback for a parameter
 * @param key Parameter enum key
 * @return Callback function pointer, or NULL if no callback registered
 */
ParamChangeCallback_t param_get_callback(ParamKey_e key);

#endif /* PARAM_H */
