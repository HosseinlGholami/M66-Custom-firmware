#ifndef BOARD_H
#define BOARD_H

#include "ql_type.h"
#include "uart/uart.h"

s32 board_init(UartRxCallback_t uart_rx_callback);

#endif /* BOARD_H */
