/* SPDX-License-Identifier: MIT */
/*
 * Bluetooth command frames and YMODEM-1K file transfer (custom firmware addition, not in stock 2.05).
 *
 * Interface to the stock code: the USART2 interrupt hands every received byte to bt_rx_isr() first and every
 * transmit-empty event to bt_tx_isr(); TIM2 calls bt_tick() and asks bt_status_paused() before it sends the
 * 0xF2 status packet; the main loop calls bt_poll(). The state lives in RAM above the original image's variables
 * (section .bt_ram, cleared by the start-up code); while nothing is in progress none of these functions writes
 * RAM, so the stock paths behave as before.
 */
#ifndef BT_XFER_H
#define BT_XFER_H

#include <stdint.h>

uint8_t bt_rx_isr(uint8_t byte, uint8_t stock_index);
uint8_t bt_tx_isr(void);
void bt_tick(void);
uint8_t bt_status_paused(void);
uint8_t bt_xfer_active(void);
void bt_poll(void);

#endif /* BT_XFER_H */
