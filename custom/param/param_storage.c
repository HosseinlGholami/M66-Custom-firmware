/**
 * @file    param_storage.c
 * @brief   Parameter storage operations implementation
 * @author  Hossein Gholami
 * @date    2025-11-01
 */

#include "param_storage.h"
#include "param.h"
#include "file.h"
#include "uart/uart.h"
#include "ql_stdlib.h"

/*============================================================================
 * Constants
 *===========================================================================*/
#define PARAM_STORAGE_FILE      "param.dat"
#define PARAM_MAGIC             0x50524D56  /* "PRMV" */
#define PARAM_VERSION           2

/*============================================================================
 * Private Types
 *===========================================================================*/

/**
 * @brief Parameter data structure (must match param.c)
 */
typedef struct ParamData_s {
    ParamKey_e key;
    ParamValue_u value;
    bool dirty;
} ParamData_t;

/**
 * @brief String storage structure (must match param.c)
 */
typedef struct StringStorage_s {
    ParamKey_e key;
    char data[PARAM_STRING_MAX_LEN];
} StringStorage_t;

/**
 * @brief File header
 */
typedef struct {
    u32 magic;
    u32 version;
    u32 count;
    u32 str_count;
    u32 checksum;
} ParamFileHeader_t;

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

s32 param_storage_save(struct ParamData_s* param_data, u32 param_count,
                       struct StringStorage_s* string_storage, u32 string_count)
{
    u8* buffer;
    u32 buffer_size;
    u32 offset = 0;
    ParamFileHeader_t header;
    u32 i;
    s32 ret;
    
    if (param_data == NULL) {
        return -1;
    }
    
    /* Calculate buffer size */
    buffer_size = sizeof(ParamFileHeader_t) + 
                  (param_count * sizeof(ParamData_t)) +
                  (string_count * sizeof(StringStorage_t));
    
    /* Allocate buffer */
    buffer = (u8*)Ql_MEM_Alloc(buffer_size);
    if (buffer == NULL) {
        APP_DEBUG("ERROR: Failed to allocate buffer for param save\r\n");
        return -2;
    }
    
    Ql_memset(buffer, 0, buffer_size);
    
    /* Prepare header */
    header.magic = PARAM_MAGIC;
    header.version = PARAM_VERSION;
    header.count = param_count;
    header.str_count = string_count;
    header.checksum = 0;
    
    /* Pack header */
    Ql_memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);
    
    /* Pack parameter data */
    Ql_memcpy(buffer + offset, param_data, param_count * sizeof(ParamData_t));
    offset += param_count * sizeof(ParamData_t);
    
    /* Pack string storage */
    if (string_count > 0 && string_storage != NULL) {
        Ql_memcpy(buffer + offset, string_storage, string_count * sizeof(StringStorage_t));
        offset += string_count * sizeof(StringStorage_t);
    }
    
    /* Save to file */
    ret = file_save(PARAM_STORAGE_FILE, buffer, buffer_size);
    
    /* Free buffer */
    Ql_MEM_Free(buffer);
    
    if (ret < 0) {
        APP_DEBUG("ERROR: Failed to save parameters to file\r\n");
        return -3;
    }
    
    APP_DEBUG("Saved %d parameters (%d strings) to NVRAM\r\n", 
             param_count, string_count);
    
    return 0;
}

s32 param_storage_load(struct ParamData_s* param_data, u32 param_count,
                       struct StringStorage_s* string_storage, u32* string_count_out,
                       u32 max_strings)
{
    u8* buffer;
    u32 buffer_size;
    u32 offset = 0;
    ParamFileHeader_t header;
    s32 ret;
    u32 loaded = 0;
    u32 i;
    
    if (param_data == NULL || string_count_out == NULL) {
        return -1;
    }
    
    *string_count_out = 0;
    
    /* Check if file exists */
    if (!file_exists(PARAM_STORAGE_FILE)) {
        APP_DEBUG("No existing param file found (first boot?)\r\n");
        return 0;
    }
    
    /* Get file size */
    buffer_size = file_size(PARAM_STORAGE_FILE);
    if (buffer_size == 0) {
        APP_DEBUG("ERROR: Invalid param file size\r\n");
        return -2;
    }
    
    /* Allocate buffer */
    buffer = (u8*)Ql_MEM_Alloc(buffer_size);
    if (buffer == NULL) {
        APP_DEBUG("ERROR: Failed to allocate buffer for param load\r\n");
        return -3;
    }
    
    /* Load from file */
    ret = file_load(PARAM_STORAGE_FILE, buffer, buffer_size);
    if (ret < 0) {
        APP_DEBUG("ERROR: Failed to load parameters from file\r\n");
        Ql_MEM_Free(buffer);
        return -4;
    }
    
    /* Unpack header */
    Ql_memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);
    
    /* Validate header */
    if (header.magic != PARAM_MAGIC) {
        APP_DEBUG("ERROR: Invalid magic number: 0x%08X\r\n", header.magic);
        Ql_MEM_Free(buffer);
        return -5;
    }
    
    if (header.version != PARAM_VERSION) {
        APP_DEBUG("WARNING: Version mismatch: %d (expected %d)\r\n", 
                 header.version, PARAM_VERSION);
    }
    
    APP_DEBUG("Loading %d parameters (%d strings) from NVRAM...\r\n", 
             header.count, header.str_count);
    
    /* Unpack parameters */
    if (header.count > param_count) {
        APP_DEBUG("WARNING: File has more params than expected\r\n");
        header.count = param_count;
    }
    
    Ql_memcpy(param_data, buffer + offset, header.count * sizeof(ParamData_t));
    offset += header.count * sizeof(ParamData_t);
    loaded = header.count;
    
    /* Unpack strings */
    if (header.str_count > 0 && string_storage != NULL) {
        u32 str_to_load = header.str_count;
        if (str_to_load > max_strings) {
            APP_DEBUG("WARNING: File has more strings than storage\r\n");
            str_to_load = max_strings;
        }
        
        Ql_memcpy(string_storage, buffer + offset, str_to_load * sizeof(StringStorage_t));
        *string_count_out = str_to_load;
    }
    
    /* Free buffer */
    Ql_MEM_Free(buffer);
    
    APP_DEBUG("Loaded %d parameters (%d strings) from NVRAM\r\n", 
             loaded, *string_count_out);
    
    return loaded;
}

s32 param_storage_delete(void)
{
    return file_delete(PARAM_STORAGE_FILE);
}

