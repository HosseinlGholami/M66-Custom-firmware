# Quick Migration Guide - I2C Bus Refactoring

## TL;DR - What Changed?

**OLD:** Each I2C module initialized its own I2C bus → conflicts!  
**NEW:** Single I2C bus controller, modules register themselves → works perfectly!

---

## For Users: What You Need to Do

### ✅ Nothing! (If using default configuration)

Your code will work automatically with the refactored architecture. The main.c has been updated for you.

### ⚙️ To Customize Modules

Edit `config/module_config.h`:

```c
// Disable OLED to save flash/RAM
// #define MODULE_OLED_ENABLED

// Disable IO Expander
// #define MODULE_IO_EXPANDER_ENABLED

// Disable entire I2C subsystem
// #define MODULE_I2C_BUS_ENABLED
```

**Comment out the #define to disable a module.**

---

## For Developers: API Changes

### I2C Bus (New)

```c
// In main.c - initialize I2C bus ONCE
#include "i2c_bus/i2c_bus.h"

i2c_bus_init(PINNAME_RI, PINNAME_DCD);  // Initialize bus
i2c_bus_scan();                          // Optional: scan for devices
```

### OLED Module

```c
// OLD API:
oled_init(PINNAME_RI, PINNAME_DCD);

// NEW API:
oled_init();  // No parameters, uses shared bus
```

### IO Expander Module

```c
// OLD API:
io_expander_init(PINNAME_RI, PINNAME_DCD, PINNAME_CTS, config, count);

// NEW API:
io_expander_init(PINNAME_CTS, config, count);  // No I2C pins
```

### I2C Scanner

```c
// OLD API:
i2c_scanner_init(PINNAME_RI, PINNAME_DCD, 0);
i2c_scanner_scan();

// NEW API:
i2c_bus_scan();  // Built into bus controller
```

---

## Initialization Sequence

### ✅ Correct Order

```c
// 1. Initialize I2C bus FIRST
i2c_bus_init(PINNAME_RI, PINNAME_DCD);

// 2. Then initialize I2C devices (in any order)
oled_init();
io_expander_init(PINNAME_CTS, config, count);
```

### ❌ Wrong Order

```c
// DON'T DO THIS:
oled_init();  // ERROR: Bus not initialized!
i2c_bus_init(PINNAME_RI, PINNAME_DCD);
```

---

## Adding New I2C Device

```c
// my_i2c_device.c

#include "../i2c_bus/i2c_bus.h"

#define MY_DEVICE_ADDR  0x50  // 7-bit address

s32 my_device_init(void) {
    // 1. Check bus
    if (!i2c_bus_is_initialized()) {
        return ERROR;
    }
    
    // 2. Register device
    i2c_bus_config_device(MY_DEVICE_ADDR, "My Device");
    
    // 3. Use I2C
    u32 channel = i2c_bus_get_channel();
    // ... your device code ...
    
    return OK;
}
```

Then add to `module_config.h`:
```c
#define MODULE_MY_DEVICE_ENABLED
```

---

## Debugging

### Check Bus Status
```c
i2c_bus_print_status();
```

Output:
```
=== I2C Bus Status ===
Initialized: Yes
Channel: 0
SCL Pin: 23
SDA Pin: 24
Registered devices: 3

Active devices:
  • 0x3C - SSD1306 OLED
  • 0x42 - PCF8574_0x42
  • 0x43 - PCF8574_0x43
```

### Scan for Devices
```c
u8 found = i2c_bus_scan();
// Shows all devices on bus with addresses
```

### Test Specific Device
```c
if (i2c_bus_device_exists(0x3C)) {
    APP_DEBUG("OLED found!\n");
}
```

---

## Common Issues

### Issue: "I2C bus not initialized"
**Solution:** Call `i2c_bus_init()` before any I2C device init.

### Issue: Module won't compile
**Solution:** Check `module_config.h` - module might be disabled.

### Issue: Wrong I2C address
**Solution:** Use `i2c_bus_scan()` to find actual addresses.

### Issue: OLED/IO expander conflict
**Solution:** They now share the bus - no conflict!

---

## Benefits Summary

| Aspect | Before | After |
|--------|--------|-------|
| I2C Init | Each module | Once, centrally |
| Conflicts | Common | None |
| New Devices | Hard to add | Easy |
| Debugging | Per-module | Centralized |
| Module Control | None | Full control |
| Code Size | Larger | Can be smaller |

---

## Files Modified

**New Files:**
- `i2c_bus/i2c_bus.h` - Bus controller
- `i2c_bus/i2c_bus.c` - Implementation
- `config/module_config.h` - Module control
- `REFACTORING_SUMMARY.md` - Full documentation
- `QUICK_MIGRATION_GUIDE.md` - This file

**Modified Files:**
- `main.c` - New init sequence, module guards
- `oled/oled.h` - Updated API
- `oled/oled.c` - Uses shared bus
- `io_expander/io_expander.h` - Updated API
- `io_expander/io_expander.c` - Uses shared bus

**Deprecated:**
- `i2c_scanner/` - Use `i2c_bus_scan()` instead

---

## Quick Reference

### Include Files
```c
#include "config/module_config.h"     // Module control
#include "i2c_bus/i2c_bus.h"          // I2C bus
#include "oled/oled.h"                // OLED
#include "io_expander/io_expander.h"  // IO expander
```

### Common Functions
```c
// Bus
i2c_bus_init(SCL, SDA);
i2c_bus_scan();
i2c_bus_print_status();

// OLED
oled_init();
oled_clear();
oled_draw_string(x, y, "text");
oled_update();

// IO Expander
io_expander_init(INT, config, count);
io_expander_write_pin(dev, pin, state);
io_expander_read_pin(dev, pin, &state);
io_expander_print_status();
```

---

## Need More Info?

- **Full Documentation:** `REFACTORING_SUMMARY.md`
- **Module Configuration:** `config/module_config.h`
- **I2C Bus API:** `i2c_bus/i2c_bus.h`
- **IO Expander Guide:** `io_expander/README.md`

---

**Updated:** 2025-11-17  
**Status:** ✅ Complete, tested, no linter errors

