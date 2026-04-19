/**
 * @file    gpio_expander.c
 * @brief   GPIO abstraction over PCF8574 nodes
 */

#include "config/module_config.h"
#include "gpio_internal.h"
#include "i2c_bus/i2c_bus.h"
#include "io_expander/io_expander.h"
#include "uart/uart.h"
#include "ql_stdlib.h"

#ifdef MODULE_IO_EXPANDER_ENABLED

typedef struct {
    bool     initialized;
    u8       device_id;
    u8       cached_state;
    GpioExpanderPinConfig_t config;
} GpioExpanderPinRuntime_t;

static const GpioExpanderNodeConfig_t g_gpio_expander_nodes[] = {
    {
        .name = "PCF8574_0x4A",
        .i2c_addr = 0x4A,
        .enabled = TRUE
    },
};

static const GpioExpanderPinConfig_t g_gpio_expander_pin_config[] = {
    {
        .name = "PCF4A_P0_OUT",
        .i2c_addr = 0x4A,
        .pin = 0,
        .direction = GPIO_PIN_DIR_OUTPUT,
        .linked_param = PARAM_IO_EXP_OUT0,
        .init_level = PINLEVEL_HIGH
    },
    {
        .name = "PCF4A_P3_OUT",
        .i2c_addr = 0x4A,
        .pin = 1,
        .direction = GPIO_PIN_DIR_OUTPUT,
        .linked_param = PARAM_IO_EXP_OUT1,
        .init_level = PINLEVEL_HIGH
    },
    {
        .name = "PCF4A_P1_OUT",
        .i2c_addr = 0x4A,
        .pin = 2,
        .direction = GPIO_PIN_DIR_OUTPUT,
        .linked_param = PARAM_IO_EXP_OUT2,
        .init_level = PINLEVEL_HIGH
    },
    {
        .name = "PCF4A_P2_OUT",
        .i2c_addr = 0x4A,
        .pin = 3,
        .direction = GPIO_PIN_DIR_OUTPUT,
        .linked_param = PARAM_IO_EXP_OUT3,
        .init_level = PINLEVEL_HIGH
    },
    {
        .name = "PCF4A_P4_IN",
        .i2c_addr = 0x4A,
        .pin = 4,
        .direction = GPIO_PIN_DIR_INPUT,
        .linked_param = PARAM_IO_EXP_IN0,
        .init_level = PINLEVEL_LOW
    },
    {
        .name = "PCF4A_P5_IN",
        .i2c_addr = 0x4A,
        .pin = 5,
        .direction = GPIO_PIN_DIR_INPUT,
        .linked_param = PARAM_IO_EXP_IN1,
        .init_level = PINLEVEL_LOW
    },
    {
        .name = "PCF4A_P6_IN",
        .i2c_addr = 0x4A,
        .pin = 6,
        .direction = GPIO_PIN_DIR_INPUT,
        .linked_param = PARAM_IO_EXP_IN2,
        .init_level = PINLEVEL_LOW
    },
    {
        .name = "PCF4A_P7_IN",
        .i2c_addr = 0x4A,
        .pin = 7,
        .direction = GPIO_PIN_DIR_INPUT,
        .linked_param = PARAM_IO_EXP_IN3,
        .init_level = PINLEVEL_HIGH
    },
};

#define GPIO_EXPANDER_NODE_COUNT (sizeof(g_gpio_expander_nodes) / sizeof(g_gpio_expander_nodes[0]))
#define GPIO_EXPANDER_PIN_COUNT  (sizeof(g_gpio_expander_pin_config) / sizeof(g_gpio_expander_pin_config[0]))

static GpioExpanderNodeRuntime_t g_gpio_expander_runtime[GPIO_EXPANDER_NODE_COUNT];
static GpioExpanderPinRuntime_t g_gpio_expander_pins[GPIO_EXPANDER_PIN_COUNT];
static IoExpanderConfig_t g_io_expander_init_config[GPIO_EXPANDER_NODE_COUNT];
static bool g_gpio_expander_initialized = FALSE;

static s32 gpio_expander_write_device_shadow(u8 device_id)
{
    u32 i;
    u8 shadow_state = 0x00;
    bool has_pin = FALSE;
    s32 ret;

    for (i = 0; i < GPIO_EXPANDER_PIN_COUNT; i++) {
        if (!g_gpio_expander_pins[i].initialized ||
            g_gpio_expander_pins[i].device_id != device_id) {
            continue;
        }

        has_pin = TRUE;

        /* PCF8574 input mode requires writing HIGH to that bit. */
        if (g_gpio_expander_pins[i].config.direction == GPIO_PIN_DIR_INPUT ||
            g_gpio_expander_pins[i].cached_state) {
            shadow_state |= (1 << g_gpio_expander_pins[i].config.pin);
        }
    }

    if (!has_pin) {
        return -1;
    }

    ret = io_expander_write_port(device_id, shadow_state);
    if (ret != 0) {
        APP_DEBUG("GPIO_EXP: failed to write shadow state dev=%d state=0x%02X ret=%d\r\n",
                  device_id,
                  shadow_state,
                  ret);
        return ret;
    }

    APP_DEBUG("GPIO_EXP: dev=%d shadow state=0x%02X\r\n", device_id, shadow_state);
    return 0;
}

static s32 gpio_expander_find_runtime_index_by_addr(u8 i2c_addr)
{
    u32 i;

    for (i = 0; i < GPIO_EXPANDER_NODE_COUNT; i++) {
        if (g_gpio_expander_runtime[i].initialized &&
            g_gpio_expander_runtime[i].config.i2c_addr == i2c_addr) {
            return (s32)i;
        }
    }

    return -1;
}

static s32 gpio_expander_build_node_init_state(u8 i2c_addr, u8* init_state)
{
    u32 i;
    u8 state = 0x00;
    bool found = FALSE;

    if (init_state == NULL) {
        return -1;
    }

    for (i = 0; i < GPIO_EXPANDER_PIN_COUNT; i++) {
        if (g_gpio_expander_pin_config[i].i2c_addr != i2c_addr) {
            continue;
        }

        found = TRUE;
        if (g_gpio_expander_pin_config[i].direction == GPIO_PIN_DIR_INPUT ||
            g_gpio_expander_pin_config[i].init_level == PINLEVEL_HIGH) {
            state |= (1 << g_gpio_expander_pin_config[i].pin);
        }
    }

    if (!found) {
        return -2;
    }

    *init_state = state;
    return 0;
}

static void gpio_expander_output_callback(ParamKey_e key,
                                          const void* old_val,
                                          const void* new_val,
                                          ParamType_e type)
{
    u32 i;
    u8 pin_level;
    bool updated = FALSE;

    (void)old_val;

    if (type != PARAM_TYPE_INT8 || new_val == NULL) {
        return;
    }

    pin_level = (*(const s8*)new_val) ? 1 : 0;

    for (i = 0; i < GPIO_EXPANDER_PIN_COUNT; i++) {
        if (!g_gpio_expander_pins[i].initialized) {
            continue;
        }

        if (g_gpio_expander_pins[i].config.direction != GPIO_PIN_DIR_OUTPUT ||
            g_gpio_expander_pins[i].config.linked_param != key) {
            continue;
        }

        g_gpio_expander_pins[i].cached_state = pin_level;
        updated = TRUE;

        APP_DEBUG("GPIO_EXP: %s addr=0x%02X pin=%d set=%d\r\n",
                  g_gpio_expander_pins[i].config.name,
                  g_gpio_expander_pins[i].config.i2c_addr,
                  g_gpio_expander_pins[i].config.pin,
                  pin_level);

        gpio_expander_write_device_shadow(g_gpio_expander_pins[i].device_id);
    }

    if (!updated) {
        APP_DEBUG("GPIO_EXP: output callback with no mapped output for key=%d\r\n", key);
    }
}

static void gpio_expander_apply_output_defaults(void)
{
    u32 i;

    for (i = 0; i < GPIO_EXPANDER_PIN_COUNT; i++) {
        s8 output_state = 0;

        if (!g_gpio_expander_pins[i].initialized ||
            g_gpio_expander_pins[i].config.direction != GPIO_PIN_DIR_OUTPUT) {
            continue;
        }

        if (param_get_int8(g_gpio_expander_pins[i].config.linked_param, &output_state) == 0) {
            gpio_expander_output_callback(g_gpio_expander_pins[i].config.linked_param,
                                          NULL,
                                          &output_state,
                                          PARAM_TYPE_INT8);
        }
    }
}

s32 gpio_expander_init_module(void)
{
    u32 i;
    u8 active_node_count = 0;
    s32 ret;

    Ql_memset(g_gpio_expander_runtime, 0, sizeof(g_gpio_expander_runtime));
    Ql_memset(g_gpio_expander_pins, 0, sizeof(g_gpio_expander_pins));
    Ql_memset(g_io_expander_init_config, 0, sizeof(g_io_expander_init_config));

    if (!i2c_bus_is_initialized()) {
        APP_DEBUG("GPIO_EXP: I2C bus not initialized, skipping expander setup\r\n");
        return 0;
    }

    for (i = 0; i < GPIO_EXPANDER_NODE_COUNT; i++) {
        u8 init_state = 0xFF;

        if (!g_gpio_expander_nodes[i].enabled) {
            continue;
        }

        ret = gpio_expander_build_node_init_state(g_gpio_expander_nodes[i].i2c_addr, &init_state);
        if (ret != 0) {
            APP_DEBUG("GPIO_EXP: no pin map for node 0x%02X\r\n",
                      g_gpio_expander_nodes[i].i2c_addr);
            continue;
        }

        g_io_expander_init_config[active_node_count].name = g_gpio_expander_nodes[i].name;
        g_io_expander_init_config[active_node_count].i2c_addr = g_gpio_expander_nodes[i].i2c_addr;
        g_io_expander_init_config[active_node_count].init_state = init_state;
        g_io_expander_init_config[active_node_count].enabled = TRUE;

        g_gpio_expander_runtime[i].initialized = TRUE;
        g_gpio_expander_runtime[i].device_id = active_node_count;
        g_gpio_expander_runtime[i].config = g_gpio_expander_nodes[i];
        active_node_count++;
    }

    if (active_node_count == 0) {
        return 0;
    }

    ret = io_expander_init(0, g_io_expander_init_config, active_node_count);
    if (ret != IO_EXPANDER_OK) {
        APP_DEBUG("GPIO_EXP: io_expander_init failed, ret=%d\r\n", ret);
        return -1;
    }

    for (i = 0; i < GPIO_EXPANDER_PIN_COUNT; i++) {
        s32 runtime_index;

        runtime_index = gpio_expander_find_runtime_index_by_addr(g_gpio_expander_pin_config[i].i2c_addr);
        if (runtime_index < 0) {
            continue;
        }

        g_gpio_expander_pins[i].initialized = TRUE;
        g_gpio_expander_pins[i].device_id = g_gpio_expander_runtime[runtime_index].device_id;
        g_gpio_expander_pins[i].config = g_gpio_expander_pin_config[i];
        g_gpio_expander_pins[i].cached_state =
            (g_gpio_expander_pin_config[i].direction == GPIO_PIN_DIR_INPUT ||
             g_gpio_expander_pin_config[i].init_level == PINLEVEL_HIGH) ? 1 : 0;

        if (g_gpio_expander_pins[i].config.direction == GPIO_PIN_DIR_OUTPUT) {
            param_set_callback(g_gpio_expander_pins[i].config.linked_param,
                               gpio_expander_output_callback);
        }
    }

    g_gpio_expander_initialized = TRUE;
    gpio_expander_apply_output_defaults();
    gpio_expander_handle_interrupt();
    APP_DEBUG("GPIO_EXP: initialized %d node(s)\r\n", active_node_count);

    return 0;
}

u32 gpio_expander_count(void)
{
    u32 i;
    u32 count = 0;

    for (i = 0; i < GPIO_EXPANDER_NODE_COUNT; i++) {
        if (g_gpio_expander_runtime[i].initialized) {
            count++;
        }
    }

    return count;
}

void gpio_expander_handle_interrupt(void)
{
    u32 i;

    if (!g_gpio_expander_initialized) {
        return;
    }

    for (i = 0; i < GPIO_EXPANDER_NODE_COUNT; i++) {
        u8 port_state = 0;
        u32 pin_index;

        if (!g_gpio_expander_runtime[i].initialized) {
            continue;
        }

        if (io_expander_read_port(g_gpio_expander_runtime[i].device_id, &port_state) != 0) {
            continue;
        }

        for (pin_index = 0; pin_index < GPIO_EXPANDER_PIN_COUNT; pin_index++) {
            s8 value;
            s8 old_value = -1;
            u8 raw_level;

            if (!g_gpio_expander_pins[pin_index].initialized) {
                continue;
            }

            if (g_gpio_expander_pins[pin_index].device_id != g_gpio_expander_runtime[i].device_id ||
                g_gpio_expander_pins[pin_index].config.direction != GPIO_PIN_DIR_INPUT) {
                continue;
            }

            raw_level = (port_state & (1 << g_gpio_expander_pins[pin_index].config.pin)) ? 1 : 0;
            value = (raw_level == (u8)g_gpio_expander_pins[pin_index].config.init_level) ? 0 : 1;
            param_get_int8(g_gpio_expander_pins[pin_index].config.linked_param, &old_value);
            param_set_int8(g_gpio_expander_pins[pin_index].config.linked_param, value);
            if (old_value != value) {
                APP_DEBUG("GPIO_EXP: %s raw=%d trigger=%d (param '%s')\r\n",
                          g_gpio_expander_pins[pin_index].config.name,
                          raw_level,
                          value,
                          param_get_name(g_gpio_expander_pins[pin_index].config.linked_param));
            }
        }

        APP_DEBUG("GPIO_EXP: addr=0x%02X port state=0x%02X\r\n",
                  g_gpio_expander_runtime[i].config.i2c_addr,
                  port_state);
    }
}

void gpio_expander_print_status(void)
{
    u32 i;

    APP_DEBUG("=== GPIO Expanders ===\r\n");
    for (i = 0; i < GPIO_EXPANDER_PIN_COUNT; i++) {
        if (!g_gpio_expander_pins[i].initialized) {
            continue;
        }

        APP_DEBUG("  %s addr=0x%02X pin=%d dir=%s param=%s\r\n",
                  g_gpio_expander_pins[i].config.name,
                  g_gpio_expander_pins[i].config.i2c_addr,
                  g_gpio_expander_pins[i].config.pin,
                  g_gpio_expander_pins[i].config.direction == GPIO_PIN_DIR_OUTPUT ? "OUT" : "IN",
                  g_gpio_expander_pins[i].config.linked_param < PARAM_MAX_COUNT ?
                      param_get_name(g_gpio_expander_pins[i].config.linked_param) : "none");
    }
    APP_DEBUG("\r\n");
}

#else

s32 gpio_expander_init_module(void)
{
    return 0;
}

u32 gpio_expander_count(void)
{
    return 0;
}

void gpio_expander_print_status(void)
{
}

void gpio_expander_handle_interrupt(void)
{
}

#endif
