#ifndef GPIO_INTERNAL_H
#define GPIO_INTERNAL_H

#include "gpio.h"

typedef struct {
    bool               initialized;
    GpioOutputConfig_t config;
} GpioOutputRuntime_t;

typedef struct {
    bool              initialized;
    GpioInputConfig_t config;
} GpioInputRuntime_t;

typedef struct {
    bool                 initialized;
    u8                   device_id;
    GpioExpanderNodeConfig_t config;
} GpioExpanderNodeRuntime_t;

s32 gpio_output_init(void);
u32 gpio_output_count(void);
s32 gpio_output_set_level(Enum_PinName pin, Enum_PinLevel level);
s32 gpio_output_get_level(Enum_PinName pin, Enum_PinLevel* level);
void gpio_output_print_status(void);

s32 gpio_input_init(void);
u32 gpio_input_count(void);
void gpio_input_print_status(void);
void gpio_input_poll(void);

s32 gpio_expander_init_module(void);
u32 gpio_expander_count(void);
void gpio_expander_print_status(void);
void gpio_expander_handle_interrupt(void);

#endif /* GPIO_INTERNAL_H */
