/**
 * @file    sms_internal.h
 * @brief   Internal SMS helper declarations
 */

#ifndef SMS_INTERNAL_H
#define SMS_INTERNAL_H

#include "ql_type.h"

#define SMS_PHONE_MAX_LEN    32
#define SMS_MAX_CANDIDATES   3
#define SMS_DEFAULT_CC       "98"

void sms_build_candidates(const char* phone_number,
                          char candidates[][SMS_PHONE_MAX_LEN],
                          u32* count);

bool sms_candidates_match(char lhs[][SMS_PHONE_MAX_LEN],
                          u32 lhs_count,
                          char rhs[][SMS_PHONE_MAX_LEN],
                          u32 rhs_count);

s32 sms_send_with_candidates(const char* phone_number, const char* text, const char* tag);
bool sms_is_authorized_sender(const char* sender);
void sms_build_compact_list_reply(char* out, u32 out_len);
bool sms_is_compact_list_command(const char* cmd);
s32 sms_translate_param_command(const char* input_cmd, char* output_cmd, u32 output_len);

#endif /* SMS_INTERNAL_H */
