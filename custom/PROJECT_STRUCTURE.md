# Project Structure

This document describes the current structure of the project and the role of each module.

## High-Level Layout

The project is split into a few main areas:

- `custom/`
  Application code written for this project.
- `include/`
  Quectel OpenCPU SDK headers such as GPIO, UART, timers, filesystem, and system APIs.
- `ril/`
  Quectel RIL sources and telephony support.
- `make/`
  Build scripts and GCC makefiles.

## Current Runtime Flow

The active startup path is:

1. [`custom/main.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/main.c)
   Main task entrypoint.
2. [`custom/param/param.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/param/param.c)
   Initializes the parameter system.
3. [`custom/logic/logic.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/logic/logic.c)
   Initializes runtime command buffering and handles URC/system messages.
4. [`custom/i2c_bus/i2c_bus.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/i2c_bus/i2c_bus.c)
   Initializes the shared I2C bus.
5. [`custom/gpio/gpio.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/gpio/gpio.c)
   Initializes local GPIO outputs, GPIO expander support, and input interrupt handling.
6. [`custom/com/com.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/com/com.c)
   Handles UART commands like `S,<key>,<value>!`, `G,<key>!`, `L!`, and `I!`.

## Main Application Modules

### `custom/main.c`

Owns the module initialization order and the main message loop.

Responsibilities:

- Initialize UART
- Print boot banner
- Initialize params
- Initialize runtime logic
- Initialize I2C bus
- Initialize GPIO
- Initialize command interface
- Dispatch system messages to `logic_handle_message()`

### `custom/logic/`

Files:

- [`custom/logic/logic.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/logic/logic.c)
- [`custom/logic/logic.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/logic/logic.h)

Responsibilities:

- UART receive buffering
- Command framing using `!`
- Runtime status messages
- Handling RIL/URC messages

This module is intentionally lightweight. It does not own board bring-up anymore.

### `custom/com/`

Files:

- [`custom/com/com.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/com/com.c)
- [`custom/com/com.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/com/com.h)

Responsibilities:

- Parse UART command strings
- Find params by index or name
- Get/set param values
- Run on-demand I2C scan with `I!`
- Print command help

### `custom/uart/`

Files:

- [`custom/uart/uart.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/uart/uart.c)
- [`custom/uart/uart.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/uart/uart.h)

Responsibilities:

- UART initialization
- TX/RX wrapper APIs
- UART callback registration

## Parameter System

### `custom/param/`

Files:

- [`custom/param/param.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/param/param.c)
- [`custom/param/param.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/param/param.h)
- [`custom/param/param_storage.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/param/param_storage.c)
- [`custom/param/param_storage.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/param/param_storage.h)
- [`custom/param/file.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/param/file.c)
- [`custom/param/file.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/param/file.h)

Responsibilities:

- Define all parameters in a static config table
- Create a runtime table with current values
- Provide type-safe get/set APIs
- Support per-param callbacks
- Persist selected params to `param.dat`
- Create one mutex per param entry

Current important params:

- `apn`
- `mqtt_host`
- `device_id`
- `mqtt_port`
- `io_state`
- `net_rssi`
- `io_exp_out0` .. `io_exp_out3`
- `io_exp_in0` .. `io_exp_in3`

## GPIO and Board I/O

### `custom/gpio/`

Files:

- [`custom/gpio/gpio.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/gpio/gpio.c)
- [`custom/gpio/gpio.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/gpio/gpio.h)
- [`custom/gpio/gpio_internal.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/gpio/gpio_internal.h)
- [`custom/gpio/gpio_out.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/gpio/gpio_out.c)
- [`custom/gpio/gpio_in.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/gpio/gpio_in.c)
- [`custom/gpio/gpio_expander.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/gpio/gpio_expander.c)

#### `gpio.c`

Coordinator module.

Responsibilities:

- Initialize GPIO outputs
- Initialize IO expander abstraction
- Initialize GPIO interrupt inputs

#### `gpio_out.c`

Local output mapping layer.

Responsibilities:

- Table-driven mapping of physical MCU outputs to params
- Register param callbacks for output pins
- Apply output state when params change

Example:

- `PINNAME_NETLIGHT` is controlled by `io_state`

#### `gpio_in.c`

Local input / interrupt layer.

Responsibilities:

- Configure local EINT sources
- Receive the PCF interrupt on `DTR`
- Trigger expander input refresh

#### `gpio_expander.c`

PCF8574 integration through the GPIO abstraction.

Responsibilities:

- Define expander nodes in a node table
- Define per-pin direction and linked param in a pin map table
- Configure `0x4A` as:
  - `P0-P3` outputs
  - `P4-P7` inputs
- Mirror output param changes to PCF pins
- Refresh input params on interrupt

Current mapping:

- `P0` -> `io_exp_out0`
- `P1` -> `io_exp_out1`
- `P2` -> `io_exp_out2`
- `P3` -> `io_exp_out3`
- `P4` -> `io_exp_in0`
- `P5` -> `io_exp_in1`
- `P6` -> `io_exp_in2`
- `P7` -> `io_exp_in3`

## I2C

### `custom/i2c_bus/`

Files:

- [`custom/i2c_bus/i2c_bus.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/i2c_bus/i2c_bus.c)
- [`custom/i2c_bus/i2c_bus.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/i2c_bus/i2c_bus.h)

Responsibilities:

- Shared I2C initialization
- Device configuration
- I2C device scan
- Simple device registry

This is the active I2C entrypoint used by the runtime.

### `custom/io_expander/`

Files:

- [`custom/io_expander/io_expander.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/io_expander/io_expander.c)
- [`custom/io_expander/io_expander.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/io_expander/io_expander.h)
- [`custom/io_expander/io_expander_config.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/io_expander/io_expander_config.h)
- [`custom/io_expander/io_expander_param.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/io_expander/io_expander_param.c)
- [`custom/io_expander/io_expander_param.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/io_expander/io_expander_param.h)

Responsibilities:

- Low-level PCF8574 driver
- Port read/write APIs
- Per-pin access helpers

Important note:

- `io_expander.c` is the real low-level driver.
- `io_expander_param.c` is now only a legacy compatibility shim.
- New param integration is implemented in `custom/gpio/gpio_expander.c`.

### `custom/i2c_scanner/`

Old standalone scanner module. The active path is `i2c_bus_scan()` from `custom/i2c_bus/`.

## Optional / Legacy / Not On Critical Path

### `custom/board/`

Files:

- [`custom/board/board.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/board/board.c)
- [`custom/board/board.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/board/board.h)

This folder exists from the earlier refactor direction, but the active startup path is now driven directly from `main.c`.

### `custom/oled/`

Files:

- [`custom/oled/oled.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/oled/oled.c)
- [`custom/oled/oled.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/oled/oled.h)

OLED support is present in the codebase but currently disabled in `module_config.h`.

### `custom/debug/`

Restart log support and debug helpers.

### `custom/fota/`

FOTA implementation. Present in the codebase but not part of the current board bring-up work.

### `custom/QUICK_MIGRATION_GUIDE.md` and `custom/REFACTORING_SUMMARY.md`

Historical documentation from earlier refactors. Useful as notes, but they are not the source of truth for the current runtime structure.

## Configuration

### `custom/config/`

Important files:

- [`custom/config/module_config.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/config/module_config.h)
- [`custom/config/custom_feature_def.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/config/custom_feature_def.h)
- [`custom/config/custom_sys_cfg.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/config/custom_sys_cfg.c)
- [`custom/config/sys_config.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/config/sys_config.c)

Responsibilities:

- Enable/disable modules
- SDK/customer configuration
- Task/system configuration

## Build System

### `make/`

Important files:

- [`make/gcc/gcc_makefile`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/make/gcc/gcc_makefile)
- [`make/gcc/gcc_makefiledef`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/make/gcc/gcc_makefiledef)
- `make/make.exe`

Responsibilities:

- Collect source files
- Configure include paths
- Compile and link the firmware

Current build command:

```bash
./make/make.exe -f make/gcc/gcc_makefile new
```

## SDK Headers

### `include/`

Quectel SDK headers live here. Common ones used in the project:

- `ql_gpio.h`
- `ql_uart.h`
- `ql_timer.h`
- `ql_system.h`
- `ql_eint.h`
- `ql_iic.h`
- `ql_fs.h`
- `ql_stdlib.h`

## Recommended Mental Model

When working on this project, think of it in layers:

1. `main.c`
   Boot and main loop
2. `logic/` and `com/`
   Runtime behavior and user commands
3. `param/`
   Shared state and persistence
4. `gpio/` and `i2c_bus/`
   Hardware abstraction
5. `io_expander/`
   Low-level PCF8574 driver
6. `include/` and `ril/`
   SDK and modem support

## Current Source Of Truth

For the board behavior you are using now, these files are the most important:

- [`custom/main.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/main.c)
- [`custom/config/module_config.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/config/module_config.h)
- [`custom/param/param.h`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/param/param.h)
- [`custom/param/param.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/param/param.c)
- [`custom/gpio/gpio_expander.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/gpio/gpio_expander.c)
- [`custom/com/com.c`](/mnt/c/Users/hosse/OneDrive/Desktop/M66_QuecOpen_GS3_SDK_V2.6/custom/com/com.c)
