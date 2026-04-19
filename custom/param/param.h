/**
 * @file    param.h
 * @brief   Parameter configuration and runtime storage
 * @author  Hossein Gholami
 * @date    2025-11-01
 */

#ifndef PARAM_H
#define PARAM_H

#include "ql_type.h"
#include "ql_stdlib.h"

#define PARAM_STRING_MAX_LEN    256

typedef enum {
    PARAM_APN = 0,
    PARAM_MQTT_HOST,
    PARAM_DEVICE_ID,
    PARAM_MQTT_PORT,
    PARAM_IO_STATE,
    PARAM_NET_RSSI,
    PARAM_BATTERY_ACTIVATION,
    PARAM_IO_EXP_OUT0,
    PARAM_IO_EXP_OUT1,
    PARAM_IO_EXP_OUT2,
    PARAM_IO_EXP_OUT3,
    PARAM_IO_EXP_IN0,
    PARAM_IO_EXP_IN1,
    PARAM_IO_EXP_IN2,
    PARAM_IO_EXP_IN3,
    PARAM_MAX_COUNT,
    PARAM_NONE,
} ParamKey_e;

#define PARAM_Battery_activation PARAM_BATTERY_ACTIVATION

typedef enum {
    PARAM_TYPE_INT8 = 0,
    PARAM_TYPE_INT16,
    PARAM_TYPE_INT32,
    PARAM_TYPE_STRING
} ParamType_e;

typedef union {
    s8  i8;
    s16 i16;
    s32 i32;
} ParamValue_u;

typedef union {
    s8          i8;
    s16         i16;
    s32         i32;
    const char* str;
} ParamDefaultValue_u;

typedef struct {
    const char*         name;
    ParamType_e         type;
    bool                persist;
    ParamDefaultValue_u default_value;
} ParamConfig_t;

typedef void (*ParamChangeCallback_t)(ParamKey_e key,
                                      const void* old_value,
                                      const void* new_value,
                                      ParamType_e type);

s32 param_init(void);
s32 param_register(ParamKey_e key);
s32 param_set_int8(ParamKey_e key, s8 value);
s32 param_set_int16(ParamKey_e key, s16 value);
s32 param_set_int32(ParamKey_e key, s32 value);
s32 param_set_string(ParamKey_e key, const char* value);
s32 param_get_int8(ParamKey_e key, s8* value);
s32 param_get_int16(ParamKey_e key, s16* value);
s32 param_get_int32(ParamKey_e key, s32* value);
s32 param_get_string(ParamKey_e key, char* value, u32 max_len);
u32 param_count(void);
void param_print_all(void);
s32 param_get_type(ParamKey_e key, ParamType_e* type);
s32 param_reset(ParamKey_e key);
void param_reset_all(void);
const char* param_get_name(ParamKey_e key);
s32 param_get_persist(ParamKey_e key, bool* persist);
void param_get_ram_usage(u32* total_bytes, u32* int_params, u32* str_params);
s32 param_set_callback(ParamKey_e key, ParamChangeCallback_t callback);
ParamChangeCallback_t param_get_callback(ParamKey_e key);

#endif /* PARAM_H */
