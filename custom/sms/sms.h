#ifndef SMS_H
#define SMS_H

#include "ql_type.h"

s32 sms_init(void);
s32 sms_handle_new_sms(u32 sms_index);
s32 sms_send_text(const char* phone_number, const char* text);
s32 sms_poll_inbox(void);

#endif /* SMS_H */
