#ifndef LOGIC_H
#define LOGIC_H

#include "ql_system.h"
#include "ql_type.h"

void logic_init(void);
void logic_start(void);
void logic_uart_callback(u8* data, u32 len);
void logic_handle_message(const ST_MSG* msg);
void logic_print_ready(void);

#endif /* LOGIC_H */
