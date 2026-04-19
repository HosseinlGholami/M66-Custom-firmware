/**
 * @file    param_storage.c
 * @brief   Parameter storage implementation
 */

#include "param_storage.h"
#include "file.h"
#include "uart/uart.h"
#include "ql_stdlib.h"

#define PARAM_STORAGE_FILE      "param.dat"
#define PARAM_MAGIC             0x50524D56
#define PARAM_VERSION           7

typedef struct {
    u32 magic;
    u32 version;
    u32 count;
    u32 checksum;
} ParamFileHeader_t;

static u32 param_storage_checksum(const u8* data, u32 len)
{
    u32 i;
    u32 checksum = 5381;

    for (i = 0; i < len; i++) {
        checksum = ((checksum << 5) + checksum) ^ data[i];
    }

    return checksum;
}

bool param_storage_exists(void)
{
    return file_exists(PARAM_STORAGE_FILE);
}

s32 param_storage_save(const ParamStorageRecord_t* records, u32 count)
{
    ParamFileHeader_t header;
    u8* buffer;
    u32 buffer_size;
    s32 ret;

    if (count > 0 && records == NULL) {
        return -1;
    }

    buffer_size = sizeof(ParamFileHeader_t) + (count * sizeof(ParamStorageRecord_t));
    buffer = (u8*)Ql_MEM_Alloc(buffer_size);
    if (buffer == NULL) {
        APP_DEBUG("ERROR: Failed to allocate param storage buffer\r\n");
        return -2;
    }

    header.magic = PARAM_MAGIC;
    header.version = PARAM_VERSION;
    header.count = count;
    header.checksum = 0;

    Ql_memcpy(buffer, &header, sizeof(header));
    if (count > 0) {
        Ql_memcpy(buffer + sizeof(header), records, count * sizeof(ParamStorageRecord_t));
    }

    header.checksum = param_storage_checksum(buffer + sizeof(header),
                                             count * sizeof(ParamStorageRecord_t));
    Ql_memcpy(buffer, &header, sizeof(header));

    ret = file_save(PARAM_STORAGE_FILE, buffer, buffer_size);
    Ql_MEM_Free(buffer);

    if (ret < 0) {
        APP_DEBUG("ERROR: Failed to save param storage\r\n");
        return -3;
    }

    return 0;
}

s32 param_storage_load(ParamStorageRecord_t* records, u32 max_count, u32* count_out)
{
    ParamFileHeader_t header;
    u8* buffer;
    u32 file_size_bytes;
    u32 payload_size;
    u32 copy_count;
    s32 ret;

    if (count_out == NULL) {
        return -1;
    }

    *count_out = 0;

    if (!param_storage_exists()) {
        APP_DEBUG("No existing param file found (first boot?)\r\n");
        return 0;
    }

    file_size_bytes = file_size(PARAM_STORAGE_FILE);
    if (file_size_bytes < sizeof(ParamFileHeader_t)) {
        APP_DEBUG("ERROR: Invalid param storage size\r\n");
        return -2;
    }

    buffer = (u8*)Ql_MEM_Alloc(file_size_bytes);
    if (buffer == NULL) {
        APP_DEBUG("ERROR: Failed to allocate load buffer\r\n");
        return -3;
    }

    ret = file_load(PARAM_STORAGE_FILE, buffer, file_size_bytes);
    if (ret < 0 || (u32)ret != file_size_bytes) {
        APP_DEBUG("ERROR: Failed to read param storage\r\n");
        Ql_MEM_Free(buffer);
        return -4;
    }

    Ql_memcpy(&header, buffer, sizeof(header));
    if (header.magic != PARAM_MAGIC || header.version != PARAM_VERSION) {
        APP_DEBUG("WARNING: Param storage header mismatch\r\n");
        Ql_MEM_Free(buffer);
        return -5;
    }

    payload_size = header.count * sizeof(ParamStorageRecord_t);
    if (sizeof(ParamFileHeader_t) + payload_size != file_size_bytes) {
        APP_DEBUG("WARNING: Param storage size mismatch\r\n");
        Ql_MEM_Free(buffer);
        return -6;
    }

    if (header.checksum != param_storage_checksum(buffer + sizeof(header), payload_size)) {
        APP_DEBUG("WARNING: Param storage checksum mismatch\r\n");
        Ql_MEM_Free(buffer);
        return -7;
    }

    copy_count = header.count;
    if (copy_count > max_count) {
        copy_count = max_count;
    }

    if (copy_count > 0 && records != NULL) {
        Ql_memcpy(records, buffer + sizeof(header), copy_count * sizeof(ParamStorageRecord_t));
    }

    *count_out = copy_count;
    Ql_MEM_Free(buffer);

    return copy_count;
}

s32 param_storage_delete(void)
{
    return file_delete(PARAM_STORAGE_FILE);
}
