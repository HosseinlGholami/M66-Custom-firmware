/**
 * @file    restart_log.h
 * @brief   Restart Detection and Logging
 * @author  Hossein Gholami
 * @date    2025-11-17
 * 
 * Helps diagnose unexpected restarts by logging restart reasons
 * and tracking initialization progress.
 */

#ifndef RESTART_LOG_H
#define RESTART_LOG_H

#include "ql_type.h"

/*============================================================================
 * Boot Stage Tracking
 *===========================================================================*/

typedef enum {
    BOOT_STAGE_START = 0,
    BOOT_STAGE_UART_INIT,
    BOOT_STAGE_PARAM_INIT,
    BOOT_STAGE_GPIO_INIT,
    BOOT_STAGE_COM_INIT,
    BOOT_STAGE_I2C_BUS_INIT,
    BOOT_STAGE_I2C_SCAN,
    BOOT_STAGE_OLED_INIT,
    BOOT_STAGE_IO_EXPANDER_INIT,
    BOOT_STAGE_COMPLETE,
    BOOT_STAGE_RUNNING
} BootStage_e;

/*============================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize restart logging system
 * Call this as early as possible in main
 */
void restart_log_init(void);

/**
 * @brief Update current boot stage
 * Call this at each initialization step
 */
void restart_log_set_stage(BootStage_e stage);

/**
 * @brief Mark boot as successful (call after all init complete)
 */
void restart_log_boot_complete(void);

/**
 * @brief Print restart information and statistics
 */
void restart_log_print(void);

/**
 * @brief Get restart reason string
 */
const char* restart_log_get_reason(void);

/**
 * @brief Check if this is a restart (vs cold boot)
 */
bool restart_log_is_restart(void);

/**
 * @brief Get boot stage name
 */
const char* restart_log_get_stage_name(BootStage_e stage);

#endif /* RESTART_LOG_H */

