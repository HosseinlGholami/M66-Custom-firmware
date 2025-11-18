/**
 * @file    io_expander.h
 * @brief   PCF8574 I2C IO Expander Driver with Cascade Support
 * @author  Hossein Gholami
 * @date    2025-11-17
 * 
 * Features:
 * - Support for multiple cascaded PCF8574 devices
 * - Configurable I2C addresses
 * - Interrupt support (INT pin to GPIO)
 * - 8-bit parallel I/O expansion per device
 * - Individual pin control and read-back
 * - Change notification callbacks
 */

#ifndef IO_EXPANDER_H
#define IO_EXPANDER_H

#include "ql_type.h"
#include "ql_gpio.h"

/*============================================================================
 * Constants
 *===========================================================================*/

/* Maximum number of PCF8574 devices supported */
#define IO_EXPANDER_MAX_DEVICES     8

/* PCF8574 I2C addresses (7-bit format) */
#define PCF8574_ADDR_BASE           0x20  /* Base address for PCF8574 */
#define PCF8574A_ADDR_BASE          0x38  /* Base address for PCF8574A */

/* Pin masks for individual bit operations */
#define IO_EXPANDER_PIN0            (1 << 0)
#define IO_EXPANDER_PIN1            (1 << 1)
#define IO_EXPANDER_PIN2            (1 << 2)
#define IO_EXPANDER_PIN3            (1 << 3)
#define IO_EXPANDER_PIN4            (1 << 4)
#define IO_EXPANDER_PIN5            (1 << 5)
#define IO_EXPANDER_PIN6            (1 << 6)
#define IO_EXPANDER_PIN7            (1 << 7)
#define IO_EXPANDER_ALL_PINS        0xFF

/* I2C Configuration */
#define IO_EXPANDER_I2C_CHANNEL     0
#define IO_EXPANDER_I2C_SPEED       100   /* 100 kHz */

/*============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief IO Expander result codes
 */
typedef enum {
    IO_EXPANDER_OK = 0,
    IO_EXPANDER_ERR_I2C_INIT = -1,
    IO_EXPANDER_ERR_I2C_WRITE = -2,
    IO_EXPANDER_ERR_I2C_READ = -3,
    IO_EXPANDER_ERR_INVALID_DEVICE = -4,
    IO_EXPANDER_ERR_DEVICE_NOT_FOUND = -5,
    IO_EXPANDER_ERR_MAX_DEVICES = -6,
    IO_EXPANDER_ERR_PARAM = -7,
    IO_EXPANDER_ERR_EINT_INIT = -8,
    IO_EXPANDER_ERR_NOT_INITIALIZED = -9
} IoExpanderResult_e;

/**
 * @brief IO Expander device configuration
 */
typedef struct {
    const char*     name;           /* Device name (for debug) */
    u8              i2c_addr;       /* I2C address (7-bit format, e.g., 0x21 for A0 HIGH) */
    u8              init_state;     /* Initial pin states (1=input/high, 0=output/low) */
    bool            enabled;        /* Enable this device */
} IoExpanderConfig_t;

/**
 * @brief IO Expander interrupt callback
 * Called when any configured device's INT pin is triggered
 * 
 * @param device_id Device ID (0-based index in config table)
 * @param pin_states Current state of all 8 pins (bit 0 = P0, bit 7 = P7)
 */
typedef void (*IoExpanderIntCallback_t)(u8 device_id, u8 pin_states);

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize IO expander module
 * 
 * I2C bus must be initialized with i2c_bus_init() before calling this function.
 * 
 * @param pinINT Interrupt pin (e.g., PINNAME_CTS), set to 0 to disable interrupts
 * @param config Array of device configurations
 * @param device_count Number of devices in config array
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_init(Enum_PinName pinINT,
                     const IoExpanderConfig_t* config, 
                     u8 device_count);

/**
 * @brief Register interrupt callback
 * 
 * @param callback Callback function to call on interrupt
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_register_int_callback(IoExpanderIntCallback_t callback);

/**
 * @brief Write all 8 pins of a device
 * 
 * @param device_id Device ID (0-based index in config table)
 * @param pin_states Pin states (bit 0 = P0, bit 7 = P7)
 *                   1 = high/input, 0 = low/output
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_write_port(u8 device_id, u8 pin_states);

/**
 * @brief Read all 8 pins of a device
 * 
 * @param device_id Device ID (0-based index in config table)
 * @param pin_states Pointer to store pin states
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_read_port(u8 device_id, u8* pin_states);

/**
 * @brief Set specific pins high (set bits)
 * 
 * @param device_id Device ID
 * @param pin_mask Pins to set high (use IO_EXPANDER_PINx macros)
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_set_pins(u8 device_id, u8 pin_mask);

/**
 * @brief Clear specific pins low (clear bits)
 * 
 * @param device_id Device ID
 * @param pin_mask Pins to clear low (use IO_EXPANDER_PINx macros)
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_clear_pins(u8 device_id, u8 pin_mask);

/**
 * @brief Toggle specific pins
 * 
 * @param device_id Device ID
 * @param pin_mask Pins to toggle (use IO_EXPANDER_PINx macros)
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_toggle_pins(u8 device_id, u8 pin_mask);

/**
 * @brief Configure specific pins as inputs (set high for input mode)
 * 
 * @param device_id Device ID
 * @param pin_mask Pins to configure as inputs
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_set_as_input(u8 device_id, u8 pin_mask);

/**
 * @brief Read specific pin state
 * 
 * @param device_id Device ID
 * @param pin Pin number (0-7)
 * @param state Pointer to store pin state (0 or 1)
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_read_pin(u8 device_id, u8 pin, u8* state);

/**
 * @brief Write specific pin state
 * 
 * @param device_id Device ID
 * @param pin Pin number (0-7)
 * @param state Pin state (0=low, 1=high)
 * @return IO_EXPANDER_OK on success, error code otherwise
 */
s32 io_expander_write_pin(u8 device_id, u8 pin, u8 state);

/**
 * @brief Get device count
 * 
 * @return Number of configured devices
 */
u8 io_expander_get_device_count(void);

/**
 * @brief Get device name by ID
 * 
 * @param device_id Device ID
 * @return Device name or NULL if invalid
 */
const char* io_expander_get_device_name(u8 device_id);

/**
 * @brief Scan all devices and print status
 * Useful for debugging
 */
void io_expander_print_status(void);

/**
 * @brief Test I2C communication with a device
 * 
 * @param device_id Device ID
 * @return TRUE if device responds, FALSE otherwise
 */
bool io_expander_test_device(u8 device_id);

/**
 * @brief Check if IO expander is initialized
 * 
 * @return TRUE if initialized, FALSE otherwise
 */
bool io_expander_is_initialized(void);

#endif /* IO_EXPANDER_H */

