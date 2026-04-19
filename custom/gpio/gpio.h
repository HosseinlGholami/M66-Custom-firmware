/**
 * @file    gpio.h
 * @brief   GPIO abstraction for local pins and IO expanders
 */

#ifndef GPIO_H
#define GPIO_H

#include "param/param.h"
#include "ql_eint.h"
#include "ql_gpio.h"
#include "ql_type.h"

#define GPIO_MAX_PINS                16
#define GPIO_EXPANDER_MAX_PIN_MAPS   32

typedef enum {
    GPIO_PIN_DIR_INPUT = 0,
    GPIO_PIN_DIR_OUTPUT
} GpioPinDir_e;

typedef struct {
    const char*   name;
    Enum_PinName  pin;
    ParamKey_e    linked_param;
    Enum_PinLevel init_level;
} GpioOutputConfig_t;

typedef struct {
    const char*          name;
    Enum_PinName         pin;
    ParamKey_e           linked_param;
    Enum_PinPullSel      pull_sel;
    Enum_EintType        eint_type;
    Callback_EINT_Handle eint_callback;
} GpioInputConfig_t;

typedef struct {
    const char* name;
    u8          i2c_addr;
    bool        enabled;
} GpioExpanderNodeConfig_t;

typedef struct {
    const char*    name;
    u8             i2c_addr;
    u8             pin;
    GpioPinDir_e   direction;
    ParamKey_e     linked_param;
    Enum_PinLevel  init_level;
} GpioExpanderPinConfig_t;

s32 gpio_init(void);
s32 gpio_set_level(Enum_PinName pin, Enum_PinLevel level);
s32 gpio_get_level(Enum_PinName pin, Enum_PinLevel* level);
s32 gpio_toggle(Enum_PinName pin);
void gpio_print_status(void);
u32 gpio_get_count(void);
void gpio_poll_inputs(void);

#endif /* GPIO_H */
