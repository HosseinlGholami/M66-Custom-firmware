/**
 * @file    gpio.c
 * @brief   GPIO management implementation
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * This module provides table-driven GPIO configuration with parameter integration.
 * Output GPIOs can be automatically controlled via parameters using callbacks.
 */

#include "config/module_config.h"  /* Must be first to define module flags */
#include "gpio.h"
#include "uart/uart.h"
#include "ql_stdlib.h"
#include "ql_eint.h"

#ifdef MODULE_IO_EXPANDER_ENABLED
#include "io_expander/io_expander.h"
#endif

/*============================================================================
 * GPIO Configuration Table
 * 
 * Define your GPIOs here!
 * - For INPUT: Set eint_type and eint_callback
 * - For OUTPUT: Set linked_param (or PARAM_MAX_COUNT if no link) and init_level
 *===========================================================================*/

/* Forward declaration for EINT callback */
static void io_expander_eint_callback(Enum_PinName pin, Enum_PinLevel level, void* user_data);

/* GPIO CONFIGURATION
 * - LED output controlled by parameter
 * - DTR input with EINT for IO expander interrupt handling
 * 
 * IO Expander INT Pin Connection:
 * - PCF8574 INT pin → DTR (pin 1)
 * - INT goes LOW when any PCF8574 pin changes state
 * - EINT triggers, callback reads all IO expander states
 */
static const GpioConfig_t gpio_config[] = {
    /* LED output controlled by parameter */
    {
        .name = "LED1",
        .pin = PINNAME_NETLIGHT,
        .direction = GPIO_DIR_OUTPUT,
        .eint_type = 0,
        .eint_callback = NULL,
        .linked_param = PARAM_IO_STATE,
        .init_level = PINLEVEL_LOW
    },
    
    /* DTR input with EINT for IO Expander interrupt
     * Using LEVEL_TRIGGERED with mask/unmask pattern (per Quectel example):
     * - PCF8574 INT goes LOW on change
     * - EINT callback masks interrupt at start
     * - Read device to clear PCF INT (goes HIGH)
     * - Unmask interrupt at end
     * - Ready for next change!
     */
    {
        .name = "DTR_IO_EXP_INT",
        .pin = PINNAME_DTR,
        .direction = GPIO_DIR_INPUT,
        .eint_type = EINT_LEVEL_TRIGGERED,
        .eint_callback = io_expander_eint_callback,
        .linked_param = PARAM_MAX_COUNT,
        .init_level = PINLEVEL_LOW
    },
};

#define GPIO_CONFIG_COUNT   (sizeof(gpio_config) / sizeof(gpio_config[0]))

/*============================================================================
 * Private Data
 *===========================================================================*/
static GpioData_t gpio_data[GPIO_MAX_PINS];
static u32 gpio_count = 0;
static bool gpio_initialized = FALSE;

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Find GPIO data by pin name
 */
static GpioData_t* find_gpio_by_pin(Enum_PinName pin)
{
    u32 i;
    for (i = 0; i < gpio_count; i++) {
        if (gpio_data[i].config.pin == pin && gpio_data[i].initialized) {
            return &gpio_data[i];
        }
    }
    return NULL;
}

/**
 * @brief Parameter change callback for GPIO outputs
 * This is called whenever a linked parameter changes
 */
static void gpio_param_callback(ParamKey_e key, const void* old_val, const void* new_val, ParamType_e type)
{
    u32 i;
    Enum_PinLevel new_level;
    
    /* Find all GPIOs linked to this parameter */
    for (i = 0; i < gpio_count; i++) {
        if (!gpio_data[i].initialized) {
            continue;
        }
        
        if (gpio_data[i].config.direction != GPIO_DIR_OUTPUT) {
            continue;
        }
        
        if (gpio_data[i].config.linked_param != key) {
            continue;
        }
        
        /* Determine new level based on parameter type */
        switch (type) {
            case PARAM_TYPE_INT8:
                new_level = (*(s8*)new_val) ? PINLEVEL_HIGH : PINLEVEL_LOW;
                break;
            case PARAM_TYPE_INT16:
                new_level = (*(s16*)new_val) ? PINLEVEL_HIGH : PINLEVEL_LOW;
                break;
            case PARAM_TYPE_INT32:
                new_level = (*(s32*)new_val) ? PINLEVEL_HIGH : PINLEVEL_LOW;
                break;
            default:
                continue;  /* Strings not supported for GPIO control */
        }
        
        /* Update GPIO */
        Ql_GPIO_SetLevel(gpio_data[i].config.pin, new_level);
        
        APP_DEBUG("GPIO: '%s' (pin %d) -> %s (param '%s' changed)\r\n",
                 gpio_data[i].config.name,
                 gpio_data[i].config.pin,
                 new_level == PINLEVEL_HIGH ? "HIGH" : "LOW",
                 param_get_name(key));
    }
}

/**
 * @brief IO Expander EINT callback
 * Called when DTR pin (connected to PCF8574 INT) goes LOW
 * 
 * PCF8574 INT Behavior:
 * - INT is normally HIGH (pulled up)
 * - INT goes LOW when ANY pin on PCF8574 changes state
 * - Reading from PCF8574 clears the interrupt
 * - INT returns to HIGH after read
 * 
 * CRITICAL: Must follow Quectel's mask/unmask pattern for repeated interrupts!
 * 1. Mask interrupt at start
 * 2. Do work (read IO expander)
 * 3. Unmask interrupt at end
 */
static void io_expander_eint_callback(Enum_PinName pin, Enum_PinLevel level, void* user_data)
{
    /* STEP 1: Mask the interrupt immediately (per Quectel example) */
    Ql_EINT_Mask(pin);
    
    APP_DEBUG("\r\n");
    APP_DEBUG("╔══════════════════════════════════════╗\r\n");
    APP_DEBUG("║   IO EXPANDER INTERRUPT DETECTED    ║\r\n");
    APP_DEBUG("╚══════════════════════════════════════╝\r\n");
    APP_DEBUG("Pin: %d, Level: %s\r\n", pin, level == PINLEVEL_HIGH ? "HIGH" : "LOW");
    
#ifdef MODULE_IO_EXPANDER_ENABLED
    /* STEP 2: Read all IO expander devices to clear interrupt and get current states */
    u8 device_id;
    u8 num_devices = io_expander_get_device_count();
    
    APP_DEBUG("Reading %d IO expander device(s)...\r\n", num_devices);
    
    for (device_id = 0; device_id < num_devices; device_id++) {
        u8 pin_states;
        s32 ret = io_expander_read_port(device_id, &pin_states);
        
        if (ret == 0) {
            const char* dev_name = io_expander_get_device_name(device_id);
            APP_DEBUG("Device %d (%s): 0x%02X = ", device_id, 
                     dev_name ? dev_name : "Unknown", pin_states);
            
            /* Print binary */
            u8 i;
            for (i = 0; i < 8; i++) {
                APP_DEBUG("%d", (pin_states >> i) & 1);
            }
            APP_DEBUG("\r\n");
            
#ifdef MODULE_PARAM_ENABLED
            /* Update parameter for Device 0 (inputs) */
            if (device_id == 0) {  /* IO_EXP_DEVICE_0 (0x42) configured as inputs */
                param_set_int8(PARAM_IO_EXP0_IN, (s8)pin_states);
                APP_DEBUG("  → Updated PARAM_IO_EXP0_IN\r\n");
            }
#endif
        } else {
            APP_DEBUG("Device %d: Read failed (ret=%d)\r\n", device_id, ret);
        }
    }
#else
    APP_DEBUG("IO Expander module not enabled!\r\n");
#endif
    
    APP_DEBUG("════════════════════════════════════════\r\n");
    
    /* STEP 3: Unmask the interrupt to allow next trigger (per Quectel example) */
    Ql_EINT_Unmask(pin);
}

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

s32 gpio_init(void)
{
    u32 i;
    s32 ret;
    
    if (gpio_initialized) {
        APP_DEBUG("WARNING: GPIO already initialized\r\n");
        return 0;
    }
    
    if (GPIO_CONFIG_COUNT > GPIO_MAX_PINS) {
        APP_DEBUG("ERROR: GPIO config count (%d) exceeds max (%d)\r\n",
                 GPIO_CONFIG_COUNT, GPIO_MAX_PINS);
        return -1;
    }
    
    /* Clear runtime data */
    Ql_memset(gpio_data, 0, sizeof(gpio_data));
    gpio_count = 0;
    
    APP_DEBUG("\r\n=== Initializing GPIO Module ===\r\n");
    APP_DEBUG("Total GPIOs: %d\r\n", GPIO_CONFIG_COUNT);
    
    /* Configure each GPIO */
    for (i = 0; i < GPIO_CONFIG_COUNT; i++) {
        const GpioConfig_t* cfg = &gpio_config[i];
        
        APP_DEBUG("[%d] %s (pin %d) - %s\r\n",
                 i,
                 cfg->name,
                 cfg->pin,
                 cfg->direction == GPIO_DIR_INPUT ? "INPUT" : "OUTPUT");
        
        /* Copy configuration */
        Ql_memcpy(&gpio_data[gpio_count].config, cfg, sizeof(GpioConfig_t));
        
        if (cfg->direction == GPIO_DIR_OUTPUT) {
            /* Configure as output */
            ret = Ql_GPIO_Init(cfg->pin, PINDIRECTION_OUT, cfg->init_level, PINPULLSEL_DISABLE);
            if (ret < 0) {
                APP_DEBUG("  ERROR: Failed to init output GPIO, ret=%d\r\n", ret);
                continue;
            }
            
            /* Register parameter callback if linked */
            if (cfg->linked_param < PARAM_MAX_COUNT) {
                ret = param_set_callback(cfg->linked_param, gpio_param_callback);
                if (ret != 0) {
                    APP_DEBUG("  ERROR: Failed to register param callback, ret=%d\r\n", ret);
                    continue;
                }
                APP_DEBUG("  ✅ Linked to param '%s'\r\n", param_get_name(cfg->linked_param));
            }
            
        } else { /* GPIO_DIR_INPUT */
            /* CRITICAL: For EINT pins, DO NOT call Ql_GPIO_Init!
             * 
             * Per Quectel's official example_eint.c:
             * - Ql_GPIO_Init is NEVER called before EINT registration
             * - Ql_EINT_Register must be the FIRST operation on the pin
             * - Calling Ql_GPIO_Init makes the pin "owned" by GPIO, blocking EINT
             * 
             * EINT registration and init will configure the pin automatically.
             */
            if (cfg->eint_callback != NULL) {
                APP_DEBUG("  Skipping Ql_GPIO_Init (EINT will configure pin directly)...\r\n");
                /* Don't call Ql_GPIO_Init - proceed directly to EINT registration */
            } else {
                /* Regular input pin without EINT - configure as input with pull-up */
                APP_DEBUG("  Configuring as INPUT with PULL-UP...\r\n");
                ret = Ql_GPIO_Init(cfg->pin, PINDIRECTION_IN, PINLEVEL_LOW, PINPULLSEL_PULLUP);
                if (ret < 0) {
                    APP_DEBUG("  ❌ ERROR: Failed to init input GPIO, ret=%d\r\n", ret);
                    APP_DEBUG("     (Pin may be in use by another peripheral)\r\n");
                    continue;
                }
                APP_DEBUG("  ✅ GPIO init OK\r\n");
            }
            
            /* Register EINT if configured */
            if (cfg->eint_callback != NULL) {
                APP_DEBUG("  [TEST] Attempting EINT registration...\r\n");
                ret = Ql_EINT_Register(cfg->pin, cfg->eint_callback, NULL);
                if (ret < 0) {
                    APP_DEBUG("  ❌ EINT Register FAILED: ret=%d", ret);
                    /* Decode common error codes */
                    if (ret == -1) {
                        APP_DEBUG(" (Generic error)");
                    } else if (ret == -16) {
                        APP_DEBUG(" (Pin in use / not available)");
                    } else if (ret == -5) {
                        APP_DEBUG(" (Invalid parameter)");
                    }
                    APP_DEBUG("\r\n");
                    APP_DEBUG("     >>> %s does NOT support EINT <<<\r\n", cfg->name);
                    continue;  /* Skip this pin, try next */
                }
                APP_DEBUG("  ✅ EINT Register OK\r\n");
                
                APP_DEBUG("  [TEST] Initializing EINT...\r\n");
                ret = Ql_EINT_Init(cfg->pin, cfg->eint_type, 0, 5, 0);
                if (ret < 0) {
                    APP_DEBUG("  ❌ EINT Init FAILED: ret=%d\r\n", ret);
                    APP_DEBUG("     >>> %s does NOT support EINT <<<\r\n", cfg->name);
                    Ql_EINT_Uninit(cfg->pin);
                    continue;  /* Skip this pin, try next */
                }
                
                APP_DEBUG("  ✅✅✅ SUCCESS! %s SUPPORTS EINT! ✅✅✅\r\n", cfg->name);
                /* Display EINT type (M66 supports: EDGE_TRIGGERED=0, LEVEL_TRIGGERED=1) */
                switch (cfg->eint_type) {
                    case EINT_EDGE_TRIGGERED:
                        APP_DEBUG("     Type: EDGE_TRIGGERED (HIGH→LOW edge)\r\n");
                        break;
                    case EINT_LEVEL_TRIGGERED:
                        APP_DEBUG("     Type: LEVEL_TRIGGERED (when LOW)\r\n");
                        break;
                    default:
                        APP_DEBUG("     Type: %d (unknown)\r\n", cfg->eint_type);
                        break;
                }
                APP_DEBUG("     Pull-up: ENABLED\r\n");
                APP_DEBUG("     >>> Connect %s to GND to trigger <<<\r\n", cfg->name);
            }
        }
        
        gpio_data[gpio_count].initialized = TRUE;
        gpio_count++;
    }
    
    gpio_initialized = TRUE;
    
    APP_DEBUG("\r\n");
    APP_DEBUG("╔════════════════════════════════════════════════╗\r\n");
    APP_DEBUG("║         EINT TEST SUMMARY                      ║\r\n");
    APP_DEBUG("╚════════════════════════════════════════════════╝\r\n");
    APP_DEBUG("Total GPIOs configured: %d/%d\r\n", gpio_count, GPIO_CONFIG_COUNT);
    APP_DEBUG("\r\n");
    APP_DEBUG("EINT-capable pins (successfully initialized):\r\n");
    
    /* Print summary of EINT-capable pins */
    {
        u32 i;
        u32 eint_count = 0;
        for (i = 0; i < gpio_count; i++) {
            if (gpio_data[i].initialized && 
                gpio_data[i].config.direction == GPIO_DIR_INPUT &&
                gpio_data[i].config.eint_callback != NULL) {
                APP_DEBUG("  ✅ %s (pin %d) - Connect to GND to test\r\n", 
                         gpio_data[i].config.name,
                         gpio_data[i].config.pin);
                eint_count++;
            }
        }
        
        if (eint_count == 0) {
            APP_DEBUG("  ❌ NO PINS SUPPORT EINT!\r\n");
        } else {
            APP_DEBUG("\r\n");
            APP_DEBUG("SUCCESS! %d pin(s) support EINT\r\n", eint_count);
        }
    }
    
    APP_DEBUG("════════════════════════════════════════════════\r\n\r\n");
    
    return 0;
}

s32 gpio_set_level(Enum_PinName pin, Enum_PinLevel level)
{
    GpioData_t* gpio;
    
    if (!gpio_initialized) {
        APP_DEBUG("ERROR: GPIO not initialized\r\n");
        return -1;
    }
    
    gpio = find_gpio_by_pin(pin);
    if (gpio == NULL) {
        APP_DEBUG("ERROR: GPIO pin %d not found\r\n", pin);
        return -2;
    }
    
    if (gpio->config.direction != GPIO_DIR_OUTPUT) {
        APP_DEBUG("ERROR: GPIO '%s' is not an output\r\n", gpio->config.name);
        return -3;
    }
    
    Ql_GPIO_SetLevel(pin, level);
    
    return 0;
}

s32 gpio_get_level(Enum_PinName pin, Enum_PinLevel* level)
{
    GpioData_t* gpio;
    
    if (!gpio_initialized) {
        APP_DEBUG("ERROR: GPIO not initialized\r\n");
        return -1;
    }
    
    if (level == NULL) {
        return -2;
    }
    
    gpio = find_gpio_by_pin(pin);
    if (gpio == NULL) {
        APP_DEBUG("ERROR: GPIO pin %d not found\r\n", pin);
        return -3;
    }
    
    *level = Ql_GPIO_GetLevel(pin);
    
    return 0;
}

s32 gpio_toggle(Enum_PinName pin)
{
    Enum_PinLevel current_level;
    s32 ret;
    
    ret = gpio_get_level(pin, &current_level);
    if (ret != 0) {
        return ret;
    }
    
    return gpio_set_level(pin, current_level == PINLEVEL_HIGH ? PINLEVEL_LOW : PINLEVEL_HIGH);
}

void gpio_print_status(void)
{
    u32 i;
    Enum_PinLevel level;
    
    if (!gpio_initialized) {
        APP_DEBUG("GPIO not initialized\r\n");
        return;
    }
    
    APP_DEBUG("\r\n=== GPIO Status ===\r\n");
    APP_DEBUG("%-15s %-8s %-10s %-15s %s\r\n",
             "Name", "Pin", "Direction", "Linked Param", "Level");
    APP_DEBUG("---------------------------------------------------------------\r\n");
    
    for (i = 0; i < gpio_count; i++) {
        if (!gpio_data[i].initialized) {
            continue;
        }
        
        level = Ql_GPIO_GetLevel(gpio_data[i].config.pin);
        
        APP_DEBUG("%-15s %-8d %-10s %-15s %s\r\n",
                 gpio_data[i].config.name,
                 gpio_data[i].config.pin,
                 gpio_data[i].config.direction == GPIO_DIR_INPUT ? "INPUT" : "OUTPUT",
                 gpio_data[i].config.linked_param < PARAM_MAX_COUNT ?
                     param_get_name(gpio_data[i].config.linked_param) : "none",
                 level == PINLEVEL_HIGH ? "HIGH" : "LOW");
    }
    
    APP_DEBUG("===================\r\n\r\n");
}

const GpioConfig_t* gpio_get_config(void)
{
    return gpio_config;
}

u32 gpio_get_count(void)
{
    return gpio_count;
}

