# Logic And SMS Tasks

This document summarizes the three tasks added to the project:

1. `CTS` input mapped to `battery_activation`
2. 200 ms logic on IO expander inputs
3. SMS module that reuses the existing command interface

## 1. CTS Input And `battery_activation`

### Goal

Use the local `CTS` pin as a normal input and expose its state as a parameter.

### Result

A new parameter was added:

- `PARAM_BATTERY_ACTIVATION`
- param name: `battery_activation`

`CTS` is now configured in the local GPIO input table and linked to this param.

### Files

- [`custom/param/param.h`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/param/param.h)
- [`custom/param/param.c`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/param/param.c)
- [`custom/gpio/gpio.h`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/gpio/gpio.h)
- [`custom/gpio/gpio_in.c`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/gpio/gpio_in.c)

### Behavior

- `DTR` remains the interrupt input for the PCF8574 INT line.
- `CTS` is initialized as a normal GPIO input.
- `CTS` is polled periodically.
- Its level is written into `battery_activation`.

### Command Example

```text
G,battery_activation!
```

## 2. IO Expander Input Logic

### Goal

Implement simple logic based on the four expander input params:

- `PARAM_IO_EXP_IN0`
- `PARAM_IO_EXP_IN1`
- `PARAM_IO_EXP_IN2`
- `PARAM_IO_EXP_IN3`

and react to all 16 combinations, with special actions for selected cases.

### Result

A periodic logic task now evaluates the four input params every `50 ms`.

The logic waits until a combination stays stable for `200 ms`.

After that:

- `io_exp_in0` pressed alone toggles `io_exp_out0`
- `io_exp_in1` pressed alone toggles `io_exp_out1`
- `io_exp_in2` pressed alone toggles `io_exp_out2`
- `io_exp_in0 + io_exp_in2` pressed together toggles `io_exp_out3`
- all other nonzero combinations are only logged

### Files

- [`custom/logic/logic.h`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/logic/logic.h)
- [`custom/logic/logic.c`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/logic/logic.c)
- [`custom/main.c`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/main.c)

### How It Works

The logic layer does four things:

1. polls local GPIO inputs like `CTS`
2. reads the four expander input params
3. builds a 4-bit combination mask
4. applies actions only if the same mask remains stable for `200 ms`

### Important Assumption

The expander buttons are treated as active-low:

- param value `0` means pressed
- param value `1` means released

This matches the current pull-up style on the PCF8574 input side.

### Combination Map

Special handled combinations:

- `0x01` -> toggle `io_exp_out0`
- `0x02` -> toggle `io_exp_out1`
- `0x04` -> toggle `io_exp_out2`
- `0x05` -> toggle `io_exp_out3`

Other combinations:

- `0x00` -> no action
- all other `0x01..0x0F` values not listed above -> log only

## 3. SMS Command Module

### Goal

Allow the device to receive the same commands by SMS that it already accepts over UART.

Examples:

- `G,battery_activation!`
- `S,io_exp_out0,1!`
- `G,io_exp_in0!`
- `L!`

### Result

A new `sms` module was added under `custom/sms/`.

When a new SMS arrives:

1. the message is read from modem storage
2. the text is trimmed
3. if needed, `!` is appended automatically
4. the text is passed to the same `com` parser used by UART
5. the response is captured
6. a reply SMS is sent back to the sender
7. the original SMS is deleted

### Files

- [`custom/sms/sms.h`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/sms/sms.h)
- [`custom/sms/sms.c`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/sms/sms.c)
- [`custom/com/com.h`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/com/com.h)
- [`custom/com/com.c`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/com/com.c)
- [`custom/logic/logic.c`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/logic/logic.c)
- [`custom/config/module_config.h`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/custom/config/module_config.h)
- [`make/gcc/gcc_makefile`](/mnt/c/users/hosse/onedrive/desktop/m66_quecopen_gs3_sdk_v2.6/make/gcc/gcc_makefile)

### COM Integration

The `com` module was extended with a per-call response callback:

- UART path still prints responses to UART/debug output
- SMS path captures the command response into a temporary buffer

This keeps one command parser for both interfaces.

### SMS Flow

Incoming SMS URC:

- handled in `logic_handle_message()`

SMS processing:

- `sms_handle_new_sms(index)`

Reply sending:

- uses `RIL_SMS_SendSMS_Text()`

### Current Limitation

Commands like `I!` still print detailed scan output to UART, because `i2c_bus_scan()` logs directly.

The SMS reply only contains the command response text collected through `com`.

## Summary

After these changes:

- `CTS` is available as `battery_activation`
- expander input combinations can drive output toggles after a `200 ms` hold
- the same command protocol works from both UART and SMS

## Useful Test Commands

UART or SMS:

```text
G,battery_activation!
G,io_exp_in0!
G,io_exp_in1!
G,io_exp_in2!
G,io_exp_in3!
S,io_exp_out0,1!
S,io_exp_out1,0!
L!
I!
```
