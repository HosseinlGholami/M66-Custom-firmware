/**
 * @file    main.c
 * @brief   Main application entry point
 */

#ifdef __CUSTOMER_CODE__

#include "custom_feature_def.h"
#include "ril.h"
#include "ril_telephony.h"
#include "ql_system.h"
#include "ql_timer.h"
#include "ql_wtd.h"

#include "config/module_config.h"
#include "logic/logic.h"
#include "param/param.h"
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

static void main_print_banner(void)
{
    APP_DEBUG("\r\n");
    APP_DEBUG("==================BASE FIRMWARE======================\r\n");
    APP_DEBUG("  M66 Industrial Controller Firmware\r\n");
    APP_DEBUG("  Modular Architecture - Clean Start\r\n");
    APP_DEBUG("  Build: %s %s\r\n", __DATE__, __TIME__);
    APP_DEBUG("========================================\r\n");
    APP_DEBUG("\r\n");
}

void proc_main_task(s32 taskId)
{
    ST_MSG msg;

    (void)taskId;

    uart_init(UART_PORT1, 115200);
    main_print_banner();
    Ql_WTD_Feed(1);

    if (param_init() != 0) {
        APP_DEBUG("❌ Failed to initialize parameters\r\n");
    }
    Ql_WTD_Feed(1);

    logic_init();

#ifdef MODULE_I2C_BUS_ENABLED
    APP_DEBUG("\r\n=== I2C Bus Initialization ===\r\n");
    if (i2c_bus_init(PINNAME_RI, PINNAME_DCD) != I2C_BUS_OK) {
        APP_DEBUG("❌ Failed to initialize I2C bus\r\n");
    }
    Ql_WTD_Feed(1);

#ifdef MODULE_I2C_SCANNER_ENABLED
    if (i2c_bus_is_initialized()) {
        i2c_bus_scan();
    }
    Ql_WTD_Feed(1);
#endif
#endif

#ifdef MODULE_GPIO_ENABLED
    APP_DEBUG("\r\n=== GPIO Initialization ===\r\n");
    if (gpio_init() != 0) {
        APP_DEBUG("❌ Failed to initialize GPIO\r\n");
    }
    Ql_WTD_Feed(1);
#endif

#ifdef MODULE_COM_ENABLED
    if (com_init(NULL) == 0) {
        uart_register_callback(logic_uart_callback);
    } else {
        APP_DEBUG("❌ Failed to initialize command interface\r\n");
    }
    Ql_WTD_Feed(1);
#endif

    logic_start();

#ifdef MODULE_OLED_ENABLED
    APP_DEBUG("\r\n=== OLED Initialization ===\r\n");
    if (oled_init() != OLED_OK) {
        APP_DEBUG("❌ Failed to initialize OLED display\r\n");
    }
    Ql_WTD_Feed(1);
#endif

    logic_print_ready();

    while (TRUE) {
        Ql_OS_GetMessage(&msg);
        Ql_WTD_Feed(1);
        logic_handle_message(&msg);
    }
}

#endif /* __CUSTOMER_CODE__ */
