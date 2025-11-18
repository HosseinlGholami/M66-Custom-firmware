/**
 * @file    io_expander_param.c
 * @brief   IO Expander Parameter Integration Implementation
 * @author  Hossein Gholami
 * @date    2025-11-17
 */

#include "io_expander_param.h"
#include "io_expander.h"
#include "../param/param.h"
#include "../uart/uart.h"

/*============================================================================
 * Configuration
 *===========================================================================*/

/* TEMPORARY: Using Device 0 (0x42) for both inputs AND outputs
 * Change this back when Device 1 (0x4A) is connected:
 *   #define IO_EXP_INPUT_DEVICE     0
 *   #define IO_EXP_OUTPUT_DEVICE    1
 */
#define IO_EXP_INPUT_DEVICE     0
#define IO_EXP_OUTPUT_DEVICE    0   /* TEMP: Same as input device for testing */

/*============================================================================
 * Private Data
 *===========================================================================*/

static bool g_initialized = FALSE;
static u8 g_last_input_state = 0xFF;  /* Track input changes */

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Parameter change callback for IO expander outputs
 * 
 * Called automatically when PARAM_IO_EXP1_OUT changes.
 * Writes the new value to Device 1 (0x4A - outputs).
 */
static void io_expander_param_callback(ParamKey_e key, 
                                       const void* old_val, 
                                       const void* new_val, 
                                       ParamType_e type)
{
    s32 ret;
    u8 new_state;
    u8 old_state;
    
    APP_DEBUG("\r\n[IO_EXP_CALLBACK] Triggered! Key=%d, Type=%d\r\n", key, type);
    
    /* Only handle our parameter */
    if (key != PARAM_IO_EXP1_OUT) {
        APP_DEBUG("[IO_EXP_CALLBACK] Not our parameter (expected %d), ignoring\r\n", PARAM_IO_EXP1_OUT);
        return;
    }
    
    APP_DEBUG("[IO_EXP_CALLBACK] Handling PARAM_IO_EXP1_OUT\r\n");
    
    /* Get new output value */
    if (type != PARAM_TYPE_INT8) {
        APP_DEBUG("[IO_EXP_CALLBACK] ERROR - Expected INT8 type, got %d\r\n", type);
        return;
    }
    
    old_state = old_val ? *(u8*)old_val : 0;
    new_state = *(u8*)new_val;
    
    APP_DEBUG("[IO_EXP_CALLBACK] Value change: 0x%02X -> 0x%02X\r\n", old_state, new_state);
    
    /* Write to IO expander Device 1 (0x4A - outputs) */
    ret = io_expander_write_port(IO_EXP_OUTPUT_DEVICE, new_state);
    if (ret < 0) {
        APP_DEBUG("[IO_EXP_CALLBACK] ❌ Failed to write to PCF8574: %d\r\n", ret);
        return;
    }
    
    /* Get device info for debug message */
    const char* dev_name = io_expander_get_device_name(IO_EXP_OUTPUT_DEVICE);
    APP_DEBUG("[IO_EXP_CALLBACK] ✅ Device %d (%s) outputs updated to 0x%02X\r\n", 
             IO_EXP_OUTPUT_DEVICE, dev_name ? dev_name : "Unknown", new_state);
}

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

s32 io_expander_param_init(void)
{
    s32 ret;
    
    if (g_initialized) {
        APP_DEBUG("IO_EXP_PARAM: Already initialized\r\n");
        return 0;
    }
    
    APP_DEBUG("\r\n=== IO Expander Parameter Integration ===\r\n");
    
    /* Verify dependencies */
    if (!io_expander_is_initialized()) {
        APP_DEBUG("IO_EXP_PARAM: ❌ IO Expander not initialized!\r\n");
        return -1;
    }
    
    /* Check how many devices are available */
    u8 device_count = io_expander_get_device_count();
    APP_DEBUG("IO_EXP_PARAM: Detected %d IO expander device(s)\r\n", device_count);
    
    if (device_count < 2) {
        APP_DEBUG("IO_EXP_PARAM: ⚠️ WARNING: Expected 2 devices, found %d\r\n", device_count);
        APP_DEBUG("IO_EXP_PARAM: Check I2C connections: 0x42 and 0x4A\r\n");
    }
    
    /* Configure Device 0 (0x42) as inputs (all HIGH for pull-ups) */
    APP_DEBUG("IO_EXP_PARAM: Configuring Device 0 (0x42) as INPUTS\r\n");
    ret = io_expander_write_port(IO_EXP_INPUT_DEVICE, 0xFF);  /* All inputs (high-Z) */
    if (ret < 0) {
        APP_DEBUG("IO_EXP_PARAM: ❌ Failed to configure Device 0\r\n");
        return -2;
    }
    
    /* Configure Device 1 (0x4A) as outputs (all LOW initially) */
    APP_DEBUG("IO_EXP_PARAM: Configuring Device 1 (0x4A) as OUTPUTS\r\n");
    APP_DEBUG("IO_EXP_PARAM: Calling io_expander_write_port(device=%d, value=0x00)...\r\n", IO_EXP_OUTPUT_DEVICE);
    ret = io_expander_write_port(IO_EXP_OUTPUT_DEVICE, 0x00);  /* All outputs LOW */
    APP_DEBUG("IO_EXP_PARAM: io_expander_write_port returned: %d\r\n", ret);
    
    if (ret < 0) {
        APP_DEBUG("IO_EXP_PARAM: ⚠️ WARNING: Failed to configure Device 1\r\n");
        APP_DEBUG("IO_EXP_PARAM: This usually means:\r\n");
        APP_DEBUG("IO_EXP_PARAM:   1. Device not connected to I2C bus\r\n");
        APP_DEBUG("IO_EXP_PARAM:   2. Wrong I2C address (expecting 0x4A)\r\n");
        APP_DEBUG("IO_EXP_PARAM:   3. I2C wiring issue (SDA/SCL)\r\n");
        APP_DEBUG("IO_EXP_PARAM: Continuing anyway to register callbacks...\r\n");
        /* Don't return error - continue to register callbacks for testing */
    } else {
        APP_DEBUG("IO_EXP_PARAM: ✅ Device 1 configured successfully\r\n");
    }
    
    /* Register parameter callback for output control */
    APP_DEBUG("IO_EXP_PARAM: Registering callback for PARAM_IO_EXP1_OUT (key=%d)...\r\n", PARAM_IO_EXP1_OUT);
    ret = param_set_callback(PARAM_IO_EXP1_OUT, io_expander_param_callback);
    APP_DEBUG("IO_EXP_PARAM: param_set_callback() returned: %d\r\n", ret);
    if (ret != 0) {
        APP_DEBUG("IO_EXP_PARAM: ❌ Failed to register callback, ret=%d\r\n", ret);
        return -4;
    }
    APP_DEBUG("IO_EXP_PARAM: ✅ Callback registered successfully\r\n");
    
    /* Initialize output parameter to 0x00 */
    APP_DEBUG("IO_EXP_PARAM: Setting initial value to 0x00...\r\n");
    ret = param_set_int8(PARAM_IO_EXP1_OUT, 0x00);
    APP_DEBUG("IO_EXP_PARAM: param_set_int8() returned: %d\r\n", ret);
    
    /* Verify the value was set */
    s8 verify_value;
    ret = param_get_int8(PARAM_IO_EXP1_OUT, &verify_value);
    APP_DEBUG("IO_EXP_PARAM: Verification read: ret=%d, value=%d (0x%02X)\r\n", ret, verify_value, (u8)verify_value);
    
    /* Read initial input state from Device 0 */
    ret = io_expander_read_port(IO_EXP_INPUT_DEVICE, &g_last_input_state);
    if (ret < 0) {
        APP_DEBUG("IO_EXP_PARAM: ⚠️ Failed to read initial inputs\r\n");
        g_last_input_state = 0xFF;
    } else {
        param_set_int8(PARAM_IO_EXP0_IN, g_last_input_state);
        APP_DEBUG("IO_EXP_PARAM: Initial input state: 0x%02X\r\n", g_last_input_state);
    }
    
    g_initialized = TRUE;
    
    APP_DEBUG("✅ IO Expander parameter integration initialized\r\n");
    
    /* Show actual device assignments */
    const char* input_dev = io_expander_get_device_name(IO_EXP_INPUT_DEVICE);
    const char* output_dev = io_expander_get_device_name(IO_EXP_OUTPUT_DEVICE);
    
    APP_DEBUG("   INPUT device:  %d (%s) - updates 'io_exp0_in'\r\n", 
             IO_EXP_INPUT_DEVICE, input_dev ? input_dev : "Unknown");
    APP_DEBUG("   OUTPUT device: %d (%s) - controlled by 'io_exp1_out'\r\n", 
             IO_EXP_OUTPUT_DEVICE, output_dev ? output_dev : "Unknown");
    
    if (IO_EXP_INPUT_DEVICE == IO_EXP_OUTPUT_DEVICE) {
        APP_DEBUG("   ⚠️ NOTE: Same device used for both input AND output (testing mode)\r\n");
    }
    APP_DEBUG("\r\n");
    
    return 0;
}

s32 io_expander_param_update(void)
{
    s32 ret;
    u8 current_state;
    u8 changed_bits;
    u8 i;
    
    if (!g_initialized) {
        return -1;
    }
    
    /* Read current input state from Device 1 */
    ret = io_expander_read_port(IO_EXP_INPUT_DEVICE, &current_state);
    if (ret < 0) {
        return -2;
    }
    
    /* Check if any inputs changed */
    if (current_state != g_last_input_state) {
        changed_bits = current_state ^ g_last_input_state;
        
        /* Update parameter */
        param_set_int8(PARAM_IO_EXP0_IN, current_state);
        
        /* Debug: Show which pins changed */
        APP_DEBUG("IO_EXP_PARAM: Device 0 (0x42) inputs changed: 0x%02X -> 0x%02X\r\n",
                 g_last_input_state, current_state);
        
        for (i = 0; i < 8; i++) {
            if (changed_bits & (1 << i)) {
                bool new_level = (current_state & (1 << i)) != 0;
                APP_DEBUG("  P%d: %s\r\n", i, new_level ? "HIGH" : "LOW");
            }
        }
        
        g_last_input_state = current_state;
    }
    
    return 0;
}

bool io_expander_param_is_initialized(void)
{
    return g_initialized;
}

