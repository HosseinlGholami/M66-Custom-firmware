# Restart Logging and Watchdog Management

## Overview

This module provides comprehensive restart detection and logging to help diagnose unexpected system restarts. It tracks boot progress and saves information to persistent storage.

## Features

- **Restart Detection** - Identifies unexpected restarts vs normal boots
- **Boot Stage Tracking** - Records which initialization stage failed
- **Persistent Storage** - Survives restarts using NVRAM
- **Boot Failure Counter** - Tracks repeated boot failures
- **Watchdog Integration** - Includes watchdog feeding in main loop

## How It Works

### Boot Stage Tracking

Every initialization stage is logged:

1. **START** - Boot beginning
2. **UART_INIT** - UART initialization
3. **PARAM_INIT** - Parameter storage
4. **GPIO_INIT** - GPIO module
5. **COM_INIT** - Command interface
6. **I2C_BUS_INIT** - I2C bus controller
7. **I2C_SCAN** - I2C device scanning
8. **OLED_INIT** - OLED display
9. **IO_EXPANDER_INIT** - IO expander
10. **COMPLETE** - All init done
11. **RUNNING** - Normal operation

### What Happens on Restart

If the device restarts unexpectedly:

```
╔════════════════════════════════════════════╗
║  ⚠️  UNEXPECTED RESTART DETECTED           ║
╚════════════════════════════════════════════╝

⚠️  System restarted unexpectedly!
   Restart count: 3
   Boot failures: 2
   Last stage:    I2C_BUS_INIT

Possible causes:
  • Watchdog timeout (no feeding)
  • Stack overflow
  • NULL pointer access
  • Memory corruption
  • I2C bus lockup
  • Power supply issue
```

This tells you:
- **Restart count**: Total number of boots
- **Boot failures**: How many didn't complete
- **Last stage**: Where it failed (I2C_BUS_INIT in this example)

### Successful Boot

When boot completes successfully:

```
╔════════════════════════════════════════════╗
║  ✅ BOOT SEQUENCE COMPLETE                 ║
╚════════════════════════════════════════════╝

System initialized successfully
Total boots: 5
```

## Watchdog Management

The module also feeds the watchdog in the main loop:

```c
while(TRUE) {
    Ql_OS_GetMessage(&msg);
    
    /* Feed watchdog to prevent reset */
    Ql_WTD_Feed(1);
    
    // ... handle messages ...
}
```

This prevents watchdog resets during normal operation.

## Common Restart Causes

### 1. Watchdog Timeout

**Symptom:** Regular restarts, last stage is usually RUNNING  
**Cause:** Main loop blocked, watchdog not fed  
**Solution:** Check for blocking code, ensure Ql_WTD_Feed() is called

### 2. I2C Bus Lockup

**Symptom:** Restarts during I2C_BUS_INIT or I2C_SCAN  
**Cause:** I2C pins stuck, bad connection, wrong address  
**Solution:** Check hardware, verify addresses with scanner

### 3. Stack Overflow

**Symptom:** Random restarts at different stages  
**Cause:** Too much stack usage, deep recursion  
**Solution:** Reduce local variables, check array sizes

### 4. Memory Corruption

**Symptom:** Random crashes, corrupted data  
**Cause:** Buffer overflow, wild pointers  
**Solution:** Review array bounds, pointer usage

### 5. NULL Pointer Access

**Symptom:** Immediate restart in specific code  
**Cause:** Dereferencing NULL pointer  
**Solution:** Check all pointer validity before use

### 6. Power Supply Issues

**Symptom:** Restarts under load (I2C, display active)  
**Cause:** Insufficient power, voltage drops  
**Solution:** Check power supply capacity, add capacitors

## Diagnosing Restart Issues

### Step 1: Check Last Stage

The last stage tells you where it failed:

```c
Last stage: I2C_BUS_INIT
→ Problem is in I2C initialization
→ Check I2C hardware, pins, addresses
```

### Step 2: Look at Restart Pattern

**Multiple restarts at same stage:**
- Hardware issue (connections, power)
- Configuration problem (wrong address, pins)

**Random restart locations:**
- Memory corruption
- Stack overflow
- Power supply issue

**Restarts in RUNNING stage:**
- Watchdog timeout (blocking code)
- Periodic crash (timer, interrupt)

### Step 3: Add Debug Output

Add more logging around the failing stage:

```c
APP_DEBUG("Before I2C init\r\n");
i2c_bus_init(pins...);
APP_DEBUG("After I2C init\r\n");  // If this doesn't print, crash is in i2c_bus_init
```

## API Usage

### Basic Usage (Already Integrated)

The restart logging is already integrated in `main.c`. You don't need to do anything - it works automatically.

### Manual Stage Tracking

If you add new initialization code:

```c
restart_log_set_stage(BOOT_STAGE_MY_MODULE);
my_module_init();
```

### Checking Restart Status

```c
// Check if this is a restart
if (restart_log_is_restart()) {
    APP_DEBUG("This is a restart, not first boot\r\n");
}

// Get restart reason
const char* reason = restart_log_get_reason();
APP_DEBUG("Restart reason: %s\r\n", reason);

// Print full status
restart_log_print();
```

## Files

- `restart_log.h` - API header
- `restart_log.c` - Implementation
- `restart.log` - Persistent data file (NVRAM)
- `README.md` - This file

## Storage

Restart data is stored in `restart.log` file in NVRAM:
- **Size**: ~20 bytes
- **Persistence**: Survives restarts and power loss
- **Location**: NVRAM filesystem

## Testing Restart Detection

### Test 1: Force a Restart

Remove watchdog feeding temporarily:

```c
// Comment out in main loop:
// Ql_WTD_Feed(1);
```

Device will restart from watchdog timeout.

### Test 2: Simulate Init Failure

Add crash in specific stage:

```c
restart_log_set_stage(BOOT_STAGE_I2C_BUS_INIT);
*(int*)0 = 0;  // NULL pointer crash
```

Next boot will show "Last stage: I2C_BUS_INIT".

### Test 3: Check Counter

Restart device manually several times - counter should increment.

## Troubleshooting

### "Restart log not initialized"

Make sure `restart_log_init()` is called early in main.

### "File error"

NVRAM might be full or corrupted. Try deleting `restart.log`:

```c
Ql_FS_Delete("restart.log");
```

### "Corrupted data"

Log file corrupted - will auto-reset on next boot.

## Example Output

### Normal Boot
```
==================BASE FIRMWARE======================
  M66 Industrial Controller Firmware
  Build: Nov 17 2025 10:30:00
========================================

[RESTART] First boot - initializing restart log
[RESTART] Boot stage: START
[RESTART] Boot stage: UART_INIT
... (all stages) ...
[RESTART] Boot stage: COMPLETE

╔════════════════════════════════════════════╗
║  ✅ BOOT SEQUENCE COMPLETE                 ║
╚════════════════════════════════════════════╝

System initialized successfully
Total boots: 1
```

### After Unexpected Restart
```
==================BASE FIRMWARE======================
  M66 Industrial Controller Firmware
  Build: Nov 17 2025 10:30:00
========================================

╔════════════════════════════════════════════╗
║  ⚠️  UNEXPECTED RESTART DETECTED           ║
╚════════════════════════════════════════════╝

⚠️  System restarted unexpectedly!
   Restart count: 2
   Boot failures: 1
   Last stage:    I2C_BUS_INIT

Possible causes:
  • Watchdog timeout
  • Stack overflow  
  • NULL pointer access
  • Memory corruption
  • I2C bus lockup
  • Power supply issue
```

## Integration

Already integrated in `main.c`:

```c
// Early initialization
uart_init(UART_PORT1, 115200);
restart_log_set_stage(BOOT_STAGE_UART_INIT);
restart_log_init();  // Initialize restart logging

// Track each stage
restart_log_set_stage(BOOT_STAGE_PARAM_INIT);
param_init();

restart_log_set_stage(BOOT_STAGE_I2C_BUS_INIT);
i2c_bus_init();

// ... more stages ...

// Mark complete
restart_log_set_stage(BOOT_STAGE_COMPLETE);
restart_log_boot_complete();

// Main loop with watchdog
while(TRUE) {
    Ql_OS_GetMessage(&msg);
    Ql_WTD_Feed(1);  // Feed watchdog
    // ... handle messages ...
}
```

## Version History

**v1.0** (2025-11-17)
- Initial implementation
- Boot stage tracking
- Restart detection
- Watchdog feeding
- Persistent storage

## Author

Hossein Gholami  
Date: 2025-11-17  
Platform: M66 QuecOpen GS3 SDK V2.6

