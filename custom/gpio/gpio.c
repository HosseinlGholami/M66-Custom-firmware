/**
 * @file    gpio.c
 * @brief   GPIO management implementation
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * This module provides table-driven GPIO configuration with parameter integration.
 * Output GPIOs can be automatically controlled via parameters using callbacks.
 */

#include "gpio.h"
#include "uart/uart.h"
#include "ql_stdlib.h"
#include "ql_eint.h"

/*============================================================================
 * GPIO Configuration Table
 * 
 * Define your GPIOs here!
 * - For INPUT: Set eint_type and eint_callback
 * - For OUTPUT: Set linked_param (or PARAM_MAX_COUNT if no link) and init_level
 *===========================================================================*/

/* Forward declaration for button callback (example, uncomment when needed) */
/* static void button_eint_callback(Enum_PinName pin, Enum_PinLevel level, void* user_data); */

static const GpioConfig_t gpio_config[] = {
    /* Example configuration - customize for your hardware! */
    
    /* LED outputs controlled by parameters */
    {
        .name = "LED1",
        .pin = PINNAME_NETLIGHT,  /* Example: Use NET_LIGHT pin as LED */
        .direction = GPIO_DIR_OUTPUT,
        .eint_type = 0,  /* Not used for outputs */
        .eint_callback = NULL,
        .linked_param = PARAM_IO_STATE,  /* Bit 0 of IO_STATE controls this LED */
        .init_level = PINLEVEL_LOW
    },
    
    /* Button input with EINT */
    /* Uncomment and configure when you have actual button hardware:
    {
        .name = "BUTTON1",
        .pin = PINNAME_RI,  // Example pin
        .direction = GPIO_DIR_INPUT,
        .eint_type = EINT_LEVEL_TRIGGERED,
        .eint_callback = button_eint_callback,
        .linked_param = PARAM_MAX_COUNT,  // Not linked
        .init_level = PINLEVEL_LOW  // Not used for inputs
    },
    */
    
    /* Add more GPIO configurations here... */
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
 * @brief Example button EINT callback (commented out - uncomment when needed)
 * Called when button is pressed/released
 */
/*
static void button_eint_callback(Enum_PinName pin, Enum_PinLevel level, void* user_data)
{
    GpioData_t* gpio = find_gpio_by_pin(pin);
    if (gpio == NULL) {
        return;
    }
    
    APP_DEBUG("GPIO: Button '%s' %s\r\n", 
             gpio->config.name,
             level == PINLEVEL_HIGH ? "PRESSED" : "RELEASED");
    
    // Example: Toggle LED on button press
    if (level == PINLEVEL_HIGH) {
        s8 io_state;
        param_get_int8(PARAM_IO_STATE, &io_state);
        io_state ^= 0x01;  // Toggle bit 0
        param_set_int8(PARAM_IO_STATE, io_state);
    }
}
*/

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
            /* Configure as input */
            ret = Ql_GPIO_Init(cfg->pin, PINDIRECTION_IN, PINLEVEL_LOW, PINPULLSEL_PULLUP);
            if (ret < 0) {
                APP_DEBUG("  ERROR: Failed to init input GPIO, ret=%d\r\n", ret);
                continue;
            }
            
            /* Register EINT if configured */
            if (cfg->eint_callback != NULL) {
                ret = Ql_EINT_Register(cfg->pin, cfg->eint_callback, NULL);
                if (ret < 0) {
                    APP_DEBUG("  ERROR: Failed to register EINT, ret=%d\r\n", ret);
                    continue;
                }
                
                ret = Ql_EINT_Init(cfg->pin, cfg->eint_type, 50, 50, TRUE);  /* 50ms hw/sw debounce, auto unmask */
                if (ret < 0) {
                    APP_DEBUG("  ERROR: Failed to init EINT, ret=%d\r\n", ret);
                    continue;
                }
                
                APP_DEBUG("  ✅ EINT registered (type %d)\r\n", cfg->eint_type);
            }
        }
        
        gpio_data[gpio_count].initialized = TRUE;
        gpio_count++;
    }
    
    gpio_initialized = TRUE;
    
    APP_DEBUG("GPIO module initialized: %d/%d GPIOs configured\r\n", gpio_count, GPIO_CONFIG_COUNT);
    APP_DEBUG("==================================\r\n\r\n");
    
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

