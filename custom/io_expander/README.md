# PCF8574 IO Expander Module

## Overview

This module provides a driver for the PCF8574 I2C IO expander with support for:
- Multiple cascaded devices (up to 8)
- Configurable I2C addresses
- Interrupt support (INT pin)
- Individual pin control and read-back
- Change notification callbacks

## Hardware Connections

### I2C Bus
- **SCL**: Connect to RI pin (PINNAME_RI)
- **SDA**: Connect to DCD pin (PINNAME_DCD)
- **Pull-ups**: 4.7kΩ resistors on both SCL and SDA lines

### Interrupt (Optional)
- **INT**: Connect all PCF8574 INT pins together to CTS pin (PINNAME_CTS)
- **Pull-up**: 10kΩ resistor on INT line
- INT pin goes LOW when any input changes

### PCF8574 Power
- **VCC**: 2.5V to 6V (typically 3.3V or 5V)
- **GND**: Ground
- **A0-A2**: Address selection pins

## I2C Addresses

### Standard PCF8574
- Base address: 0x20 (7-bit)
- Range: 0x20 - 0x27 (A0-A2 pins)

### PCF8574A Variant
- Base address: 0x38 (7-bit)
- Range: 0x38 - 0x3F (A0-A2 pins)

### Custom Addresses (Your Configuration)
- Device 0: 0x42
- Device 1: 0x43

**Note**: Addresses 0x42-0x43 are non-standard. Verify these with your specific hardware.

## Configuration

### Basic Configuration (io_expander_config.h)

```c
static const IoExpanderConfig_t io_expander_default_config[] = {
    {
        .name = "PCF8574_0x42",
        .i2c_addr = 0x42,       // 7-bit address
        .init_state = 0xFF,     // All pins as inputs
        .enabled = TRUE
    },
    {
        .name = "PCF8574_0x43",
        .i2c_addr = 0x43,
        .init_state = 0xFF,
        .enabled = TRUE
    }
};
```

### Pin State Format
Each PCF8574 has 8 pins (P0-P7):
- Bit 0 = P0, Bit 1 = P1, ..., Bit 7 = P7
- `1` = Input (high impedance) or Output HIGH
- `0` = Output LOW

## Usage Examples

### 1. Initialization

```c
#include "io_expander/io_expander.h"
#include "io_expander/io_expander_config.h"

// Initialize with interrupt support
s32 ret = io_expander_init(
    PINNAME_RI,                         // SCL pin
    PINNAME_DCD,                        // SDA pin
    PINNAME_CTS,                        // INT pin (or 0 to disable)
    io_expander_default_config,         // Configuration array
    IO_EXPANDER_DEFAULT_CONFIG_COUNT    // Number of devices
);

if (ret == IO_EXPANDER_OK) {
    APP_DEBUG("IO Expander initialized\n");
}
```

### 2. Register Interrupt Callback

```c
void my_int_handler(u8 device_id, u8 pin_states) {
    APP_DEBUG("Device %d changed: 0x%02X\n", device_id, pin_states);
    
    // Check specific pins
    if (pin_states & (1 << 4)) {
        APP_DEBUG("Pin 4 is HIGH\n");
    }
}

io_expander_register_int_callback(my_int_handler);
```

### 3. Write Entire Port

```c
// Set P0-P3 as outputs (LOW), P4-P7 as inputs (HIGH)
io_expander_write_port(0, 0xF0);  // Device 0: 11110000
```

### 4. Control Individual Pins

```c
// Set pin P0 HIGH on device 0
io_expander_write_pin(0, 0, 1);

// Set pin P1 LOW on device 0
io_expander_write_pin(0, 1, 0);

// Toggle pin P2 on device 1
io_expander_toggle_pins(1, IO_EXPANDER_PIN2);
```

### 5. Control Multiple Pins

```c
// Set pins P0 and P1 HIGH on device 0
io_expander_set_pins(0, IO_EXPANDER_PIN0 | IO_EXPANDER_PIN1);

// Clear pins P2 and P3 LOW on device 0
io_expander_clear_pins(0, IO_EXPANDER_PIN2 | IO_EXPANDER_PIN3);

// Toggle multiple pins
io_expander_toggle_pins(0, IO_EXPANDER_PIN0 | IO_EXPANDER_PIN1);
```

### 6. Read Input Pins

```c
// Read entire port
u8 pin_states;
io_expander_read_port(0, &pin_states);
APP_DEBUG("Port state: 0x%02X\n", pin_states);

// Read individual pin
u8 pin_state;
io_expander_read_pin(0, 4, &pin_state);  // Read P4
if (pin_state) {
    APP_DEBUG("Pin 4 is HIGH\n");
}
```

### 7. Configure Pins as Inputs

```c
// Set P4-P7 as inputs (must write HIGH)
io_expander_set_as_input(0, 0xF0);  // Pins 4-7
```

### 8. Print Status

```c
// Print status of all devices
io_expander_print_status();
```

## Complete Example: LED and Button Control

```c
// Configure Device 0: P0-P3 as LED outputs, P4-P7 as button inputs
io_expander_write_port(0, 0xF0);

// Turn on LED on P0
io_expander_set_pins(0, IO_EXPANDER_PIN0);

// Blink LED on P1
for (int i = 0; i < 5; i++) {
    io_expander_set_pins(0, IO_EXPANDER_PIN1);
    Ql_Sleep(200);
    io_expander_clear_pins(0, IO_EXPANDER_PIN1);
    Ql_Sleep(200);
}

// Read button on P4
u8 button_state;
io_expander_read_pin(0, 4, &button_state);
if (button_state == 0) {  // Button pressed (assuming active LOW)
    APP_DEBUG("Button pressed!\n");
}
```

## Interrupt Handling

The PCF8574 INT pin goes LOW when:
1. Any input pin changes state
2. The port has not been read since the change

To clear the interrupt:
1. Read the port with `io_expander_read_port()`
2. INT pin will go HIGH again

## Pin Usage Guidelines

### Outputs
1. Write 0 to make pin LOW (active output)
2. Pin can sink up to 25mA

### Inputs
1. Write 1 to configure as input (high impedance)
2. External pull-up/pull-down may be needed
3. Reading input returns actual pin voltage level
4. Internal weak pull-up (~100µA) pulls pin HIGH

### Typical Input Configurations

**Active LOW Button:**
```
Button ----[Pin]
          |
         GND
```
Pin configured as input (write 1), button pulls to GND when pressed.

**Active HIGH Sensor:**
```
VCC ---[Sensor]----[Pin]
                   |
                  10kΩ
                   |
                  GND
```
Pin configured as input (write 1), sensor pulls HIGH when active.

## Cascading Multiple Devices

Add more devices to the configuration:

```c
static const IoExpanderConfig_t io_expander_config[] = {
    { .name = "Device_0", .i2c_addr = 0x42, .init_state = 0xFF, .enabled = TRUE },
    { .name = "Device_1", .i2c_addr = 0x43, .init_state = 0xFF, .enabled = TRUE },
    { .name = "Device_2", .i2c_addr = 0x44, .init_state = 0xFF, .enabled = TRUE },
    { .name = "Device_3", .i2c_addr = 0x45, .init_state = 0xFF, .enabled = TRUE },
};
```

Total I/O pins: 4 devices × 8 pins = 32 pins

## Troubleshooting

### No devices found
- Check I2C connections (SCL, SDA)
- Verify pull-up resistors (4.7kΩ)
- Check I2C addresses with I2C scanner
- Verify power supply to PCF8574

### Interrupts not working
- Check INT pin connection to CTS
- Verify pull-up resistor on INT line (10kΩ)
- Ensure pins are configured as inputs
- Read port to clear interrupt

### Wrong pin states
- Remember: PCF8574 is quasi-bidirectional
- Writing 1 makes pin input (high impedance)
- Writing 0 makes pin output LOW
- For inputs, write 1 first, then read

### I2C communication errors
- Check for address conflicts
- Verify I2C speed (100 kHz recommended)
- Check for shorts on I2C bus
- Ensure only one device per address

## API Reference

| Function | Description |
|----------|-------------|
| `io_expander_init()` | Initialize module with devices |
| `io_expander_register_int_callback()` | Register interrupt handler |
| `io_expander_write_port()` | Write all 8 pins |
| `io_expander_read_port()` | Read all 8 pins |
| `io_expander_write_pin()` | Write single pin |
| `io_expander_read_pin()` | Read single pin |
| `io_expander_set_pins()` | Set multiple pins HIGH |
| `io_expander_clear_pins()` | Clear multiple pins LOW |
| `io_expander_toggle_pins()` | Toggle multiple pins |
| `io_expander_set_as_input()` | Configure pins as inputs |
| `io_expander_print_status()` | Print device status |
| `io_expander_test_device()` | Test device communication |

## Performance

- I2C Speed: 100 kHz (standard mode)
- Read/Write time: ~1ms per operation
- Interrupt latency: <1ms (depends on system load)
- Maximum cascaded devices: 8

## PCF8574 Specifications

- Supply voltage: 2.5V - 6V
- I/O current: 25mA per pin (sink)
- Total current: 100mA max (all pins)
- Input voltage: -0.5V to VCC+0.5V
- I2C speed: 100 kHz (standard mode)

## License

Part of M66 QuecOpen SDK - Custom Application
Copyright (c) 2025 Hossein Gholami

