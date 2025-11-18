/**
 * @file    restart_log.c
 * @brief   Restart Detection and Logging Implementation
 * @author  Hossein Gholami
 * @date    2025-11-17
 */

#include "restart_log.h"
#include "ql_system.h"
#include "ql_stdlib.h"
#include "ql_fs.h"
#include "../uart/uart.h"

/*============================================================================
 * Configuration
 *===========================================================================*/

#define RESTART_LOG_FILE    "restart.log"
#define RESTART_LOG_MAGIC   0xDEADBEEF

/*============================================================================
 * Data Structures
 *===========================================================================*/

typedef struct {
    u32 magic;                  /* Magic number for validation */
    u32 restart_count;          /* Total restart counter */
    u32 boot_fail_count;        /* Failed boot attempts */
    BootStage_e last_stage;     /* Last successful boot stage */
    bool watchdog_reset;        /* Was last reset from watchdog? */
} RestartLogData_t;

/*============================================================================
 * Private Data
 *===========================================================================*/

static RestartLogData_t g_restart_data;
static BootStage_e g_current_stage = BOOT_STAGE_START;
static bool g_initialized = FALSE;

static const char* g_stage_names[] = {
    "START",
    "UART_INIT",
    "PARAM_INIT",
    "GPIO_INIT",
    "COM_INIT",
    "I2C_BUS_INIT",
    "I2C_SCAN",
    "OLED_INIT",
    "IO_EXPANDER_INIT",
    "COMPLETE",
    "RUNNING"
};

/*============================================================================
 * Private Functions
 *===========================================================================*/

/**
 * @brief Load restart data from file
 */
static void load_restart_data(void)
{
    s32 handle;
    s32 ret;
    u32 read_bytes;
    
    /* Try to open existing file */
    handle = Ql_FS_Open(RESTART_LOG_FILE, QL_FS_READ_ONLY);
    if (handle < 0) {
        /* File doesn't exist - first boot */
        APP_DEBUG("[RESTART] First boot - initializing restart log\r\n");
        Ql_memset(&g_restart_data, 0, sizeof(g_restart_data));
        g_restart_data.magic = RESTART_LOG_MAGIC;
        return;
    }
    
    /* Read existing data */
    ret = Ql_FS_Read(handle, (u8*)&g_restart_data, sizeof(g_restart_data), &read_bytes);
    Ql_FS_Close(handle);
    
    if (ret < 0 || read_bytes != sizeof(g_restart_data) || 
        g_restart_data.magic != RESTART_LOG_MAGIC) {
        /* Corrupted data - reset */
        APP_DEBUG("[RESTART] Log corrupted - resetting\r\n");
        Ql_memset(&g_restart_data, 0, sizeof(g_restart_data));
        g_restart_data.magic = RESTART_LOG_MAGIC;
        return;
    }
    
    /* Valid data loaded */
    APP_DEBUG("[RESTART] Previous restart data loaded\r\n");
    APP_DEBUG("╔══════════════════════════════════════════════╗\r\n");
    APP_DEBUG("║      PREVIOUS RESTART INFORMATION           ║\r\n");
    APP_DEBUG("╚══════════════════════════════════════════════╝\r\n");
    APP_DEBUG("  Total Restarts: %u\r\n", g_restart_data.restart_count);
    APP_DEBUG("  Failed Boots:   %u\r\n", g_restart_data.boot_fail_count);
    APP_DEBUG("  Last Stage:     %s\r\n", restart_log_get_stage_name(g_restart_data.last_stage));
    APP_DEBUG("  Watchdog Reset: %s\r\n", g_restart_data.watchdog_reset ? "YES ⚠️" : "NO");
    APP_DEBUG("\r\n");
}

/**
 * @brief Save restart data to file
 */
static void save_restart_data(void)
{
    s32 handle;
    s32 ret;
    u32 written;
    
    /* Delete old file */
    Ql_FS_Delete(RESTART_LOG_FILE);
    
    /* Create new file */
    handle = Ql_FS_Open(RESTART_LOG_FILE, QL_FS_CREATE | QL_FS_READ_WRITE);
    if (handle < 0) {
        APP_DEBUG("[RESTART] Failed to create log file: %d\r\n", handle);
        return;
    }
    
    /* Write data */
    ret = Ql_FS_Write(handle, (u8*)&g_restart_data, sizeof(g_restart_data), &written);
    Ql_FS_Close(handle);
    
    if (ret < 0 || written != sizeof(g_restart_data)) {
        APP_DEBUG("[RESTART] Failed to write log: %d\r\n", ret);
    }
}

/*============================================================================
 * Public API Implementation
 *===========================================================================*/

/**
 * @brief Initialize restart logging
 */
void restart_log_init(void)
{
    if (g_initialized) {
        return;
    }
    
    /* Load previous restart data */
    load_restart_data();
    
    /* Check if this is a restart (not first boot) */
    if (g_restart_data.restart_count > 0) {
        /* This is a restart */
        g_restart_data.restart_count++;
        
        /* Check if previous boot completed */
        if (g_restart_data.last_stage != BOOT_STAGE_RUNNING) {
            g_restart_data.boot_fail_count++;
            APP_DEBUG("\r\n");
            APP_DEBUG("╔════════════════════════════════════════════╗\r\n");
            APP_DEBUG("║  ⚠️  UNEXPECTED RESTART DETECTED           ║\r\n");
            APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
            APP_DEBUG("\r\n");
            APP_DEBUG("⚠️  System restarted unexpectedly!\r\n");
            APP_DEBUG("   Restart count: %d\r\n", g_restart_data.restart_count);
            APP_DEBUG("   Boot failures: %d\r\n", g_restart_data.boot_fail_count);
            APP_DEBUG("   Last stage:    %s\r\n", 
                     restart_log_get_stage_name(g_restart_data.last_stage));
            APP_DEBUG("\r\n");
            APP_DEBUG("Possible causes:\r\n");
            APP_DEBUG("  • Watchdog timeout (no feeding)\r\n");
            APP_DEBUG("  • Stack overflow\r\n");
            APP_DEBUG("  • NULL pointer access\r\n");
            APP_DEBUG("  • Memory corruption\r\n");
            APP_DEBUG("  • I2C bus lockup\r\n");
            APP_DEBUG("  • Power supply issue\r\n");
            APP_DEBUG("\r\n");
        }
    } else {
        /* First boot */
        g_restart_data.restart_count = 1;
    }
    
    /* Reset stage */
    g_restart_data.last_stage = BOOT_STAGE_START;
    
    /* Save updated data */
    save_restart_data();
    
    g_initialized = TRUE;
}

/**
 * @brief Update boot stage
 */
void restart_log_set_stage(BootStage_e stage)
{
    if (!g_initialized) {
        return;
    }
    
    g_current_stage = stage;
    g_restart_data.last_stage = stage;
    
    APP_DEBUG("[RESTART] Boot stage: %s\r\n", restart_log_get_stage_name(stage));
    
    /* Save progress */
    save_restart_data();
}

/**
 * @brief Mark boot complete
 */
void restart_log_boot_complete(void)
{
    if (!g_initialized) {
        return;
    }
    
    g_restart_data.last_stage = BOOT_STAGE_RUNNING;
    g_restart_data.boot_fail_count = 0;  /* Reset failure counter on successful boot */
    
    save_restart_data();
    
    APP_DEBUG("\r\n");
    APP_DEBUG("╔════════════════════════════════════════════╗\r\n");
    APP_DEBUG("║  ✅ BOOT SEQUENCE COMPLETE                 ║\r\n");
    APP_DEBUG("╚════════════════════════════════════════════╝\r\n");
    APP_DEBUG("\r\n");
    APP_DEBUG("System initialized successfully\r\n");
    APP_DEBUG("Total boots: %d\r\n", g_restart_data.restart_count);
    APP_DEBUG("\r\n");
}

/**
 * @brief Print restart information
 */
void restart_log_print(void)
{
    if (!g_initialized) {
        APP_DEBUG("[RESTART] Not initialized\r\n");
        return;
    }
    
    APP_DEBUG("\r\n");
    APP_DEBUG("=== Restart Log Information ===\r\n");
    APP_DEBUG("Total restarts:   %d\r\n", g_restart_data.restart_count);
    APP_DEBUG("Boot failures:    %d\r\n", g_restart_data.boot_fail_count);
    APP_DEBUG("Current stage:    %s\r\n", restart_log_get_stage_name(g_current_stage));
    APP_DEBUG("Last boot stage:  %s\r\n", restart_log_get_stage_name(g_restart_data.last_stage));
    APP_DEBUG("\r\n");
}

/**
 * @brief Get restart reason string
 */
const char* restart_log_get_reason(void)
{
    if (!g_initialized || g_restart_data.restart_count == 1) {
        return "FIRST_BOOT";
    }
    
    if (g_restart_data.last_stage != BOOT_STAGE_RUNNING) {
        return "BOOT_FAILURE";
    }
    
    if (g_restart_data.watchdog_reset) {
        return "WATCHDOG_RESET";
    }
    
    return "NORMAL_RESTART";
}

/**
 * @brief Check if this is a restart
 */
bool restart_log_is_restart(void)
{
    return (g_initialized && g_restart_data.restart_count > 1);
}

/**
 * @brief Get stage name
 */
const char* restart_log_get_stage_name(BootStage_e stage)
{
    if (stage >= 0 && stage < (sizeof(g_stage_names) / sizeof(g_stage_names[0]))) {
        return g_stage_names[stage];
    }
    return "UNKNOWN";
}

