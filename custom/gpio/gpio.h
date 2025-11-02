/**
 * @file    gpio.h
 * @brief   GPIO management module with parameter integration
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * Features:
 * - Table-driven GPIO configuration
 * - Input GPIOs with EINT support and callbacks
 * - Output GPIOs linked to parameters (auto-update on param change)
 * - Thread-safe parameter integration
 * - Easy configuration - just modify gpio_config table
 */

#ifndef GPIO_H
#define GPIO_H

#include "ql_type.h"
#include "ql_gpio.h"
#include "ql_eint.h"
#include "custom_gpio_cfg.h"
#include "param/param.h"

/*============================================================================
 * Constants
 *===========================================================================*/
#define GPIO_MAX_PINS       16      /* Maximum number of GPIOs to manage */

/*============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief GPIO direction
 */
typedef enum {
    GPIO_DIR_INPUT,         /* Input pin (with optional EINT) */
    GPIO_DIR_OUTPUT         /* Output pin (can be linked to parameter) */
} GpioDir_e;

/**
 * @brief GPIO configuration structure
 * Define one entry for each GPIO you want to manage
 */
typedef struct {
    const char*         name;           /* GPIO name (for debug) */
    Enum_PinName        pin;            /* Physical pin number */
    GpioDir_e           direction;      /* Input or output */
    
    /* === For INPUT GPIOs === */
    Enum_EintType       eint_type;      /* EINT trigger type (or EINT_NONE if not used) */
    Callback_EINT_Handle eint_callback; /* Callback for EINT events */
    
    /* === For OUTPUT GPIOs === */
    ParamKey_e          linked_param;   /* Parameter to link (PARAM_MAX_COUNT if none) */
    Enum_PinLevel       init_level;     /* Initial output level */
} GpioConfig_t;

/**
 * @brief GPIO runtime data (internal use)
 */
typedef struct {
    bool                initialized;    /* Is this GPIO configured? */
    GpioConfig_t        config;         /* Copy of configuration */
} GpioData_t;

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize GPIO module
 * Configures all GPIOs based on gpio_config table
 * Registers parameter callbacks for linked output GPIOs
 * @return 0 on success, negative on error
 */
s32 gpio_init(void);

/**
 * @brief Set output GPIO level directly
 * @param pin Pin name
 * @param level High or low
 * @return 0 on success, negative on error
 */
s32 gpio_set_level(Enum_PinName pin, Enum_PinLevel level);

/**
 * @brief Get input GPIO level
 * @param pin Pin name
 * @param level Pointer to store level
 * @return 0 on success, negative on error
 */
s32 gpio_get_level(Enum_PinName pin, Enum_PinLevel* level);

/**
 * @brief Toggle output GPIO
 * @param pin Pin name
 * @return 0 on success, negative on error
 */
s32 gpio_toggle(Enum_PinName pin);

/**
 * @brief Print GPIO status (for debug)
 */
void gpio_print_status(void);

/**
 * @brief Get GPIO configuration table (for external configuration)
 * You can define your own gpio_config in your application
 * @return Pointer to GPIO configuration table
 */
const GpioConfig_t* gpio_get_config(void);

/**
 * @brief Get GPIO configuration count
 * @return Number of configured GPIOs
 */
u32 gpio_get_count(void);

#endif /* GPIO_H */

