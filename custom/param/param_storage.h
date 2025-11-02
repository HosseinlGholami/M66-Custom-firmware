/**
 * @file    param_storage.h
 * @brief   Parameter storage operations (file I/O)
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * Handles loading/saving parameters to/from NVRAM
 * Separated from main param.c for cleaner architecture
 */

#ifndef PARAM_STORAGE_H
#define PARAM_STORAGE_H

#include "ql_type.h"

/*============================================================================
 * Forward Declarations
 *===========================================================================*/
struct ParamData_s;
struct StringStorage_s;

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Save parameters to NVRAM
 * @param param_data Array of parameter data
 * @param param_count Number of parameters
 * @param string_storage Array of string storage
 * @param string_count Number of strings
 * @return 0 on success, negative on error
 */
s32 param_storage_save(struct ParamData_s* param_data, u32 param_count,
                       struct StringStorage_s* string_storage, u32 string_count);

/**
 * @brief Load parameters from NVRAM
 * @param param_data Array to load parameter data into
 * @param param_count Number of parameters
 * @param string_storage Array to load strings into
 * @param string_count_out Pointer to store number of strings loaded
 * @param max_strings Maximum strings that can be loaded
 * @return Number of parameters loaded, negative on error
 */
s32 param_storage_load(struct ParamData_s* param_data, u32 param_count,
                       struct StringStorage_s* string_storage, u32* string_count_out,
                       u32 max_strings);

/**
 * @brief Delete parameter storage file
 * @return 0 on success
 */
s32 param_storage_delete(void);

#endif /* PARAM_STORAGE_H */

