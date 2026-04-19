/**
 * @file    gpio_in.c
 * @brief   Local GPIO input and EINT handling
 */

#include "gpio_internal.h"
#include "uart/uart.h"
#include "ql_eint.h"
#include "ql_stdlib.h"

static void gpio_input_expander_irq(Enum_PinName pin, Enum_PinLevel level, void* user_data)
{
    (void)user_data;

    Ql_EINT_Mask(pin);
    APP_DEBUG("GPIO_IN: interrupt on pin %d, level=%s\r\n",
              pin, level == PINLEVEL_HIGH ? "HIGH" : "LOW");
    gpio_expander_handle_interrupt();
    Ql_EINT_Unmask(pin);
}

static const GpioInputConfig_t g_gpio_input_config[] = {
    {
        .name = "DTR_IO_EXP_INT",
        .pin = PINNAME_DTR,
        .linked_param = PARAM_NONE,
        .pull_sel = PINPULLSEL_PULLUP,
        .eint_type = EINT_LEVEL_TRIGGERED,
        .eint_callback = gpio_input_expander_irq
    },
    {
        .name = "CTS_BATTERY_ACTIVATION",
        .pin = PINNAME_CTS,
        .linked_param = PARAM_BATTERY_ACTIVATION,
        .pull_sel = PINPULLSEL_PULLUP,
        .eint_type = EINT_LEVEL_TRIGGERED,
        .eint_callback = NULL
    },
};

#define GPIO_INPUT_COUNT (sizeof(g_gpio_input_config) / sizeof(g_gpio_input_config[0]))

static GpioInputRuntime_t g_gpio_inputs[GPIO_INPUT_COUNT];

static void gpio_input_update_param(const GpioInputRuntime_t* input)
{
    Enum_PinLevel level;
    s8 old_value = -1;

    if (input == NULL || !input->initialized) {
        return;
    }

    if (input->config.linked_param >= PARAM_MAX_COUNT) {
        return;
    }

    level = Ql_GPIO_GetLevel(input->config.pin);
    param_get_int8(input->config.linked_param, &old_value);
    param_set_int8(input->config.linked_param, (s8)level);

    if (old_value != (s8)level) {
        APP_DEBUG("GPIO_IN: %s changed -> %d (param '%s')\r\n",
                  input->config.name,
                  level,
                  param_get_name(input->config.linked_param));
    }
}

s32 gpio_input_init(void)
{
    u32 i;
    s32 ret;

    Ql_memset(g_gpio_inputs, 0, sizeof(g_gpio_inputs));

    for (i = 0; i < GPIO_INPUT_COUNT; i++) {
        g_gpio_inputs[i].config = g_gpio_input_config[i];

        if (g_gpio_inputs[i].config.eint_callback != NULL) {
            ret = Ql_EINT_Register(g_gpio_inputs[i].config.pin,
                                   g_gpio_inputs[i].config.eint_callback,
                                   NULL);
            if (ret < 0) {
                APP_DEBUG("GPIO_IN: failed to register EINT for %s, ret=%d\r\n",
                          g_gpio_inputs[i].config.name, ret);
                return -1;
            }

            ret = Ql_EINT_Init(g_gpio_inputs[i].config.pin,
                               g_gpio_inputs[i].config.eint_type,
                               0,
                               5,
                               FALSE);
            if (ret < 0) {
                APP_DEBUG("GPIO_IN: failed to init EINT for %s, ret=%d\r\n",
                          g_gpio_inputs[i].config.name, ret);
                Ql_EINT_Uninit(g_gpio_inputs[i].config.pin);
                return -2;
            }
        } else {
            ret = Ql_GPIO_Init(g_gpio_inputs[i].config.pin,
                               PINDIRECTION_IN,
                               PINLEVEL_LOW,
                               g_gpio_inputs[i].config.pull_sel);
            if (ret < 0) {
                APP_DEBUG("GPIO_IN: failed to init GPIO input for %s, ret=%d\r\n",
                          g_gpio_inputs[i].config.name, ret);
                return -3;
            }
        }

        g_gpio_inputs[i].initialized = TRUE;
        APP_DEBUG("GPIO_IN: %s ready on pin %d\r\n",
                  g_gpio_inputs[i].config.name,
                  g_gpio_inputs[i].config.pin);
    }

    gpio_input_poll();
    return 0;
}

u32 gpio_input_count(void)
{
    return GPIO_INPUT_COUNT;
}

void gpio_input_poll(void)
{
    u32 i;

    for (i = 0; i < GPIO_INPUT_COUNT; i++) {
        if (!g_gpio_inputs[i].initialized) {
            continue;
        }

        if (g_gpio_inputs[i].config.eint_callback != NULL) {
            continue;
        }

        gpio_input_update_param(&g_gpio_inputs[i]);
    }
}

void gpio_input_print_status(void)
{
    u32 i;

    APP_DEBUG("=== Local GPIO Inputs ===\r\n");
    for (i = 0; i < GPIO_INPUT_COUNT; i++) {
        if (!g_gpio_inputs[i].initialized) {
            continue;
        }

        APP_DEBUG("  %s pin=%d mode=%s",
                  g_gpio_inputs[i].config.name,
                  g_gpio_inputs[i].config.pin,
                  g_gpio_inputs[i].config.eint_callback != NULL ? "EINT" : "GPIO");
        if (g_gpio_inputs[i].config.linked_param < PARAM_MAX_COUNT) {
            APP_DEBUG(" param=%s", param_get_name(g_gpio_inputs[i].config.linked_param));
        }
        APP_DEBUG("\r\n");
    }
    APP_DEBUG("\r\n");
}
