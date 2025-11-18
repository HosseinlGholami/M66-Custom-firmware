/**
 * @file    main.c
 * @brief   Main application entry point
 * @author  Hossein Gholami
 * @date    2025-11-01
 * 
 * M66 Industrial Controller - Main Application
 * Organized modular architecture with UART and Parameter storage
 */

#ifdef __CUSTOMER_CODE__

#include "custom_feature_def.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_telephony.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_system.h"
#include "ql_wtd.h"
#include "ql_gpio.h"
#include "ql_timer.h"

/* Module configuration */
#include "config/module_config.h"

/* Custom modules */
#include "uart/uart.h"
// #include "debug/restart_log.h"  /* Debug module disabled */

#ifdef MODULE_PARAM_ENABLED
#include "param/param.h"
#endif

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

/* Note: Test functions for Ql_vsnprintf removed - bug confirmed and fixed.
 * Solution: Use Ql_sprintf directly, avoid Ql_vsnprintf with va_list. */

/*============================================================================
 * Command Interface Support
 *===========================================================================*/

/* Command buffer for accumulating UART input */
static char cmd_buffer[128];
static u32 cmd_buffer_len = 0;

/*============================================================================
 * IO Expander Timer Callback
 *===========================================================================*/

#if defined(MODULE_IO_EXPANDER_ENABLED) && defined(MODULE_PARAM_ENABLED)
/* Timer ID for IO expander blink 
 * Note: Timer IDs 0-9 may be reserved by system
 * Using ID 100 to avoid conflicts
 */
#define TIMER_ID_IO_BLINK   100

/* Current blink state */
static bool g_io_blink_state = FALSE;

/**
 * @brief Timer callback for IO expander blink
 * Toggles all outputs between 0xFF and 0x00 every 1 second
 */
static void io_expander_timer_callback(u32 timer_id, void* user_data)
{
    s32 ret;
    
    APP_DEBUG("\r\n[TIMER] Callback triggered! Timer ID: %d\r\n", timer_id);
    
    /* Toggle state */
    g_io_blink_state = !g_io_blink_state;
    
    /* Set all outputs HIGH (255) or LOW (0) */
    u8 output_value = g_io_blink_state ? 0xFF : 0x00;
    
    APP_DEBUG("[TIMER] Setting io_exp1_out to: 0x%02X (%s)\r\n", 
             output_value, g_io_blink_state ? "ALL ON" : "ALL OFF");
    
    ret = param_set_int8(PARAM_IO_EXP1_OUT, output_value);
    
    if (ret == 0) {
        APP_DEBUG("[TIMER] ✅ Output toggled successfully via parameter\r\n");
    } else {
        APP_DEBUG("[TIMER] ❌ Failed to set parameter: %d\r\n", ret);
    }
}
#endif

/*============================================================================
 * IO Expander Interrupt Callback
 *===========================================================================*/

#ifdef MODULE_IO_EXPANDER_ENABLED
/**
 * @brief Callback for IO expander interrupts (DISABLED - using polling mode)
 * Called when any pin on any PCF8574 device changes state
 */
__attribute__((unused)) static void io_expander_int_handler(u8 device_id, u8 pin_states)
{
    const char* device_name = io_expander_get_device_name(device_id);
    
    APP_DEBUG("\r\n*** IO EXPANDER INTERRUPT ***\r\n");
    APP_DEBUG("Device: %d (%s)\r\n", device_id, device_name ? device_name : "Unknown");
    APP_DEBUG("Pin States: 0x%02X (", pin_states);
    
    /* Print binary representation */
    {
        u8 i;
        for (i = 0; i < 8; i++) {
            APP_DEBUG("%d", (pin_states >> i) & 1);
        }
    }
    APP_DEBUG(")\r\n");
    
    /* Print individual pin states */
    APP_DEBUG("P0-P3: %d %d %d %d\r\n",
             (pin_states >> 0) & 1, (pin_states >> 1) & 1,
             (pin_states >> 2) & 1, (pin_states >> 3) & 1);
    APP_DEBUG("P4-P7: %d %d %d %d\r\n",
             (pin_states >> 4) & 1, (pin_states >> 5) & 1,
             (pin_states >> 6) & 1, (pin_states >> 7) & 1);
    APP_DEBUG("****************************\r\n\r\n");
    
    /* Example: Update OLED display with new state */
    /* You can add custom handling here based on which pins changed */
}
#endif

/**
 * @brief UART callback for command processing
 * Accumulates characters until '!' terminator, then processes command
 */
static void uart_command_callback(u8* data, u32 len)
{
    u32 i;

    for (i = 0; i < len; i++) {
        char ch = data[i];

        /* Echo character */
        APP_DEBUG("%c", ch);

        /* Accumulate in buffer */
        if (cmd_buffer_len < sizeof(cmd_buffer) - 1) {
            cmd_buffer[cmd_buffer_len++] = ch;

            /* Check for command terminator */
            if (ch == '!') {
                cmd_buffer[cmd_buffer_len] = '\0';

                /* Process command */
                APP_DEBUG("\r\n");
                com_process_command(cmd_buffer, cmd_buffer_len);

                /* Reset buffer */
                cmd_buffer_len = 0;
            }
        } else {
            /* Buffer overflow - reset */
            APP_DEBUG("\r\nERROR: Command buffer overflow\r\n");
            cmd_buffer_len = 0;
        }
    }
}

/*============================================================================
 * Main Task
 *===========================================================================*/

void proc_main_task(s32 taskId)
{
    ST_MSG msg;
    
    /* Initialize UART for debug output and AT commands */
    /* DO NOT pre-configure GPIO - let EINT register claim pins directly */
    uart_init(UART_PORT1, 115200);
    // restart_log_set_stage(BOOT_STAGE_UART_INIT);  /* Debug disabled */
    
    /* Initialize restart logging (do this early!) */
    // restart_log_init();  /* Debug disabled */
    
    /* Print startup banner */
    APP_DEBUG("\r\n");
    APP_DEBUG("==================BASE FIRMWARE======================\r\n");
    APP_DEBUG("  M66 Industrial Controller Firmware\r\n");
    APP_DEBUG("  Modular Architecture - Clean Start\r\n");
    APP_DEBUG("  Build: %s %s\r\n", __DATE__, __TIME__);
    APP_DEBUG("========================================\r\n");
    APP_DEBUG("\r\n");
    
    /* Feed watchdog early to prevent timeout during initialization */
    Ql_WTD_Feed(1);
    
    /* Initialize parameter storage (auto-registers all params from enum) */
#ifdef MODULE_PARAM_ENABLED
    // restart_log_set_stage(BOOT_STAGE_PARAM_INIT);  /* Debug disabled */
    if (param_init() == 0) {
        APP_DEBUG("✅ Parameter storage initialized\r\n");
        APP_DEBUG("   Total parameters: %d\r\n", param_count());
    } else {
        APP_DEBUG("❌ Failed to initialize parameters\r\n");
    }
    Ql_WTD_Feed(1);  /* Feed watchdog after param init */
#endif
    
    /* Initialize GPIO module (links outputs to parameters) */
#ifdef MODULE_GPIO_ENABLED
    // restart_log_set_stage(BOOT_STAGE_GPIO_INIT);  /* Debug disabled */
    
    /* NOTE: IO Expander interrupt handling setup:
     * - PCF8574 INT pins physically connected to DTR
     * - DTR configured with EINT (external interrupt)
     * - When any PCF pin changes, INT goes LOW → EINT triggers
     * - EINT callback reads IO expander states automatically
     * - Parameters updated in real-time via interrupt
     */
    APP_DEBUG("\r\n╔════════════════════════════════════════════╗\r\n");
    APP_DEBUG("║    GPIO & IO EXPANDER INTERRUPT SETUP     ║\r\n");
    APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
    APP_DEBUG("INFO: DTR EINT configured for IO expander INT\r\n");
    APP_DEBUG("      Real-time interrupt-driven I/O monitoring\r\n\r\n");
    
    if (gpio_init() == 0) {
        APP_DEBUG("\r\n✅ GPIO module initialized\r\n");
        APP_DEBUG("   Total GPIOs: %d\r\n\r\n", gpio_get_count());
    } else {
        APP_DEBUG("❌ Failed to initialize GPIO\r\n");
    }
    
    Ql_WTD_Feed(1);  /* Feed watchdog after GPIO init */
#endif
    
    /* Initialize command interface (for UART/SMS control) */
#ifdef MODULE_COM_ENABLED
    // restart_log_set_stage(BOOT_STAGE_COM_INIT);  /* Debug disabled */
    if (com_init(NULL) == 0) {
        APP_DEBUG("✅ Command interface initialized\r\n");
        APP_DEBUG("   Type ?! for help\r\n");

        /* Register UART callback for command processing */
        uart_register_callback(uart_command_callback);
    } else {
        APP_DEBUG("❌ Failed to initialize command interface\r\n");
    }
    Ql_WTD_Feed(1);  /* Feed watchdog after COM init */
#endif
    
    /* Initialize I2C Bus (required for all I2C devices) */
#ifdef MODULE_I2C_BUS_ENABLED
    // restart_log_set_stage(BOOT_STAGE_I2C_BUS_INIT);  /* Debug disabled */
    Ql_WTD_Feed(1);  /* Feed watchdog before I2C init */
    APP_DEBUG("\r\n");
    APP_DEBUG("=== I2C Bus Initialization ===\r\n");
    if (i2c_bus_init(PINNAME_RI, PINNAME_DCD) == I2C_BUS_OK) {
        APP_DEBUG("✅ I2C bus initialized\r\n");
        Ql_WTD_Feed(1);  /* Feed watchdog after I2C init */
        
        /* Scan I2C bus to find devices */
#ifdef MODULE_I2C_SCANNER_ENABLED
        // restart_log_set_stage(BOOT_STAGE_I2C_SCAN);  /* Debug disabled */
        APP_DEBUG("\r\n");
        i2c_bus_scan();
        Ql_WTD_Feed(1);  /* Feed watchdog after I2C scan */
#endif
    } else {
        APP_DEBUG("❌ Failed to initialize I2C bus\r\n");
    }
#endif
    
    /* Initialize OLED display */
#ifdef MODULE_OLED_ENABLED
    // restart_log_set_stage(BOOT_STAGE_OLED_INIT);  /* Debug disabled */
    Ql_WTD_Feed(1);  /* Feed watchdog before OLED init */
    APP_DEBUG("\r\n");
    APP_DEBUG("=== OLED Display Initialization ===\r\n");
    if (oled_init() == OLED_OK) {
        APP_DEBUG("✅ OLED display initialized\r\n");
        APP_DEBUG("   128x64 SSD1306 @ 0x%02X\r\n", OLED_I2C_ADDR);
        
        /* Display Hello World demo */
        oled_clear();
        oled_draw_string(10, 0, "Hello World!");
        oled_draw_string(0, 16, "M66 Firmware");
        oled_draw_string(0, 32, "Build:");
        oled_draw_string(42, 32, __DATE__);
        oled_draw_rect(0, 0, 128, 64, FALSE);  /* Border */
        oled_update();
        
        APP_DEBUG("✅ 'Hello World' displayed on OLED\r\n");
    } else {
        APP_DEBUG("❌ Failed to initialize OLED display\r\n");
        APP_DEBUG("   Check I2C bus initialization\r\n");
    }
    Ql_WTD_Feed(1);  /* Feed watchdog after OLED init */
#endif
    
    /* Initialize IO Expander (PCF8574) */
#ifdef MODULE_IO_EXPANDER_ENABLED
    // restart_log_set_stage(BOOT_STAGE_IO_EXPANDER_INIT);  /* Debug disabled */
    Ql_WTD_Feed(1);  /* Feed watchdog before IO expander init */
    APP_DEBUG("\r\n");
    APP_DEBUG("=== IO Expander Initialization ===\r\n");
    /* NOTE: Interrupt handling via GPIO module EINT callback
     * - PCF8574 INT pins connected to DTR
     * - DTR EINT triggers when any PCF pin changes
     * - EINT callback reads IO expander states
     * - IO expander module stays in polling mode (doesn't manage interrupt itself)
     */
    if (io_expander_init(0,  /* 0 = polling mode, EINT handled by GPIO module */
                         io_expander_default_config, 
                         IO_EXPANDER_DEFAULT_CONFIG_COUNT) == IO_EXPANDER_OK) {
        APP_DEBUG("✅ IO Expander initialized\r\n");
        APP_DEBUG("   Devices: %d\r\n", io_expander_get_device_count());
        APP_DEBUG("   INT pins → DTR (handled by GPIO EINT)\r\n");
        Ql_WTD_Feed(1);  /* Feed watchdog after IO expander init */
        
        /* Print initial status */
        io_expander_print_status();
        
        /* Initialize parameter integration (requires param + io_expander) */
#if defined(MODULE_PARAM_ENABLED)
        if (io_expander_param_init() == 0) {
            APP_DEBUG("✅ IO Expander parameter integration enabled\r\n");
            
            /* Start blink timer - toggles all outputs every 1 second */
            APP_DEBUG("\r\n");
            APP_DEBUG("=== Starting IO Blink Timer ===\r\n");
            
            s32 ret = Ql_Timer_Register(TIMER_ID_IO_BLINK, io_expander_timer_callback, NULL);
            APP_DEBUG("Ql_Timer_Register(ID=%d) returned: %d\r\n", TIMER_ID_IO_BLINK, ret);
            
            if (ret < 0) {
                APP_DEBUG("❌ Failed to register timer: %d\r\n", ret);
            } else {
                APP_DEBUG("✅ Timer registered successfully\r\n");
                
                ret = Ql_Timer_Start(TIMER_ID_IO_BLINK, 1000, TRUE);  /* 1000ms, repeat */
                APP_DEBUG("Ql_Timer_Start(ID=%d, 1000ms, repeat=TRUE) returned: %d\r\n", 
                         TIMER_ID_IO_BLINK, ret);
                
                if (ret < 0) {
                    APP_DEBUG("❌ Failed to start timer: %d\r\n", ret);
                } else {
                    APP_DEBUG("✅ IO Blink timer started (1 second interval)\r\n");
                    APP_DEBUG("   Pattern: ALL ON (255) ↔ ALL OFF (0)\r\n");
                    APP_DEBUG("   Waiting for first callback...\r\n");
                }
            }
            APP_DEBUG("================================\r\n\r\n");
        } else {
            APP_DEBUG("❌ Failed to initialize IO Expander param integration\r\n");
        }
#endif
    } else {
        APP_DEBUG("❌ Failed to initialize IO Expander\r\n");
        APP_DEBUG("   Check I2C bus initialization\r\n");
        APP_DEBUG("   Check device addresses: 0x42, 0x4A\r\n");
    }
    Ql_WTD_Feed(1);  /* Feed watchdog after IO expander section */
#endif
    
    /* Mark boot as complete */
    // restart_log_set_stage(BOOT_STAGE_COMPLETE);  /* Debug disabled */
    // restart_log_boot_complete();  /* Debug disabled */
    
    APP_DEBUG("\r\n");
    APP_DEBUG("System ready. Type commands or waiting for events...\r\n");
    APP_DEBUG("Examples: S,4,1! or G,4! or L!\r\n");
    APP_DEBUG("IO Expander: S,io_exp1_out,255! (outputs) or G,io_exp0_in! (inputs)\r\n");
    APP_DEBUG("\r\n");
    
    /* Main message loop */
    while(TRUE)
    {
        Ql_OS_GetMessage(&msg);
        
        /* Feed watchdog to prevent reset */
        Ql_WTD_Feed(1);
        
        /* Message loop is stable - debug logging can be enabled if needed */
        
        switch(msg.message)
        {
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();
            break;
            
        case MSG_ID_URC_INDICATION:
            switch (msg.param1)
            {
            case URC_SYS_INIT_STATE_IND:
                APP_DEBUG("<-- Sys Init Status: %d -->\r\n", msg.param2);
                break;
                
            case URC_SIM_CARD_STATE_IND:
                APP_DEBUG("<-- SIM Card Status: %d -->\r\n", msg.param2);
                break;
                
            case URC_GSM_NW_STATE_IND:
                APP_DEBUG("<-- GSM Network Status: %d -->\r\n", msg.param2);
                break;
                
            case URC_GPRS_NW_STATE_IND:
                APP_DEBUG("<-- GPRS Network Status: %d -->\r\n", msg.param2);
                break;
                
            case URC_CFUN_STATE_IND:
                APP_DEBUG("<-- CFUN Status: %d -->\r\n", msg.param2);
                break;
                
            case URC_COMING_CALL_IND:
                {
                    ST_ComingCall* call_info = (ST_ComingCall*)msg.param2;
                    APP_DEBUG("<-- Incoming call: %s, type:%d -->\r\n", 
                             call_info->phoneNumber, call_info->type);
                }
                break;
                
            case URC_CALL_STATE_IND:
                APP_DEBUG("<-- Call state: %d -->\r\n", msg.param2);
                break;
                
            case URC_NEW_SMS_IND:
                APP_DEBUG("<-- New SMS arrived: index=%d -->\r\n", msg.param2);
                break;
                
            case URC_MODULE_VOLTAGE_IND:
                APP_DEBUG("<-- Battery Voltage: type=%d -->\r\n", msg.param2);
                break;
                
            default:
                APP_DEBUG("<-- URC: type=%d -->\r\n", msg.param1);
                break;
            }
            break;
            
        default:
            break;
        }
    }
}

#endif /* __CUSTOMER_CODE__ */
