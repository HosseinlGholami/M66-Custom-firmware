/**
 * @file    io_expander_param.h
 * @brief   IO Expander Parameter Integration
 * @author  Hossein Gholami
 * @date    2025-11-17
 * 
 * Integrates IO expander with parameter system for automatic control:
 * - Device 0 (0x42): OUTPUTS controlled by PARAM_IO_EXP0_OUT
 * - Device 1 (0x4A): INPUTS that update PARAM_IO_EXP1_IN
 */

#ifndef IO_EXPANDER_PARAM_H
#define IO_EXPANDER_PARAM_H

#include "ql_type.h"

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize IO expander parameter integration
 * 
 * Registers callbacks and sets up parameter links.
 * Must be called AFTER both param_init() and io_expander_init().
 * 
 * @return 0 on success, negative on error
 */
s32 io_expander_param_init(void);

/**
 * @brief Update input parameters from IO expander
 * 
 * Reads inputs from Device 1 and updates PARAM_IO_EXP1_IN.
 * Call this periodically from main loop (e.g., every 100ms).
 * 
 * @return 0 on success, negative on error
 */
s32 io_expander_param_update(void);

/**
 * @brief Check if IO expander param integration is initialized
 * 
 * @return TRUE if initialized, FALSE otherwise
 */
bool io_expander_param_is_initialized(void);

#endif /* IO_EXPANDER_PARAM_H */

