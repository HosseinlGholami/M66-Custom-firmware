/**
 * @file    sms.c
 * @brief   SMS command bridge
 */

#include "sms.h"

#include "com/com.h"
#include "config/module_config.h"
#include "ril.h"
#include "ril_sms.h"
#include "uart/uart.h"
#include "ql_stdlib.h"

#ifdef MODULE_SMS_ENABLED

#define SMS_MAX_COMMAND_LEN  COM_CMD_MAX_LEN
#define SMS_MAX_REPLY_LEN    160

static bool g_sms_initialized = FALSE;
static char g_sms_reply_buffer[SMS_MAX_REPLY_LEN];
static u32 g_sms_reply_len = 0;
static bool g_sms_reply_truncated = FALSE;

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

    ret = RIL_SMS_SetStorage(RIL_SMS_STORAGE_TYPE_MT, NULL, NULL);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to set storage, ret=%d\r\n", ret);
        return ret;
    }

    return 0;
}

static void sms_send_reply(const char* phone_number, const char* text)
{
    s32 ret;
    u32 msg_ref = 0;
    u32 msg_len;

    if (phone_number == NULL || text == NULL) {
        return;
    }

    msg_len = Ql_strlen(text);
    if (msg_len == 0) {
        return;
    }

    ret = RIL_SMS_SendSMS_Text((char*)phone_number,
                               (u8)Ql_strlen(phone_number),
                               LIB_SMS_CHARSET_GSM,
                               (u8*)text,
                               msg_len,
                               &msg_ref);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to send reply to %s, ret=%d\r\n", phone_number, ret);
    } else {
        APP_DEBUG("SMS: reply sent to %s, ref=%d\r\n", phone_number, msg_ref);
    }
}

s32 sms_init(void)
{
    Ql_memset(g_sms_reply_buffer, 0, sizeof(g_sms_reply_buffer));
    g_sms_reply_len = 0;
    g_sms_reply_truncated = FALSE;
    g_sms_initialized = TRUE;
    return 0;
}

s32 sms_handle_new_sms(u32 sms_index)
{
    ST_RIL_SMS_TextInfo text_info;
    char command_buffer[SMS_MAX_COMMAND_LEN];
    const char* sender;
    s32 ret;

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
    APP_DEBUG("SMS: command from %s -> %s\r\n", sender, command_buffer);

    Ql_memset(g_sms_reply_buffer, 0, sizeof(g_sms_reply_buffer));
    g_sms_reply_len = 0;
    g_sms_reply_truncated = FALSE;

    ret = com_process_command_with_callback(command_buffer,
                                            Ql_strlen(command_buffer),
                                            sms_response_capture);
    if (g_sms_reply_len == 0) {
        Ql_sprintf(g_sms_reply_buffer, "%s\r\n", com_result_string((ComResult_e)ret));
    } else if (g_sms_reply_truncated) {
        u32 len = Ql_strlen(g_sms_reply_buffer);
        if (len > 4) {
            Ql_memcpy(g_sms_reply_buffer + len - 4, "...", 3);
            g_sms_reply_buffer[len - 1] = '\0';
        }
    }

    sms_send_reply(sender, g_sms_reply_buffer);

    ret = RIL_SMS_DeleteSMS(sms_index, RIL_SMS_DEL_INDEXED_MSG);
    if (ret != RIL_AT_SUCCESS) {
        APP_DEBUG("SMS: failed to delete SMS index %d, ret=%d\r\n", sms_index, ret);
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

#endif
