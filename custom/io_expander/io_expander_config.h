/**
 * @file    io_expander_config.h
 * @brief   IO Expander Configuration Example
 * @author  Hossein Gholami
 * @date    2025-11-17
 * 
 * This file shows how to configure multiple PCF8574 devices.
 * Customize this file for your hardware setup.
 */

#ifndef IO_EXPANDER_CONFIG_H
#define IO_EXPANDER_CONFIG_H

#include "io_expander.h"

/*============================================================================
 * PCF8574 Device Configuration
 * 
 * The user has two PCF8574 devices with addresses:
 * - 0x42 (7-bit address, or 0x84 in 8-bit format)
 * - 0x43 (7-bit address, or 0x86 in 8-bit format)
 * 
 * Note: PCF8574 standard addresses are 0x20-0x27 (A0-A2 pins)
 *       PCF8574A addresses are 0x38-0x3F (A0-A2 pins)
 *       Custom addresses 0x42-0x43 might be from custom addressing
 *       or different variants. Verify with your hardware datasheet.
 *===========================================================================*/

/**
 * @brief Default IO expander configuration
 * 
 * Each device can be configured with:
 * - name: Descriptive name for debugging
 * - i2c_addr: 7-bit I2C address (e.g., 0x21 for A0=HIGH)
 * - init_state: Initial pin states (1=input/high, 0=output/low)
 * - enabled: Enable/disable this device
 * 
 * Pin state bits:
 *   Bit 0 = P0, Bit 1 = P1, ..., Bit 7 = P7
 *   1 = Input (high impedance) or Output HIGH
 *   0 = Output LOW
 */
static const IoExpanderConfig_t io_expander_default_config[] = {
    /* Device 0: PCF8574 at 0x42 - INPUTS with interrupt monitoring */
    {
        .name = "PCF8574_0x42",
        .i2c_addr = 0x42,           /* 7-bit address */
        .init_state = 0xFF,         /* All pins as inputs (high-Z) with pull-ups */
        .enabled = TRUE
    },
    
    /* Device 1: PCF8574 at 0x4A - OUTPUTS for LED/relay control */
    {
        .name = "PCF8574_0x4A",
        .i2c_addr = 0x4A,           /* 7-bit address */
        .init_state = 0x00,         /* All pins as outputs, initially LOW */
        .enabled = TRUE
    },
    
    /* Add more devices here for cascading...
    {
        .name = "PCF8574_0x44",
        .i2c_addr = 0x44,
        .init_state = 0xFF,
        .enabled = FALSE
    },
    */
};

#define IO_EXPANDER_DEFAULT_CONFIG_COUNT \
    (sizeof(io_expander_default_config) / sizeof(io_expander_default_config[0]))

/*============================================================================
 * Pin Definitions (Optional - for easier code readability)
 * 
 * Define meaningful names for your I/O pins here
 *===========================================================================*/

/* Example pin definitions for Device 0 (0x42) */
#define IO_EXP_DEV0_LED1        0   /* Device 0, Pin 0 */
#define IO_EXP_DEV0_LED2        1   /* Device 0, Pin 1 */
#define IO_EXP_DEV0_RELAY1      2   /* Device 0, Pin 2 */
#define IO_EXP_DEV0_RELAY2      3   /* Device 0, Pin 3 */
#define IO_EXP_DEV0_BUTTON1     4   /* Device 0, Pin 4 (input) */
#define IO_EXP_DEV0_BUTTON2     5   /* Device 0, Pin 5 (input) */
#define IO_EXP_DEV0_SENSOR1     6   /* Device 0, Pin 6 (input) */
#define IO_EXP_DEV0_SENSOR2     7   /* Device 0, Pin 7 (input) */

/* Example pin definitions for Device 1 (0x43) */
#define IO_EXP_DEV1_LED1        0   /* Device 1, Pin 0 */
#define IO_EXP_DEV1_LED2        1   /* Device 1, Pin 1 */
#define IO_EXP_DEV1_RELAY1      2   /* Device 1, Pin 2 */
#define IO_EXP_DEV1_RELAY2      3   /* Device 1, Pin 3 */
#define IO_EXP_DEV1_BUTTON1     4   /* Device 1, Pin 4 (input) */
#define IO_EXP_DEV1_BUTTON2     5   /* Device 1, Pin 5 (input) */
#define IO_EXP_DEV1_SENSOR1     6   /* Device 1, Pin 6 (input) */
#define IO_EXP_DEV1_SENSOR2     7   /* Device 1, Pin 7 (input) */

/*============================================================================
 * Helper Macros
 *===========================================================================*/

/* Device IDs (based on config array index) */
#define IO_EXP_DEVICE_0         0
#define IO_EXP_DEVICE_1         1

#endif /* IO_EXPANDER_CONFIG_H */

