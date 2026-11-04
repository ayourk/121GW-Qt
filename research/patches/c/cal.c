/* SPDX-License-Identifier: MIT */
/* CAL.BIN with the current-zero trims (zombu4 OF-12): stock 2.05 saves/loads data EEPROM 0x000-0x4AF (1200 B) and
 * leaves out the six current-zero slots at 0x4D0-0x4FF, so a restored backup loses them. These replace CalFile_Save
 * and CalFile_Load (hooked at BootScreen's two calls) and cover 0x000-0x4FF (1280 B). Loading accepts both sizes:
 * an old 1200-byte file restores 0x000-0x4AF and leaves 0x4B0-0x4FF as they are. Same mount, clock, timestamp and
 * read-back verify steps and the same result codes as stock (0 ok, 1 no card/mount, 7 write, 8 read/size,
 * 9 mismatch), so BootScreen shows "SAvEd" / "-Err-" exactly as before. */
#include "stock.h"

#define CAL_NEW 0x500u                     /* 0x000-0x4FF */
#define CAL_OLD 0x4B0u                     /* stock 2.05 files */

static int read_all(BYTE *dst, UINT max, UINT *got)
{
    UINT total = 0;
    while (total < max) {
        UINT want = max - total > 512 ? 512 : max - total, br = 0;
        if (stock_f_read(STOCK_FIL, STOCK_RDBUF, want, &br) != 0) return 8;
        for (UINT i = 0; i < br; i++) dst[total + i] = STOCK_RDBUF[i];
        total += br;
        if (br < want) break;              /* end of file */
    }
    *got = total;
    return 0;
}

char cal_save(void)
{
    BYTE *buf = STOCK_BUF;
    char err = 0;
    for (UINT i = 0; i < CAL_NEW; i++) buf[i] = stock_CalFile_EeRead(i);
    stock_Board_ClockSelect(1);
    if (stock_SD_MountRetry(10) != 0) err = 1;
    if (!err) {
        stock_f_mount(0, STOCK_FATFS);
        if (stock_CalFile_CreateRetry(10) != 0) err = 7;     /* stock ignored a failed create here */
    }
    if (!err) {
        UINT bw = 0;
        if (stock_f_write(STOCK_FIL, buf, CAL_NEW, &bw) != 0 || bw != CAL_NEW) err = 7;
    }
    stock_f_close(STOCK_FIL);
    if (!err) {                            /* read back and compare, as stock does */
        UINT got = 0;
        if (stock_f_open(STOCK_FIL, "cal.bin", 1) != 0) err = 8;
        else {
            UINT done = 0;
            while (!err && done < CAL_NEW) {
                UINT want = CAL_NEW - done > 512 ? 512 : CAL_NEW - done;
                if (stock_f_read(STOCK_FIL, STOCK_RDBUF, want, &got) != 0 || got != want) { err = 8; break; }
                for (UINT i = 0; i < want; i++) if (STOCK_RDBUF[i] != buf[done + i]) { err = 9; break; }
                done += want;
            }
            stock_f_close(STOCK_FIL);
        }
    }
    stock_File_SetTimestamp(1);
    stock_SD_PowerOFF();
    stock_Board_ClockSelect(0);
    return err;
}

char cal_load(void)
{
    BYTE *buf = STOCK_BUF;
    char err = 0;
    UINT got = 0;
    stock_Board_ClockSelect(1);
    if (stock_SD_MountRetry(10) != 0) err = 1;
    if (!err) {
        stock_f_mount(0, STOCK_FATFS);
        if (stock_f_open(STOCK_FIL, "cal.bin", 1) != 0) err = 8;
        else {
            err = (char)read_all(buf, CAL_NEW + 1, &got);   /* +1: a longer file is not a cal.bin */
            if (!err && got != CAL_NEW && got != CAL_OLD) err = 8;
            stock_f_close(STOCK_FIL);
        }
    }
    stock_SD_PowerOFF();
    stock_Board_ClockSelect(0);
    if (!err) {
        for (UINT i = 0; i < got; i++) stock_CalFile_EeWrite(i, buf[i]);
        for (UINT i = 0; i < got; i++) if (stock_CalFile_EeRead(i) != buf[i]) return 9;
    }
    return err;
}
