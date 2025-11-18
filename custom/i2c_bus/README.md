# I2C Bus Controller

## Overview

Centralized I2C bus controller for M66 QuecOpen platform. Manages I2C bus initialization and device registration for all I2C peripherals.

## Features

- Single point I2C bus initialization
- Device registry for up to 16 I2C devices
- Built-in bus scanner
- Device discovery and management
- Known device database

## Usage

### Initialization

```c
#include "i2c_bus/i2c_bus.h"

// Initialize I2C bus (do this once in main)
s32 ret = i2c_bus_init(PINNAME_RI, PINNAME_DCD);
if (ret != I2C_BUS_OK) {
    // Handle error
}
```

### Device Registration

```c
// Register your I2C device (7-bit address)
i2c_bus_config_device(0x3C, "SSD1306 OLED");
```

### Bus Scanning

```c
// Scan for all I2C devices
u8 found = i2c_bus_scan();
```

### Status Check

```c
// Check if bus is initialized
if (i2c_bus_is_initialized()) {
    // Use I2C
}

// Print bus status
i2c_bus_print_status();
```

## Important Notes

### Simulated I2C Mode

The M66 uses **simulated (bit-banged) I2C**, not hardware I2C. The speed parameter in `Ql_IIC_Init()` must be `0` or `FALSE`.

**Correct:**
```c
Ql_IIC_Init(channel, pinSCL, pinSDA, 0);      // ✅ Simulated I2C
Ql_IIC_Init(channel, pinSCL, pinSDA, FALSE);  // ✅ Simulated I2C
```

**Incorrect:**
```c
Ql_IIC_Init(channel, pinSCL, pinSDA, 100);    // ❌ Will fail with error -1
```

### I2C Address Format

This module uses **7-bit I2C addresses** (not 8-bit).

**7-bit address examples:**
- OLED SSD1306: `0x3C` (not 0x78)
- PCF8574: `0x20-0x27` (not 0x40-0x4E)
- PCF8574A: `0x38-0x3F` (not 0x70-0x7E)

When using Quectel I2C functions directly, you need to convert:
```c
u8 addr_7bit = 0x3C;
u8 addr_8bit_write = addr_7bit << 1;       // 0x78
u8 addr_8bit_read = (addr_7bit << 1) | 1;  // 0x79
```

## API Reference

### Initialization Functions

| Function | Description |
|----------|-------------|
| `i2c_bus_init(pinSCL, pinSDA)` | Initialize I2C bus |
| `i2c_bus_config_device(addr, name)` | Register device on bus |
| `i2c_bus_is_initialized()` | Check if bus is ready |
| `i2c_bus_reinit()` | Reinitialize bus (recovery) |
| `i2c_bus_uninit()` | Release bus resources |

### Discovery Functions

| Function | Description |
|----------|-------------|
| `i2c_bus_scan()` | Scan for all devices |
| `i2c_bus_device_exists(addr)` | Test specific address |
| `i2c_bus_print_status()` | Debug output |
| `i2c_bus_get_device_name(addr)` | Lookup device name |

### Getter Functions

| Function | Description |
|----------|-------------|
| `i2c_bus_get_channel()` | Get I2C channel number |
| `i2c_bus_get_scl_pin()` | Get SCL pin |
| `i2c_bus_get_sda_pin()` | Get SDA pin |

## Error Codes

| Code | Description |
|------|-------------|
| `I2C_BUS_OK` (0) | Success |
| `I2C_BUS_ERR_INIT` (-1) | Failed to initialize |
| `I2C_BUS_ERR_CONFIG` (-2) | Device config failed |
| `I2C_BUS_ERR_NOT_INITIALIZED` (-3) | Bus not initialized |
| `I2C_BUS_ERR_PARAM` (-4) | Invalid parameter |
| `I2C_BUS_ERR_ALREADY_INITIALIZED` (-5) | Already initialized |

## Troubleshooting

### Error: "I2C Init failed: -1"

**Problem:** I2C initialization returns error -1

**Solution:** This means invalid parameters. Common causes:
1. Wrong speed parameter (should be 0 for simulated I2C)
2. Invalid pin numbers
3. Pins already in use

**Fix:**
```c
// Use 0 for simulated I2C
i2c_bus_init(PINNAME_RI, PINNAME_DCD);  // ✅ Correct
```

### Error: "I2C bus not initialized"

**Problem:** Modules report bus not initialized

**Solution:** Initialize bus before any I2C device:
```c
// 1. Initialize bus FIRST
i2c_bus_init(PINNAME_RI, PINNAME_DCD);

// 2. Then initialize devices
oled_init();
io_expander_init(PINNAME_CTS, config, count);
```

### No Devices Found on Scan

**Check:**
1. Hardware connections (SCL, SDA, GND, VCC)
2. Pull-up resistors (4.7kΩ on SCL and SDA)
3. Device power supply
4. Correct I2C addresses

## Hardware Requirements

### Pull-up Resistors

I2C requires pull-up resistors on both SCL and SDA lines:
- **Value:** 4.7kΩ (typical)
- **Connection:** SCL/SDA to VCC
- **Required:** Yes (unless built into module)

### Pin Configuration

Default pins (can be changed in `i2c_bus_init()`):
- **SCL:** PINNAME_RI (pin 2)
- **SDA:** PINNAME_DCD (pin 3)

## Integration with Modules

### OLED Module

```c
// Old (before refactoring):
oled_init(PINNAME_RI, PINNAME_DCD);

// New (after refactoring):
i2c_bus_init(PINNAME_RI, PINNAME_DCD);
oled_init();
```

### IO Expander Module

```c
// Old (before refactoring):
io_expander_init(PINNAME_RI, PINNAME_DCD, PINNAME_CTS, config, count);

// New (after refactoring):
i2c_bus_init(PINNAME_RI, PINNAME_DCD);
io_expander_init(PINNAME_CTS, config, count);
```

### Adding New I2C Module

```c
#include "i2c_bus/i2c_bus.h"

s32 my_device_init(void) {
    // Check bus
    if (!i2c_bus_is_initialized()) {
        return ERROR;
    }
    
    // Register device
    i2c_bus_config_device(MY_DEVICE_ADDR, "My Device");
    
    // Use I2C
    u32 channel = i2c_bus_get_channel();
    // ... your code ...
    
    return OK;
}
```

## Known Device Database

The module includes a database of common I2C devices:

**Displays:**
- SSD1306 OLED: 0x3C, 0x3D

**I/O Expanders:**
- PCF8574: 0x20-0x27
- PCF8574A: 0x38-0x3F

**Sensors:**
- MPU6050: 0x68, 0x69
- BMP280/BME280: 0x76, 0x77
- ADS1115: 0x48, 0x49

**Others:**
- EEPROM AT24C: 0x50-0x53

Devices are automatically identified during scan.

## Files

- `i2c_bus.h` - API header
- `i2c_bus.c` - Implementation
- `README.md` - This file

## Version History

**v1.1** (2025-11-17) - Bug Fix
- Fixed I2C initialization to use simulated mode (speed = 0)
- Updated documentation

**v1.0** (2025-11-17) - Initial Release
- Centralized I2C bus controller
- Device registration system
- Built-in scanner
- Known device database

## Author

Hossein Gholami  
Date: 2025-11-17  
Platform: M66 QuecOpen GS3 SDK V2.6

