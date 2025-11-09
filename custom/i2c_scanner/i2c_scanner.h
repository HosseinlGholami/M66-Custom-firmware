/**
 * @file    i2c_scanner.h
 * @brief   I2C Bus Scanner - Finds all devices on I2C bus
 * @author  Hossein Gholami
 * @date    2025-11-03
 * 
 * Similar to Arduino I2C Scanner:
 * https://playground.arduino.cc/Main/I2cScanner/
 * 
 * Scans all 7-bit I2C addresses (0x01-0x7F) and reports found devices.
 */

#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

#include "ql_type.h"
#include "ql_gpio.h"

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize I2C bus for scanning
 * 
 * Sets up I2C hardware on specified pins.
 * Must be called before i2c_scanner_scan().
 * 
 * @param pinSCL SCL pin (e.g., PINNAME_CTS)
 * @param pinSDA SDA pin (e.g., PINNAME_RTS)
 * @param channel I2C channel number (0 for simulated I2C)
 * @return 0 on success, negative on error
 */
s32 i2c_scanner_init(Enum_PinName pinSCL, Enum_PinName pinSDA, u32 channel);

/**
 * @brief Scan I2C bus for all devices
 * 
 * Tests all 7-bit addresses (0x01-0x7F) and reports found devices.
 * Similar to Arduino Wire.beginTransmission() / endTransmission().
 * 
 * Output is sent via APP_DEBUG to UART.
 * 
 * @return Number of devices found
 */
u8 i2c_scanner_scan(void);

/**
 * @brief Release I2C bus resources
 * 
 * Frees I2C channel and pins.
 */
void i2c_scanner_uninit(void);

#endif /* I2C_SCANNER_H */


