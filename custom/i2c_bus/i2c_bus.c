/**
 * @file    i2c_bus.c
 * @brief   I2C Bus Controller Implementation
 * @author  Hossein Gholami
 * @date    2025-11-17
 */

#include "i2c_bus.h"
#include "ql_iic.h"
#include "ql_system.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "../uart/uart.h"

/*============================================================================
 * Private State
 *===========================================================================*/

static bool g_initialized = FALSE;
static Enum_PinName g_pinSCL = PINNAME_END;
static Enum_PinName g_pinSDA = PINNAME_END;
static u8 g_configured_devices = 0;

/* Device registry */
#define MAX_REGISTERED_DEVICES  16
static I2cDeviceInfo_t g_devices[MAX_REGISTERED_DEVICES];

/*============================================================================
 * Known Device Database
 *===========================================================================*/

typedef struct {
    u8 addr;
    const char* name;
} KnownDevice_t;

static const KnownDevice_t g_known_devices[] = {
    /* OLED Displays */
    {0x3C, "SSD1306 OLED (128x64)"},
    {0x3D, "SSD1306 OLED (alternate)"},
    {0x78, "SSD1306 OLED (8-bit addr)"},
    
    /* I/O Expanders */
    {0x20, "PCF8574 I/O Expander"},
    {0x21, "PCF8574 I/O Expander"},
    {0x22, "PCF8574 I/O Expander"},
    {0x23, "PCF8574 I/O Expander"},
    {0x24, "PCF8574 I/O Expander"},
    {0x25, "PCF8574 I/O Expander"},
    {0x26, "PCF8574 I/O Expander"},
    {0x27, "PCF8574 I/O Expander"},
    {0x38, "PCF8574A I/O Expander"},
    {0x39, "PCF8574A I/O Expander"},
    {0x3A, "PCF8574A I/O Expander"},
    {0x3B, "PCF8574A I/O Expander"},
    {0x3C, "PCF8574A I/O Expander"},
    {0x3D, "PCF8574A I/O Expander"},
    {0x3E, "PCF8574A I/O Expander"},
    {0x3F, "PCF8574A I/O Expander"},
    {0x40, "PCF8574 (custom/8-bit?)"},
    {0x41, "PCF8574 (custom/8-bit?)"},
    {0x42, "PCF8574 (custom/8-bit?)"},
    {0x43, "PCF8574 (custom/8-bit?)"},
    
    /* Sensors */
    {0x68, "MPU6050 IMU / DS3231 RTC"},
    {0x69, "MPU6050 IMU (alternate)"},
    {0x76, "BMP280/BME280 Pressure"},
    {0x77, "BMP280/BME280 (alternate)"},
    {0x48, "ADS1115 ADC"},
    {0x49, "ADS1115 ADC (alternate)"},
    {0x44, "SHT31 Temperature"},
    {0x5A, "MLX90614 IR Temperature"},
    
    /* EEPROM */
    {0x50, "AT24C EEPROM"},
    {0x51, "AT24C EEPROM"},
    {0x52, "AT24C EEPROM"},
    {0x53, "AT24C EEPROM"},
    
    /* End marker */
    {0x00, NULL}
};

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Get device name from known device database
 */
static const char* lookup_device_name(u8 addr)
{
    u8 i;
    for (i = 0; g_known_devices[i].name != NULL; i++) {
        if (g_known_devices[i].addr == addr) {
            return g_known_devices[i].name;
        }
    }
    return NULL;
}

/**
 * @brief Find registered device by address
 */
static I2cDeviceInfo_t* find_device(u8 addr)
{
    u8 i;
    for (i = 0; i < MAX_REGISTERED_DEVICES; i++) {
        if (g_devices[i].active && g_devices[i].addr == addr) {
            return &g_devices[i];
        }
    }
    return NULL;
}

/*============================================================================
 * Public API Implementation - Bus Management
 *===========================================================================*/

/**
 * @brief Initialize I2C bus
 */
s32 i2c_bus_init(Enum_PinName pinSCL, Enum_PinName pinSDA)
{
    s32 ret;
    
    if (g_initialized) {
        APP_DEBUG("[I2C_BUS] Already initialized\r\n");
        return I2C_BUS_ERR_ALREADY_INITIALIZED;
    }
    
    APP_DEBUG("\r\n");
    APP_DEBUG("╔════════════════════════════════════════════╗\r\n");
    APP_DEBUG("║    I2C BUS CONTROLLER - INITIALIZATION     ║\r\n");
    APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
    APP_DEBUG("\r\n");
    APP_DEBUG("Channel: %d\r\n", I2C_BUS_CHANNEL);
    APP_DEBUG("SCL Pin: %d\r\n", pinSCL);
    APP_DEBUG("SDA Pin: %d\r\n", pinSDA);
    APP_DEBUG("Mode:    Simulated I2C\r\n");
    APP_DEBUG("\r\n");
    
    /* Save configuration */
    g_pinSCL = pinSCL;
    g_pinSDA = pinSDA;
    
    /* Initialize device registry */
    Ql_memset(g_devices, 0, sizeof(g_devices));
    g_configured_devices = 0;
    
    /* Initialize I2C hardware (0 = simulated I2C) */
    ret = Ql_IIC_Init(I2C_BUS_CHANNEL, pinSCL, pinSDA, 0);
    if (ret < 0) {
        APP_DEBUG("❌ I2C Init failed: %d\r\n", ret);
        return I2C_BUS_ERR_INIT;
    }
    
    APP_DEBUG("✅ I2C bus initialized successfully\r\n");
    APP_DEBUG("\r\n");
    
    g_initialized = TRUE;
    
    return I2C_BUS_OK;
}

/**
 * @brief Configure I2C device address
 */
s32 i2c_bus_config_device(u8 addr, const char* device_name)
{
    s32 ret;
    I2cDeviceInfo_t* device;
    
    if (!g_initialized) {
        APP_DEBUG("[I2C_BUS] Not initialized\r\n");
        return I2C_BUS_ERR_NOT_INITIALIZED;
    }
    
    /* Check if already configured */
    device = find_device(addr);
    if (device != NULL) {
        APP_DEBUG("[I2C_BUS] Device 0x%02X already configured\r\n", addr);
        return I2C_BUS_OK;  /* Already configured, OK */
    }
    
    /* Find free slot */
    u8 i;
    for (i = 0; i < MAX_REGISTERED_DEVICES; i++) {
        if (!g_devices[i].active) {
            /* Configure device in Quectel I2C layer */
            ret = Ql_IIC_Config(I2C_BUS_CHANNEL, TRUE, addr, 300);
            if (ret < 0) {
                APP_DEBUG("[I2C_BUS] Failed to configure device 0x%02X: %d\r\n", addr, ret);
                return I2C_BUS_ERR_CONFIG;
            }
            
            /* Register in our device list */
            g_devices[i].addr = addr;
            g_devices[i].name = device_name;
            g_devices[i].active = TRUE;
            g_configured_devices++;
            
            APP_DEBUG("[I2C_BUS] Configured device 0x%02X", addr);
            if (device_name != NULL) {
                APP_DEBUG(" (%s)", device_name);
            }
            APP_DEBUG("\r\n");
            
            return I2C_BUS_OK;
        }
    }
    
    APP_DEBUG("[I2C_BUS] Device registry full\r\n");
    return I2C_BUS_ERR_CONFIG;
}

/**
 * @brief Get I2C channel number
 */
u32 i2c_bus_get_channel(void)
{
    return I2C_BUS_CHANNEL;
}

/**
 * @brief Get SCL pin
 */
Enum_PinName i2c_bus_get_scl_pin(void)
{
    return g_pinSCL;
}

/**
 * @brief Get SDA pin
 */
Enum_PinName i2c_bus_get_sda_pin(void)
{
    return g_pinSDA;
}

/**
 * @brief Check if bus is initialized
 */
bool i2c_bus_is_initialized(void)
{
    return g_initialized;
}

/**
 * @brief Reinitialize I2C bus
 */
s32 i2c_bus_reinit(void)
{
    s32 ret;
    
    if (!g_initialized) {
        return I2C_BUS_ERR_NOT_INITIALIZED;
    }
    
    // APP_DEBUG("[I2C_BUS] Reinitializing bus...\r\n");
    
    /* Uninitialize */
    Ql_IIC_Uninit(I2C_BUS_CHANNEL);
    Ql_Sleep(50);
    
    /* Reinitialize (0 = simulated I2C) */
    ret = Ql_IIC_Init(I2C_BUS_CHANNEL, g_pinSCL, g_pinSDA, 0);
    if (ret < 0) {
        APP_DEBUG("[I2C_BUS] Reinit failed: %d\r\n", ret);
        g_initialized = FALSE;
        return I2C_BUS_ERR_INIT;
    }
    
    /* Reconfigure all registered devices */
    u8 i;
    for (i = 0; i < MAX_REGISTERED_DEVICES; i++) {
        if (g_devices[i].active) {
            ret = Ql_IIC_Config(I2C_BUS_CHANNEL, TRUE, g_devices[i].addr, 300);
            if (ret < 0) {
                APP_DEBUG("[I2C_BUS] Failed to reconfigure 0x%02X: %d\r\n", 
                         g_devices[i].addr, ret);
            }
        }
    }
    
    // APP_DEBUG("[I2C_BUS] Reinit complete\r\n");
    return I2C_BUS_OK;
}

/**
 * @brief Uninitialize I2C bus
 */
void i2c_bus_uninit(void)
{
    if (g_initialized) {
        Ql_IIC_Uninit(I2C_BUS_CHANNEL);
        g_initialized = FALSE;
        g_configured_devices = 0;
        Ql_memset(g_devices, 0, sizeof(g_devices));
        APP_DEBUG("[I2C_BUS] Uninitialized\r\n");
    }
}

/*============================================================================
 * Public API Implementation - Device Discovery
 *===========================================================================*/

/**
 * @brief Scan I2C bus for all devices
 */
u8 i2c_bus_scan(void)
{
    u8 addr;
    s32 ret_config, ret_write_read;
    u8 reg_addr = 0x00;
    u8 read_data[4];
    u8 found_count = 0;
    u8 found_addresses[127];
    u8 configured_slaves = 0;
    const char* device_name;
    u8 i;
    bool is_valid_device;
    
    if (!g_initialized) {
        APP_DEBUG("❌ I2C bus not initialized! Call i2c_bus_init() first.\r\n");
        return 0;
    }
    
    APP_DEBUG("\r\n");
    APP_DEBUG("╔════════════════════════════════════════════╗\r\n");
    APP_DEBUG("║         I2C BUS SCAN - STARTING            ║\r\n");
    APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
    APP_DEBUG("\r\n");
    APP_DEBUG("Scanning 7-bit addresses 0x01 to 0x7F...\r\n");
    APP_DEBUG("Method: Write + Read validation\r\n");
    APP_DEBUG("\r\n");
    
    Ql_memset(found_addresses, 0, sizeof(found_addresses));
    
    for (addr = 0x01; addr < 0x80; addr++) {
        /* Skip odd addresses >0x07 (they're read variants of even addresses) */
        /* This prevents showing duplicate devices like 0x42/0x43, 0x4A/0x4B, 0x78/0x79 */
        if (addr > 0x07 && (addr & 0x01)) {
            continue;
        }
        
        Ql_Sleep(10);
        
        /* Reinit every 6 devices to avoid slave limit */
        if (configured_slaves >= 6) {
            // APP_DEBUG("  🔄 Reinitializing (slave limit)...\r\n");
            i2c_bus_reinit();
            configured_slaves = 0;
        }
        
        /* Configure this address */
        ret_config = Ql_IIC_Config(I2C_BUS_CHANNEL, TRUE, addr, 300);
        
        if (ret_config >= 0) {
            configured_slaves++;
            
            /* Try to read from device */
            Ql_memset(read_data, 0, sizeof(read_data));
            ret_write_read = Ql_IIC_Write_Read(I2C_BUS_CHANNEL, addr, &reg_addr, 1, read_data, 4);
            
            is_valid_device = FALSE;
            
            if (ret_write_read >= 0) {
                /* Validate response */
                bool all_0xff = (read_data[0] == 0xFF && read_data[1] == 0xFF && 
                                 read_data[2] == 0xFF && read_data[3] == 0xFF);
                bool all_0x00 = (read_data[0] == 0x00 && read_data[1] == 0x00 && 
                                 read_data[2] == 0x00 && read_data[3] == 0x00);
                bool all_same = (read_data[0] == read_data[1] && 
                                 read_data[1] == read_data[2] && 
                                 read_data[2] == read_data[3]);
                
                if (!all_0xff && !(all_0x00 && all_same)) {
                    is_valid_device = TRUE;
                }
                
                /* More lenient in common device range */
                if (!is_valid_device && addr >= 0x20 && addr <= 0x77) {
                    if (!all_0xff) {
                        is_valid_device = TRUE;
                    }
                }
            }
            
            if (is_valid_device) {
                APP_DEBUG("✓ Device at 0x%02X", addr);
                found_addresses[found_count] = addr;
                found_count++;
                
                device_name = lookup_device_name(addr);
                if (device_name != NULL) {
                    APP_DEBUG(" - %s", device_name);
                }
                APP_DEBUG("\r\n");
                
                Ql_Sleep(50);
            }
        } else if (ret_config == QL_RET_ERR_IIC_SLAVE_TOO_MANY) {
            /* Reinit and retry */
            i2c_bus_reinit();
            configured_slaves = 0;
            addr--;
        }
        
        /* Progress indicator */
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
        APP_DEBUG("Troubleshooting:\r\n");
        APP_DEBUG("• Check wiring (SCL, SDA, GND, VCC)\r\n");
        APP_DEBUG("• Check pull-up resistors (4.7kΩ)\r\n");
        APP_DEBUG("• Verify device power\r\n");
    } else {
        APP_DEBUG("║   ✅ FOUND %d DEVICE(S) ON BUS             ║\r\n", found_count);
        APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
        APP_DEBUG("\r\n");
        APP_DEBUG("Found devices:\r\n");
        for (i = 0; i < found_count; i++) {
            APP_DEBUG("  • 0x%02X", found_addresses[i]);
            device_name = lookup_device_name(found_addresses[i]);
            if (device_name != NULL) {
                APP_DEBUG(" (%s)", device_name);
            }
            APP_DEBUG("\r\n");
        }
    }
    APP_DEBUG("\r\n");
    
    return found_count;
}

/**
 * @brief Test if specific device exists
 */
bool i2c_bus_device_exists(u8 addr)
{
    s32 ret;
    u8 reg_addr = 0x00;
    u8 read_data;
    
    if (!g_initialized) {
        return FALSE;
    }
    
    /* Try to configure device */
    ret = Ql_IIC_Config(I2C_BUS_CHANNEL, TRUE, addr, 300);
    if (ret < 0) {
        return FALSE;
    }
    
    /* Try to read from device */
    ret = Ql_IIC_Write_Read(I2C_BUS_CHANNEL, addr, &reg_addr, 1, &read_data, 1);
    return (ret >= 0);
}

/**
 * @brief Get list of registered devices
 */
u8 i2c_bus_get_devices(I2cDeviceInfo_t* devices, u8 max_devices)
{
    u8 count = 0;
    u8 i;
    
    if (devices == NULL || max_devices == 0) {
        return 0;
    }
    
    for (i = 0; i < MAX_REGISTERED_DEVICES && count < max_devices; i++) {
        if (g_devices[i].active) {
            devices[count] = g_devices[i];
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Print bus status
 */
void i2c_bus_print_status(void)
{
    u8 i;
    
    APP_DEBUG("\r\n");
    APP_DEBUG("=== I2C Bus Status ===\r\n");
    APP_DEBUG("Initialized: %s\r\n", g_initialized ? "Yes" : "No");
    
    if (g_initialized) {
        APP_DEBUG("Channel: %d\r\n", I2C_BUS_CHANNEL);
        APP_DEBUG("SCL Pin: %d\r\n", g_pinSCL);
        APP_DEBUG("SDA Pin: %d\r\n", g_pinSDA);
        APP_DEBUG("Registered devices: %d\r\n", g_configured_devices);
        APP_DEBUG("\r\n");
        
        if (g_configured_devices > 0) {
            APP_DEBUG("Active devices:\r\n");
            for (i = 0; i < MAX_REGISTERED_DEVICES; i++) {
                if (g_devices[i].active) {
                    APP_DEBUG("  • 0x%02X", g_devices[i].addr);
                    if (g_devices[i].name != NULL) {
                        APP_DEBUG(" - %s", g_devices[i].name);
                    }
                    APP_DEBUG("\r\n");
                }
            }
        }
    }
    APP_DEBUG("\r\n");
}

/**
 * @brief Get device name from address
 */
const char* i2c_bus_get_device_name(u8 addr)
{
    return lookup_device_name(addr);
}

