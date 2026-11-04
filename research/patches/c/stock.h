/* SPDX-License-Identifier: MIT */
/* Stock 2.05 functions and objects the C patches use (addresses: stock_syms.ld from names.tsv). FatFs here is the
   stock R0.08a build until the transplant replaces it. */
#pragma once
#include <stdint.h>
typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef unsigned int UINT;
typedef int FRESULT;

/* stock FatFs R0.08a (objects are opaque here: fixed RAM addresses below) */
FRESULT stock_f_mount(BYTE vol, void *fs);
FRESULT stock_f_open(void *fp, const char *path, BYTE mode);
FRESULT stock_f_read(void *fp, void *buf, UINT btr, UINT *br);
FRESULT stock_f_write(void *fp, const void *buf, UINT btw, UINT *bw);
FRESULT stock_f_close(void *fp);
#define STOCK_FATFS   ((void *)0x20006830)
#define STOCK_FIL     ((void *)0x20006a60)
#define STOCK_BUF     ((BYTE *)0x20001240)  /* 10 KB shared buffer (logger, CAL.BIN); free at boot */
#define STOCK_RDBUF   ((BYTE *)0x20006ea8)  /* 512 B read-back buffer */

int  stock_SD_MountRetry(int tries);        /* 0 ok */
int  stock_CalFile_CreateRetry(int tries);  /* 0 ok: f_open("cal.bin", create | write) into STOCK_FIL */
int  stock_File_SetTimestamp(int cal);      /* 1 = cal.bin */
int  stock_SD_PowerOFF(void);
void stock_Board_ClockSelect(int hse);
BYTE stock_CalFile_EeRead(UINT off);
void stock_CalFile_EeWrite(UINT off, BYTE v);
