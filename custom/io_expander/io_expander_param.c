/**
 * @file    io_expander_param.c
 * @brief   Legacy compatibility shim
 */

#include "io_expander_param.h"
#include "../uart/uart.h"
#include "ql_stdlib.h"

static bool g_initialized = FALSE;

s32 io_expander_param_init(void)
{
    g_initialized = TRUE;
    APP_DEBUG("IO_EXP_PARAM: legacy shim active, handled by gpio_expander.c\r\n");
    return 0;
}

s32 io_expander_param_update(void)
{
    return 0;
}

bool io_expander_param_is_initialized(void)
{
    return g_initialized;
}
