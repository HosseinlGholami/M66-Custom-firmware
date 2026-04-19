/**
 * @file    sms_phone.c
 * @brief   SMS phone formatting and send helpers
 */

#include "config/module_config.h"

#ifdef MODULE_SMS_ENABLED

#include "sms_internal.h"

#include "ril.h"
#include "ril_sms.h"
#include "uart/uart.h"
#include "ql_stdlib.h"

static bool sms_is_digit(char ch)
{
    return (ch >= '0' && ch <= '9') ? TRUE : FALSE;
}

static void sms_sanitize_phone(const char* input, char* output, u32 output_len)
{
    u32 in_idx = 0;
    u32 out_idx = 0;

    if (output == NULL || output_len == 0) {
        return;
    }

    output[0] = '\0';
    if (input == NULL) {
        return;
    }

    while (input[in_idx] != '\0' && out_idx < (output_len - 1)) {
        char ch = input[in_idx++];

        if (sms_is_digit(ch)) {
            output[out_idx++] = ch;
            continue;
        }

        if (ch == '+' && out_idx == 0) {
            output[out_idx++] = ch;
            continue;
        }
    }

    output[out_idx] = '\0';
}

static bool sms_add_candidate(char candidates[][SMS_PHONE_MAX_LEN],
                              u32* count,
                              const char* number)
{
    u32 i;
    u32 len;

    if (candidates == NULL || count == NULL || number == NULL || number[0] == '\0') {
        return FALSE;
    }

    len = Ql_strlen(number);
    if (len == 0 || len >= SMS_PHONE_MAX_LEN) {
        return FALSE;
    }

    for (i = 0; i < *count; i++) {
        if (Ql_strcmp(candidates[i], number) == 0) {
            return TRUE;
        }
    }

    if (*count >= SMS_MAX_CANDIDATES) {
        return FALSE;
    }

    Ql_strcpy(candidates[*count], number);
    (*count)++;
    return TRUE;
}

void sms_build_candidates(const char* phone_number,
                          char candidates[][SMS_PHONE_MAX_LEN],
                          u32* count)
{
    char cleaned[SMS_PHONE_MAX_LEN];
    char temp[SMS_PHONE_MAX_LEN];

    if (count == NULL) {
        return;
    }

    *count = 0;
    Ql_memset(cleaned, 0, sizeof(cleaned));
    Ql_memset(temp, 0, sizeof(temp));

    sms_sanitize_phone(phone_number, cleaned, sizeof(cleaned));
    if (cleaned[0] == '\0') {
        return;
    }

    sms_add_candidate(candidates, count, cleaned);

    if (cleaned[0] == '+') {
        return;
    }

    if (cleaned[0] == '0' && cleaned[1] != '\0') {
        Ql_sprintf(temp, "+%s%s", SMS_DEFAULT_CC, cleaned + 1);
        sms_add_candidate(candidates, count, temp);
    }

    Ql_sprintf(temp, "+%s", cleaned);
    sms_add_candidate(candidates, count, temp);
}

bool sms_candidates_match(char lhs[][SMS_PHONE_MAX_LEN],
                          u32 lhs_count,
                          char rhs[][SMS_PHONE_MAX_LEN],
                          u32 rhs_count)
{
    u32 i;
    u32 j;

    for (i = 0; i < lhs_count; i++) {
        for (j = 0; j < rhs_count; j++) {
            if (Ql_strcmp(lhs[i], rhs[j]) == 0) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

s32 sms_send_with_candidates(const char* phone_number, const char* text, const char* tag)
{
    char candidates[SMS_MAX_CANDIDATES][SMS_PHONE_MAX_LEN];
    u32 candidate_count = 0;
    u32 i;
    s32 last_ret = RIL_AT_INVALID_PARAM;
    u32 msg_ref = 0;
    u32 text_len;

    if (phone_number == NULL || text == NULL) {
        return RIL_AT_INVALID_PARAM;
    }

    text_len = Ql_strlen(text);
    if (text_len == 0) {
        return RIL_AT_INVALID_PARAM;
    }

    sms_build_candidates(phone_number, candidates, &candidate_count);
    if (candidate_count == 0) {
        APP_DEBUG("SMS: no valid phone candidate from '%s'\r\n", phone_number);
        return RIL_AT_INVALID_PARAM;
    }

    for (i = 0; i < candidate_count; i++) {
        s32 ret = RIL_SMS_SendSMS_Text(candidates[i],
                                       (u8)Ql_strlen(candidates[i]),
                                       LIB_SMS_CHARSET_GSM,
                                       (u8*)text,
                                       text_len,
                                       &msg_ref);
        if (ret == RIL_AT_SUCCESS) {
            APP_DEBUG("SMS: %s sent to %s, ref=%d\r\n", tag, candidates[i], msg_ref);
            return RIL_AT_SUCCESS;
        }

        APP_DEBUG("SMS: %s send failed to %s, ret=%d, at_err=%d\r\n",
                  tag, candidates[i], ret, Ql_RIL_AT_GetErrCode());
        last_ret = ret;
    }

    return last_ret;
}

#endif
