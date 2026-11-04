/* Custom firmware (not in 2.05): MSI measured against the HSE crystal; the BLE UART divisor from it (msi_cal.c). */
#ifndef MSI_CAL_H
#define MSI_CAL_H

#include <stdint.h>
#include "stm32l1xx.h"

/* MSI frequency in Hz measured against HSE (about 64 ms); 0 when not possible now (not on MSI, HSE busy or failed) */
uint32_t msi_cal_measure(void);
/* measure, then set usart's BRR for baud from the measured frequency; 0 = done, 1 = nominal divisor kept */
uint8_t msi_cal_set_baud(USART_TypeDef *usart, uint32_t baud);
/* the last successful measurement (Hz), 0 if none since reset */
uint32_t msi_cal_last_hz(void);
/* the last attempt: status, counted capture pairs, summed counts, HSE ready-wait polls (for INFO) */
void msi_cal_last(uint8_t *status, uint16_t *periods, uint32_t *total, uint32_t *hse_polls);

#define MSI_ST_NONE             0u  /* never tried */
#define MSI_ST_OK               1u
#define MSI_ST_BUSY             2u  /* not on MSI, or HSE already on (a card session) */
#define MSI_ST_NO_HSE           3u  /* HSERDY never came */
#define MSI_ST_NO_CAPTURE       4u  /* TIM11 never captured */
#define MSI_ST_CAPTURE_STOPPED  5u  /* captures stopped part way */
#define MSI_ST_OVERCAPTURES     6u  /* too many pairs lost to overcaptures */
#define MSI_ST_IMPLAUSIBLE      7u  /* result outside +-10 % of nominal */
#define MSI_ST_NO_PLL           8u  /* PLLRDY never came */
#define MSI_ST_NO_SWITCH        9u  /* SYSCLK did not switch to the PLL */

#endif
