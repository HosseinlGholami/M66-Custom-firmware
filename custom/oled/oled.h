/**
 * @file    oled.h
 * @brief   SSD1306 OLED Display Driver (128x64, I2C)
 * @author  Hossein Gholami
 * @date    2025-11-03
 * 
 * Simple driver for SSD1306-based OLED displays over I2C.
 * Supports basic text rendering and graphics primitives.
 */

#ifndef OLED_H
#define OLED_H

#include "ql_type.h"
#include "ql_gpio.h"

/*============================================================================
 * Constants
 *===========================================================================*/

/* Display dimensions */
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_PAGES          (OLED_HEIGHT / 8)  /* 8 pages of 8 pixels each */

/* I2C Configuration */
#define OLED_I2C_CHANNEL    0
#define OLED_I2C_ADDR       0x78  /* SSD1306 I2C address (8-bit write address, 7-bit 0x3C) */
#define OLED_I2C_SPEED      100   /* 100 kHz (ignored for simulated I2C) */

/* Font dimensions (8x6 font) */
#define FONT_WIDTH          6
#define FONT_HEIGHT         8

/*============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief OLED initialization result codes
 */
typedef enum {
    OLED_OK = 0,
    OLED_ERR_I2C_INIT = -1,
    OLED_ERR_I2C_CONFIG = -2,
    OLED_ERR_DISPLAY_INIT = -3,
    OLED_ERR_PARAM = -4
} OledResult_e;

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize OLED display
 * 
 * @param pinSCL SCL pin (e.g., PINNAME_DTR)
 * @param pinSDA SDA pin (e.g., PINNAME_DCD)
 * @return OLED_OK on success, error code otherwise
 */
s32 oled_init(Enum_PinName pinSCL, Enum_PinName pinSDA);

/**
 * @brief Clear the entire display
 */
void oled_clear(void);

/**
 * @brief Update display with current buffer content
 */
void oled_update(void);

/**
 * @brief Set a single pixel
 * 
 * @param x X coordinate (0-127)
 * @param y Y coordinate (0-63)
 * @param color 1=on, 0=off
 */
void oled_set_pixel(u8 x, u8 y, u8 color);

/**
 * @brief Draw a string at specified position
 * 
 * @param x X coordinate (0-127)
 * @param y Y coordinate (0-63, must be multiple of 8)
 * @param str Null-terminated string
 */
void oled_draw_string(u8 x, u8 y, const char* str);

/**
 * @brief Draw a single character
 * 
 * @param x X coordinate (0-127)
 * @param y Y coordinate (0-63, must be multiple of 8)
 * @param ch Character to draw
 */
void oled_draw_char(u8 x, u8 y, char ch);

/**
 * @brief Set display brightness
 * 
 * @param brightness 0-255 (0=dim, 255=bright)
 */
void oled_set_brightness(u8 brightness);

/**
 * @brief Turn display on/off
 * 
 * @param on TRUE=on, FALSE=off
 */
void oled_display_on(bool on);

/**
 * @brief Invert display colors
 * 
 * @param invert TRUE=invert, FALSE=normal
 */
void oled_invert(bool invert);

/**
 * @brief Draw a horizontal line
 * 
 * @param x Starting X coordinate
 * @param y Y coordinate
 * @param width Line width
 */
void oled_draw_hline(u8 x, u8 y, u8 width);

/**
 * @brief Draw a vertical line
 * 
 * @param x X coordinate
 * @param y Starting Y coordinate
 * @param height Line height
 */
void oled_draw_vline(u8 x, u8 y, u8 height);

/**
 * @brief Draw a rectangle
 * 
 * @param x Starting X coordinate
 * @param y Starting Y coordinate
 * @param width Rectangle width
 * @param height Rectangle height
 * @param fill TRUE=filled, FALSE=outline
 */
void oled_draw_rect(u8 x, u8 y, u8 width, u8 height, bool fill);

/**
 * @brief Test I2C communication with display
 * 
 * Sends a simple command and checks if device responds.
 * Useful for debugging connection issues.
 * 
 * @return TRUE if display responds, FALSE otherwise
 */
bool oled_test_communication(void);

#endif /* OLED_H */

