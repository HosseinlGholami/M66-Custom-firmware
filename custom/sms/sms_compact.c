/**
 * @file    sms_compact.c
 * @brief   SMS compact list formatting and SMS-only key mapping
 */

#include "config/module_config.h"

#ifdef MODULE_SMS_ENABLED

#include "sms_internal.h"

#include "com/com.h"
#include "param/param.h"
#include "ql_stdlib.h"

typedef struct {
    u8 idx;
    const char* alias;
    ParamKey_e key;
    bool is_string;
} SmsListEntry_t;

static const SmsListEntry_t g_sms_list_map[] = {
    {1, "Relay1", PARAM_IO_EXP_OUT0,        FALSE},
    {2, "Relay2", PARAM_IO_EXP_OUT2,        FALSE},
    {3, "Relay3", PARAM_IO_EXP_OUT3,        FALSE},
    {4, "Relay4", PARAM_IO_EXP_OUT1,        FALSE},
    {5, "power",  PARAM_BATTERY_ACTIVATION, FALSE},
    {6, "phone1", PARAM_ALERT_PHONE_1,      TRUE},
    {7, "phone2", PARAM_ALERT_PHONE_2,      TRUE},
    {8, "phone3", PARAM_ALERT_PHONE_3,      TRUE},
};

static bool sms_char_ieq(char a, char b)
{
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
    }
    return (a == b) ? TRUE : FALSE;
}

static bool sms_str_ieq(const char* a, const char* b)
{
    u32 i = 0;

    if (a == NULL || b == NULL) {
        return FALSE;
    }

    while (a[i] != '\0' && b[i] != '\0') {
        if (!sms_char_ieq(a[i], b[i])) {
            return FALSE;
        }
        i++;
    }

    return (a[i] == '\0' && b[i] == '\0') ? TRUE : FALSE;
}

static void sms_reply_append(char* out, u32 out_len, const char* chunk)
{
    u32 out_used;
    u32 i = 0;

    if (out == NULL || chunk == NULL || out_len == 0) {
        return;
    }

    out_used = Ql_strlen(out);
    while (chunk[i] != '\0' && out_used < (out_len - 1)) {
        out[out_used++] = chunk[i++];
    }
    out[out_used] = '\0';
}

static bool sms_find_map_key(const char* token, ParamKey_e* key_out)
{
    u32 i;
    bool all_digits = TRUE;

    if (token == NULL || key_out == NULL || token[0] == '\0') {
        return FALSE;
    }

    for (i = 0; token[i] != '\0'; i++) {
        if (token[i] < '0' || token[i] > '9') {
            all_digits = FALSE;
            break;
        }
    }

    if (all_digits) {
        s32 idx = Ql_atoi(token);
        for (i = 0; i < (sizeof(g_sms_list_map) / sizeof(g_sms_list_map[0])); i++) {
            if ((s32)g_sms_list_map[i].idx == idx) {
                *key_out = g_sms_list_map[i].key;
                return TRUE;
            }
        }
    }

    for (i = 0; i < (sizeof(g_sms_list_map) / sizeof(g_sms_list_map[0])); i++) {
        if (sms_str_ieq(g_sms_list_map[i].alias, token)) {
            *key_out = g_sms_list_map[i].key;
            return TRUE;
        }
    }

    return FALSE;
}

void sms_build_compact_list_reply(char* out, u32 out_len)
{
    u32 i;
    char temp[80];

    if (out == NULL || out_len == 0) {
        return;
    }

    out[0] = '\0';
    for (i = 0; i < (sizeof(g_sms_list_map) / sizeof(g_sms_list_map[0])); i++) {
        if (g_sms_list_map[i].is_string) {
            char str_val[PARAM_STRING_MAX_LEN];

            Ql_memset(str_val, 0, sizeof(str_val));
            if (param_get_string(g_sms_list_map[i].key, str_val, sizeof(str_val)) != 0) {
                Ql_strcpy(str_val, "?");
            }
            Ql_sprintf(temp, "%d-%s:%s", g_sms_list_map[i].idx, g_sms_list_map[i].alias, str_val);
        } else {
            s8 i8_val = 0;
            if (param_get_int8(g_sms_list_map[i].key, &i8_val) != 0) {
                i8_val = -1;
            }
            Ql_sprintf(temp, "%d-%s:%d", g_sms_list_map[i].idx, g_sms_list_map[i].alias, i8_val);
        }

        if (i > 0) {
            sms_reply_append(out, out_len, "\n");
        }
        sms_reply_append(out, out_len, temp);
    }
}

bool sms_is_compact_list_command(const char* cmd)
{
    if (cmd == NULL) {
        return FALSE;
    }

    return ((cmd[0] == 'L' || cmd[0] == 'l') &&
            cmd[1] == COM_CMD_TERMINATOR &&
            cmd[2] == '\0');
}

s32 sms_translate_param_command(const char* input_cmd, char* output_cmd, u32 output_len)
{
    char key_token[32];
    char value_token[COM_CMD_MAX_LEN];
    const char* p;
    const char* p2;
    ParamKey_e mapped_key;
    u32 key_len;
    u32 value_len;

    if (input_cmd == NULL || output_cmd == NULL || output_len == 0) {
        return -1;
    }

    output_cmd[0] = '\0';

    if (sms_is_compact_list_command(input_cmd)) {
        Ql_strncpy(output_cmd, input_cmd, output_len - 1);
        output_cmd[output_len - 1] = '\0';
        return 0;
    }

    if (!((input_cmd[0] == 'G' || input_cmd[0] == 'g' ||
           input_cmd[0] == 'S' || input_cmd[0] == 's') &&
          input_cmd[1] == ',')) {
        Ql_strncpy(output_cmd, input_cmd, output_len - 1);
        output_cmd[output_len - 1] = '\0';
        return 0;
    }

    p = input_cmd + 2;
    p2 = p;
    while (*p2 != '\0' && *p2 != ',' && *p2 != COM_CMD_TERMINATOR) {
        p2++;
    }

    key_len = (u32)(p2 - p);
    if (key_len == 0 || key_len >= sizeof(key_token)) {
        return -1;
    }

    Ql_memset(key_token, 0, sizeof(key_token));
    Ql_memcpy(key_token, p, key_len);
    key_token[key_len] = '\0';

    if (!sms_find_map_key(key_token, &mapped_key)) {
        return -1;
    }

    if (input_cmd[0] == 'G' || input_cmd[0] == 'g') {
        if (*p2 != COM_CMD_TERMINATOR || *(p2 + 1) != '\0') {
            return -1;
        }
        Ql_sprintf(output_cmd, "G,%d!", mapped_key);
        return 1;
    }

    if (*p2 != ',') {
        return -1;
    }

    p = p2 + 1;
    p2 = p;
    while (*p2 != '\0' && *p2 != COM_CMD_TERMINATOR) {
        p2++;
    }

    if (*p2 != COM_CMD_TERMINATOR || *(p2 + 1) != '\0') {
        return -1;
    }

    value_len = (u32)(p2 - p);
    if (value_len == 0 || value_len >= sizeof(value_token)) {
        return -1;
    }

    Ql_memset(value_token, 0, sizeof(value_token));
    Ql_memcpy(value_token, p, value_len);
    value_token[value_len] = '\0';

    Ql_sprintf(output_cmd, "S,%d,%s!", mapped_key, value_token);
    return 1;
}

#endif
