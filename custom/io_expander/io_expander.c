/**
 * @file    io_expander.c
 * @brief   PCF8574 IO Expander Implementation
 * @author  Hossein Gholami
 * @date    2025-11-17
 * 
 * Modified to use shared I2C bus controller
 */

#include "io_expander.h"
#include "ql_iic.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_eint.h"
#include "../uart/uart.h"
#include "../i2c_bus/i2c_bus.h"

/*============================================================================
 * PCF8574 Device Characteristics
 *===========================================================================*/
/* PCF8574 is quasi-bidirectional I/O:
 * - Writing 1 to a pin makes it an input (high impedance)
 * - Writing 0 to a pin makes it an output LOW
 * - To read a pin, first write 1 to it, then read
 * - INT pin goes LOW when any input changes from previous read
 */

/*============================================================================
 * Private Data
 *===========================================================================*/

/**
 * @brief Device runtime data
 */
typedef struct {
    u8              i2c_addr;           /* I2C address (7-bit) */
    u8              current_state;      /* Last known pin states */
    bool            enabled;            /* Is this device enabled? */
    const char*     name;               /* Device name */
} IoExpanderDevice_t;

static IoExpanderDevice_t g_devices[IO_EXPANDER_MAX_DEVICES];
static u8 g_device_count = 0;
static bool g_initialized = FALSE;
static Enum_PinName g_pin_int = 0;
static IoExpanderIntCallback_t g_int_callback = NULL;

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Write byte to PCF8574
 */
static s32 pcf8574_write(u8 i2c_addr, u8 data)
{
    s32 ret;
    
    /* Quectel API uses 7-bit addresses directly, no shifting needed */
    ret = Ql_IIC_Write(i2c_bus_get_channel(), i2c_addr, &data, 1);
    if (ret < 0) {
        APP_DEBUG("IO_EXP: I2C Write failed at 0x%02X, ret=%d\r\n", i2c_addr, ret);
        return IO_EXPANDER_ERR_I2C_WRITE;
    }
    
    return IO_EXPANDER_OK;
}

/**
 * @brief Read byte from PCF8574
 */
static s32 pcf8574_read(u8 i2c_addr, u8* data)
{
    s32 ret;
    
    /* Quectel API uses 7-bit addresses directly, no shifting needed */
    ret = Ql_IIC_Read(i2c_bus_get_channel(), i2c_addr, data, 1);
    if (ret < 0) {
        APP_DEBUG("IO_EXP: I2C Read failed at 0x%02X, ret=%d\r\n", i2c_addr, ret);
        return IO_EXPANDER_ERR_I2C_READ;
    }
    
    return IO_EXPANDER_OK;
}

/**
 * @brief Find device by ID
 */
static IoExpanderDevice_t* get_device(u8 device_id)
{
    if (device_id >= g_device_count) {
        return NULL;
    }
    
    if (!g_devices[device_id].enabled) {
        return NULL;
    }
    
    return &g_devices[device_id];
}

/**
 * @brief EINT callback for interrupt handling
 * Called when INT pin goes LOW (any input changed on any device)
 */
static void io_expander_eint_callback(Enum_PinName pin, Enum_PinLevel level, void* user_data)
{
    u8 i;
    
    /* INT is active LOW - trigger on falling edge */
    if (level == PINLEVEL_LOW) {
        /* Read all devices to determine which one changed */
        for (i = 0; i < g_device_count; i++) {
            if (g_devices[i].enabled) {
                u8 pin_states;
                if (pcf8574_read(g_devices[i].i2c_addr, &pin_states) == IO_EXPANDER_OK) {
                    /* Check if state changed */
                    if (pin_states != g_devices[i].current_state) {
                        g_devices[i].current_state = pin_states;
                        
                        /* Call user callback */
                        if (g_int_callback != NULL) {
                            g_int_callback(i, pin_states);
                        }
                        
                        APP_DEBUG("IO_EXP: Device %d ('%s') changed: 0x%02X\r\n",
                                 i, g_devices[i].name, pin_states);
                    }
                }
            }
        }
    }
}

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

/**
 * @brief Initialize IO expander module
 */
s32 io_expander_init(Enum_PinName pinINT,
                     const IoExpanderConfig_t* config, 
                     u8 device_count)
{
    s32 ret;
    u8 i;
    
    /* Validate parameters */
    if (config == NULL || device_count == 0) {
        return IO_EXPANDER_ERR_PARAM;
    }
    
    if (device_count > IO_EXPANDER_MAX_DEVICES) {
        return IO_EXPANDER_ERR_MAX_DEVICES;
    }
    
    /* Check if I2C bus is initialized */
    if (!i2c_bus_is_initialized()) {
        APP_DEBUG("IO_EXP: ❌ I2C bus not initialized! Call i2c_bus_init() first.\r\n");
        return IO_EXPANDER_ERR_I2C_INIT;
    }
    
    APP_DEBUG("IO_EXP: Using shared I2C bus (channel %d)\r\n", i2c_bus_get_channel());
    
    /* Initialize device structures */
    Ql_memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0;
    
    /* Configure each device */
    for (i = 0; i < device_count; i++) {
        if (!config[i].enabled) {
            continue;
        }
        
        /* Register device on I2C bus */
        ret = i2c_bus_config_device(config[i].i2c_addr, config[i].name);
        if (ret < 0) {
            APP_DEBUG("IO_EXP: Failed to configure device '%s' at 0x%02X on bus: %d\r\n",
                     config[i].name, config[i].i2c_addr, ret);
            continue;
        }
        
        g_devices[g_device_count].i2c_addr = config[i].i2c_addr;
        g_devices[g_device_count].current_state = config[i].init_state;
        g_devices[g_device_count].enabled = TRUE;
        g_devices[g_device_count].name = config[i].name;
        
        /* Write initial state to device */
        ret = pcf8574_write(config[i].i2c_addr, config[i].init_state);
        if (ret < 0) {
            APP_DEBUG("IO_EXP: Failed to initialize device %d ('%s') at 0x%02X\r\n",
                     g_device_count, config[i].name, config[i].i2c_addr);
            /* Continue with other devices */
            g_devices[g_device_count].enabled = FALSE;
        } else {
            APP_DEBUG("IO_EXP: Device %d ('%s') initialized at 0x%02X, state=0x%02X\r\n",
                     g_device_count, config[i].name, config[i].i2c_addr, config[i].init_state);
            g_device_count++;
        }
    }
    
    if (g_device_count == 0) {
        APP_DEBUG("IO_EXP: No devices successfully initialized\r\n");
        return IO_EXPANDER_ERR_DEVICE_NOT_FOUND;
    }
    
    /* Setup interrupt pin if provided (optional - won't fail if EINT unavailable) */
    if (pinINT != 0) {
        g_pin_int = pinINT;
        
        /* Configure INT pin as input with pull-up */
        ret = Ql_GPIO_Init(pinINT, PINDIRECTION_IN, PINLEVEL_HIGH, PINPULLSEL_PULLUP);
        if (ret < 0) {
            APP_DEBUG("IO_EXP: ⚠️ Failed to initialize INT pin (ret=%d) - continuing without interrupts\r\n", ret);
            g_pin_int = 0;  /* Disable interrupts */
        } else {
            /* Register EINT callback for falling edge (INT is active LOW) */
            ret = Ql_EINT_Register(pinINT, io_expander_eint_callback, NULL);
            if (ret < 0) {
                APP_DEBUG("IO_EXP: ⚠️ Failed to register EINT (ret=%d) - continuing without interrupts\r\n", ret);
                g_pin_int = 0;  /* Disable interrupts */
            } else {
                /* Initialize EINT with falling edge trigger */
                ret = Ql_EINT_Init(pinINT, EINT_LEVEL_TRIGGERED, PINLEVEL_LOW, 50, TRUE);
                if (ret < 0) {
                    APP_DEBUG("IO_EXP: ⚠️ Failed to initialize EINT (ret=%d) - continuing without interrupts\r\n", ret);
                    Ql_EINT_Uninit(pinINT);
                    g_pin_int = 0;  /* Disable interrupts */
                } else {
                    APP_DEBUG("IO_EXP: ✅ Interrupt configured on pin %d\r\n", pinINT);
                }
            }
        }
    }
    
    g_initialized = TRUE;
    APP_DEBUG("IO_EXP: Module initialized with %d device(s)\r\n", g_device_count);
    
    return IO_EXPANDER_OK;
}

/**
 * @brief Register interrupt callback
 */
s32 io_expander_register_int_callback(IoExpanderIntCallback_t callback)
{
    if (!g_initialized) {
        return IO_EXPANDER_ERR_NOT_INITIALIZED;
    }
    
    g_int_callback = callback;
    return IO_EXPANDER_OK;
}

/**
 * @brief Write all 8 pins of a device
 */
s32 io_expander_write_port(u8 device_id, u8 pin_states)
{
    IoExpanderDevice_t* dev;
    s32 ret;
    
    if (!g_initialized) {
        return IO_EXPANDER_ERR_NOT_INITIALIZED;
    }
    
    dev = get_device(device_id);
    if (dev == NULL) {
        return IO_EXPANDER_ERR_INVALID_DEVICE;
    }
    
    ret = pcf8574_write(dev->i2c_addr, pin_states);
    if (ret < 0) {
        return ret;
    }
    
    dev->current_state = pin_states;
    return IO_EXPANDER_OK;
}

/**
 * @brief Read all 8 pins of a device
 */
s32 io_expander_read_port(u8 device_id, u8* pin_states)
{
    IoExpanderDevice_t* dev;
    s32 ret;
    
    if (!g_initialized) {
        return IO_EXPANDER_ERR_NOT_INITIALIZED;
    }
    
    if (pin_states == NULL) {
        return IO_EXPANDER_ERR_PARAM;
    }
    
    dev = get_device(device_id);
    if (dev == NULL) {
        return IO_EXPANDER_ERR_INVALID_DEVICE;
    }
    
    ret = pcf8574_read(dev->i2c_addr, pin_states);
    if (ret < 0) {
        return ret;
    }
    
    dev->current_state = *pin_states;
    return IO_EXPANDER_OK;
}

/**
 * @brief Set specific pins high
 */
s32 io_expander_set_pins(u8 device_id, u8 pin_mask)
{
    IoExpanderDevice_t* dev;
    u8 new_state;
    
    if (!g_initialized) {
        return IO_EXPANDER_ERR_NOT_INITIALIZED;
    }
    
    dev = get_device(device_id);
    if (dev == NULL) {
        return IO_EXPANDER_ERR_INVALID_DEVICE;
    }
    
    new_state = dev->current_state | pin_mask;
    return io_expander_write_port(device_id, new_state);
}

/**
 * @brief Clear specific pins low
 */
s32 io_expander_clear_pins(u8 device_id, u8 pin_mask)
{
    IoExpanderDevice_t* dev;
    u8 new_state;
    
    if (!g_initialized) {
        return IO_EXPANDER_ERR_NOT_INITIALIZED;
    }
    
    dev = get_device(device_id);
    if (dev == NULL) {
        return IO_EXPANDER_ERR_INVALID_DEVICE;
    }
    
    new_state = dev->current_state & ~pin_mask;
    return io_expander_write_port(device_id, new_state);
}

/**
 * @brief Toggle specific pins
 */
s32 io_expander_toggle_pins(u8 device_id, u8 pin_mask)
{
    IoExpanderDevice_t* dev;
    u8 new_state;
    
    if (!g_initialized) {
        return IO_EXPANDER_ERR_NOT_INITIALIZED;
    }
    
    dev = get_device(device_id);
    if (dev == NULL) {
        return IO_EXPANDER_ERR_INVALID_DEVICE;
    }
    
    new_state = dev->current_state ^ pin_mask;
    return io_expander_write_port(device_id, new_state);
}

/**
 * @brief Configure pins as inputs
 */
s32 io_expander_set_as_input(u8 device_id, u8 pin_mask)
{
    /* For PCF8574, setting pins HIGH makes them inputs */
    return io_expander_set_pins(device_id, pin_mask);
}

/**
 * @brief Read specific pin state
 */
s32 io_expander_read_pin(u8 device_id, u8 pin, u8* state)
{
    IoExpanderDevice_t* dev;
    u8 pin_states;
    s32 ret;
    
    if (!g_initialized) {
        return IO_EXPANDER_ERR_NOT_INITIALIZED;
    }
    
    if (state == NULL || pin > 7) {
        return IO_EXPANDER_ERR_PARAM;
    }
    
    dev = get_device(device_id);
    if (dev == NULL) {
        return IO_EXPANDER_ERR_INVALID_DEVICE;
    }
    
    ret = pcf8574_read(dev->i2c_addr, &pin_states);
    if (ret < 0) {
        return ret;
    }
    
    dev->current_state = pin_states;
    *state = (pin_states >> pin) & 0x01;
    
    return IO_EXPANDER_OK;
}

/**
 * @brief Write specific pin state
 */
s32 io_expander_write_pin(u8 device_id, u8 pin, u8 state)
{
    if (pin > 7) {
        return IO_EXPANDER_ERR_PARAM;
    }
    
    if (state) {
        return io_expander_set_pins(device_id, (1 << pin));
    } else {
        return io_expander_clear_pins(device_id, (1 << pin));
    }
}

/**
 * @brief Get device count
 */
u8 io_expander_get_device_count(void)
{
    return g_device_count;
}

/**
 * @brief Get device name
 */
const char* io_expander_get_device_name(u8 device_id)
{
    if (device_id >= g_device_count) {
        return NULL;
    }
    
    return g_devices[device_id].name;
}

/**
 * @brief Print status of all devices
 */
void io_expander_print_status(void)
{
    u8 i;
    u8 pin_states;
    s32 ret;
    
    if (!g_initialized) {
        APP_DEBUG("IO_EXP: Module not initialized\r\n");
        return;
    }
    
    APP_DEBUG("\r\n=== IO Expander Status ===\r\n");
    APP_DEBUG("Devices: %d\r\n", g_device_count);
    APP_DEBUG("INT Pin: %d\r\n", g_pin_int);
    APP_DEBUG("\r\n");
    
    for (i = 0; i < g_device_count; i++) {
        if (!g_devices[i].enabled) {
            continue;
        }
        
        APP_DEBUG("Device %d: %s\r\n", i, g_devices[i].name);
        APP_DEBUG("  I2C Addr: 0x%02X (7-bit: 0x%02X)\r\n", 
                 g_devices[i].i2c_addr << 1, g_devices[i].i2c_addr);
        
        ret = pcf8574_read(g_devices[i].i2c_addr, &pin_states);
        if (ret == IO_EXPANDER_OK) {
            g_devices[i].current_state = pin_states;
            APP_DEBUG("  Pin States: 0x%02X (", pin_states);
            {
                u8 j;
                for (j = 0; j < 8; j++) {
                    APP_DEBUG("%d", (pin_states >> j) & 1);
                }
            }
            APP_DEBUG(")\r\n");
            APP_DEBUG("  P0-P3: %d %d %d %d\r\n",
                     (pin_states >> 0) & 1, (pin_states >> 1) & 1,
                     (pin_states >> 2) & 1, (pin_states >> 3) & 1);
            APP_DEBUG("  P4-P7: %d %d %d %d\r\n",
                     (pin_states >> 4) & 1, (pin_states >> 5) & 1,
                     (pin_states >> 6) & 1, (pin_states >> 7) & 1);
        } else {
            APP_DEBUG("  Status: ERROR (ret=%d)\r\n", ret);
        }
        APP_DEBUG("\r\n");
    }
}

/**
 * @brief Test device communication
 */
bool io_expander_test_device(u8 device_id)
{
    IoExpanderDevice_t* dev;
    u8 test_data;
    s32 ret;
    
    if (!g_initialized) {
        return FALSE;
    }
    
    dev = get_device(device_id);
    if (dev == NULL) {
        return FALSE;
    }
    
    /* Try to read from device */
    ret = pcf8574_read(dev->i2c_addr, &test_data);
    return (ret == IO_EXPANDER_OK);
}

bool io_expander_is_initialized(void)
{
    return g_initialized;
}

