/**
 * @file    sms_auth.c
 * @brief   SMS sender authorization helpers
 */

#include "config/module_config.h"

#ifdef MODULE_SMS_ENABLED

#include "sms_internal.h"

#include "param/param.h"
#include "ql_stdlib.h"

bool sms_is_authorized_sender(const char* sender)
{
    static const ParamKey_e authorized_keys[3] = {
        PARAM_ALERT_PHONE_1,
        PARAM_ALERT_PHONE_2,
        PARAM_ALERT_PHONE_3
    };
    char sender_candidates[SMS_MAX_CANDIDATES][SMS_PHONE_MAX_LEN];
    u32 sender_count = 0;
    u32 i;

    if (sender == NULL || sender[0] == '\0') {
        return FALSE;
    }

    sms_build_candidates(sender, sender_candidates, &sender_count);
    if (sender_count == 0) {
        return FALSE;
    }

    for (i = 0; i < 3; i++) {
        char allowed_raw[PARAM_STRING_MAX_LEN];
        char allowed_candidates[SMS_MAX_CANDIDATES][SMS_PHONE_MAX_LEN];
        u32 allowed_count = 0;

        Ql_memset(allowed_raw, 0, sizeof(allowed_raw));
        if (param_get_string(authorized_keys[i], allowed_raw, sizeof(allowed_raw)) != 0) {
            continue;
        }

        sms_build_candidates(allowed_raw, allowed_candidates, &allowed_count);
        if (allowed_count == 0) {
            continue;
        }

        if (sms_candidates_match(sender_candidates, sender_count,
                                 allowed_candidates, allowed_count)) {
            return TRUE;
        }
    }

    return FALSE;
}

#endif
