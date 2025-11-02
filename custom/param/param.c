/**
 * @file    param.c
 * @brief   Fast RAM-based parameter system implementation
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * RAM Optimization:
 * - Integer params: 4 bytes each (union)
 * - String params: 256 bytes each (separate storage)
 * - Only allocate string storage for actual string parameters
 * 
 * Architecture:
 * - File I/O handled by param_storage.h/c module
 * - Clean, simple parameter management
 */

#include "param.h"
#include "param_storage.h"
#include "uart/uart.h"
#include "ql_stdlib.h"
#include "ql_system.h"

/*============================================================================
 * Private Constants
 *===========================================================================*/
#define MAX_STRING_PARAMS       16          /* Max number of string parameters */

/*============================================================================
 * Private Types
 *===========================================================================*/

/**
 * @brief Parameter configuration and metadata
 */
typedef struct {
    const char* name;       /* Parameter name (for debug) */
    ParamType_e type;       /* Data type */
    bool persist;           /* Save to NVRAM? */
} ParamConfig_t;

/**
 * @brief Parameter storage structure (RAM-optimized!)
 */
typedef struct ParamData_s {
    ParamKey_e key;         /* Parameter key (enum) */
    ParamValue_u value;     /* Value union (only 4 bytes!) */
    bool dirty;             /* Modified since save? */
    bool persist;           /* Save to NVRAM? */
} ParamData_t;

/**
 * @brief String storage entry
 */
typedef struct StringStorage_s {
    ParamKey_e key;                     /* Which param owns this string */
    char data[PARAM_STRING_MAX_LEN];    /* String data */
} StringStorage_t;

/*============================================================================
 * Parameter Configuration Table
 * Define your parameters here with name, type, and persistence
 *===========================================================================*/
static const ParamConfig_t param_config[PARAM_MAX_COUNT] = {
    /* Network Configuration (persistent, strings) */
    {"apn",          PARAM_TYPE_STRING, TRUE},   /* PARAM_APN */
    {"mqtt_host",    PARAM_TYPE_STRING, TRUE},   /* PARAM_MQTT_HOST */
    {"device_id",    PARAM_TYPE_STRING, TRUE},   /* PARAM_DEVICE_ID */
    
    /* Network Configuration (persistent, numeric) */
    {"mqtt_port",    PARAM_TYPE_INT16,  TRUE},   /* PARAM_MQTT_PORT */
    
    /* Runtime State (RAM-only) */
    {"io_state",     PARAM_TYPE_INT8,   FALSE},  /* PARAM_IO_STATE */
    {"sensor_temp",  PARAM_TYPE_INT16,  FALSE},  /* PARAM_SENSOR_TEMP */
    {"net_rssi",     PARAM_TYPE_INT8,   FALSE},  /* PARAM_NET_RSSI */
    {"task_counter", PARAM_TYPE_INT32,  FALSE},  /* PARAM_TASK_COUNTER */
    {"gps_lat",      PARAM_TYPE_INT32,  FALSE},  /* PARAM_GPS_LAT */
    {"gps_lon",      PARAM_TYPE_INT32,  FALSE},  /* PARAM_GPS_LON */
};

/*============================================================================
 * Private Data
 *===========================================================================*/
/* Integer parameters (small footprint: 4 bytes per param) */
static ParamData_t param_data[PARAM_MAX_COUNT];

/* String parameters (separate storage: 256 bytes each, only for strings) */
static StringStorage_t string_storage[MAX_STRING_PARAMS];
static u32 string_count = 0;

/* Callback array (one callback per parameter, optional) */
static ParamChangeCallback_t param_callbacks[PARAM_MAX_COUNT];

static bool param_initialized = FALSE;

/* Mutex for thread-safe access (RTOS task synchronization) */
static u32 param_mutex = 0;

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Validate parameter key
 */
static bool is_valid_key(ParamKey_e key)
{
    return (key >= 0 && key < PARAM_MAX_COUNT);
}

/**
 * @brief Find or allocate string storage for a parameter
 */
static s8 get_string_index(ParamKey_e key)
{
    u8 i;
    
    /* Find existing */
    for (i = 0; i < string_count; i++) {
        if (string_storage[i].key == key) {
            return i;
        }
    }
    
    /* Allocate new */
    if (string_count >= MAX_STRING_PARAMS) {
        APP_DEBUG("ERROR: String storage full! Max %d\r\n", MAX_STRING_PARAMS);
        return -1;
    }
    
    string_storage[string_count].key = key;
    Ql_memset(string_storage[string_count].data, 0, PARAM_STRING_MAX_LEN);
    
    return string_count++;
}

/**
 * @brief Get string storage by index
 */
static char* get_string_storage(u8 idx)
{
    if (idx >= string_count) {
        return NULL;
    }
    return string_storage[idx].data;
}

/**
 * @brief Count dirty persistent parameters
 */
static u32 count_dirty_persistent(void)
{
    u32 i;
    u32 count = 0;
    
    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        if (param_data[i].persist && param_data[i].dirty) {
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Lock parameter mutex
 */
static void param_lock(void)
{
    if (param_mutex != 0) {
        Ql_OS_TakeMutex(param_mutex);
    }
}

/**
 * @brief Unlock parameter mutex
 */
static void param_unlock(void)
{
    if (param_mutex != 0) {
        Ql_OS_GiveMutex(param_mutex);
    }
}

/**
 * @brief Invoke registered callback for a parameter (if any)
 * @note Must be called with mutex LOCKED
 */
static void invoke_callback(ParamKey_e key, const void* old_value, const void* new_value)
{
    if (param_callbacks[key] != NULL) {
        param_callbacks[key](key, old_value, new_value, param_config[key].type);
    }
}

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

s32 param_init(void)
{
    u32 i;
    
    if (param_initialized) {
        APP_DEBUG("WARNING: param already initialized\r\n");
        return 0;
    }
    
    /* Create mutex for thread safety */
    param_mutex = Ql_OS_CreateMutex("param");
    if (param_mutex == 0) {
        APP_DEBUG("ERROR: Failed to create param mutex\r\n");
        return -1;
    }
    
    /* Initialize all parameters */
    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        param_data[i].key = (ParamKey_e)i;
        Ql_memset(&param_data[i].value, 0, sizeof(ParamValue_u));
        param_data[i].dirty = FALSE;
        param_data[i].persist = param_config[i].persist;  /* Copy from config */
        param_callbacks[i] = NULL;  /* No callbacks initially */
    }
    
    /* Initialize string storage */
    Ql_memset(string_storage, 0, sizeof(string_storage));
    string_count = 0;
    
    /* Load persistent parameters from storage */
    param_storage_load((struct ParamData_s*)param_data, PARAM_MAX_COUNT,
                      (struct StringStorage_s*)string_storage, &string_count,
                      MAX_STRING_PARAMS);
    
    param_initialized = TRUE;
    
    /* Calculate RAM usage */
    u32 total_bytes, int_params, str_params;
    param_get_ram_usage(&total_bytes, &int_params, &str_params);
    
    APP_DEBUG("✅ Parameter system initialized (thread-safe)\r\n");
    APP_DEBUG("   Total params: %d (%d int, %d string)\r\n", 
             PARAM_MAX_COUNT, int_params, str_params);
    APP_DEBUG("   RAM usage: %d bytes (%.2f KB)\r\n", 
             total_bytes, (float)total_bytes / 1024.0f);
    APP_DEBUG("   Mutex: 0x%08X\r\n", param_mutex);
    
    return 0;
}

s32 param_set_int8(ParamKey_e key, s8 value)
{
    s8 old_value;
    
    if (!param_initialized) {
        APP_DEBUG("ERROR: param not initialized\r\n");
        return -1;
    }
    
    if (!is_valid_key(key)) {
        APP_DEBUG("ERROR: Invalid key: %d\r\n", key);
        return -2;
    }
    
    if (param_config[key].type != PARAM_TYPE_INT8) {
        APP_DEBUG("ERROR: Type mismatch for '%s' (expected int8)\r\n", 
                 param_config[key].name);
        return -3;
    }
    
    param_lock();
    old_value = param_data[key].value.i8;
    param_data[key].value.i8 = value;
    param_data[key].dirty = TRUE;
    
    /* Invoke callback if registered */
    invoke_callback(key, &old_value, &value);
    
    param_unlock();
    
    return 0;
}

s32 param_set_int16(ParamKey_e key, s16 value)
{
    s16 old_value;
    
    if (!param_initialized) {
        APP_DEBUG("ERROR: param not initialized\r\n");
        return -1;
    }
    
    if (!is_valid_key(key)) {
        APP_DEBUG("ERROR: Invalid key: %d\r\n", key);
        return -2;
    }
    
    if (param_config[key].type != PARAM_TYPE_INT16) {
        APP_DEBUG("ERROR: Type mismatch for '%s' (expected int16)\r\n", 
                 param_config[key].name);
        return -3;
    }
    
    param_lock();
    old_value = param_data[key].value.i16;
    param_data[key].value.i16 = value;
    param_data[key].dirty = TRUE;
    
    /* Invoke callback if registered */
    invoke_callback(key, &old_value, &value);
    
    param_unlock();
    
    return 0;
}

s32 param_set_int32(ParamKey_e key, s32 value)
{
    s32 old_value;
    
    if (!param_initialized) {
        APP_DEBUG("ERROR: param not initialized\r\n");
        return -1;
    }
    
    if (!is_valid_key(key)) {
        APP_DEBUG("ERROR: Invalid key: %d\r\n", key);
        return -2;
    }
    
    if (param_config[key].type != PARAM_TYPE_INT32) {
        APP_DEBUG("ERROR: Type mismatch for '%s' (expected int32)\r\n", 
                 param_config[key].name);
        return -3;
    }
    
    param_lock();
    old_value = param_data[key].value.i32;
    param_data[key].value.i32 = value;
    param_data[key].dirty = TRUE;
    
    /* Invoke callback if registered */
    invoke_callback(key, &old_value, &value);
    
    param_unlock();
    
    return 0;
}

s32 param_set_string(ParamKey_e key, const char* value)
{
    s8 str_idx;
    char* str_ptr;
    
    if (!param_initialized) {
        APP_DEBUG("ERROR: param not initialized\r\n");
        return -1;
    }
    
    if (!is_valid_key(key)) {
        APP_DEBUG("ERROR: Invalid key: %d\r\n", key);
        return -2;
    }
    
    if (param_config[key].type != PARAM_TYPE_STRING) {
        APP_DEBUG("ERROR: Type mismatch for '%s' (expected string)\r\n", 
                 param_config[key].name);
        return -3;
    }
    
    if (value == NULL) {
        APP_DEBUG("ERROR: NULL value for '%s'\r\n", param_config[key].name);
        return -4;
    }
    
    if (Ql_strlen(value) >= PARAM_STRING_MAX_LEN) {
        APP_DEBUG("ERROR: String too long for '%s'\r\n", param_config[key].name);
        return -5;
    }
    
    param_lock();
    
    /* Get or allocate string storage */
    str_idx = get_string_index(key);
    if (str_idx < 0) {
        param_unlock();
        return -6;
    }
    
    str_ptr = get_string_storage(str_idx);
    if (str_ptr == NULL) {
        param_unlock();
        return -7;
    }
    
    /* Save old string for callback (need a buffer for old value) */
    char old_string[PARAM_STRING_MAX_LEN];
    Ql_strcpy(old_string, str_ptr);
    
    /* Store new string */
    Ql_strcpy(str_ptr, value);
    
    /* Store index in value union */
    param_data[key].value.str_idx = str_idx;
    param_data[key].dirty = TRUE;
    
    /* Invoke callback if registered */
    invoke_callback(key, old_string, value);
    
    param_unlock();
    
    return 0;
}

s32 param_get_int8(ParamKey_e key, s8* value)
{
    s8 temp;
    
    if (!param_initialized) {
        return -1;
    }
    
    if (!is_valid_key(key)) {
        return -2;
    }
    
    if (param_config[key].type != PARAM_TYPE_INT8) {
        return -3;
    }
    
    if (value == NULL) {
        return -4;
    }
    
    param_lock();
    temp = param_data[key].value.i8;
    param_unlock();
    
    *value = temp;
    return 0;
}

s32 param_get_int16(ParamKey_e key, s16* value)
{
    s16 temp;
    
    if (!param_initialized) {
        return -1;
    }
    
    if (!is_valid_key(key)) {
        return -2;
    }
    
    if (param_config[key].type != PARAM_TYPE_INT16) {
        return -3;
    }
    
    if (value == NULL) {
        return -4;
    }
    
    param_lock();
    temp = param_data[key].value.i16;
    param_unlock();
    
    *value = temp;
    return 0;
}

s32 param_get_int32(ParamKey_e key, s32* value)
{
    s32 temp;
    
    if (!param_initialized) {
        return -1;
    }
    
    if (!is_valid_key(key)) {
        return -2;
    }
    
    if (param_config[key].type != PARAM_TYPE_INT32) {
        return -3;
    }
    
    if (value == NULL) {
        return -4;
    }
    
    param_lock();
    temp = param_data[key].value.i32;
    param_unlock();
    
    *value = temp;
    return 0;
}

s32 param_get_string(ParamKey_e key, char* value, u32 max_len)
{
    u8 str_idx;
    char* str_ptr;
    
    if (!param_initialized) {
        return -1;
    }
    
    if (!is_valid_key(key)) {
        return -2;
    }
    
    if (param_config[key].type != PARAM_TYPE_STRING) {
        return -3;
    }
    
    if (value == NULL) {
        return -4;
    }
    
    param_lock();
    
    /* Get string index */
    str_idx = param_data[key].value.str_idx;
    str_ptr = get_string_storage(str_idx);
    
    if (str_ptr == NULL) {
        /* String not yet set - return empty */
        value[0] = '\0';
        param_unlock();
        return 0;
    }
    
    if (max_len < Ql_strlen(str_ptr) + 1) {
        param_unlock();
        return -5;
    }
    
    Ql_strcpy(value, str_ptr);
    
    param_unlock();
    
    return 0;
}

s32 param_commit(void)
{
    u32 dirty_count;
    s32 ret;
    u32 i;
    
    if (!param_initialized) {
        APP_DEBUG("ERROR: param not initialized\r\n");
        return -1;
    }
    
    param_lock();
    
    /* Count dirty persistent parameters */
    dirty_count = count_dirty_persistent();
    
    if (dirty_count == 0) {
        APP_DEBUG("No dirty parameters to commit\r\n");
        param_unlock();
        return 0;
    }
    
    APP_DEBUG("Committing %d dirty parameters...\r\n", dirty_count);
    
    /* Save to storage */
    ret = param_storage_save((struct ParamData_s*)param_data, PARAM_MAX_COUNT,
                            (struct StringStorage_s*)string_storage, string_count);
    
    if (ret == 0) {
        /* Mark all as clean */
        for (i = 0; i < PARAM_MAX_COUNT; i++) {
            param_data[i].dirty = FALSE;
        }
    }
    
    param_unlock();
    
    return ret;
}

u32 param_count(void)
{
    return PARAM_MAX_COUNT;
}

void param_print_all(void)
{
    u32 i;
    u8 str_idx;
    char* str_ptr;
    
    APP_DEBUG("\r\n=== Parameter Table ===\r\n");
    APP_DEBUG("Total: %d parameters\r\n", PARAM_MAX_COUNT);
    APP_DEBUG("%-20s %-8s %-8s %-8s %s\r\n", 
             "Name", "Type", "Persist", "Dirty", "Value");
    APP_DEBUG("----------------------------------------------------------------\r\n");
    
    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        APP_DEBUG("%-20s ", param_config[i].name);
        
        switch (param_config[i].type) {
        case PARAM_TYPE_INT8:
            APP_DEBUG("int8     ");
            break;
        case PARAM_TYPE_INT16:
            APP_DEBUG("int16    ");
            break;
        case PARAM_TYPE_INT32:
            APP_DEBUG("int32    ");
            break;
        case PARAM_TYPE_STRING:
            APP_DEBUG("string   ");
            break;
        }
        
        APP_DEBUG("%-8s %-8s ", 
                 param_config[i].persist ? "yes" : "no",
                 param_data[i].dirty ? "yes" : "no");
        
        switch (param_config[i].type) {
        case PARAM_TYPE_INT8:
            APP_DEBUG("%d\r\n", param_data[i].value.i8);
            break;
        case PARAM_TYPE_INT16:
            APP_DEBUG("%d\r\n", param_data[i].value.i16);
            break;
        case PARAM_TYPE_INT32:
            APP_DEBUG("%d\r\n", param_data[i].value.i32);
            break;
        case PARAM_TYPE_STRING:
            str_idx = param_data[i].value.str_idx;
            str_ptr = get_string_storage(str_idx);
            APP_DEBUG("%s\r\n", str_ptr ? str_ptr : "(empty)");
            break;
        }
    }
    
    APP_DEBUG("================================================================\r\n");
    APP_DEBUG("RAM: %d int params × 8 bytes = %d bytes\r\n", 
             PARAM_MAX_COUNT, PARAM_MAX_COUNT * sizeof(ParamData_t));
    APP_DEBUG("RAM: %d string params × 256 bytes = %d bytes\r\n", 
             string_count, string_count * sizeof(StringStorage_t));
    APP_DEBUG("Total RAM: %d bytes (%.2f KB)\r\n\r\n",
             PARAM_MAX_COUNT * sizeof(ParamData_t) + string_count * sizeof(StringStorage_t),
             (float)(PARAM_MAX_COUNT * sizeof(ParamData_t) + string_count * sizeof(StringStorage_t)) / 1024.0f);
}

s32 param_get_type(ParamKey_e key, ParamType_e* type)
{
    if (!is_valid_key(key)) {
        return -1;
    }
    
    if (type != NULL) {
        *type = param_config[key].type;
    }
    
    return 0;
}

s32 param_reset(ParamKey_e key)
{
    u8 str_idx;
    char* str_ptr;
    
    if (!param_initialized) {
        return -1;
    }
    
    if (!is_valid_key(key)) {
        return -2;
    }
    
    /* Clear value */
    if (param_config[key].type == PARAM_TYPE_STRING) {
        str_idx = param_data[key].value.str_idx;
        str_ptr = get_string_storage(str_idx);
        if (str_ptr != NULL) {
            Ql_memset(str_ptr, 0, PARAM_STRING_MAX_LEN);
        }
    } else {
        Ql_memset(&param_data[key].value, 0, sizeof(ParamValue_u));
    }
    
    param_data[key].dirty = TRUE;
    
    APP_DEBUG("Reset param '%s'\r\n", param_config[key].name);
    
    return 0;
}

void param_reset_all(void)
{
    u32 i;
    
    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        Ql_memset(&param_data[i].value, 0, sizeof(ParamValue_u));
        param_data[i].dirty = FALSE;
    }
    
    /* Clear string storage */
    Ql_memset(string_storage, 0, sizeof(string_storage));
    string_count = 0;
    
    /* Delete storage file */
    param_storage_delete();
    
    APP_DEBUG("All parameters reset\r\n");
}

const char* param_get_name(ParamKey_e key)
{
    if (!is_valid_key(key)) {
        return "INVALID";
    }
    
    return param_config[key].name;
}

s32 param_set_persist(ParamKey_e key, bool persist)
{
    if (!param_initialized) {
        return -1;
    }
    
    if (!is_valid_key(key)) {
        return -2;
    }
    
    param_lock();
    param_data[key].persist = persist;
    param_unlock();
    
    return 0;
}

s32 param_get_persist(ParamKey_e key, bool* persist)
{
    bool temp;
    
    if (!param_initialized) {
        return -1;
    }
    
    if (!is_valid_key(key)) {
        return -2;
    }
    
    if (persist == NULL) {
        return -3;
    }
    
    param_lock();
    temp = param_data[key].persist;
    param_unlock();
    
    *persist = temp;
    return 0;
}

void param_get_ram_usage(u32* total_bytes, u32* int_params, u32* str_params)
{
    u32 i;
    u32 int_count = 0;
    u32 str_count_config = 0;
    
    /* Count parameter types */
    for (i = 0; i < PARAM_MAX_COUNT; i++) {
        if (param_config[i].type == PARAM_TYPE_STRING) {
            str_count_config++;
        } else {
            int_count++;
        }
    }
    
    if (int_params != NULL) {
        *int_params = int_count;
    }
    
    if (str_params != NULL) {
        *str_params = str_count_config;
    }
    
    if (total_bytes != NULL) {
        *total_bytes = PARAM_MAX_COUNT * sizeof(ParamData_t) + 
                       string_count * sizeof(StringStorage_t);
    }
}

s32 param_set_callback(ParamKey_e key, ParamChangeCallback_t callback)
{
    if (!param_initialized) {
        APP_DEBUG("ERROR: param not initialized\r\n");
        return -1;
    }
    
    if (!is_valid_key(key)) {
        APP_DEBUG("ERROR: Invalid key: %d\r\n", key);
        return -2;
    }
    
    param_lock();
    param_callbacks[key] = callback;
    param_unlock();
    
    if (callback != NULL) {
        APP_DEBUG("Registered callback for '%s'\r\n", param_config[key].name);
    } else {
        APP_DEBUG("Removed callback for '%s'\r\n", param_config[key].name);
    }
    
    return 0;
}

ParamChangeCallback_t param_get_callback(ParamKey_e key)
{
    ParamChangeCallback_t callback;
    
    if (!param_initialized) {
        return NULL;
    }
    
    if (!is_valid_key(key)) {
        return NULL;
    }
    
    param_lock();
    callback = param_callbacks[key];
    param_unlock();
    
    return callback;
}
