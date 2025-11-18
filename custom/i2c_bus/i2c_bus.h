/**
 * @file    i2c_bus.h
 * @brief   Centralized I2C Bus Controller
 * @author  Hossein Gholami
 * @date    2025-11-17
 * 
 * Manages I2C bus initialization and configuration for all I2C devices.
 * Replaces individual module I2C initialization to avoid conflicts.
 * 
 * Features:
 * - Single I2C bus initialization point
 * - Shared by all I2C modules (OLED, IO Expander, etc.)
 * - Built-in bus scanner for device discovery
 * - Device registry for address management
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "ql_type.h"
#include "ql_gpio.h"

/*============================================================================
 * Configuration
 *===========================================================================*/

/* I2C Bus Configuration */
#define I2C_BUS_CHANNEL         0       /* I2C channel (0 = simulated I2C) */

/* Default pins (can be overridden in i2c_bus_init) */
#define I2C_BUS_DEFAULT_SCL     PINNAME_RI      /* Default SCL pin */
#define I2C_BUS_DEFAULT_SDA     PINNAME_DCD     /* Default SDA pin */

/*============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief I2C bus result codes
 */
typedef enum {
    I2C_BUS_OK = 0,
    I2C_BUS_ERR_INIT = -1,
    I2C_BUS_ERR_CONFIG = -2,
    I2C_BUS_ERR_NOT_INITIALIZED = -3,
    I2C_BUS_ERR_PARAM = -4,
    I2C_BUS_ERR_ALREADY_INITIALIZED = -5
} I2cBusResult_e;

/**
 * @brief I2C device information
 */
typedef struct {
    u8          addr;           /* 7-bit I2C address */
    const char* name;           /* Device name/description */
    bool        active;         /* Is device currently in use */
} I2cDeviceInfo_t;

/*============================================================================
 * API Functions - Bus Management
 *===========================================================================*/

/**
 * @brief Initialize I2C bus
 * 
 * Must be called before any I2C device is used.
 * Only one module should call this - typically in main initialization.
 * 
 * @param pinSCL SCL pin (e.g., PINNAME_RI)
 * @param pinSDA SDA pin (e.g., PINNAME_DCD)
 * @return I2C_BUS_OK on success, error code otherwise
 */
s32 i2c_bus_init(Enum_PinName pinSCL, Enum_PinName pinSDA);

/**
 * @brief Configure I2C device address
 * 
 * Registers a device on the bus. Call this before communicating with device.
 * 
 * @param addr 7-bit I2C address
 * @param device_name Device name for debugging (can be NULL)
 * @return I2C_BUS_OK on success, error code otherwise
 */
s32 i2c_bus_config_device(u8 addr, const char* device_name);

/**
 * @brief Get I2C channel number
 * 
 * @return I2C channel number
 */
u32 i2c_bus_get_channel(void);

/**
 * @brief Get SCL pin
 * 
 * @return SCL pin name
 */
Enum_PinName i2c_bus_get_scl_pin(void);

/**
 * @brief Get SDA pin
 * 
 * @return SDA pin name
 */
Enum_PinName i2c_bus_get_sda_pin(void);

/**
 * @brief Check if bus is initialized
 * 
 * @return TRUE if initialized, FALSE otherwise
 */
bool i2c_bus_is_initialized(void);

/**
 * @brief Reinitialize I2C bus
 * 
 * Useful when bus becomes stuck or after errors.
 * Uses previously configured pins.
 * 
 * @return I2C_BUS_OK on success, error code otherwise
 */
s32 i2c_bus_reinit(void);

/**
 * @brief Uninitialize I2C bus
 * 
 * Releases I2C resources. Should be called before shutdown.
 */
void i2c_bus_uninit(void);

/*============================================================================
 * API Functions - Device Discovery
 *===========================================================================*/

/**
 * @brief Scan I2C bus for all devices
 * 
 * Searches all 7-bit addresses (0x01-0x7F) and reports found devices.
 * Useful for debugging and device discovery.
 * 
 * @return Number of devices found
 */
u8 i2c_bus_scan(void);

/**
 * @brief Test if specific device exists on bus
 * 
 * @param addr 7-bit I2C address to test
 * @return TRUE if device responds, FALSE otherwise
 */
bool i2c_bus_device_exists(u8 addr);

/**
 * @brief Get list of registered devices
 * 
 * @param devices Array to store device info (must be at least max_devices size)
 * @param max_devices Maximum number of devices to return
 * @return Number of devices returned
 */
u8 i2c_bus_get_devices(I2cDeviceInfo_t* devices, u8 max_devices);

/**
 * @brief Print bus status and registered devices
 * 
 * Useful for debugging.
 */
void i2c_bus_print_status(void);

/*============================================================================
 * Known Device Database
 *===========================================================================*/

/**
 * @brief Get device name from address
 * 
 * Looks up device in known device database.
 * 
 * @param addr 7-bit I2C address
 * @return Device name or NULL if unknown
 */
const char* i2c_bus_get_device_name(u8 addr);

#endif /* I2C_BUS_H */

