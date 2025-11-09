/**

 * @file    i2c_scanner.c
 * @brief   I2C Bus Scanner Implementation
 * @author  Hossein Gholami
 * @date    2025-11-03
 * 
 * Based on Arduino I2C Scanner by Krodal:
 * https://playground.arduino.cc/Main/I2cScanner/
 */

#include "i2c_scanner.h"
#include "ql_iic.h"
#include "ql_system.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "../uart/uart.h"  /* For APP_DEBUG */

/*============================================================================
 * Private State
 *===========================================================================*/
static u32 g_i2c_channel = 0;
static bool g_i2c_initialized = FALSE;
static Enum_PinName g_pinSCL = PINNAME_END;
static Enum_PinName g_pinSDA = PINNAME_END;

/*============================================================================
 * Device Type Database
 *===========================================================================*/
typedef struct {
    u8 addr;
    const char* name;
} I2CDevice_t;

static const I2CDevice_t known_devices[] = {
    /* OLED Displays */
    {0x3C, "SSD1306 OLED (128x64)"},
    {0x3D, "SSD1306 OLED (alternate)"},
    
    /* Sensors */
    {0x68, "MPU6050 IMU / DS3231 RTC"},
    {0x69, "MPU6050 IMU (alternate)"},
    {0x76, "BMP280/BME280 Pressure"},
    {0x77, "BMP280/BME280 (alternate)"},
    {0x48, "ADS1115 ADC"},
    {0x49, "ADS1115 ADC (alternate)"},
    {0x40, "PCA9685 PWM / Si7021 Temp"},
    {0x44, "SHT31 Temperature"},
    {0x5A, "MLX90614 IR Temperature"},
    
    /* EEPROM */
    {0x50, "AT24C EEPROM"},
    {0x51, "AT24C EEPROM"},
    {0x52, "AT24C EEPROM"},
    {0x53, "AT24C EEPROM"},
    
    /* I/O Expanders */
    {0x20, "MCP23017 I/O Expander"},
    {0x21, "MCP23017 I/O Expander"},
    {0x38, "PCF8574 I/O Expander"},
    
    /* End marker */
    {0x00, NULL}
};

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Get device name from address
 */
static const char* get_device_name(u8 addr)
{
    u8 i;
    for (i = 0; known_devices[i].name != NULL; i++) {
        if (known_devices[i].addr == addr) {
            return known_devices[i].name;
        }
    }
    return NULL;
}

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

s32 i2c_scanner_init(Enum_PinName pinSCL, Enum_PinName pinSDA, u32 channel)
{
    s32 ret;
    
    APP_DEBUG("\r\n");
    APP_DEBUG("╔════════════════════════════════════════════╗\r\n");
    APP_DEBUG("║      I2C SCANNER - INITIALIZATION          ║\r\n");
    APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
    APP_DEBUG("\r\n");
    APP_DEBUG("Channel: %d\r\n", channel);
    APP_DEBUG("SCL Pin: %d\r\n", pinSCL);
    APP_DEBUG("SDA Pin: %d\r\n", pinSDA);
    APP_DEBUG("\r\n");
    
    /* Save configuration for later reinit */
    g_i2c_channel = channel;
    g_pinSCL = pinSCL;
    g_pinSDA = pinSDA;
    
    /* Initialize I2C */
    ret = Ql_IIC_Init(channel, pinSCL, pinSDA, FALSE);  /* FALSE = SIMULATED I2C */
    if (ret < 0) {
        APP_DEBUG("❌ I2C Init failed: %d\r\n", ret);
        return ret;
    }
    
    APP_DEBUG("✅ I2C initialized successfully\r\n");
    g_i2c_initialized = TRUE;
    
    return 0;
}

u8 i2c_scanner_scan(void)
{
    u8 addr;
    s32 ret_config, ret_write_read, ret;
    u8 reg_addr = 0x00;
    u8 read_data[4];  /* Buffer to read multiple bytes for validation */
    u8 found_count = 0;
    u8 found_addresses[127];  /* Store found addresses for summary */
    u8 configured_slaves = 0;  /* Counter for configured slaves (max 6 per channel) */
    const char* device_name;
    u8 i;
    bool is_valid_device;
    
    if (!g_i2c_initialized) {
        APP_DEBUG("❌ I2C not initialized! Call i2c_scanner_init() first.\r\n");
        return 0;
    }
    
    APP_DEBUG("\r\n");
    APP_DEBUG("╔════════════════════════════════════════════╗\r\n");
    APP_DEBUG("║         I2C BUS SCAN - STARTING            ║\r\n");
    APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
    APP_DEBUG("\r\n");
    APP_DEBUG("Scanning 7-bit addresses 0x01 to 0x7F...\r\n");
    APP_DEBUG("Method: Write + Read validation (smart detection)\r\n");
    APP_DEBUG("Note: Channel reinit every 6 configs (slave limit)\r\n");
    APP_DEBUG("Note: Validates read data to filter false positives\r\n");
    APP_DEBUG("\r\n");
    
    Ql_memset(found_addresses, 0, sizeof(found_addresses));
    
    for (addr = 0x01; addr < 0x80; addr++) {
        Ql_Sleep(100);

        /* 
         * Reinitialize channel every 6 configured slaves to avoid 
         * QL_RET_ERR_IIC_SLAVE_TOO_MANY (-310) error
         */
        if (configured_slaves >= 6) {
            APP_DEBUG("  🔄 Reinitializing channel (slave limit reached)...\r\n");
            Ql_IIC_Uninit(g_i2c_channel);
            Ql_Sleep(50);
            ret = Ql_IIC_Init(g_i2c_channel, g_pinSCL, g_pinSDA, FALSE);
            if (ret < 0) {
                APP_DEBUG("❌ Reinit failed: %d. Stopping scan.\r\n", ret);
                return found_count;
            }
            configured_slaves = 0;
        }
        
        /* 
         * Step 1: Configure this address as a slave
         * Note: Ql_IIC_Config always succeeds - it just sets up the address
         */
        ret_config = Ql_IIC_Config(g_i2c_channel, TRUE, addr, 300);
        
        if (ret_config >= 0) {

            /* Config succeeded - now we have a slave slot configured */
            configured_slaves++;  /* Increment slave counter */
            
            /* 
             * Step 2: Smart detection - Write register address then read data
             * Simulated I2C doesn't detect NACK properly, so we validate the response
             * 
             * Strategy:
             * 1. Try to read register 0x00 from device
             * 2. Check if read succeeds AND data looks valid
             * 3. Invalid patterns: all 0xFF (floating), all 0x00 (no response), all same
             */
            Ql_memset(read_data, 0, sizeof(read_data));
            ret_write_read = Ql_IIC_Write_Read(g_i2c_channel, addr, &reg_addr, 1, read_data, 4);
            
            is_valid_device = FALSE;
            
            if (ret_write_read >= 0) {
                /* Read succeeded - now validate the data */
                
                /* Check for invalid patterns that indicate no device */
                bool all_0xff = (read_data[0] == 0xFF && read_data[1] == 0xFF && 
                                 read_data[2] == 0xFF && read_data[3] == 0xFF);
                bool all_0x00 = (read_data[0] == 0x00 && read_data[1] == 0x00 && 
                                 read_data[2] == 0x00 && read_data[3] == 0x00);
                bool all_same = (read_data[0] == read_data[1] && 
                                 read_data[1] == read_data[2] && 
                                 read_data[2] == read_data[3]);
                
                /* Device likely exists if data isn't all the same or all 0xFF */
                if (!all_0xff && !(all_0x00 && all_same)) {
                    is_valid_device = TRUE;
                }
                
                /* Special case: Some devices legitimately have 0x00 or repeated values */
                /* If we're in the common I2C device range (0x20-0x77), be more lenient */
                if (!is_valid_device && addr >= 0x20 && addr <= 0x77) {
                    /* Accept any non-0xFF response in common device range */
                    if (!all_0xff) {
                        is_valid_device = TRUE;
                    }
                }
            }
            
            if (is_valid_device) {
                /* Valid device detected! */
                APP_DEBUG("✓ Device at 0x%02X", addr);
                found_addresses[found_count] = addr;  /* Store address */
                found_count++;
                
                /* Check if it's a known device */
                device_name = get_device_name(addr);
                if (device_name != NULL) {
                    APP_DEBUG(" - %s", device_name);
                }
                APP_DEBUG("\r\n");
                
                /* Delay between detections (allow device to stabilize) */
                Ql_Sleep(50);
            }
        } else {
            /* Device not found or error occurred */
            switch (ret_config) {
                case QL_RET_ERR_IIC_SLAVE_NOT_FOUND:  /* -308 */
                    /* This is normal - no device at this address */
                    /* Don't print anything to avoid spam */
                    break;
                    
                case QL_RET_ERR_IIC_SLAVE_TOO_MANY:  /* -310 */
                    /* Slave limit reached - reinit and retry this address */
                    APP_DEBUG("⚠ 0x%02X: Slave limit reached (ret:%d), reinitializing...\r\n", addr, ret_config);
                    Ql_IIC_Uninit(g_i2c_channel);
                    Ql_Sleep(50);
                    ret = Ql_IIC_Init(g_i2c_channel, g_pinSCL, g_pinSDA, FALSE);
                    if (ret < 0) {
                        APP_DEBUG("❌ Reinit failed: %d. Stopping scan.\r\n", ret);
                        return found_count;
                    }
                    configured_slaves = 0;
                    addr--;  /* Retry this address */
                    break;
                    
                case QL_RET_ERR_PARAM:  /* -1 */
                    APP_DEBUG("⚠ 0x%02X: Invalid parameter (ret:%d)\r\n", addr, ret_config);
                    break;
                    
                case QL_RET_ERR_CHANNEL_NOT_FOUND:  /* -307 */
                    APP_DEBUG("❌ 0x%02X: Channel not found (ret:%d)\r\n", addr, ret_config);
                    break;
                    
                case QL_RET_ERR_I2CHWFAILED:  /* -34 */
                    APP_DEBUG("❌ 0x%02X: Hardware I2C failed (ret:%d)\r\n", addr, ret_config);
                    break;
                    
                case QL_RET_ERR_CHANNEL_OUTRANGE:  /* -305 */
                    APP_DEBUG("❌ 0x%02X: Channel out of range (ret:%d)\r\n", addr, ret_config);
                    break;
                    
                case QL_RET_ERR_FULLI2CBUS:  /* -21 */
                    APP_DEBUG("❌ 0x%02X: I2C bus full (ret:%d)\r\n", addr, ret_config);
                    break;
                    
                default:
                    /* Unknown error - show it for debugging */
                    APP_DEBUG("⚠ 0x%02X: Unknown error (ret:%d)\r\n", addr, ret_config);
                    break;
            }
        }
        
        /* Progress indicator every 32 addresses */
        if ((addr & 0x1F) == 0x1F) {
            APP_DEBUG("  ...scanned up to 0x%02X\r\n", addr);
        }
    }
    
    /* Summary */
    APP_DEBUG("\r\n");
    APP_DEBUG("╔════════════════════════════════════════════╗\r\n");
    if (found_count == 0) {
        APP_DEBUG("║   ❌ NO DEVICES FOUND                      ║\r\n");
        APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
        APP_DEBUG("\r\n");
        APP_DEBUG("⚠️  I2C BUS IS EMPTY OR NOT WORKING\r\n");
        APP_DEBUG("\r\n");
        APP_DEBUG("Troubleshooting Checklist:\r\n");
        APP_DEBUG("\r\n");
        APP_DEBUG("1. Check Wiring:\r\n");
        APP_DEBUG("   • SCL and SDA pins correctly connected?\r\n");
        APP_DEBUG("   • No loose wires?\r\n");
        APP_DEBUG("   • Correct pin numbers in code?\r\n");
        APP_DEBUG("\r\n");
        APP_DEBUG("2. Check Power:\r\n");
        APP_DEBUG("   • Device powered (VCC to 3.3V)?\r\n");
        APP_DEBUG("   • Common ground (GND) connected?\r\n");
        APP_DEBUG("   • NOT using 5V on 3.3V device?\r\n");
        APP_DEBUG("\r\n");
        APP_DEBUG("3. Pull-up Resistors:\r\n");
        APP_DEBUG("   • Most I2C devices need pull-ups\r\n");
        APP_DEBUG("   • Try 4.7kΩ from SCL to 3.3V\r\n");
        APP_DEBUG("   • Try 4.7kΩ from SDA to 3.3V\r\n");
        APP_DEBUG("\r\n");
        APP_DEBUG("4. Test Device:\r\n");
        APP_DEBUG("   • Does it work with Arduino?\r\n");
        APP_DEBUG("   • What address does Arduino see?\r\n");
        APP_DEBUG("   • Is device damaged?\r\n");
    } else {
        APP_DEBUG("║   ✅ FOUND %d DEVICE(S) ON BUS             ║\r\n", found_count);
        APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
        APP_DEBUG("\r\n");
        APP_DEBUG("✅ I2C communication working!\r\n");
        APP_DEBUG("\r\n");
        APP_DEBUG("Found device(s) at address:\r\n");
        for (i = 0; i < found_count; i++) {
            APP_DEBUG("  • 0x%02X", found_addresses[i]);
            device_name = get_device_name(found_addresses[i]);
            if (device_name != NULL) {
                APP_DEBUG(" (%s)", device_name);
            }
            APP_DEBUG("\r\n");
        }
        APP_DEBUG("\r\n");
        APP_DEBUG("Use these addresses in your code.\r\n");
    }
    APP_DEBUG("\r\n");
    
    return found_count;
}

void i2c_scanner_uninit(void)
{
    if (g_i2c_initialized) {
        Ql_IIC_Uninit(g_i2c_channel);
        g_i2c_initialized = FALSE;
        APP_DEBUG("[I2C] Scanner uninitialized\r\n");
    }
}

