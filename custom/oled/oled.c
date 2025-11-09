/**
 * @file    oled.c
 * @brief   SSD1306 OLED Display Driver Implementation
 * @author  Hossein Gholami
 * @date    2025-11-03
 */

#include "oled.h"
#include "ql_iic.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_system.h"   /* For Ql_Sleep */
#include "../uart/uart.h"  /* For APP_DEBUG macro */

/*============================================================================
 * SSD1306 Commands
 *===========================================================================*/
#define SSD1306_CMD_DISPLAY_OFF         0xAE
#define SSD1306_CMD_DISPLAY_ON          0xAF
#define SSD1306_CMD_SET_CONTRAST        0x81
#define SSD1306_CMD_NORMAL_DISPLAY      0xA6
#define SSD1306_CMD_INVERT_DISPLAY      0xA7
#define SSD1306_CMD_SET_MUX_RATIO       0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET  0xD3
#define SSD1306_CMD_SET_START_LINE      0x40
#define SSD1306_CMD_SEG_REMAP           0xA1
#define SSD1306_CMD_COM_SCAN_DEC        0xC8
#define SSD1306_CMD_SET_COM_PINS        0xDA
#define SSD1306_CMD_SET_PRECHARGE       0xD9
#define SSD1306_CMD_SET_VCOM_DESELECT   0xDB
#define SSD1306_CMD_CHARGE_PUMP         0x8D
#define SSD1306_CMD_SET_MEMORY_MODE     0x20
#define SSD1306_CMD_SET_COLUMN_ADDR     0x21
#define SSD1306_CMD_SET_PAGE_ADDR       0x22

/*============================================================================
 * Frame Buffer (128x64 = 1024 bytes)
 *===========================================================================*/
static u8 g_frame_buffer[OLED_WIDTH * OLED_PAGES];
static bool g_oled_initialized = FALSE;

/*============================================================================
 * 8x6 ASCII Font (characters 32-127)
 * Each character is 6 bytes (6 columns x 8 rows)
 *===========================================================================*/
static const u8 font_8x6[][6] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
    {0x00, 0x00, 0x5F, 0x00, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62, 0x00}, // %
    {0x36, 0x49, 0x56, 0x20, 0x50, 0x00}, // &
    {0x00, 0x08, 0x07, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00, 0x00}, // )
    {0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x00}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08, 0x00}, // +
    {0x00, 0x80, 0x70, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08, 0x00}, // -
    {0x00, 0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02, 0x00}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00, 0x00}, // 1
    {0x72, 0x49, 0x49, 0x49, 0x46, 0x00}, // 2
    {0x21, 0x41, 0x49, 0x4D, 0x33, 0x00}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10, 0x00}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39, 0x00}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x31, 0x00}, // 6
    {0x41, 0x21, 0x11, 0x09, 0x07, 0x00}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36, 0x00}, // 8
    {0x46, 0x49, 0x49, 0x29, 0x1E, 0x00}, // 9
    {0x00, 0x00, 0x14, 0x00, 0x00, 0x00}, // :
    {0x00, 0x40, 0x34, 0x00, 0x00, 0x00}, // ;
    {0x00, 0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14, 0x00}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08, 0x00}, // >
    {0x02, 0x01, 0x59, 0x09, 0x06, 0x00}, // ?
    {0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x00}, // @
    {0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36, 0x00}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22, 0x00}, // C
    {0x7F, 0x41, 0x41, 0x41, 0x3E, 0x00}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41, 0x00}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01, 0x00}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A, 0x00}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01, 0x00}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41, 0x00}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40, 0x00}, // L
    {0x7F, 0x02, 0x1C, 0x02, 0x7F, 0x00}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06, 0x00}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46, 0x00}, // R
    {0x26, 0x49, 0x49, 0x49, 0x32, 0x00}, // S
    {0x03, 0x01, 0x7F, 0x01, 0x03, 0x00}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F, 0x00}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63, 0x00}, // X
    {0x03, 0x04, 0x78, 0x04, 0x03, 0x00}, // Y
    {0x61, 0x59, 0x49, 0x4D, 0x43, 0x00}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20, 0x00}, // Backslash
    {0x00, 0x41, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04, 0x00}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x00}, // _
    {0x00, 0x03, 0x07, 0x08, 0x00, 0x00}, // `
    {0x20, 0x54, 0x54, 0x78, 0x40, 0x00}, // a
    {0x7F, 0x28, 0x44, 0x44, 0x38, 0x00}, // b
    {0x38, 0x44, 0x44, 0x44, 0x28, 0x00}, // c
    {0x38, 0x44, 0x44, 0x28, 0x7F, 0x00}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18, 0x00}, // e
    {0x00, 0x08, 0x7E, 0x09, 0x02, 0x00}, // f
    {0x18, 0xA4, 0xA4, 0x9C, 0x78, 0x00}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78, 0x00}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00, 0x00}, // i
    {0x20, 0x40, 0x40, 0x3D, 0x00, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00, 0x00}, // l
    {0x7C, 0x04, 0x78, 0x04, 0x78, 0x00}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78, 0x00}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38, 0x00}, // o
    {0xFC, 0x18, 0x24, 0x24, 0x18, 0x00}, // p
    {0x18, 0x24, 0x24, 0x18, 0xFC, 0x00}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08, 0x00}, // r
    {0x48, 0x54, 0x54, 0x54, 0x24, 0x00}, // s
    {0x04, 0x04, 0x3F, 0x44, 0x24, 0x00}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44, 0x00}, // x
    {0x4C, 0x90, 0x90, 0x90, 0x7C, 0x00}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44, 0x00}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00, 0x00}, // {
    {0x00, 0x00, 0x77, 0x00, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00, 0x00}, // }
    {0x02, 0x01, 0x02, 0x04, 0x02, 0x00}, // ~
};

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Send command to SSD1306
 */
static s32 oled_send_cmd(u8 cmd)
{
    u8 data[2];
    data[0] = 0x00;  /* Command mode (Co=0, D/C=0) */
    data[1] = cmd;
    
    return Ql_IIC_Write(OLED_I2C_CHANNEL, OLED_I2C_ADDR, data, 2);
}

/**
 * @brief Send data to SSD1306
 */
static s32 oled_send_data(u8* data, u32 len)
{
    u8 buffer[129];  /* Max 128 data bytes + 1 control byte */
    
    if (len > 128) {
        return QL_RET_ERR_PARAM;
    }
    
    buffer[0] = 0x40;  /* Data mode (Co=0, D/C=1) */
    Ql_memcpy(&buffer[1], data, len);
    
    return Ql_IIC_Write(OLED_I2C_CHANNEL, OLED_I2C_ADDR, buffer, len + 1);
}

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

s32 oled_init(Enum_PinName pinSCL, Enum_PinName pinSDA)
{
    s32 ret;
    
    APP_DEBUG("[OLED] Init: SCL=%d, SDA=%d, Addr=0x%02X (8-bit)\r\n", pinSCL, pinSDA, OLED_I2C_ADDR);
    
    /* Initialize I2C */
    ret = Ql_IIC_Init(OLED_I2C_CHANNEL, pinSCL, pinSDA, 0);  /* 0 = Simulated I2C */
    if (ret < 0) {
        APP_DEBUG("[OLED] ❌ I2C Init failed: %d\r\n", ret);
        return OLED_ERR_I2C_INIT;
    }
    
    /* Configure I2C for SSD1306 (using 8-bit address 0x78 = 7-bit 0x3C) */
    ret = Ql_IIC_Config(OLED_I2C_CHANNEL, TRUE, OLED_I2C_ADDR, 0);  /* Speed ignored for simulated I2C */
    if (ret < 0) {
        APP_DEBUG("[OLED] ❌ I2C Config failed: %d\r\n", ret);
        return OLED_ERR_I2C_CONFIG;
    }
    
    /* Small delay for display power-up */
    Ql_Sleep(100);
    
    /* Test communication with first command */
    ret = oled_send_cmd(SSD1306_CMD_DISPLAY_OFF);
    if (ret < 0) {
        APP_DEBUG("[OLED] ❌ Display not responding (error %d)\r\n", ret);
        APP_DEBUG("[OLED] Run oled_scan_i2c() to find device\r\n");
        return OLED_ERR_DISPLAY_INIT;
    }
    
    oled_send_cmd(SSD1306_CMD_SET_MUX_RATIO);
    oled_send_cmd(0x3F);  /* 64 rows */
    
    oled_send_cmd(SSD1306_CMD_SET_DISPLAY_OFFSET);
    oled_send_cmd(0x00);  /* No offset */
    
    oled_send_cmd(SSD1306_CMD_SET_START_LINE | 0x00);  /* Start line = 0 */
    
    oled_send_cmd(SSD1306_CMD_SEG_REMAP);  /* Column 127 mapped to SEG0 */
    oled_send_cmd(SSD1306_CMD_COM_SCAN_DEC);  /* Scan from COM[N-1] to COM0 */
    
    oled_send_cmd(SSD1306_CMD_SET_COM_PINS);
    oled_send_cmd(0x12);  /* Alternative COM pin config */
    
    oled_send_cmd(SSD1306_CMD_SET_CONTRAST);
    oled_send_cmd(0x7F);  /* Mid contrast */
    
    oled_send_cmd(SSD1306_CMD_NORMAL_DISPLAY);  /* Not inverted */
    
    oled_send_cmd(SSD1306_CMD_SET_PRECHARGE);
    oled_send_cmd(0xF1);  /* Precharge period */
    
    oled_send_cmd(SSD1306_CMD_SET_VCOM_DESELECT);
    oled_send_cmd(0x40);  /* VCOM deselect level */
    
    oled_send_cmd(SSD1306_CMD_CHARGE_PUMP);
    oled_send_cmd(0x14);  /* Enable charge pump */
    
    oled_send_cmd(SSD1306_CMD_SET_MEMORY_MODE);
    oled_send_cmd(0x00);  /* Horizontal addressing mode */
    
    /* Clear frame buffer */
    Ql_memset(g_frame_buffer, 0x00, sizeof(g_frame_buffer));
    
    /* Turn on display */
    oled_send_cmd(SSD1306_CMD_DISPLAY_ON);
    
    g_oled_initialized = TRUE;
    
    APP_DEBUG("[OLED] ✅ Init complete!\r\n");
    return OLED_OK;
}

void oled_clear(void)
{
    Ql_memset(g_frame_buffer, 0x00, sizeof(g_frame_buffer));
}

void oled_update(void)
{
    u8 page;
    
    if (!g_oled_initialized) {
        return;
    }
    
    /* Set column address range (0-127) */
    oled_send_cmd(SSD1306_CMD_SET_COLUMN_ADDR);
    oled_send_cmd(0);
    oled_send_cmd(OLED_WIDTH - 1);
    
    /* Set page address range (0-7) */
    oled_send_cmd(SSD1306_CMD_SET_PAGE_ADDR);
    oled_send_cmd(0);
    oled_send_cmd(OLED_PAGES - 1);
    
    /* Send frame buffer in chunks (max 128 bytes per I2C write) */
    for (page = 0; page < OLED_PAGES; page++) {
        oled_send_data(&g_frame_buffer[page * OLED_WIDTH], OLED_WIDTH);
    }
}

void oled_set_pixel(u8 x, u8 y, u8 color)
{
    u8 page, bit;
    u16 index;
    
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    
    page = y / 8;
    bit = y % 8;
    index = page * OLED_WIDTH + x;
    
    if (color) {
        g_frame_buffer[index] |= (1 << bit);
    } else {
        g_frame_buffer[index] &= ~(1 << bit);
    }
}

void oled_draw_char(u8 x, u8 y, char ch)
{
    u8 i;
    u8 page;
    const u8* font_data;
    
    if (ch < 32 || ch > 126) {
        ch = ' ';  /* Default to space for unsupported characters */
    }
    
    font_data = font_8x6[ch - 32];
    page = y / 8;
    
    if (page >= OLED_PAGES || x + FONT_WIDTH > OLED_WIDTH) {
        return;
    }
    
    /* Copy font data to frame buffer */
    for (i = 0; i < FONT_WIDTH; i++) {
        g_frame_buffer[page * OLED_WIDTH + x + i] = font_data[i];
    }
}

void oled_draw_string(u8 x, u8 y, const char* str)
{
    u8 cur_x = x;
    
    while (*str) {
        if (cur_x + FONT_WIDTH > OLED_WIDTH) {
            break;  /* Stop if string goes off screen */
        }
        
        oled_draw_char(cur_x, y, *str);
        cur_x += FONT_WIDTH;
        str++;
    }
}

void oled_set_brightness(u8 brightness)
{
    if (!g_oled_initialized) {
        return;
    }
    
    oled_send_cmd(SSD1306_CMD_SET_CONTRAST);
    oled_send_cmd(brightness);
}

void oled_display_on(bool on)
{
    if (!g_oled_initialized) {
        return;
    }
    
    if (on) {
        oled_send_cmd(SSD1306_CMD_DISPLAY_ON);
    } else {
        oled_send_cmd(SSD1306_CMD_DISPLAY_OFF);
    }
}

void oled_invert(bool invert)
{
    if (!g_oled_initialized) {
        return;
    }
    
    if (invert) {
        oled_send_cmd(SSD1306_CMD_INVERT_DISPLAY);
    } else {
        oled_send_cmd(SSD1306_CMD_NORMAL_DISPLAY);
    }
}

void oled_draw_hline(u8 x, u8 y, u8 width)
{
    u8 i;
    for (i = 0; i < width; i++) {
        oled_set_pixel(x + i, y, 1);
    }
}

void oled_draw_vline(u8 x, u8 y, u8 height)
{
    u8 i;
    for (i = 0; i < height; i++) {
        oled_set_pixel(x, y + i, 1);
    }
}

void oled_draw_rect(u8 x, u8 y, u8 width, u8 height, bool fill)
{
    u8 i, j;
    
    if (fill) {
        /* Filled rectangle */
        for (i = 0; i < width; i++) {
            for (j = 0; j < height; j++) {
                oled_set_pixel(x + i, y + j, 1);
            }
        }
    } else {
        /* Outline rectangle */
        oled_draw_hline(x, y, width);
        oled_draw_hline(x, y + height - 1, width);
        oled_draw_vline(x, y, height);
        oled_draw_vline(x + width - 1, y, height);
    }
}

bool oled_test_communication(void)
{
    s32 ret;
    
    if (!g_oled_initialized) {
        APP_DEBUG("[OLED] Not initialized, cannot test\r\n");
        return FALSE;
    }
    
    /* Try to send a simple command */
    ret = oled_send_cmd(SSD1306_CMD_SET_CONTRAST);
    
    if (ret < 0) {
        APP_DEBUG("[OLED] ❌ Communication test FAILED: %d\r\n", ret);
        APP_DEBUG("[OLED] Possible issues:\r\n");
        APP_DEBUG("[OLED]   - Loose wire connection\r\n");
        APP_DEBUG("[OLED]   - Wrong I2C address (currently 0x%02X)\r\n", OLED_I2C_ADDR);
        APP_DEBUG("[OLED]   - Display not powered\r\n");
        return FALSE;
    }
    
    APP_DEBUG("[OLED] ✓ Communication test PASSED\r\n");
    return TRUE;
}

