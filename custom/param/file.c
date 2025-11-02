/**
 * @file    file.c
 * @brief   File system abstraction layer implementation
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * Provides high-level file operations with error handling
 */

#include "file.h"
#include "ql_fs.h"
#include "ql_stdlib.h"
#include "uart/uart.h"

/*============================================================================
 * Constants
 *===========================================================================*/
#define FILE_INVALID_HANDLE     (-1)

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

FileHandle_t file_open(const char* filename, FileMode_e mode)
{
    s32 handle;
    u32 ql_mode;
    
    if (filename == NULL) {
        APP_DEBUG("ERROR: file_open: NULL filename\r\n");
        return FILE_INVALID_HANDLE;
    }
    
    /* Map our mode to Quectel mode */
    switch (mode) {
    case FILE_MODE_READ:
        ql_mode = QL_FS_READ_ONLY;
        break;
        
    case FILE_MODE_WRITE:
        ql_mode = QL_FS_CREATE_ALWAYS;
        break;
        
    case FILE_MODE_APPEND:
        ql_mode = QL_FS_CREATE;  /* Open or create */
        break;
        
    default:
        APP_DEBUG("ERROR: file_open: Invalid mode %d\r\n", mode);
        return FILE_INVALID_HANDLE;
    }
    
    /* Open file */
    handle = Ql_FS_Open((char*)filename, ql_mode);
    if (handle < 0) {
        if (mode == FILE_MODE_READ) {
            /* Not an error - file might not exist yet */
            return FILE_INVALID_HANDLE;
        }
        APP_DEBUG("ERROR: file_open: Failed to open '%s', ret=%d\r\n", 
                 filename, handle);
        return FILE_INVALID_HANDLE;
    }
    
    return handle;
}

s32 file_close(FileHandle_t handle)
{
    if (handle < 0) {
        return -1;
    }
    
    Ql_FS_Close(handle);  /* Returns void */
    
    return 0;
}

s32 file_write(FileHandle_t handle, const u8* data, u32 len)
{
    s32 ret;
    u32 bytes_written = 0;
    
    if (handle < 0) {
        APP_DEBUG("ERROR: file_write: Invalid handle\r\n");
        return -1;
    }
    
    if (data == NULL || len == 0) {
        APP_DEBUG("ERROR: file_write: Invalid data or length\r\n");
        return -2;
    }
    
    ret = Ql_FS_Write(handle, (u8*)data, len, &bytes_written);
    if (ret < 0) {
        APP_DEBUG("ERROR: file_write: Failed to write, ret=%d\r\n", ret);
        return ret;
    }
    
    if (bytes_written != len) {
        APP_DEBUG("WARNING: file_write: Partial write %d/%d bytes\r\n", 
                 bytes_written, len);
        return bytes_written;
    }
    
    return bytes_written;
}

s32 file_read(FileHandle_t handle, u8* data, u32 len)
{
    s32 ret;
    u32 bytes_read = 0;
    
    if (handle < 0) {
        APP_DEBUG("ERROR: file_read: Invalid handle\r\n");
        return -1;
    }
    
    if (data == NULL || len == 0) {
        APP_DEBUG("ERROR: file_read: Invalid data or length\r\n");
        return -2;
    }
    
    ret = Ql_FS_Read(handle, data, len, &bytes_read);
    if (ret < 0) {
        APP_DEBUG("ERROR: file_read: Failed to read, ret=%d\r\n", ret);
        return ret;
    }
    
    return bytes_read;
}

s32 file_delete(const char* filename)
{
    s32 ret;
    
    if (filename == NULL) {
        APP_DEBUG("ERROR: file_delete: NULL filename\r\n");
        return -1;
    }
    
    ret = Ql_FS_Delete((char*)filename);
    if (ret < 0) {
        /* Not necessarily an error - file might not exist */
        return ret;
    }
    
    return 0;
}

bool file_exists(const char* filename)
{
    FileHandle_t handle;
    
    if (filename == NULL) {
        return FALSE;
    }
    
    /* Try to open for reading */
    handle = file_open(filename, FILE_MODE_READ);
    if (handle < 0) {
        return FALSE;
    }
    
    file_close(handle);
    return TRUE;
}

s32 file_get_size(const char* filename)
{
    s32 size;
    
    if (filename == NULL) {
        return -1;
    }
    
    size = Ql_FS_GetSize((char*)filename);
    if (size < 0) {
        return -1;
    }
    
    return size;
}

s32 file_seek(FileHandle_t handle, u32 offset)
{
    s32 ret;
    
    if (handle < 0) {
        return -1;
    }
    
    ret = Ql_FS_Seek(handle, offset, QL_FS_FILE_BEGIN);
    if (ret < 0) {
        APP_DEBUG("ERROR: file_seek: Failed to seek, ret=%d\r\n", ret);
        return ret;
    }
    
    return 0;
}

/*============================================================================
 * High-Level API Implementation
 *===========================================================================*/

s32 file_save(const char* filename, const u8* data, u32 len)
{
    FileHandle_t fh;
    s32 ret;
    
    if (filename == NULL || data == NULL || len == 0) {
        APP_DEBUG("ERROR: file_save: Invalid parameters\r\n");
        return -1;
    }
    
    /* Delete old file first */
    file_delete(filename);
    
    /* Open for writing */
    fh = file_open(filename, FILE_MODE_WRITE);
    if (fh < 0) {
        APP_DEBUG("ERROR: file_save: Failed to create '%s'\r\n", filename);
        return -2;
    }
    
    /* Write data */
    ret = file_write(fh, data, len);
    if (ret != (s32)len) {
        APP_DEBUG("ERROR: file_save: Write failed for '%s', wrote %d/%d bytes\r\n", 
                 filename, ret, len);
        file_close(fh);
        return -3;
    }
    
    /* Close file */
    file_close(fh);
    
    return 0;
}

s32 file_load(const char* filename, u8* data, u32 len)
{
    FileHandle_t fh;
    s32 ret;
    
    if (filename == NULL || data == NULL || len == 0) {
        APP_DEBUG("ERROR: file_load: Invalid parameters\r\n");
        return -1;
    }
    
    /* Check if file exists */
    if (!file_exists(filename)) {
        return 0;  /* Not an error - file doesn't exist yet */
    }
    
    /* Open for reading */
    fh = file_open(filename, FILE_MODE_READ);
    if (fh < 0) {
        APP_DEBUG("ERROR: file_load: Failed to open '%s'\r\n", filename);
        return -2;
    }
    
    /* Clear buffer */
    Ql_memset(data, 0, len);
    
    /* Read data */
    ret = file_read(fh, data, len);
    if (ret < 0) {
        APP_DEBUG("ERROR: file_load: Read failed for '%s'\r\n", filename);
        file_close(fh);
        return -3;
    }
    
    /* Close file */
    file_close(fh);
    
    return ret;  /* Return bytes read */
}

u32 file_size(const char* filename)
{
    s32 size;
    
    if (filename == NULL) {
        return 0;
    }
    
    if (!file_exists(filename)) {
        return 0;
    }
    
    size = file_get_size(filename);
    if (size < 0) {
        return 0;
    }
    
    return (u32)size;
}

