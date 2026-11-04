/* SPDX-License-Identifier: MIT */
/* Stock 2.05 functions and objects used by the measB patches (addresses from patches/c/stock_syms.ld). */
#pragma once
#include "stock.h"

/* HY3131 bus (bit-banged on PF0..PF3). HY_Read ignores its first argument: it always stores the n bytes read from
   register addr upward at g_hy_mirror[0..n-1]. HY_Write sends n bytes from buf to register addr upward. */
void stock_HY_Read(void *unused, unsigned addr, unsigned n);
void stock_HY_Write(const void *buf, unsigned addr, unsigned n);
#define HY_MIRROR ((volatile BYTE *)0x200077d0)    /* 0x34-byte image of registers 0x00..0x33 */
#define HY_INTF   0x1E

#define G_MODE      (*(volatile BYTE *)0x2000803c)
#define G_RANGE     (*(volatile BYTE *)0x2000803d)
#define G_LPF_1KHZ  (*(volatile BYTE *)0x200080b9)
int stock_ACFreq_Correct(int mode, int reading);
int stock_ACFreq_LpfCompensate(int reading);

void stock_LCD_SubDbm(void);
void stock_LCD_ShowOFL(int line);
#define DBM_FLOAT   (*(volatile uint32_t *)0x20007e94)  /* ACV_ComputeDbm result (IEEE single) */
#define DBM_SHOWN   (*(volatile int32_t *)0x20007e98)   /* signed value as drawn (BLE sub field) */
#define SUB_DEC     (*(volatile BYTE *)0x20008040)      /* sub-display decimals */
#define G_OL        (*(volatile BYTE *)0x20008046)
#define LCD_SUB_MINUS_WORD (*(volatile uint32_t *)0x200077a0)
#define LCD_SUB_MINUS_BIT  0x20000000u
#define SUB_DRAWN   (*(volatile BYTE *)0x2000805b)
