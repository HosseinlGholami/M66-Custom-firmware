/**
 * @file    board.c
 * @brief   Board-level device initialization
 */

#include "board.h"

#include "config/module_config.h"
#include "param/param.h"
#include "ql_gpio.h"
#include "ql_wtd.h"
#include "uart/uart.h"

#ifdef MODULE_GPIO_ENABLED
#include "gpio/gpio.h"
#endif

#ifdef MODULE_COM_ENABLED
#include "com/com.h"
#endif

#ifdef MODULE_I2C_BUS_ENABLED
#include "i2c_bus/i2c_bus.h"
#endif

#ifdef MODULE_OLED_ENABLED
#include "oled/oled.h"
#endif

#ifdef MODULE_IO_EXPANDER_ENABLED
#include "io_expander/io_expander.h"
#include "io_expander/io_expander_config.h"
#include "io_expander/io_expander_param.h"
#endif

static void board_print_banner(void)
{
    APP_DEBUG("\r\n");
    APP_DEBUG("==================BASE FIRMWARE======================\r\n");
    APP_DEBUG("  M66 Industrial Controller Firmware\r\n");
    APP_DEBUG("  Modular Architecture - Clean Start\r\n");
    APP_DEBUG("  Build: %s %s\r\n", __DATE__, __TIME__);
    APP_DEBUG("========================================\r\n");
    APP_DEBUG("\r\n");
}

s32 board_init(UartRxCallback_t uart_rx_callback)
{
    s32 first_error = 0;

    if (uart_init(UART_PORT1, 115200) != 0) {
        return -1;
    }

    board_print_banner();
    Ql_WTD_Feed(1);

    if (param_init() != 0 && first_error == 0) {
        APP_DEBUG("BOARD: param init failed\r\n");
        first_error = -2;
    }
    Ql_WTD_Feed(1);

#ifdef MODULE_GPIO_ENABLED
    if (gpio_init() != 0 && first_error == 0) {
        APP_DEBUG("BOARD: gpio init failed\r\n");
        first_error = -3;
    }
    Ql_WTD_Feed(1);
#endif

#ifdef MODULE_COM_ENABLED
    if (com_init(NULL) != 0) {
        if (first_error == 0) {
            first_error = -4;
        }
        APP_DEBUG("BOARD: com init failed\r\n");
    } else if (uart_rx_callback != NULL) {
        uart_register_callback(uart_rx_callback);
    }
    Ql_WTD_Feed(1);
#endif

#ifdef MODULE_I2C_BUS_ENABLED
    if (i2c_bus_init(PINNAME_RI, PINNAME_DCD) != I2C_BUS_OK) {
        if (first_error == 0) {
            first_error = -5;
        }
        APP_DEBUG("BOARD: i2c bus init failed\r\n");
    }
    Ql_WTD_Feed(1);

#ifdef MODULE_I2C_SCANNER_ENABLED
    if (i2c_bus_is_initialized()) {
        i2c_bus_scan();
    }
    Ql_WTD_Feed(1);
#endif
#endif

#ifdef MODULE_OLED_ENABLED
    if (oled_init() != OLED_OK) {
        if (first_error == 0) {
            first_error = -6;
        }
        APP_DEBUG("BOARD: oled init failed\r\n");
    }
    Ql_WTD_Feed(1);
#endif

#ifdef MODULE_IO_EXPANDER_ENABLED
    if (io_expander_init(0, io_expander_default_config,
                         IO_EXPANDER_DEFAULT_CONFIG_COUNT) != IO_EXPANDER_OK) {
        if (first_error == 0) {
            first_error = -7;
        }
        APP_DEBUG("BOARD: io expander init failed\r\n");
    } else {
        io_expander_print_status();

        if (io_expander_param_init() != 0 && first_error == 0) {
            APP_DEBUG("BOARD: io expander param integration failed\r\n");
            first_error = -8;
        }
    }
    Ql_WTD_Feed(1);
#endif

    return first_error;
}
