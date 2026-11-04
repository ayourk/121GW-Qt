/*
 * FatFs (R0.16) disk I/O layer for the SD card (single drive 0).
 *
 * Custom (not the original's): the 2.05 layer read one block whatever the
 * count, formed byte addresses as sector * 512 in 32 bits (wrapping at
 * 4 GiB), ignored the driver's errors, reported a timeout as RES_WRPRT,
 * gave a fixed 64 MB size and a fixed 2006 timestamp, and passed the
 * caller's buffer straight to the DMA. This layer:
 *  - moves every sector through DMA2 channel 4, whose 32-bit memory accesses
 *    ignore the low two address bits on the real chip: a buffer that is not
 *    4-byte aligned goes through an aligned 512-byte bounce buffer, one
 *    sector at a time (aligned buffers are transferred in place);
 *  - addresses sector * 512 as a 64-bit byte address (the driver divides it
 *    back into a block address for SDHC/SDXC cards);
 *  - reads and writes all 'count' sectors, one block command each, as the
 *    original's write loop does;
 *  - waits for the end of every transfer (DMA count, SDIO end/error flags,
 *    bounded) and returns RES_ERROR for any driver, data or timeout error;
 *  - reports the card's real size from the CSD;
 *  - timestamps files with the meter's clock (get_fattime).
 */
#include <stdint.h>
#include "ff.h"
#include "diskio.h"
#include "sd_card.h"
#include "rtc.h"

#define SECTOR_SIZE  512u
#define STATUS_POLLS 20000u

/* Aligned sector buffer for callers' unaligned buffers; in .sd_ram (cleared at start-up, see the linker script). */
static uint8_t bounce[SECTOR_SIZE] __attribute__((section(".sd_ram"), aligned(4)));

static int aligned4(const void *p)
{
    return ((uintptr_t)p & 3u) == 0;
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, uint32_t n)
{
    while (n-- != 0) {
        *dst++ = *src++;
    }
}

/* Poll the card (CMD13) until it is back in the transfer state */
static DRESULT wait_card_ready(void)
{
    for (uint32_t polls = STATUS_POLLS; polls != 0; polls--) {
        if (sd_get_transfer_status() == SD_TRANSFER_OK) {
            return RES_OK;
        }
    }
    return RES_ERROR;
}

/*
 * An error ends the operation. g_sd_transfer_error doubles as the NUL after the "0:" path string the logging code
 * builds at 0x200080F5 (sd_log.c log_create), so it is set back to 0 here before FatFs parses that path again.
 */
static DRESULT failed(void)
{
    g_sd_transfer_error = SD_OK;
    return RES_ERROR;
}

static DRESULT read_sector(uint8_t *dst, uint64_t address)
{
    if (sd_read_block(dst, address, SECTOR_SIZE) != SD_OK || sd_wait_data_end() != SD_OK) {
        return failed();
    }
    return (wait_card_ready() == RES_OK) ? RES_OK : failed();
}

static DRESULT write_sector(const uint8_t *src, uint64_t address)
{
    if (sd_write_multi_blocks(src, address, SECTOR_SIZE, 1) != SD_OK || sd_wait_data_end() != SD_OK) {
        return failed();
    }
    return (wait_card_ready() == RES_OK) ? RES_OK : failed();
}

/* orig: 0x08023F5C (rewritten: STA_NOINIT on any initialization error instead of the driver's code) */
DSTATUS disk_initialize(BYTE drv)
{
    if (drv != 0) {
        return STA_NOINIT;
    }
    return (sd_init() == SD_OK) ? 0 : STA_NOINIT;
}

/* orig: 0x08023F6E */
DSTATUS disk_status(BYTE drv)
{
    return (drv != 0) ? STA_NOINIT : 0;
}

/* orig: 0x08023F7E (rewritten) */
DRESULT disk_read(BYTE drv, BYTE *buff, LBA_t sector, UINT count)
{
    if (drv != 0 || count == 0) {
        return RES_PARERR;
    }
    for (uint32_t i = 0; i < count; i++) {
        uint8_t *dst = buff + i * SECTOR_SIZE;
        uint64_t address = ((uint64_t)sector + i) * SECTOR_SIZE;

        if (aligned4(dst)) {
            if (read_sector(dst, address) != RES_OK) {
                return RES_ERROR;
            }
        } else {
            if (read_sector(bounce, address) != RES_OK) {
                return RES_ERROR;
            }
            copy_bytes(dst, bounce, SECTOR_SIZE);
        }
    }
    return RES_OK;
}

/* orig: 0x08023FB8 (rewritten) */
DRESULT disk_write(BYTE drv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (drv != 0 || count == 0) {
        return RES_PARERR;
    }
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *src = buff + i * SECTOR_SIZE;
        uint64_t address = ((uint64_t)sector + i) * SECTOR_SIZE;

        if (!aligned4(src)) {
            copy_bytes(bounce, src, SECTOR_SIZE);
            src = bounce;
        }
        if (write_sector(src, address) != RES_OK) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

/* orig: 0x08024038 (rewritten: the card's size from the CSD instead of a fixed 0x20000 sectors) */
DRESULT disk_ioctl(BYTE drv, BYTE ctrl, void *buff)
{
    if (drv != 0) {
        return RES_PARERR;
    }
    switch (ctrl) {
    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = sd_card_sectors();
        break;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = SECTOR_SIZE;
        break;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 0x20;
        break;
    default:   /* CTRL_SYNC: every write has finished when disk_write returns */
        break;
    }
    return RES_OK;
}

/*
 * orig: 0x0802402E (rewritten: the original returns FatFs's sample constant, 2006-02-09 22:30:00)
 * The meter's clock: the NJU6350's date and time as the main loop last read it (rtc_load_clock, every task-b
 * tick), the same copy File_SetTimestamp (log_set_timestamp) stamps the modified time from. The chip itself is
 * not read here: FatFs calls this with the card session's HSE clock running, at which the bit-banged clock bus
 * has never been timed. rtc_load_clock already replaces each implausible chip field (an unset or garbled clock)
 * with 2016-07-01 12:00:00 (year 0x10 = 16); a field still out of range here (the copy is being edited, or was
 * never loaded) gives that whole date.
 */
DWORD get_fattime(void)
{
    uint32_t yy = g_rtc_set_reg2, mo = g_rtc_set_reg4, dd = g_rtc_set_reg6;
    uint32_t hh = g_rtc_set_reg10, mi = g_rtc_set_reg12, ss = g_rtc_set_reg14;

    if (yy > 99u || mo < 1u || mo > 12u || dd < 1u || dd > 31u || hh > 23u || mi > 59u || ss > 59u) {
        yy = 16u; mo = 7u; dd = 1u; hh = 12u; mi = 0u; ss = 0u;
    }
    return ((yy + 20u) << 25) | (mo << 21) | (dd << 16) | (hh << 11) | (mi << 5) | (ss >> 1);
}
