/**
 * @file    param.c
 * @brief   Parameter runtime table and persistence
 */

#include "param.h"
#include "param_storage.h"
#include "uart/uart.h"
#include "ql_stdlib.h"
#include "ql_system.h"

typedef struct {
    const char*            name;
    ParamType_e            type;
    bool                   persist;
    ParamValue_u           value;
    char                   string_value[PARAM_STRING_MAX_LEN];
    u32                    mutex;
    ParamChangeCallback_t  callback;
    bool                   registered;
} ParamEntry_t;

static const ParamConfig_t g_param_config[PARAM_MAX_COUNT] = {
    [PARAM_APN]       = {.name = "apn",         .type = PARAM_TYPE_STRING, .persist = TRUE,  .default_value.str = ""},
    [PARAM_MQTT_HOST] = {.name = "mqtt host",   .type = PARAM_TYPE_STRING, .persist = TRUE,  .default_value.str = ""},
    [PARAM_DEVICE_ID] = {.name = "device id",   .type = PARAM_TYPE_STRING, .persist = TRUE,  .default_value.str = ""},
    [PARAM_MQTT_PORT] = {.name = "mqtt port",   .type = PARAM_TYPE_INT16,  .persist = TRUE,  .default_value.i16 = 0},
    [PARAM_IO_STATE]  = {.name = "io state",    .type = PARAM_TYPE_INT8,   .persist = FALSE, .default_value.i8  = 0},
    [PARAM_NET_RSSI]  = {.name = "net rssi",    .type = PARAM_TYPE_INT8,   .persist = FALSE, .default_value.i8  = 0},
    [PARAM_BATTERY_ACTIVATION] = {.name = "power", .type = PARAM_TYPE_INT8, .persist = FALSE, .default_value.i8 = 0},
    [PARAM_IO_EXP_OUT0] = {.name = "Relay-1", .type = PARAM_TYPE_INT8, .persist = FALSE, .default_value.i8 = 0},
    [PARAM_IO_EXP_OUT2] = {.name = "Relay-2", .type = PARAM_TYPE_INT8, .persist = FALSE, .default_value.i8 = 0},
    [PARAM_IO_EXP_OUT3] = {.name = "Relay-3", .type = PARAM_TYPE_INT8, .persist = FALSE, .default_value.i8 = 0},
    [PARAM_IO_EXP_OUT1] = {.name = "Relay-4", .type = PARAM_TYPE_INT8, .persist = FALSE, .default_value.i8 = 0},
    [PARAM_IO_EXP_IN0] = {.name = "io exp in0", .type = PARAM_TYPE_INT8, .persist = FALSE, .default_value.i8 = 0},
    [PARAM_IO_EXP_IN1] = {.name = "io exp in1", .type = PARAM_TYPE_INT8, .persist = FALSE, .default_value.i8 = 0},
    [PARAM_IO_EXP_IN2] = {.name = "io exp in2", .type = PARAM_TYPE_INT8, .persist = FALSE, .default_value.i8 = 0},
    [PARAM_IO_EXP_IN3] = {.name = "io exp in3", .type = PARAM_TYPE_INT8, .persist = FALSE, .default_value.i8 = 0},
    [PARAM_ALERT_PHONE_1] = {.name = "phone 1", .type = PARAM_TYPE_STRING, .persist = TRUE, .default_value.str = "+989129459183"},
    [PARAM_ALERT_PHONE_2] = {.name = "phone 2", .type = PARAM_TYPE_STRING, .persist = TRUE, .default_value.str = "+989129459183"},
    [PARAM_ALERT_PHONE_3] = {.name = "phone 3", .type = PARAM_TYPE_STRING, .persist = TRUE, .default_value.str = "+989129459183"},
};

static ParamEntry_t g_param_table[PARAM_MAX_COUNT];
static u32 g_storage_mutex = 0;
static bool g_param_initialized = FALSE;

static bool param_is_valid_key(ParamKey_e key)
{
    return (key >= 0 && key < PARAM_MAX_COUNT);
}

static ParamEntry_t* param_get_entry(ParamKey_e key)
{
    if (!param_is_valid_key(key)) {
        return NULL;
    }

    if (!g_param_table[key].registered) {
        return NULL;
    }

    return &g_param_table[key];
}

static void param_lock_entry(ParamEntry_t* entry)
{
    if (entry != NULL && entry->mutex != 0) {
        Ql_OS_TakeMutex(entry->mutex);
    }
}

static void param_unlock_entry(ParamEntry_t* entry)
{
    if (entry != NULL && entry->mutex != 0) {
        Ql_OS_GiveMutex(entry->mutex);
    }
}

static s32 param_persist_snapshot(void)
{
    ParamStorageRecord_t* records;
    u32 i;
    u32 record_count = 0;
    s32 ret;

    if (g_storage_mutex == 0) {
        return -1;
    }

    records = (ParamStorageRecord_t*)Ql_MEM_Alloc(sizeof(ParamStorageRecord_t) * PARAM_MAX_COUNT);
    if (records == NULL) {
        APP_DEBUG("ERROR: Failed to allocate param snapshot buffer\r\n");
        return -2;
    }

    Ql_memset(records, 0, sizeof(ParamStorageRecord_t) * PARAM_MAX_COUNT);

    Ql_OS_TakeMutex(g_storage_mutex);

    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        ParamEntry_t* entry = &g_param_table[i];

        if (!entry->registered || !entry->persist) {
            continue;
        }

        param_lock_entry(entry);
        records[record_count].key = i;
        records[record_count].type = entry->type;
        records[record_count].persist = entry->persist ? 1 : 0;
        records[record_count].value = entry->value;
        if (entry->type == PARAM_TYPE_STRING) {
            Ql_strcpy(records[record_count].string_value, entry->string_value);
        }
        param_unlock_entry(entry);

        record_count++;
    }

    ret = param_storage_save(records, record_count);

    Ql_OS_GiveMutex(g_storage_mutex);
    Ql_MEM_Free(records);

    return ret;
}

static void param_apply_loaded_record(const ParamStorageRecord_t* record)
{
    ParamEntry_t* entry;

    if (record == NULL || !param_is_valid_key((ParamKey_e)record->key)) {
        return;
    }

    entry = param_get_entry((ParamKey_e)record->key);
    if (entry == NULL || entry->type != record->type) {
        return;
    }

    param_lock_entry(entry);
    entry->value = record->value;
    if (entry->type == PARAM_TYPE_STRING) {
        Ql_strcpy(entry->string_value, record->string_value);
    }
    param_unlock_entry(entry);
}

static void param_invoke_callback(ParamEntry_t* entry,
                                  ParamKey_e key,
                                  const void* old_value,
                                  const void* new_value)
{
    ParamChangeCallback_t callback = NULL;

    if (entry == NULL) {
        return;
    }

    param_lock_entry(entry);
    callback = entry->callback;
    param_unlock_entry(entry);

    if (callback != NULL) {
        callback(key, old_value, new_value, entry->type);
    }
}

static void param_apply_default_value(ParamKey_e key, ParamEntry_t* entry)
{
    switch (g_param_config[key].type) {
    case PARAM_TYPE_INT8:
        entry->value.i8 = g_param_config[key].default_value.i8;
        break;

    case PARAM_TYPE_INT16:
        entry->value.i16 = g_param_config[key].default_value.i16;
        break;

    case PARAM_TYPE_INT32:
        entry->value.i32 = g_param_config[key].default_value.i32;
        break;

    case PARAM_TYPE_STRING:
        if (g_param_config[key].default_value.str != NULL) {
            Ql_strcpy(entry->string_value, g_param_config[key].default_value.str);
        } else {
            entry->string_value[0] = '\0';
        }
        break;
    }
}

s32 param_register(ParamKey_e key)
{
    ParamEntry_t* entry;
    char mutex_name[20];

    if (!param_is_valid_key(key)) {
        return -1;
    }

    entry = &g_param_table[key];

    if (entry->mutex == 0) {
        Ql_memset(mutex_name, 0, sizeof(mutex_name));
        Ql_sprintf(mutex_name, "param_%d", key);
        entry->mutex = Ql_OS_CreateMutex(mutex_name);
        if (entry->mutex == 0) {
            APP_DEBUG("ERROR: Failed to create mutex for param %d\r\n", key);
            return -2;
        }
    }

    param_lock_entry(entry);
    entry->name = g_param_config[key].name;
    entry->type = g_param_config[key].type;
    entry->persist = g_param_config[key].persist;
    Ql_memset(&entry->value, 0, sizeof(entry->value));
    Ql_memset(entry->string_value, 0, sizeof(entry->string_value));
    entry->callback = NULL;
    entry->registered = TRUE;
    param_apply_default_value(key, entry);
    param_unlock_entry(entry);

    return 0;
}

s32 param_init(void)
{
    ParamStorageRecord_t records[PARAM_MAX_COUNT];
    u32 loaded_count = 0;
    u32 i;
    s32 ret;
    bool storage_exists;
    u32 total_bytes;
    u32 int_params;
    u32 str_params;

    if (g_param_initialized) {
        APP_DEBUG("WARNING: param already initialized\r\n");
        return 0;
    }

    Ql_memset(g_param_table, 0, sizeof(g_param_table));

    g_storage_mutex = Ql_OS_CreateMutex("param_db");
    if (g_storage_mutex == 0) {
        APP_DEBUG("ERROR: Failed to create param storage mutex\r\n");
        return -1;
    }

    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        ret = param_register((ParamKey_e)i);
        if (ret != 0) {
            return ret;
        }
    }

    storage_exists = param_storage_exists();
    Ql_memset(records, 0, sizeof(records));
    ret = param_storage_load(records, PARAM_MAX_COUNT, &loaded_count);
    if (ret < 0) {
        APP_DEBUG("WARNING: Param storage load failed: %d\r\n", ret);
    } else {
        for (i = 0; i < loaded_count; i++) {
            param_apply_loaded_record(&records[i]);
        }
    }

    g_param_initialized = TRUE;

    if (!storage_exists || ret < 0) {
        APP_DEBUG("Creating param storage from runtime table...\r\n");
        param_persist_snapshot();
    }

    param_get_ram_usage(&total_bytes, &int_params, &str_params);
    APP_DEBUG("✅ Parameter system initialized\r\n");
    APP_DEBUG("   Total params: %d (%d int, %d string)\r\n",
             PARAM_MAX_COUNT, int_params, str_params);
    APP_DEBUG("   RAM usage: %d bytes (%.2f KB)\r\n",
             total_bytes, (float)total_bytes / 1024.0f);
    APP_DEBUG("   Storage mutex: 0x%08X\r\n", g_storage_mutex);

    return 0;
}

s32 param_set_int8(ParamKey_e key, s8 value)
{
    ParamEntry_t* entry = param_get_entry(key);
    s8 old_value;

    if (entry == NULL) {
        return -1;
    }

    if (entry->type != PARAM_TYPE_INT8) {
        return -3;
    }

    param_lock_entry(entry);
    old_value = entry->value.i8;
    if (old_value == value) {
        param_unlock_entry(entry);
        return 0;
    }
    entry->value.i8 = value;
    param_unlock_entry(entry);

    param_invoke_callback(entry, key, &old_value, &value);

    if (entry->persist) {
        return param_persist_snapshot();
    }

    return 0;
}

s32 param_set_int16(ParamKey_e key, s16 value)
{
    ParamEntry_t* entry = param_get_entry(key);
    s16 old_value;

    if (entry == NULL) {
        return -1;
    }

    if (entry->type != PARAM_TYPE_INT16) {
        return -3;
    }

    param_lock_entry(entry);
    old_value = entry->value.i16;
    if (old_value == value) {
        param_unlock_entry(entry);
        return 0;
    }
    entry->value.i16 = value;
    param_unlock_entry(entry);

    param_invoke_callback(entry, key, &old_value, &value);

    if (entry->persist) {
        return param_persist_snapshot();
    }

    return 0;
}

s32 param_set_int32(ParamKey_e key, s32 value)
{
    ParamEntry_t* entry = param_get_entry(key);
    s32 old_value;

    if (entry == NULL) {
        return -1;
    }

    if (entry->type != PARAM_TYPE_INT32) {
        return -3;
    }

    param_lock_entry(entry);
    old_value = entry->value.i32;
    if (old_value == value) {
        param_unlock_entry(entry);
        return 0;
    }
    entry->value.i32 = value;
    param_unlock_entry(entry);

    param_invoke_callback(entry, key, &old_value, &value);

    if (entry->persist) {
        return param_persist_snapshot();
    }

    return 0;
}

s32 param_set_string(ParamKey_e key, const char* value)
{
    ParamEntry_t* entry = param_get_entry(key);
    char old_value[PARAM_STRING_MAX_LEN];

    if (entry == NULL || value == NULL) {
        return -1;
    }

    if (entry->type != PARAM_TYPE_STRING) {
        return -3;
    }

    if (Ql_strlen(value) >= PARAM_STRING_MAX_LEN) {
        return -4;
    }

    param_lock_entry(entry);
    Ql_strcpy(old_value, entry->string_value);
    if (Ql_strcmp(old_value, value) == 0) {
        param_unlock_entry(entry);
        return 0;
    }
    Ql_strcpy(entry->string_value, value);
    param_unlock_entry(entry);

    param_invoke_callback(entry, key, old_value, value);

    if (entry->persist) {
        return param_persist_snapshot();
    }

    return 0;
}

s32 param_get_int8(ParamKey_e key, s8* value)
{
    ParamEntry_t* entry = param_get_entry(key);

    if (entry == NULL || value == NULL) {
        return -1;
    }

    if (entry->type != PARAM_TYPE_INT8) {
        return -3;
    }

    param_lock_entry(entry);
    *value = entry->value.i8;
    param_unlock_entry(entry);
    return 0;
}

s32 param_get_int16(ParamKey_e key, s16* value)
{
    ParamEntry_t* entry = param_get_entry(key);

    if (entry == NULL || value == NULL) {
        return -1;
    }

    if (entry->type != PARAM_TYPE_INT16) {
        return -3;
    }

    param_lock_entry(entry);
    *value = entry->value.i16;
    param_unlock_entry(entry);
    return 0;
}

s32 param_get_int32(ParamKey_e key, s32* value)
{
    ParamEntry_t* entry = param_get_entry(key);

    if (entry == NULL || value == NULL) {
        return -1;
    }

    if (entry->type != PARAM_TYPE_INT32) {
        return -3;
    }

    param_lock_entry(entry);
    *value = entry->value.i32;
    param_unlock_entry(entry);
    return 0;
}

s32 param_get_string(ParamKey_e key, char* value, u32 max_len)
{
    ParamEntry_t* entry = param_get_entry(key);

    if (entry == NULL || value == NULL) {
        return -1;
    }

    if (entry->type != PARAM_TYPE_STRING) {
        return -3;
    }

    param_lock_entry(entry);
    if (max_len < Ql_strlen(entry->string_value) + 1) {
        param_unlock_entry(entry);
        return -4;
    }
    Ql_strcpy(value, entry->string_value);
    param_unlock_entry(entry);
    return 0;
}

u32 param_count(void)
{
    return PARAM_MAX_COUNT;
}

void param_print_all(void)
{
    u32 i;

    APP_DEBUG("\r\n=== Parameter Table ===\r\n");
    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        ParamEntry_t* entry = &g_param_table[i];

        if (!entry->registered) {
            continue;
        }

        APP_DEBUG("[%d] %s ", i, entry->name);
        switch (entry->type) {
        case PARAM_TYPE_INT8:
            APP_DEBUG("= %d\r\n", entry->value.i8);
            break;
        case PARAM_TYPE_INT16:
            APP_DEBUG("= %d\r\n", entry->value.i16);
            break;
        case PARAM_TYPE_INT32:
            APP_DEBUG("= %d\r\n", entry->value.i32);
            break;
        case PARAM_TYPE_STRING:
            APP_DEBUG("= %s\r\n", entry->string_value);
            break;
        }
    }
    APP_DEBUG("=======================\r\n\r\n");
}

s32 param_get_type(ParamKey_e key, ParamType_e* type)
{
    ParamEntry_t* entry = param_get_entry(key);

    if (entry == NULL || type == NULL) {
        return -1;
    }

    *type = entry->type;
    return 0;
}

s32 param_reset(ParamKey_e key)
{
    ParamEntry_t* entry = param_get_entry(key);
    ParamValue_u old_value;
    char old_string[PARAM_STRING_MAX_LEN];

    if (entry == NULL) {
        return -1;
    }

    param_lock_entry(entry);
    old_value = entry->value;
    Ql_strcpy(old_string, entry->string_value);
    Ql_memset(&entry->value, 0, sizeof(entry->value));
    Ql_memset(entry->string_value, 0, sizeof(entry->string_value));
    param_apply_default_value(key, entry);
    param_unlock_entry(entry);

    if (entry->type == PARAM_TYPE_STRING) {
        param_invoke_callback(entry, key, old_string, entry->string_value);
    } else {
        param_invoke_callback(entry, key, &old_value, &entry->value);
    }

    if (entry->persist) {
        return param_persist_snapshot();
    }

    return 0;
}

void param_reset_all(void)
{
    u32 i;

    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        param_reset((ParamKey_e)i);
    }

    param_persist_snapshot();
}

const char* param_get_name(ParamKey_e key)
{
    ParamEntry_t* entry = param_get_entry(key);

    if (entry == NULL) {
        return "INVALID";
    }

    return entry->name;
}

s32 param_get_persist(ParamKey_e key, bool* persist)
{
    ParamEntry_t* entry = param_get_entry(key);

    if (entry == NULL || persist == NULL) {
        return -1;
    }

    *persist = entry->persist;
    return 0;
}

void param_get_ram_usage(u32* total_bytes, u32* int_params, u32* str_params)
{
    u32 i;
    u32 int_count = 0;
    u32 str_count = 0;

    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        if (g_param_config[i].type == PARAM_TYPE_STRING) {
            str_count++;
        } else {
            int_count++;
        }
    }

    if (int_params != NULL) {
        *int_params = int_count;
    }

    if (str_params != NULL) {
        *str_params = str_count;
    }

    if (total_bytes != NULL) {
        *total_bytes = sizeof(g_param_table) + (PARAM_MAX_COUNT * sizeof(u32)) + sizeof(g_storage_mutex);
    }
}

s32 param_set_callback(ParamKey_e key, ParamChangeCallback_t callback)
{
    ParamEntry_t* entry = param_get_entry(key);

    if (entry == NULL) {
        return -1;
    }

    param_lock_entry(entry);
    entry->callback = callback;
    param_unlock_entry(entry);
    return 0;
}

ParamChangeCallback_t param_get_callback(ParamKey_e key)
{
    ParamEntry_t* entry = param_get_entry(key);
    ParamChangeCallback_t callback;

    if (entry == NULL) {
        return NULL;
    }

    param_lock_entry(entry);
    callback = entry->callback;
    param_unlock_entry(entry);
    return callback;
}
