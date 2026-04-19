/**
 * @file    sms.c
 * @brief   SMS command bridge orchestration
 */

#include "sms.h"

#include "com/com.h"
#include "config/module_config.h"
#include "sms_internal.h"
#include "ril.h"
#include "ril_sms.h"
#include "uart/uart.h"
#include "ql_stdlib.h"
#include "ql_system.h"

#ifdef MODULE_SMS_ENABLED

#define SMS_MAX_COMMAND_LEN  COM_CMD_MAX_LEN
#define SMS_MAX_REPLY_LEN    160
#define SMS_POLL_INTERVAL_MS 2000

static bool g_sms_initialized = FALSE;
static char g_sms_reply_buffer[SMS_MAX_REPLY_LEN];
static u32 g_sms_reply_len = 0;
static bool g_sms_reply_truncated = FALSE;
static u64 g_sms_last_poll_ms = 0;

static s32 sms_prepare_storage(void);

static s32 sms_configure_runtime(void)
{
    s32 ret;
    u8 curr_storage = 0;
    u32 used = 0;
    u32 total = 0;

    ret = sms_prepare_storage();
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: storage setup failed, ret=%d\r\n", ret);
        return ret;
    }
    APP_DEBUG("SMS: storage set to SM\r\n");

    ret = Ql_RIL_SendATCmd("AT+CMGF=1", Ql_strlen("AT+CMGF=1"), NULL, NULL, 0);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to set text mode, ret=%d\r\n", ret);
        return ret;
    }
    APP_DEBUG("SMS: text mode enabled\r\n");

    ret = Ql_RIL_SendATCmd("AT+CNMI=2,1", Ql_strlen("AT+CNMI=2,1"), NULL, NULL, 0);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to enable new SMS indication, ret=%d\r\n", ret);
        return ret;
    }
    APP_DEBUG("SMS: new SMS indication enabled (CNMI=2,1)\r\n");

    ret = RIL_SMS_GetStorage(&curr_storage, &used, &total);
    if (ret == RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: storage state before cleanup, used=%d total=%d\r\n", used, total);
    }

    /* Control-mode device: clear legacy inbox so new command SMS can always be received. */
    ret = RIL_SMS_DeleteSMS(0, RIL_SMS_DEL_ALL_MSG);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to clear inbox at init, ret=%d\r\n", ret);
        return ret;
    }
    APP_DEBUG("SMS: inbox cleared at startup\r\n");

    ret = RIL_SMS_GetStorage(&curr_storage, &used, &total);
    if (ret == RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: storage state after cleanup, used=%d total=%d\r\n", used, total);
    }

    return 0;
}

static void sms_response_capture(const char* response, u32 len)
{
    u32 copy_len;

    if (response == NULL || len == 0 || g_sms_reply_len >= (SMS_MAX_REPLY_LEN - 1)) {
        return;
    }

    copy_len = len;
    if ((g_sms_reply_len + copy_len) >= (SMS_MAX_REPLY_LEN - 1)) {
        copy_len = (SMS_MAX_REPLY_LEN - 1) - g_sms_reply_len;
        g_sms_reply_truncated = TRUE;
    }

    Ql_memcpy(g_sms_reply_buffer + g_sms_reply_len, response, copy_len);
    g_sms_reply_len += copy_len;
    g_sms_reply_buffer[g_sms_reply_len] = '\0';
}

static void sms_trim_text(char* text)
{
    u32 len;
    u32 start = 0;
    u32 end;

    if (text == NULL) {
        return;
    }

    len = Ql_strlen(text);
    while (start < len &&
           (text[start] == ' ' || text[start] == '\r' || text[start] == '\n' || text[start] == '\t')) {
        start++;
    }

    end = len;
    while (end > start &&
           (text[end - 1] == ' ' || text[end - 1] == '\r' || text[end - 1] == '\n' || text[end - 1] == '\t')) {
        end--;
    }

    if (start > 0) {
        u32 i;
        for (i = 0; i < (end - start); i++) {
            text[i] = text[start + i];
        }
    }

    text[end - start] = '\0';
}

static s32 sms_prepare_storage(void)
{
    s32 ret;

    /* M66 SDK ril_sms adapter only supports SM storage type. */
    ret = RIL_SMS_SetStorage(RIL_SMS_STORAGE_TYPE_SM, NULL, NULL);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to set storage, ret=%d\r\n", ret);
        return ret;
    }

    return 0;
}

static void sms_send_reply(const char* phone_number, const char* text)
{
    u32 msg_len;
    s32 ret;

    if (phone_number == NULL || text == NULL) {
        return;
    }

    msg_len = Ql_strlen(text);
    if (msg_len == 0) {
        return;
    }

    ret = sms_send_with_candidates(phone_number, text, "reply");
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to send reply to %s, ret=%d\r\n", phone_number, ret);
    }
}

s32 sms_init(void)
{
    s32 ret;

    Ql_memset(g_sms_reply_buffer, 0, sizeof(g_sms_reply_buffer));
    g_sms_reply_len = 0;
    g_sms_reply_truncated = FALSE;
    g_sms_initialized = TRUE;

    ret = sms_configure_runtime();
    if (ret == RIL_AT_UNINITIALIZED) {
        APP_DEBUG("SMS: runtime config deferred (RIL not ready yet)\r\n");
    } else if (ret != 0) {
        APP_DEBUG("SMS: runtime config failed, ret=%d\r\n", ret);
    } else {
        APP_DEBUG("SMS: runtime config OK\r\n");
    }

    return ret;
}

s32 sms_handle_new_sms(u32 sms_index)
{
    ST_RIL_SMS_TextInfo text_info;
    char command_buffer[SMS_MAX_COMMAND_LEN];
    char translated_command[SMS_MAX_COMMAND_LEN];
    const char* sender;
    s32 ret;
    s32 tr_ret;

    APP_DEBUG("SMS: new SMS URC received, index=%d\r\n", sms_index);

    if (!g_sms_initialized) {
        sms_init();
    }

    ret = sms_prepare_storage();
    if (ret != 0) {
        return ret;
    }

    Ql_memset(&text_info, 0, sizeof(text_info));
    ret = RIL_SMS_ReadSMS_Text(sms_index, LIB_SMS_CHARSET_GSM, &text_info);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to read SMS index %d, ret=%d\r\n", sms_index, ret);
        return ret;
    }

    if (text_info.param.deliverParam.length >= sizeof(command_buffer)) {
        Ql_sprintf(g_sms_reply_buffer, "ERROR: SMS command too long\r\n");
        sms_send_reply(text_info.param.deliverParam.oa, g_sms_reply_buffer);
        RIL_SMS_DeleteSMS(sms_index, RIL_SMS_DEL_INDEXED_MSG);
        return -2;
    }

    Ql_memcpy(command_buffer,
              text_info.param.deliverParam.data,
              text_info.param.deliverParam.length);
    command_buffer[text_info.param.deliverParam.length] = '\0';
    sms_trim_text(command_buffer);

    if (Ql_strlen(command_buffer) == 0) {
        APP_DEBUG("SMS: empty SMS command at index %d\r\n", sms_index);
        RIL_SMS_DeleteSMS(sms_index, RIL_SMS_DEL_INDEXED_MSG);
        return -3;
    }

    if (command_buffer[Ql_strlen(command_buffer) - 1] != COM_CMD_TERMINATOR) {
        u32 len = Ql_strlen(command_buffer);
        if (len < (sizeof(command_buffer) - 1)) {
            command_buffer[len] = COM_CMD_TERMINATOR;
            command_buffer[len + 1] = '\0';
        }
    }

    sender = text_info.param.deliverParam.oa;
    if (!sms_is_authorized_sender(sender)) {
        APP_DEBUG("SMS: sender %s is not authorized, message ignored\r\n", sender);
        RIL_SMS_DeleteSMS(sms_index, RIL_SMS_DEL_INDEXED_MSG);
        return -4;
    }

    APP_DEBUG("SMS: command from %s -> %s\r\n", sender, command_buffer);

    Ql_memset(g_sms_reply_buffer, 0, sizeof(g_sms_reply_buffer));
    g_sms_reply_len = 0;
    g_sms_reply_truncated = FALSE;
    Ql_memset(translated_command, 0, sizeof(translated_command));

    tr_ret = sms_translate_param_command(command_buffer,
                                         translated_command,
                                         sizeof(translated_command));
    if (tr_ret < 0) {
        Ql_sprintf(g_sms_reply_buffer,
                   "ERR: bad key. SMS keys:\r\n1-power\r\n2-Relay1\r\n3-Relay2\r\n4-Relay3\r\n5-Relay4\r\n6-phone1\r\n7-phone2\r\n8-phone3\r\n");
        ret = COM_ERR_INVALID_KEY;
    } else if (tr_ret > 0) {
        APP_DEBUG("SMS: translated command -> %s\r\n", translated_command);
        ret = com_process_command_with_callback(translated_command,
                                                Ql_strlen(translated_command),
                                                sms_response_capture);
    } else if (sms_is_compact_list_command(command_buffer)) {
        sms_build_compact_list_reply(g_sms_reply_buffer, sizeof(g_sms_reply_buffer));
        ret = COM_OK;
    } else {
        ret = com_process_command_with_callback(command_buffer,
                                                Ql_strlen(command_buffer),
                                                sms_response_capture);
    }

    if (g_sms_reply_len == 0 && g_sms_reply_buffer[0] == '\0') {
        Ql_sprintf(g_sms_reply_buffer, "%s\r\n", com_result_string((ComResult_e)ret));
    } else if (g_sms_reply_truncated) {
        u32 len = Ql_strlen(g_sms_reply_buffer);
        if (len > 4) {
            Ql_memcpy(g_sms_reply_buffer + len - 4, "...", 3);
            g_sms_reply_buffer[len - 1] = '\0';
        }
    }

    APP_DEBUG("SMS: reply payload -> %s\r\n", g_sms_reply_buffer);
    sms_send_reply(sender, g_sms_reply_buffer);

    ret = RIL_SMS_DeleteSMS(sms_index, RIL_SMS_DEL_INDEXED_MSG);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to delete SMS index %d, ret=%d\r\n", sms_index, ret);
    }

    return 0;
}

s32 sms_poll_inbox(void)
{
    u64 now_ms;
    u8 curr_storage = 0;
    u32 used = 0;
    u32 total = 0;
    u32 idx;
    s32 ret;

    if (!g_sms_initialized) {
        return 0;
    }

    now_ms = Ql_GetMsSincePwrOn();
    if ((now_ms - g_sms_last_poll_ms) < SMS_POLL_INTERVAL_MS) {
        return 0;
    }
    g_sms_last_poll_ms = now_ms;

    ret = sms_prepare_storage();
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: poll storage setup failed, ret=%d\r\n", ret);
        return ret;
    }

    ret = RIL_SMS_GetStorage(&curr_storage, &used, &total);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: poll get storage failed, ret=%d\r\n", ret);
        return ret;
    }

    if (used == 0 || total == 0) {
        return 0;
    }

    if (used >= total) {
        APP_DEBUG("SMS: storage full (%d/%d), waiting for cleanup/processing\r\n", used, total);
    }

    APP_DEBUG("SMS: polling inbox, used=%d total=%d\r\n", used, total);

    for (idx = 1; idx <= total; idx++) {
        ST_RIL_SMS_TextInfo text_info;

        Ql_memset(&text_info, 0, sizeof(text_info));
        ret = RIL_SMS_ReadSMS_Text(idx, LIB_SMS_CHARSET_GSM, &text_info);
        if (ret != RIL_AT_SUCCESS) {
            continue;
        }

        if (text_info.status != RIL_SMS_STATUS_TYPE_REC_UNREAD) {
            continue;
        }

        APP_DEBUG("SMS: polling picked unread index=%d\r\n", idx);
        return sms_handle_new_sms(idx);
    }

    return 0;
}

s32 sms_send_text(const char* phone_number, const char* text)
{
    s32 ret;

    if (phone_number == NULL || text == NULL) {
        return -1;
    }

    if (phone_number[0] == '\0' || text[0] == '\0') {
        return -2;
    }

    if (!g_sms_initialized) {
        sms_init();
    }

    ret = sms_send_with_candidates(phone_number, text, "text");
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to send text to '%s', ret=%d\r\n", phone_number, ret);
        return ret;
    }

    return 0;
}

#else

s32 sms_init(void)
{
    return 0;
}

s32 sms_handle_new_sms(u32 sms_index)
{
    (void)sms_index;
    return 0;
}

s32 sms_poll_inbox(void)
{
    return 0;
}

s32 sms_send_text(const char* phone_number, const char* text)
{
    (void)phone_number;
    (void)text;
    return 0;
}

#endif
