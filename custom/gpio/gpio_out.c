/**
 * @file    gpio_out.c
 * @brief   Local GPIO output handling
 */

#include "gpio_internal.h"
#include "uart/uart.h"
#include "ql_stdlib.h"

static const GpioOutputConfig_t g_gpio_output_config[] = {
    {
        .name = "LED1",
        .pin = PINNAME_NETLIGHT,
        .linked_param = PARAM_IO_STATE,
        .init_level = PINLEVEL_LOW
    },
};

#define GPIO_OUTPUT_COUNT (sizeof(g_gpio_output_config) / sizeof(g_gpio_output_config[0]))

static GpioOutputRuntime_t g_gpio_outputs[GPIO_OUTPUT_COUNT];

static void gpio_output_param_callback(ParamKey_e key,
                                       const void* old_val,
                                       const void* new_val,
                                       ParamType_e type)
{
    u32 i;
    Enum_PinLevel new_level;

    (void)old_val;

    switch (type) {
    case PARAM_TYPE_INT8:
        new_level = (*(const s8*)new_val) ? PINLEVEL_HIGH : PINLEVEL_LOW;
        break;

    case PARAM_TYPE_INT16:
        new_level = (*(const s16*)new_val) ? PINLEVEL_HIGH : PINLEVEL_LOW;
        break;

    case PARAM_TYPE_INT32:
        new_level = (*(const s32*)new_val) ? PINLEVEL_HIGH : PINLEVEL_LOW;
        break;

    default:
        return;
    }

    for (i = 0; i < GPIO_OUTPUT_COUNT; i++) {
        if (!g_gpio_outputs[i].initialized) {
            continue;
        }

        if (g_gpio_outputs[i].config.linked_param != key) {
            continue;
        }

        Ql_GPIO_SetLevel(g_gpio_outputs[i].config.pin, new_level);
        APP_DEBUG("GPIO_OUT: %s -> %s (param '%s')\r\n",
                 g_gpio_outputs[i].config.name,
                 new_level == PINLEVEL_HIGH ? "HIGH" : "LOW",
                 param_get_name(key));
    }
}

s32 gpio_output_init(void)
{
    u32 i;
    s32 ret;
    ParamType_e type;
    Enum_PinLevel applied_level;

    Ql_memset(g_gpio_outputs, 0, sizeof(g_gpio_outputs));

    for (i = 0; i < GPIO_OUTPUT_COUNT; i++) {
        g_gpio_outputs[i].config = g_gpio_output_config[i];

        ret = Ql_GPIO_Init(g_gpio_outputs[i].config.pin,
                           PINDIRECTION_OUT,
                           g_gpio_outputs[i].config.init_level,
                           PINPULLSEL_DISABLE);
        if (ret < 0) {
            APP_DEBUG("GPIO_OUT: failed to init %s, ret=%d\r\n",
                     g_gpio_outputs[i].config.name, ret);
            return -1;
        }

        ret = param_set_callback(g_gpio_outputs[i].config.linked_param, gpio_output_param_callback);
        if (ret != 0) {
            APP_DEBUG("GPIO_OUT: failed to link param '%s'\r\n",
                     param_get_name(g_gpio_outputs[i].config.linked_param));
            return -2;
        }

        applied_level = g_gpio_outputs[i].config.init_level;
        if (param_get_type(g_gpio_outputs[i].config.linked_param, &type) == 0) {
            if (type == PARAM_TYPE_INT8) {
                s8 value_i8;
                if (param_get_int8(g_gpio_outputs[i].config.linked_param, &value_i8) == 0) {
                    applied_level = value_i8 ? PINLEVEL_HIGH : PINLEVEL_LOW;
                }
            } else if (type == PARAM_TYPE_INT16) {
                s16 value_i16;
                if (param_get_int16(g_gpio_outputs[i].config.linked_param, &value_i16) == 0) {
                    applied_level = value_i16 ? PINLEVEL_HIGH : PINLEVEL_LOW;
                }
            } else if (type == PARAM_TYPE_INT32) {
                s32 value_i32;
                if (param_get_int32(g_gpio_outputs[i].config.linked_param, &value_i32) == 0) {
                    applied_level = value_i32 ? PINLEVEL_HIGH : PINLEVEL_LOW;
                }
            }
        }

        Ql_GPIO_SetLevel(g_gpio_outputs[i].config.pin, applied_level);

        g_gpio_outputs[i].initialized = TRUE;
        APP_DEBUG("GPIO_OUT: %s mapped to param '%s'\r\n",
                 g_gpio_outputs[i].config.name,
                 param_get_name(g_gpio_outputs[i].config.linked_param));
    }

    return 0;
}

u32 gpio_output_count(void)
{
    return GPIO_OUTPUT_COUNT;
}

s32 gpio_output_set_level(Enum_PinName pin, Enum_PinLevel level)
{
    u32 i;

    for (i = 0; i < GPIO_OUTPUT_COUNT; i++) {
        if (!g_gpio_outputs[i].initialized) {
            continue;
        }

        if (g_gpio_outputs[i].config.pin != pin) {
            continue;
        }

        return param_set_int8(g_gpio_outputs[i].config.linked_param,
                              level == PINLEVEL_HIGH ? 1 : 0);
    }

    return -1;
}

s32 gpio_output_get_level(Enum_PinName pin, Enum_PinLevel* level)
{
    u32 i;

    if (level == NULL) {
        return -1;
    }

    for (i = 0; i < GPIO_OUTPUT_COUNT; i++) {
        if (!g_gpio_outputs[i].initialized) {
            continue;
        }

        if (g_gpio_outputs[i].config.pin != pin) {
            continue;
        }

        *level = Ql_GPIO_GetLevel(pin);
        return 0;
    }

    return -2;
}

void gpio_output_print_status(void)
{
    u32 i;
    Enum_PinLevel level;

    APP_DEBUG("\r\n=== Local GPIO Outputs ===\r\n");
    for (i = 0; i < GPIO_OUTPUT_COUNT; i++) {
        if (!g_gpio_outputs[i].initialized) {
            continue;
        }

        level = Ql_GPIO_GetLevel(g_gpio_outputs[i].config.pin);
        APP_DEBUG("  %s pin=%d param=%s level=%s\r\n",
                 g_gpio_outputs[i].config.name,
                 g_gpio_outputs[i].config.pin,
                 param_get_name(g_gpio_outputs[i].config.linked_param),
                 level == PINLEVEL_HIGH ? "HIGH" : "LOW");
    }
    APP_DEBUG("\r\n");
}
