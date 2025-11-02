# M66 Quick Start Guide

**Author**: Hossein Gholami  
**Date**: 2025-11-01  
**Version**: 2.0 (Updated with GPIO Module)

---

## 🚀 Get Started in 5 Minutes

This guide gets you up and running with the M66 modular firmware quickly.

---

## 📂 Project Structure

```
custom/
├── main.c                  # Application entry point
├── uart/                   # UART module (debug & AT commands)
│   ├── uart.h
│   └── uart.c
├── param/                  # Parameter system (RAM + NVRAM)
│   ├── param.h             # API with callbacks
│   ├── param.c             # Implementation
│   ├── param_storage.h     # Storage layer
│   ├── param_storage.c
│   ├── file.h              # File I/O abstraction
│   └── file.c
└── gpio/                   # GPIO module (table-driven)
    ├── gpio.h              # API & config types
    └── gpio.c              # Implementation + config table
```

---

## 🔨 Building

### Prerequisites
- GCC ARM toolchain at `C:\Program Files (x86)\CodeSourcery\`
- Windows with GNU Make

### Build Commands

```powershell
# New build (clean)
.\Make.bat new

# Incremental build
.\Make.bat

# Output
build\gcc\APPGS3MDM32A01.bin  (52.91 KB)
```

---

## ⚡ Common Use Cases

### 1. Control GPIO via Parameter (Automatic!)

**Step 1**: Define GPIO in `custom/gpio/gpio.c`

```c
static const GpioConfig_t gpio_config[] = {
    {
        .name = "RELAY1",
        .pin = PINNAME_DTR,              // Your hardware pin
        .direction = GPIO_DIR_OUTPUT,
        .linked_param = PARAM_IO_STATE,  // Link to parameter
        .init_level = PINLEVEL_LOW
    },
};
```

**Step 2**: Control from anywhere in your code

```c
/* GPIO updates automatically when parameter changes! */
param_set_int8(PARAM_IO_STATE, 1);  // RELAY1 ON
param_set_int8(PARAM_IO_STATE, 0);  // RELAY1 OFF
```

**That's it!** No GPIO code needed, no polling, fully automatic!

---

### 2. Add a New Parameter

**Step 1**: Add to enum in `custom/param/param.h`

```c
typedef enum {
    PARAM_APN,
    PARAM_MQTT_HOST,
    PARAM_MY_NEW_PARAM,     // ← Add here
    // ...
    PARAM_MAX_COUNT
} ParamKey_e;
```

**Step 2**: Add config in `custom/param/param.c`

```c
static const ParamConfig_t param_config[PARAM_MAX_COUNT] = {
    {"apn", PARAM_TYPE_STRING, TRUE},
    {"mqtt_host", PARAM_TYPE_STRING, TRUE},
    {"my_new_param", PARAM_TYPE_INT16, FALSE},  // ← Add here
    // ...
};
```

**Step 3**: Use it!

```c
param_set_int16(PARAM_MY_NEW_PARAM, 1234);
param_get_int16(PARAM_MY_NEW_PARAM, &value);
```

---

### 3. Persist Configuration to NVRAM

```c
/* Set parameters */
param_set_string(PARAM_APN, "internet");
param_set_int16(PARAM_MQTT_PORT, 1883);

/* Save to NVRAM (survives reboot) */
param_commit();
```

On next boot, values automatically loaded!

---

### 4. Add Button Input with Callback

**Step 1**: Define callback in `custom/gpio/gpio.c`

```c
static void my_button_callback(Enum_PinName pin, 
                               Enum_PinLevel level, 
                               void* user_data)
{
    if (level == PINLEVEL_HIGH) {
        APP_DEBUG("Button pressed!\r\n");
        param_set_int8(PARAM_LED_STATE, 1);  // Turn on LED
    }
}
```

**Step 2**: Add to GPIO config

```c
static const GpioConfig_t gpio_config[] = {
    {
        .name = "BUTTON1",
        .pin = PINNAME_RI,
        .direction = GPIO_DIR_INPUT,
        .eint_type = EINT_LEVEL_TRIGGERED,
        .eint_callback = my_button_callback,  // ← Your callback
        .linked_param = PARAM_MAX_COUNT,
        .init_level = PINLEVEL_LOW
    },
};
```

**Done!** Button presses automatically trigger your callback.

---

### 5. Debug Output

```c
APP_DEBUG("Hello from M66! Value: %d\r\n", 123);
```

Output goes to `UART_PORT1` at 115200 baud.

---

## 🎯 Common Patterns

### Pattern 1: Remote Relay Control via SMS

```c
void handle_sms_command(const char* msg)
{
    if (strcmp(msg, "RELAY ON") == 0) {
        param_set_int8(PARAM_RELAY_1, 1);  // GPIO updates automatically!
        param_commit();  // Save state
        send_sms_reply("Relay ON");
    }
}
```

### Pattern 2: Sensor Data Sharing Between Tasks

```c
/* Task 1: Read sensor every 100ms */
void sensor_task(s32 taskId)
{
    while (1) {
        s16 temp = read_temperature();
        param_set_int16(PARAM_SENSOR_TEMP, temp);  // Thread-safe!
        Ql_Sleep(100);
    }
}

/* Task 2: Publish to MQTT every 60s */
void mqtt_task(s32 taskId)
{
    while (1) {
        s16 temp;
        param_get_int16(PARAM_SENSOR_TEMP, &temp);  // Thread-safe!
        mqtt_publish("sensor/temp", temp);
        Ql_Sleep(60000);
    }
}
```

### Pattern 3: Configuration Update from Cloud

```c
void on_mqtt_config(const char* json)
{
    /* Parse JSON and update parameters */
    param_set_string(PARAM_APN, parsed_apn);
    param_set_int16(PARAM_MQTT_PORT, parsed_port);
    
    /* Save and apply */
    param_commit();
    
    /* Reboot to apply new config */
    Ql_Sleep(1000);
    Ql_Reset(0);
}
```

---

## 🛠️ Initialization Sequence

In `custom/main.c`, the standard init sequence is:

```c
void proc_main_task(s32 taskId)
{
    /* 1. Initialize UART (for debug output) */
    uart_init(UART_PORT1, 115200);
    
    /* 2. Initialize parameter system */
    param_init();  // Loads NVRAM values
    
    /* 3. Initialize GPIO module */
    gpio_init();   // Links parameters to GPIOs
    
    /* 4. Your application code */
    // ...
}
```

---

## 📊 API Quick Reference

### Parameter System

```c
/* Initialize (call once at startup) */
param_init();

/* Set values (thread-safe) */
param_set_int8(key, value);
param_set_int16(key, value);
param_set_int32(key, value);
param_set_string(key, "value");

/* Get values (thread-safe) */
param_get_int8(key, &value);
param_get_int16(key, &value);
param_get_int32(key, &value);
param_get_string(key, buffer, max_len);

/* Persistence */
param_commit();                      // Save dirty parameters to NVRAM
param_set_persist(key, TRUE/FALSE);  // Change persistence flag

/* Callbacks */
param_set_callback(key, callback_fn);  // Register parameter change callback

/* Debug */
param_print_all();                   // Print all parameters
param_count();                       // Get total parameter count
```

### GPIO Module

```c
/* Initialize (call once at startup) */
gpio_init();

/* Direct control (optional - bypasses parameters) */
gpio_set_level(pin, PINLEVEL_HIGH/LOW);
gpio_get_level(pin, &level);
gpio_toggle(pin);

/* Debug */
gpio_print_status();  // Print all GPIO configurations
```

### UART Module

```c
/* Initialize */
uart_init(UART_PORT1, 115200);

/* Debug output */
APP_DEBUG("Format string: %d, %s\r\n", num, str);
```

---

## 🐛 Troubleshooting

### Build Fails

**Error**: `make: *** [all] Error 2`

**Solution**: Check `build\gcc\build.log` for details.

Common issues:
- Missing semicolon
- Wrong function arguments
- Missing header includes

### GPIO Not Working

**Checklist**:
1. ✅ GPIO configured in `gpio_config[]`?
2. ✅ `linked_param` correct?
3. ✅ `gpio_init()` called?
4. ✅ Try `gpio_print_status()` to debug

### Parameter Not Saving

**Problem**: Values reset after reboot

**Solution**:
```c
param_set_persist(PARAM_YOUR_KEY, TRUE);  // Enable persistence
param_commit();  // Actually write to NVRAM
```

### No Debug Output

**Checklist**:
1. ✅ Serial cable connected?
2. ✅ Correct COM port?
3. ✅ Baudrate: 115200, 8-N-1
4. ✅ `uart_init()` called before `APP_DEBUG()`?

---

## 📖 Learn More

### Detailed Guides

- **[README.md](README.md)** - Complete overview
- **[NVRAM_MODULE_GUIDE.md](NVRAM_MODULE_GUIDE.md)** - Parameter system deep-dive
- **[GPIO_MODULE_GUIDE.md](GPIO_MODULE_GUIDE.md)** - GPIO configuration & examples
- **[PERSISTENCE_STRATEGY.md](PERSISTENCE_STRATEGY.md)** - Design rationale & callbacks

### Example Code

Check `custom/main.c` for working examples of:
- Parameter initialization and usage
- GPIO control via parameters
- LED blinking demo
- Multi-relay control patterns

---

## 🚀 Next Steps

1. **Modify GPIO config** for your hardware
2. **Add your parameters** to the enum
3. **Build and flash** to device
4. **Test** basic functionality
5. **Expand** with SMS, MQTT, network features

---

## 💡 Key Concepts to Remember

1. **Parameters** = Thread-safe storage (RAM + optional NVRAM)
2. **Callbacks** = Automatic actions when parameters change
3. **GPIO Linking** = Change parameter → GPIO updates automatically
4. **No Polling** = Event-driven design (fast & power-efficient)
5. **Table-Driven** = Configuration in data, not code

---

## 🎓 Pro Tips

### Tip 1: Use Parameters for Inter-Module Communication

```c
/* Good: Decoupled modules */
mqtt_task() { param_set_int8(PARAM_CMD, 1); }
relay_task() { param_get_int8(PARAM_CMD, &cmd); }

/* Bad: Tight coupling */
mqtt_task() { relay_turn_on(); }  // Direct call creates dependencies
```

### Tip 2: Commit Parameters Strategically

```c
/* Good: Batch changes, single commit */
param_set_string(PARAM_APN, "internet");
param_set_int16(PARAM_PORT, 1883);
param_commit();  // One NVRAM write

/* Bad: Commit after each change */
param_set_string(PARAM_APN, "internet");
param_commit();  // NVRAM write
param_set_int16(PARAM_PORT, 1883);
param_commit();  // Another NVRAM write (wear + slow!)
```

### Tip 3: Use Callbacks for Automatic Actions

```c
/* Register callback once during init */
param_set_callback(PARAM_ALARM_STATE, on_alarm_change);

/* Now whenever alarm state changes, callback fires automatically */
param_set_int8(PARAM_ALARM_STATE, 1);  // on_alarm_change() called!
```

---

## ✅ Verification Checklist

Before deploying to production:

- [ ] All GPIOs tested on hardware
- [ ] Parameters persist after reboot
- [ ] Callbacks trigger correctly
- [ ] No linter warnings
- [ ] Debug output clean
- [ ] Power consumption acceptable
- [ ] Thread-safety verified (multiple tasks)
- [ ] Error handling tested
- [ ] Documentation updated

---

**Ready to build! 🚀**

For questions, refer to the detailed guides or check the example code in `custom/main.c`.

**Built with ❤️ by Hossein Gholami - November 2025**
