/**
 * @file    param_storage.h
 * @brief   Parameter storage records
 */

#ifndef PARAM_STORAGE_H
#define PARAM_STORAGE_H

#include "param.h"

typedef struct {
    u32          key;
    ParamType_e  type;
    u8           persist;
    u8           reserved[3];
    ParamValue_u value;
    char         string_value[PARAM_STRING_MAX_LEN];
} ParamStorageRecord_t;

s32 param_storage_save(const ParamStorageRecord_t* records, u32 count);
s32 param_storage_load(ParamStorageRecord_t* records, u32 max_count, u32* count_out);
bool param_storage_exists(void);
s32 param_storage_delete(void);

#endif /* PARAM_STORAGE_H */
