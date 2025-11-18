/**
 * @file    module_config.h
 * @brief   Module Enable/Disable Configuration
 * @author  Hossein Gholami
 * @date    2025-11-17
 * 
 * Central configuration file to enable/disable optional modules.
 * Comment out #define to disable a module and save flash/RAM.
 */

#ifndef MODULE_CONFIG_H
#define MODULE_CONFIG_H

/*============================================================================
 * Communication Modules
 *===========================================================================*/

/**
 * @brief Enable UART module
 * Required for debug output and AT commands
 */
#define MODULE_UART_ENABLED

/**
 * @brief Enable Command Interface module
 * Provides command parsing for UART/SMS control
 */
#define MODULE_COM_ENABLED

/*============================================================================
 * Storage Modules
 *===========================================================================*/

/**
 * @brief Enable Parameter Storage module
 * Provides persistent parameter storage in NVRAM
 */
#define MODULE_PARAM_ENABLED

/**
 * @brief Enable File System module
 * File operations on NVRAM
 */
#define MODULE_FILE_ENABLED

/*============================================================================
 * I/O Modules
 *===========================================================================*/

/**
 * @brief Enable GPIO module
 * GPIO management with parameter integration
 */
#define MODULE_GPIO_ENABLED

/*============================================================================
 * I2C Bus and Devices
 *===========================================================================*/

/**
 * @brief Enable I2C Bus Controller
 * Centralized I2C bus management
 * Required for any I2C device (OLED, IO Expander, etc.)
 */
#define MODULE_I2C_BUS_ENABLED

/**
 * @brief Enable I2C Bus Scanner
 * Device discovery and debugging tool
 * Depends on: MODULE_I2C_BUS_ENABLED
 */
#ifdef MODULE_I2C_BUS_ENABLED
    #define MODULE_I2C_SCANNER_ENABLED
#endif

/**
 * @brief Enable OLED Display module
 * SSD1306 128x64 OLED display driver
 * Depends on: MODULE_I2C_BUS_ENABLED
 */
#ifdef MODULE_I2C_BUS_ENABLED
    #define MODULE_OLED_ENABLED
#endif

/**
 * @brief Enable IO Expander module
 * PCF8574 I2C IO expander with cascade support
 * Depends on: MODULE_I2C_BUS_ENABLED
 */
#ifdef MODULE_I2C_BUS_ENABLED
    #define MODULE_IO_EXPANDER_ENABLED
#endif

/*============================================================================
 * Network Modules
 *===========================================================================*/

/**
 * @brief Enable FOTA (Firmware Over The Air) module
 * Remote firmware update capability
 */
//#define MODULE_FOTA_ENABLED

/*============================================================================
 * Feature Configuration
 *===========================================================================*/

/**
 * @brief Enable verbose debug output
 * More detailed logging for troubleshooting
 */
// #define MODULE_DEBUG_VERBOSE

/**
 * @brief Enable module initialization messages
 * Shows which modules are enabled at startup
 */
#define MODULE_SHOW_INIT_STATUS

/*============================================================================
 * Validation
 *===========================================================================*/

/* Validate I2C device dependencies */
#if defined(MODULE_OLED_ENABLED) && !defined(MODULE_I2C_BUS_ENABLED)
    #error "OLED module requires I2C bus controller (enable MODULE_I2C_BUS_ENABLED)"
#endif

#if defined(MODULE_IO_EXPANDER_ENABLED) && !defined(MODULE_I2C_BUS_ENABLED)
    #error "IO Expander module requires I2C bus controller (enable MODULE_I2C_BUS_ENABLED)"
#endif

#if defined(MODULE_I2C_SCANNER_ENABLED) && !defined(MODULE_I2C_BUS_ENABLED)
    #error "I2C Scanner requires I2C bus controller (enable MODULE_I2C_BUS_ENABLED)"
#endif

/* Validate COM module dependencies */
#if defined(MODULE_COM_ENABLED) && !defined(MODULE_UART_ENABLED)
    #error "COM module requires UART (enable MODULE_UART_ENABLED)"
#endif

#endif /* MODULE_CONFIG_H */

