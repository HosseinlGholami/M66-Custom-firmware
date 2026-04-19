/**
 * @file    logic.c
 * @brief   Runtime message handling and command parsing
 */

#include "logic.h"

#include "com/com.h"
#include "config/module_config.h"
#include "gpio/gpio.h"
#include "param/param.h"
#include "ril.h"
#include "ril_telephony.h"
#include "sms/sms.h"
#include "uart/uart.h"
#include "ql_stdlib.h"
#include "ql_system.h"
#include "ql_timer.h"

#define LOGIC_TIMER_ID           (TIMER_ID_USER_START + 10)
#define LOGIC_TIMER_INTERVAL_MS  50
#define LOGIC_PRESS_HOLD_MS      750

static char g_cmd_buffer[128];
static u32 g_cmd_buffer_len = 0;
static bool g_logic_started = FALSE;
static u8 g_logic_last_mask = 0;
static u64 g_logic_last_change_ms = 0;
static bool g_logic_action_handled = FALSE;

static void logic_send_alert_sms(ParamKey_e phone_key, u8 mask)
{
    char phone_number[PARAM_STRING_MAX_LEN];
    char message[80];
    s32 ret;

    Ql_memset(phone_number, 0, sizeof(phone_number));
    Ql_memset(message, 0, sizeof(message));

    ret = param_get_string(phone_key, phone_number, sizeof(phone_number));
    if (ret != 0 || phone_number[0] == '\0') {
        APP_DEBUG("LOGIC: SMS target missing for param '%s' (ret=%d)\r\n",
                  param_get_name(phone_key), ret);
        return;
    }

    Ql_sprintf(message, "ALERT: input combo 0x%02X triggered", mask);
    ret = sms_send_text(phone_number, message);
    if (ret != 0) {
        APP_DEBUG("LOGIC: failed to send combo SMS to %s (ret=%d)\r\n",
                  phone_number, ret);
        return;
    }

    APP_DEBUG("LOGIC: combo SMS sent to %s for mask 0x%02X\r\n",
              phone_number, mask);
}

static void logic_toggle_output_param(ParamKey_e key)
{
    s8 value = 0;

    if (param_get_int8(key, &value) != 0) {
        APP_DEBUG("LOGIC: failed to read output param '%s'\r\n", param_get_name(key));
        return;
    }

    param_set_int8(key, value ? 0 : 1);
}

static u8 logic_get_expander_press_mask(void)
{
    static const ParamKey_e input_params[4] = {
        PARAM_IO_EXP_IN0,
        PARAM_IO_EXP_IN1,
        PARAM_IO_EXP_IN2,
        PARAM_IO_EXP_IN3
    };
    u8 mask = 0;
    u32 i;

    for (i = 0; i < 4; i++) {
        s8 value = 0;

        if (param_get_int8(input_params[i], &value) != 0) {
            continue;
        }

        if (value == 1) {
            mask |= (1 << i);
        }
    }

    return mask;
}

static void logic_handle_expander_combo(u8 mask)
{
    switch (mask) {
    case 0x01:
        APP_DEBUG("LOGIC: io_exp_in0 press detected, toggling io_exp_out0\r\n");
        logic_toggle_output_param(PARAM_IO_EXP_OUT0);
        break;

    case 0x02:
        APP_DEBUG("LOGIC: io_exp_in1 press detected, toggling io_exp_out1\r\n");
        logic_toggle_output_param(PARAM_IO_EXP_OUT2);
        break;

    case 0x04:
        APP_DEBUG("LOGIC: io_exp_in2 press detected, toggling io_exp_out2\r\n");
        logic_toggle_output_param(PARAM_IO_EXP_OUT3);
        break;
    case 0x03:
        APP_DEBUG("LOGIC: io_exp_in0 + io_exp_in2 press detected, toggling io_exp_out3\r\n");
        logic_toggle_output_param(PARAM_IO_EXP_OUT1);
        break;
   
    default:
        logic_send_alert_sms(PARAM_ALERT_PHONE_1, mask);
        APP_DEBUG("LOGIC: expander combo 0x%02X pressed for %d ms (log only)\r\n",
                  mask, LOGIC_PRESS_HOLD_MS);
        break;
    }
}

static void logic_process_inputs(void)
{
    u8 current_mask;
    u64 now_ms;

    sms_poll_inbox();
    gpio_poll_inputs();

    if (!g_logic_started) {
        return;
    }

    current_mask = logic_get_expander_press_mask();
    now_ms = Ql_GetMsSincePwrOn();

    if (current_mask != g_logic_last_mask) {
        APP_DEBUG("LOGIC: expander mask changed 0x%02X -> 0x%02X\r\n",
                  g_logic_last_mask,
                  current_mask);
        g_logic_last_mask = current_mask;
        g_logic_last_change_ms = now_ms;
        g_logic_action_handled = FALSE;
        return;
    }

    if (current_mask == 0 || g_logic_action_handled) {
        return;
    }

    if ((now_ms - g_logic_last_change_ms) < LOGIC_PRESS_HOLD_MS) {
        return;
    }

    logic_handle_expander_combo(current_mask);
    g_logic_action_handled = TRUE;
}

static void logic_timer_callback(u32 timer_id, void* param)
{
    (void)timer_id;
    (void)param;
    logic_process_inputs();
}

void logic_init(void)
{
    Ql_memset(g_cmd_buffer, 0, sizeof(g_cmd_buffer));
    g_cmd_buffer_len = 0;
    g_logic_started = FALSE;
    g_logic_last_mask = 0;
    g_logic_last_change_ms = 0;
    g_logic_action_handled = FALSE;
    sms_init();
}

void logic_start(void)
{
    s32 ret;

    if (g_logic_started) {
        return;
    }

    g_logic_last_mask = logic_get_expander_press_mask();
    g_logic_last_change_ms = Ql_GetMsSincePwrOn();
    g_logic_action_handled = FALSE;
    g_logic_started = TRUE;

    ret = Ql_Timer_Register(LOGIC_TIMER_ID, logic_timer_callback, NULL);
    if (ret == 0) {
        ret = Ql_Timer_Start(LOGIC_TIMER_ID, LOGIC_TIMER_INTERVAL_MS, TRUE);
    }

    if (ret != 0) {
        APP_DEBUG("LOGIC: failed to start periodic timer, ret=%d\r\n", ret);
    }
}

void logic_print_ready(void)
{
    APP_DEBUG("\r\n");
    APP_DEBUG("System ready. Type commands or waiting for events...\r\n");
    APP_DEBUG("Examples: S,4,1! or G,4! or L!\r\n");
    APP_DEBUG("IO Expander: S,io_exp_out0,1! or G,io_exp_in0! or I!\r\n");
    APP_DEBUG("SMS control: send same command text by SMS (e.g. G,10! or S,8,1!)\r\n");
    APP_DEBUG("\r\n");
}

void logic_uart_callback(u8* data, u32 len)
{
    u32 i;

    for (i = 0; i < len; i++) {
        char ch = data[i];

        APP_DEBUG("%c", ch);

        if (g_cmd_buffer_len < sizeof(g_cmd_buffer) - 1) {
            g_cmd_buffer[g_cmd_buffer_len++] = ch;

            if (ch == '!') {
                g_cmd_buffer[g_cmd_buffer_len] = '\0';
                APP_DEBUG("\r\n");
                com_process_command(g_cmd_buffer, g_cmd_buffer_len);
                g_cmd_buffer_len = 0;
            }
        } else {
            APP_DEBUG("\r\nERROR: Command buffer overflow\r\n");
            g_cmd_buffer_len = 0;
        }
    }
}

void logic_handle_message(const ST_MSG* msg)
{
    if (msg == NULL) {
        return;
    }

    switch (msg->message)
    {
    case MSG_ID_RIL_READY:
        APP_DEBUG("<-- RIL is ready -->\r\n");
        Ql_RIL_Initialize();
        break;

    case MSG_ID_URC_INDICATION:
        switch (msg->param1)
        {
        case URC_SYS_INIT_STATE_IND:
            APP_DEBUG("<-- Sys Init Status: %d -->\r\n", msg->param2);
            if (msg->param2 == SYS_STATE_SMSOK) {
                s32 sms_ret = sms_init();
                APP_DEBUG("LOGIC: SMS runtime init at SYS_STATE_SMSOK returned %d\r\n", sms_ret);
            }
            break;

        case URC_SIM_CARD_STATE_IND:
            APP_DEBUG("<-- SIM Card Status: %d -->\r\n", msg->param2);
            break;

        case URC_GSM_NW_STATE_IND:
            APP_DEBUG("<-- GSM Network Status: %d -->\r\n", msg->param2);
            break;

        case URC_GPRS_NW_STATE_IND:
            APP_DEBUG("<-- GPRS Network Status: %d -->\r\n", msg->param2);
            break;

        case URC_CFUN_STATE_IND:
            APP_DEBUG("<-- CFUN Status: %d -->\r\n", msg->param2);
            break;

        case URC_COMING_CALL_IND:
            {
                ST_ComingCall* call_info = (ST_ComingCall*)msg->param2;
                APP_DEBUG("<-- Incoming call: %s, type:%d -->\r\n",
                          call_info->phoneNumber, call_info->type);
            }
            break;

        case URC_CALL_STATE_IND:
            APP_DEBUG("<-- Call state: %d -->\r\n", msg->param2);
            break;

        case URC_NEW_SMS_IND:
            APP_DEBUG("<-- New SMS arrived: index=%d -->\r\n", msg->param2);
            sms_handle_new_sms(msg->param2);
            break;

        default:
#ifdef MODULE_DEBUG_VERBOSE
            APP_DEBUG("<-- URC: type=%d -->\r\n", msg->param1);
#endif
            break;
        }
        break;

    default:
        break;
    }
}
