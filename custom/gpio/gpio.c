/**
 * @file    gpio.c
 * @brief   GPIO module coordinator
 */

#include "gpio.h"
#include "gpio_internal.h"
#include "uart/uart.h"

static bool g_gpio_initialized = FALSE;

s32 gpio_init(void)
{
    s32 ret;

    if (g_gpio_initialized) {
        APP_DEBUG("WARNING: GPIO already initialized\r\n");
        return 0;
    }

    APP_DEBUG("\r\n=== Initializing GPIO Module ===\r\n");

    ret = gpio_output_init();
    if (ret != 0) {
        return ret;
    }

    ret = gpio_expander_init_module();
    if (ret != 0) {
        return ret;
    }

    ret = gpio_input_init();
    if (ret != 0) {
        return ret;
    }

    g_gpio_initialized = TRUE;
    APP_DEBUG("GPIO: initialized %d local output(s), %d local input(s), %d expander node(s)\r\n",
             gpio_output_count(), gpio_input_count(), gpio_expander_count());

    return 0;
}

s32 gpio_set_level(Enum_PinName pin, Enum_PinLevel level)
{
    return gpio_output_set_level(pin, level);
}

s32 gpio_get_level(Enum_PinName pin, Enum_PinLevel* level)
{
    return gpio_output_get_level(pin, level);
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
    gpio_output_print_status();
    gpio_input_print_status();
    gpio_expander_print_status();
}

u32 gpio_get_count(void)
{
    return gpio_output_count() + gpio_input_count();
}

void gpio_poll_inputs(void)
{
    gpio_input_poll();
}
