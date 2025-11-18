# IO Expander Quick Start Guide

## Hardware Setup

### Connections
```
PCF8574 Module          M66 Module
--------------          ----------
SCL         --------→   RI (Pin for I2C Clock)
SDA         --------→   DCD (Pin for I2C Data)
INT         --------→   CTS (Pin for Interrupt)
VCC         --------→   3.3V or 5V
GND         --------→   GND

Pull-up Resistors:
- 4.7kΩ on SCL
- 4.7kΩ on SDA  
- 10kΩ on INT
```

### Important Notes for Your Configuration

**Your I2C Addresses: 0x42 and 0x43**

⚠️ **Address Verification Required**:
- Standard PCF8574: 0x20-0x27 (A0-A2 pins)
- PCF8574A variant: 0x38-0x3F (A0-A2 pins)
- Your addresses (0x42, 0x43) are **non-standard**

**Action Items:**
1. Verify actual addresses using I2C scanner:
   ```c
   i2c_scanner_init(PINNAME_RI, PINNAME_DCD, 0);
   i2c_scanner_scan();
   ```

2. If addresses are different, update `io_expander_config.h`:
   ```c
   .i2c_addr = 0xYOUR_ACTUAL_ADDRESS,  // Update this
   ```

3. Common address issues:
   - 8-bit vs 7-bit addressing (0x42 vs 0x21)
   - Module documentation might show 8-bit addresses
   - Our driver uses **7-bit addresses**

## Quick Start Code

### 1. Basic Initialization (Already in main.c)

```c
#include "io_expander/io_expander.h"
#include "io_expander/io_expander_config.h"

// In your main function:
io_expander_init(PINNAME_RI, PINNAME_DCD, PINNAME_CTS,
                 io_expander_default_config, 
                 IO_EXPANDER_DEFAULT_CONFIG_COUNT);
```

### 2. Control Outputs (Simple LED)

```c
// Turn LED ON (P0 on Device 0)
io_expander_write_pin(0, 0, 1);

// Turn LED OFF
io_expander_write_pin(0, 0, 0);

// Toggle LED
io_expander_toggle_pins(0, IO_EXPANDER_PIN0);
```

### 3. Read Inputs (Simple Button)

```c
// Configure P4 as input
io_expander_set_as_input(0, IO_EXPANDER_PIN4);

// Read button state
u8 button_pressed;
io_expander_read_pin(0, 4, &button_pressed);

if (button_pressed == 0) {  // Active LOW button
    APP_DEBUG("Button pressed!\n");
}
```

### 4. Control Multiple Pins

```c
// Turn on P0, P1, P2 together
io_expander_set_pins(0, IO_EXPANDER_PIN0 | IO_EXPANDER_PIN1 | IO_EXPANDER_PIN2);

// Turn off P0, P1
io_expander_clear_pins(0, IO_EXPANDER_PIN0 | IO_EXPANDER_PIN1);
```

### 5. Use Interrupts

```c
// Interrupt callback function
void my_interrupt_handler(u8 device_id, u8 pin_states) {
    APP_DEBUG("Device %d changed: 0x%02X\n", device_id, pin_states);
    
    // Check which pin changed
    if ((pin_states & (1 << 4)) == 0) {
        APP_DEBUG("Button on P4 pressed!\n");
    }
}

// Register callback
io_expander_register_int_callback(my_interrupt_handler);

// Configure pins as inputs to enable interrupt
io_expander_set_as_input(0, 0xF0);  // P4-P7 as inputs
```

## Pin Configuration Quick Reference

### Port Value Format
```
Bit:     7  6  5  4  3  2  1  0
Pin:     P7 P6 P5 P4 P3 P2 P1 P0
Value:   1  0  1  0  1  0  1  0
         ↑                    ↑
      Input              Output
```

### Common Configurations

```c
// All pins as inputs
io_expander_write_port(0, 0xFF);

// All pins as outputs (LOW)
io_expander_write_port(0, 0x00);

// P0-P3 outputs (LOW), P4-P7 inputs
io_expander_write_port(0, 0xF0);  // Binary: 11110000

// P0-P1 outputs (HIGH), P2-P7 inputs
io_expander_write_port(0, 0xFC);  // Binary: 11111100
```

## Troubleshooting in 30 Seconds

### Problem: "No devices found" or "I2C error"

**Quick Fixes:**
1. Check I2C addresses with scanner
2. Verify wiring (SCL, SDA, GND, VCC)
3. Check pull-up resistors (4.7kΩ)
4. Verify power supply (3.3V or 5V)

### Problem: Interrupts not working

**Quick Fixes:**
1. Configure pins as inputs first: `io_expander_set_as_input(0, 0xF0);`
2. Check INT pin connection to CTS
3. Verify 10kΩ pull-up on INT line
4. Read port to clear interrupt: `io_expander_read_port(0, &state);`

### Problem: Wrong pin states

**Remember PCF8574 Logic:**
- Write `1` = Input (high impedance) or Output HIGH
- Write `0` = Output LOW
- To read a pin: write `1` first, then read

### Problem: Can't control outputs

**Check:**
```c
// Make sure pin is configured as output (write 0)
io_expander_clear_pins(0, IO_EXPANDER_PIN0);  // P0 = output LOW

// Then control it
io_expander_set_pins(0, IO_EXPANDER_PIN0);    // P0 = output HIGH
```

## Testing Your Setup

### Step 1: Verify I2C Communication
```c
// Scan for devices
i2c_scanner_scan();

// Or test specific device
bool ok = io_expander_test_device(0);
if (ok) {
    APP_DEBUG("Device 0 is working!\n");
}
```

### Step 2: Test Output (LED)
```c
// Blink test
for (int i = 0; i < 5; i++) {
    io_expander_set_pins(0, IO_EXPANDER_PIN0);
    Ql_Sleep(500);
    io_expander_clear_pins(0, IO_EXPANDER_PIN0);
    Ql_Sleep(500);
}
```

### Step 3: Test Input (Button)
```c
// Read button 10 times
for (int i = 0; i < 10; i++) {
    u8 state;
    io_expander_read_pin(0, 4, &state);
    APP_DEBUG("P4 state: %d\n", state);
    Ql_Sleep(1000);
}
```

### Step 4: Test Interrupt
```c
// Register callback
io_expander_register_int_callback(my_interrupt_handler);

// Configure input pins
io_expander_set_as_input(0, 0xF0);

// Read to clear interrupt
u8 dummy;
io_expander_read_port(0, &dummy);

// Now press buttons and watch for interrupts
APP_DEBUG("Press buttons to test interrupt...\n");
```

## Pin Macros Reference

```c
IO_EXPANDER_PIN0    // 0x01
IO_EXPANDER_PIN1    // 0x02
IO_EXPANDER_PIN2    // 0x04
IO_EXPANDER_PIN3    // 0x08
IO_EXPANDER_PIN4    // 0x10
IO_EXPANDER_PIN5    // 0x20
IO_EXPANDER_PIN6    // 0x40
IO_EXPANDER_PIN7    // 0x80
IO_EXPANDER_ALL_PINS // 0xFF
```

## Device IDs

```c
IO_EXP_DEVICE_0     // First device (0x42)
IO_EXP_DEVICE_1     // Second device (0x43)
```

## Common Use Cases

### Relay Control
```c
// Turn relay ON
io_expander_set_pins(0, IO_EXPANDER_PIN0);

// Turn relay OFF
io_expander_clear_pins(0, IO_EXPANDER_PIN0);
```

### LED Control
```c
// Single LED
io_expander_write_pin(0, 0, 1);  // ON
io_expander_write_pin(0, 0, 0);  // OFF

// Multiple LEDs
io_expander_set_pins(0, 0x0F);   // P0-P3 ON
io_expander_clear_pins(0, 0x0F); // P0-P3 OFF
```

### Button Reading
```c
// Single button (active LOW)
u8 button;
io_expander_read_pin(0, 4, &button);
if (button == 0) {
    // Button pressed
}

// Multiple buttons
u8 buttons;
io_expander_read_port(0, &buttons);
bool btn0_pressed = ((buttons >> 4) & 1) == 0;
bool btn1_pressed = ((buttons >> 5) & 1) == 0;
```

## Next Steps

1. ✅ Verify I2C addresses with scanner
2. ✅ Test basic output (LED blink)
3. ✅ Test basic input (button read)
4. ✅ Test interrupts
5. ✅ Customize `io_expander_config.h` for your application
6. ✅ See `examples.c` for more advanced usage

## Support Files

- `io_expander.h` - API header
- `io_expander.c` - Implementation
- `io_expander_config.h` - Configuration (customize this!)
- `README.md` - Detailed documentation
- `examples.c` - Usage examples
- `QUICK_START.md` - This file

## Quick Command Reference

| Function | What it does | Example |
|----------|--------------|---------|
| `io_expander_write_pin()` | Control single pin | `io_expander_write_pin(0, 0, 1)` |
| `io_expander_read_pin()` | Read single pin | `io_expander_read_pin(0, 4, &state)` |
| `io_expander_set_pins()` | Set pins HIGH | `io_expander_set_pins(0, 0x03)` |
| `io_expander_clear_pins()` | Clear pins LOW | `io_expander_clear_pins(0, 0x03)` |
| `io_expander_toggle_pins()` | Toggle pins | `io_expander_toggle_pins(0, 0x01)` |
| `io_expander_write_port()` | Write all 8 pins | `io_expander_write_port(0, 0xF0)` |
| `io_expander_read_port()` | Read all 8 pins | `io_expander_read_port(0, &data)` |
| `io_expander_print_status()` | Print device info | `io_expander_print_status()` |

## Need More Help?

- Check `README.md` for detailed documentation
- See `examples.c` for 10+ working examples
- Use `io_expander_print_status()` to debug
- Enable I2C scanner to verify addresses

---
**Created:** 2025-11-17  
**Author:** Hossein Gholami  
**Module:** M66 QuecOpen IO Expander Driver

