/* SPDX-License-Identifier: MIT */
/* UI-area C patches (zombu4 OF-nn ports): stock 2.05 functions and RAM used by ui.c. Addresses from names.tsv and
   the disassembly of stock 2.05. */
#pragma once
#include "stock.h"

#define U8(a)  (*(volatile BYTE *)(a))
#define U32(a) (*(volatile DWORD *)(a))

/* measurement state */
#define G_MODE        U8(0x2000803c)
#define G_RANGE       U8(0x2000803d)
#define G_CAL_MODE    U8(0x2000808a)
#define G_SETTLE      U8(0x20000238)   /* display updates still to discard after an apply */
#define G_BLANK       U8(0x2000805c)   /* main display blank (Autorange) */
#define G_DISP_UPDATE U8(0x2000805d)   /* an averaged reading is ready for Display_Update */
#define G_AUTOHOLD    U8(0x20008050)
#define G_SETUP_PAGE  U8(0x20008037)
#define G_TRIM        U32(0x20007e0c)  /* live current-zero trim */

/* calibration captures in progress (Cal_ArmCapture, Key_LongPress MEM in Ohm range 6, Cal_AcFreqWindow points) */
#define CAL_OFFSET_RUN  U8(0x2000808c)
#define CAL_GAIN_RUN    U8(0x2000808e)
#define CAL_OHM6_RUN    U8(0x20008090)
#define CAL_ACPT(i)     U8(0x2000809c + (i))   /* AC frequency points 1..4 pending */

/* Bluetooth */
#define G_BLE_TX      ((volatile BYTE *)0x20007ae0)
#define BLE_RX_COPY   ((volatile BYTE *)0x20007b94)
#define BLE_RX_LEN    U8(0x2000802f)
#define USART2_BASE   ((void *)0x40004400)
#define USART2_SR     U32(0x40004400)
#define USART2_DR     U32(0x40004404)
#define USART2_CR1    U32(0x4000440c)
#define RCC_APB1ENR   U32(0x40023824)
#define NVIC_ISER0    U32(0xE000E100)
#define NVIC_ICER0    U32(0xE000E180)
#define NVIC_ICPR0    U32(0xE000E280)
#define IRQ_TIM2      28
#define IRQ_TIM3      29

/* keys */
#define KEY_QUEUED    U8(0x200080ee)   /* short press queued, dispatched at release */
#define KEY_RPT_STAGE U8(0x200080f2)   /* Keys_AutoRepeat stage, 0 = no repeat yet */

/* power-down */
#define TIM2_DIER     U32(0x4000000c)
#define TIM3_DIER     U32(0x4000040c)
#define GPIOE_BSRR    U32(0x40021018)
#define BUZ_ACTIVE    U8(0x20008079)
#define BUZ_REPEAT    U8(0x2000807a)
#define G_BACKLIGHT   U8(0x2000807b)

void  stock_Reading_SetSettleCount(void);
BYTE  stock_Cal_CaptureRejected(BYTE kind);
BYTE  stock_Cal_ArmCapture(BYTE kind);
void  stock_CurrentZero_Store(BYTE mode, BYTE range, DWORD value);
void  stock_USART_ITConfig(void *usart, UINT it, UINT on);
void  stock_BLE_BuildPacket_Thunk(void);
void  stock_BLE_Start(void);
void  stock_Keys_AutoRepeat(BYTE held);
void  stock_LowPower_Prepare(void);
void  stock_Beep_Double(void);
void  stock_LCD_RefreshDirty(void);
void  stock_LCD_SubHourMinuteBlink(BYTE flush);
