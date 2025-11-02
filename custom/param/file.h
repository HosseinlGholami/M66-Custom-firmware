/**
 * @file    file.h
 * @brief   File system abstraction layer
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * High-level file operations with error handling built-in
 * Simplifies file I/O for parameter storage and other modules
 */

#ifndef FILE_H
#define FILE_H

#include "ql_type.h"

/*============================================================================
 * API Functions - Simple Interface
 *===========================================================================*/

/**
 * @brief Check if file exists
 * @param filename File name to check
 * @return TRUE if exists, FALSE otherwise
 */
bool file_exists(const char* filename);

/**
 * @brief Delete a file (no error if doesn't exist)
 * @param filename File name to delete
 * @return 0 on success or if file didn't exist
 */
s32 file_delete(const char* filename);

/**
 * @brief Save data to file (high-level, handles all errors)
 * Creates/overwrites file, writes data, closes automatically
 * @param filename File name
 * @param data Data buffer to write
 * @param len Number of bytes to write
 * @return 0 on success, negative on error
 */
s32 file_save(const char* filename, const u8* data, u32 len);

/**
 * @brief Load data from file (high-level, handles all errors)
 * Opens file, reads data, closes automatically
 * @param filename File name
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @return Number of bytes read on success, negative on error
 */
s32 file_load(const char* filename, u8* data, u32 len);

/**
 * @brief Get file size
 * @param filename File name
 * @return File size in bytes, 0 if doesn't exist
 */
u32 file_size(const char* filename);

/*============================================================================
 * API Functions - Low-Level (if needed)
 *===========================================================================*/

typedef s32 FileHandle_t;

typedef enum {
    FILE_MODE_READ,
    FILE_MODE_WRITE,
    FILE_MODE_APPEND
} FileMode_e;

FileHandle_t file_open(const char* filename, FileMode_e mode);
s32 file_close(FileHandle_t handle);
s32 file_write(FileHandle_t handle, const u8* data, u32 len);
s32 file_read(FileHandle_t handle, u8* data, u32 len);
s32 file_seek(FileHandle_t handle, u32 offset);

#endif /* FILE_H */

