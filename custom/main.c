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

/* Custom modules */
#include "uart/uart.h"
#include "param/param.h"
#include "gpio/gpio.h"
#include "com/com.h"

/* Note: Test functions for Ql_vsnprintf removed - bug confirmed and fixed.
 * Solution: Use Ql_sprintf directly, avoid Ql_vsnprintf with va_list. */

/*============================================================================
 * Command Interface Support
 *===========================================================================*/

/* Command buffer for accumulating UART input */
static char cmd_buffer[128];
static u32 cmd_buffer_len = 0;

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
    uart_init(UART_PORT1, 115200);
    
    /* Print startup banner */
    APP_DEBUG("\r\n");
    APP_DEBUG("==================BASE FIRMWARE======================\r\n");
    APP_DEBUG("  M66 Industrial Controller Firmware\r\n");
    APP_DEBUG("  Modular Architecture - Clean Start\r\n");
    APP_DEBUG("  Build: %s %s\r\n", __DATE__, __TIME__);
    APP_DEBUG("========================================\r\n");
    APP_DEBUG("\r\n");
    
    /* Initialize parameter storage (auto-registers all params from enum) */
    if (param_init() == 0) {
        APP_DEBUG("✅ Parameter storage initialized\r\n");
        APP_DEBUG("   Total parameters: %d\r\n", param_count());
    } else {
        APP_DEBUG("❌ Failed to initialize parameters\r\n");
    }
    
    /* Initialize GPIO module (links outputs to parameters) */
    if (gpio_init() == 0) {
        APP_DEBUG("✅ GPIO module initialized\r\n");
        APP_DEBUG("   Total GPIOs: %d\r\n", gpio_get_count());
    } else {
        APP_DEBUG("❌ Failed to initialize GPIO\r\n");
    }
    
    /* Initialize command interface (for UART/SMS control) */
    if (com_init(NULL) == 0) {
        APP_DEBUG("✅ Command interface initialized\r\n");
        APP_DEBUG("   Type ?! for help\r\n");
        
        /* Register UART callback for command processing */
        uart_register_callback(uart_command_callback);
    } else {
        APP_DEBUG("❌ Failed to initialize command interface\r\n");
    }
    
    APP_DEBUG("\r\n");
    APP_DEBUG("System ready. Type commands or waiting for events...\r\n");
    APP_DEBUG("Examples: S,4,1! or G,4! or L!\r\n");
    APP_DEBUG("\r\n");
    
    /* Main message loop */
    while(TRUE)
    {
        Ql_OS_GetMessage(&msg);
        
        switch(msg.message)
        {
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();
            
            /* Example: Test parameter system with enum keys */
            APP_DEBUG("\r\n=== Testing Parameter System ===\r\n");
            
            /* Set persistent parameters (will be saved to NVRAM) */
            param_set_string(PARAM_APN, "internet");
            param_set_string(PARAM_MQTT_HOST, "mqtt.example.com");
            param_set_int16(PARAM_MQTT_PORT, 1883);
            param_set_string(PARAM_DEVICE_ID, "M66_001");
            
            /* Set RAM-only parameters (fast, no NVRAM write) */
            param_set_int8(PARAM_IO_STATE, 0x0F);
            param_set_int16(PARAM_SENSOR_TEMP, 2543);  /* 25.43°C * 100 */
            param_set_int8(PARAM_NET_RSSI, -72);
            param_set_int32(PARAM_TASK_COUNTER, 12345);
            param_set_int32(PARAM_GPS_LAT, 37774930);  /* 37.77493° * 1e6 */
            param_set_int32(PARAM_GPS_LON, -122419420); /* -122.41942° * 1e6 */
            
            /* Commit only saves dirty+persistent params to NVRAM */
            s32 saved = param_commit();
            APP_DEBUG("✅ Committed %d persistent parameters to NVRAM\r\n", saved);
            
            /* Display all parameters */
            param_print_all();
            
            /* Example: Fast read operations (type-safe with enums!) */
            s8 io_state;
            s16 temp;
            s32 counter;
            s32 lat, lon;
            char mqtt_host[256];
            
            param_get_int8(PARAM_IO_STATE, &io_state);
            param_get_int16(PARAM_SENSOR_TEMP, &temp);
            param_get_int32(PARAM_TASK_COUNTER, &counter);
            param_get_int32(PARAM_GPS_LAT, &lat);
            param_get_int32(PARAM_GPS_LON, &lon);
            param_get_string(PARAM_MQTT_HOST, mqtt_host, sizeof(mqtt_host));
            
            APP_DEBUG("\r\n--- Fast Read Test (Enum-Based) ---\r\n");
            APP_DEBUG("IO State: 0x%02X\r\n", io_state);
            APP_DEBUG("Temperature: %d.%02d°C\r\n", temp/100, temp%100);
            APP_DEBUG("Counter: %d\r\n", counter);
            APP_DEBUG("GPS: %d.%06d, %d.%06d\r\n", 
                     lat/1000000, lat%1000000, lon/1000000, lon%1000000);
            APP_DEBUG("MQTT Host: %s\r\n", mqtt_host);
            APP_DEBUG("\r\n");
            
            /* ===  GPIO Control via Parameters (AUTOMATIC!) === */
            APP_DEBUG("=== Testing GPIO Control via Parameters ===\r\n");
            APP_DEBUG("GPIO outputs are AUTOMATICALLY controlled by parameters!\r\n");
            gpio_print_status();
            
            /* Demonstrate automatic GPIO control */
            APP_DEBUG("Changing PARAM_IO_STATE will automatically update linked GPIO...\r\n");
            Ql_Sleep(1000);
            
            APP_DEBUG("Setting PARAM_IO_STATE = 1 (LED ON)\r\n");
            param_set_int8(PARAM_IO_STATE, 1);
            Ql_Sleep(1000);
            
            APP_DEBUG("Setting PARAM_IO_STATE = 0 (LED OFF)\r\n");
            param_set_int8(PARAM_IO_STATE, 0);
            Ql_Sleep(1000);
            
            APP_DEBUG("Toggling LED 5 times...\r\n");
            {
                int i;
                for (i = 0; i < 5; i++) {
                    param_set_int8(PARAM_IO_STATE, 1);
                    Ql_Sleep(200);
                    param_set_int8(PARAM_IO_STATE, 0);
                    Ql_Sleep(200);
                }
            }
            
            APP_DEBUG("✅ GPIO control via parameters working!\r\n");
            APP_DEBUG("Note: Change ANY linked parameter, GPIO updates automatically!\r\n\r\n");
            
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
