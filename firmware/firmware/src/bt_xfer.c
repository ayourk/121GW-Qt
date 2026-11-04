/* SPDX-License-Identifier: MIT */
/*
 * Bluetooth command frames and YMODEM-1K file transfer (custom firmware addition, not in stock 2.05).
 * Written from the public YMODEM description (Forsberg, "XMODEM/YMODEM Protocol Reference"); no code taken from
 * another implementation.
 *
 * Command frame (PC -> meter):  F6 LEN SEQ CMD payload[LEN] CRC_HI CRC_LO        (LEN 0..64)
 * Reply frame   (meter -> PC):  D6 LEN SEQ STATUS payload[LEN] CRC_HI CRC_LO
 * CRC-16/XMODEM (poly 0x1021, init 0) over every byte before the CRC. A frame with a bad CRC is ignored.
 *
 * Commands: 01 INFO, 10 RECEIVE (YMODEM upload to the card), 11 SEND (flags, pace, 8.3 name: YMODEM download),
 * 12 LIST (16-bit index: one root-directory entry), 1F ABORT. Statuses: 00 OK, 01 BAD_CMD, 02 BAD_ARG, 03 BUSY,
 * 04 NOT_FOUND, 05 SD_ERROR, 06 END.
 *
 * Receive path: outside a transfer the USART2 interrupt passes a frame that starts with 0xF6 (where the stock
 * parser is idle) to the RX ring and keeps all other bytes for the stock F4/F8 parser; during a transfer every
 * byte goes to the ring. The ring is drained here, in the main loop.
 * Transmit path: replies and YMODEM output go to the TX ring; TIM2 sends it in bursts (default 16 bytes per
 * 100 ms, the BLE112 drops data sent faster than about 190 B/s). The 0xF2 status packet is held back while a
 * transfer runs or the TX ring is not empty.
 * Card access: as the stock logging does, one card session (HSE clock, card power-up, mount) per operation; the
 * TX ring is drained first because the clock switch changes the UART's baud rate.
 *
 * Rules for writing (the meter owner's decisions): CAL.BIN is never written, overwritten or deleted over
 * Bluetooth (CAN). Overwriting an existing file, and writing EEVBLOG.BIN at all, needs a key press on the meter:
 * the LCD shows "OUEr" and the file name; SETUP accepts, any other key or 30 s without a key rejects (CAN). New
 * files need no confirmation. Data goes to BTRECV.TMP first and is renamed into place after the last block, so
 * an interrupted transfer leaves an existing file as it was.
 */
#include <stddef.h>
#include "bt_xfer.h"
#include "msi_cal.h"
#include "app_globals.h"
#include "ble_command.h"
#include "display_bargraph.h"
#include "display_text.h"
#include "irq.h"
#include "lcd_driver.h"
#include "rtc.h"
#include "sd_card.h"
#include "sd_log.h"
#include "system_init.h"
#include "ff.h"
#include "stm32l1xx.h"

#define RX_SIZE        2048u               /* powers of two */
#define TX_SIZE        2048u
#define FRAME_MAX      64u                 /* command payload */
#define REPLY_MAX      32u                 /* reply payload */
#define SOF_CMD        0xF6u
#define SOF_REPLY      0xD6u

#define CMD_INFO       0x01u
#define CMD_RECEIVE    0x10u
#define CMD_SEND       0x11u
#define CMD_LIST       0x12u
#define CMD_ABORT      0x1Fu

#define ST_OK          0x00u
#define ST_BAD_CMD     0x01u
#define ST_BAD_ARG     0x02u
#define ST_BUSY        0x03u
#define ST_NOT_FOUND   0x04u
#define ST_SD_ERROR    0x05u
#define ST_END         0x06u

#define PROTO_VERSION  1u
#define FEATURES       0x07u               /* bit 0: YMODEM; bit 1: INFO has the last cancel; bit 2: and MSI Hz */

#define Y_SOH          0x01u
#define Y_STX          0x02u
#define Y_EOT          0x04u
#define Y_ACK          0x06u
#define Y_NAK          0x15u
#define Y_CAN          0x18u
#define Y_C            0x43u
#define Y_PAD          0x1Au
#define MAX_RETRIES    10u

/* TIM2 ticks (about 100 per second) */
#define T_FRAME        100u                /* a started command frame must be complete within 1 s of quiet */
#define T_C            300u                /* 'C' every 3 s while waiting for a file's block 0 */
#define T_BLOCK        1000u               /* 10 s without a block: NAK */
#define T_INBLOCK      200u                /* 2 s quiet inside a block: purge, NAK */
#define T_QUIET        100u                /* purge: line quiet for 1 s */
#define T_PROMPT       3000u               /* overwrite prompt: 30 s */
#define T_RELEASE      500u                /* wait at most 5 s for the accepting key to be released */
#define T_SCROLL       40u
#define T_ANSWER       1000u               /* sender: 10 s for an answer after the block has left */
#define T_START        6000u               /* sender: 60 s for the receiver's 'C' */

#define PACE_DEFAULT   20u                 /* bytes per burst (one BLE packet: about half the damaged blocks of 16 on
                                              the meter, 2026-11-03); one burst per 10 ticks */
#define PACE_MAX       20u                 /* one BLE notification */
#define BURST_TICKS    10u

#define KEY_SETUP_BIT  0x02u               /* PG9 */

enum xstate {
    X_IDLE = 0,
    X_RX_HDR,          /* receiver: waiting for SOH/STX/EOT/CAN */
    X_RX_BLOCK,        /* receiver: collecting a block */
    X_RX_PURGE,        /* receiver: bad block, waiting for a quiet line before the NAK */
    X_TX_WAIT_C0,      /* sender: waiting for the first 'C' */
    X_TX_WAIT_ACK,     /* sender: block 0, a data block or the null block sent */
    X_TX_WAIT_C1,      /* sender: block 0 ACKed, waiting for 'C' before the data */
    X_TX_EOT,          /* sender: EOT sent */
    X_TX_WAIT_CN       /* sender: file done, waiting for 'C' before the null block 0 */
};

static const char TMP_NAME[] = "BTRECV.TMP";

struct bt_state {
    /* shared with the interrupts */
    volatile uint16_t rx_head;          /* written by USART2 */
    volatile uint16_t rx_tail;
    volatile uint16_t tx_head;
    volatile uint16_t tx_tail;          /* written by USART2 / TIM2 */
    volatile uint8_t mode;              /* 1: transfer, every received byte goes to the ring */
    volatile uint8_t frame_left;        /* bytes of a command frame still to come, 0xFF: LEN next */
    volatile uint8_t active;            /* ticks count and TX bursts run (0 = untouched stock behavior) */
    volatile uint8_t tx_active;         /* a burst owns the transmitter */
    volatile uint8_t tx_left;
    volatile uint8_t tx_wait;
    volatile uint8_t pace;              /* 0 = PACE_DEFAULT */
    volatile uint8_t gap;               /* extra BURST_TICKS between bursts (SEND pace byte bits 7:5) */
    volatile uint8_t sd_busy;           /* card session: no bursts (the clock is switched) */
    volatile uint32_t ticks;
    uint8_t rx_buf[RX_SIZE];
    uint8_t tx_buf[TX_SIZE];

    /* main loop only */
    uint8_t state;
    uint8_t prompting;
    uint8_t prompt_abort;
    uint8_t start_seq;
    uint8_t start_cmd;
    uint8_t start_reply_len;
    uint8_t start_reply[10];
    uint16_t seen_head;
    uint32_t seen_tick;
    uint32_t t_mark;
    uint8_t retries;
    uint8_t cans;
    uint8_t expect;                     /* receiver: next block number */
    uint8_t file_open;                  /* receiver: block 0 accepted, data blocks follow */
    uint8_t eots;
    uint8_t temp_made;
    uint8_t exists;
    uint8_t have_size;
    uint8_t have_mtime;
    uint8_t block_1k;                   /* sender */
    uint8_t blknum;                     /* sender: wire block number, wraps 255 -> 0 */
    uint8_t hdr_phase;                  /* sender: block 0 is the block in flight */
    uint8_t final_blk;
    uint8_t drain;
    uint8_t chunk_ok;
    uint16_t blk_need;
    uint16_t blk_have;
    uint16_t cur_len;
    uint16_t chunk_len;
    uint16_t fdate;
    uint16_t ftime;
    uint32_t size;
    uint32_t done;
    uint32_t mtime;
    uint32_t pos;
    uint32_t chunk_off;
    char name[13];
    uint8_t frame[FRAME_MAX + 6];
    /* The card's DMA moves 32-bit words and ignores the low address bits: the block data (blk[2..]) and the chunk
       are 4-byte aligned so FatFs's whole-sector transfers go straight to and from them. */
    uint8_t blk_pad[2] __attribute__((aligned(4)));
    uint8_t blk[1024 + 4];              /* number, complement, data, CRC */
    uint8_t chunk[1024] __attribute__((aligned(4)));   /* sender: file data at chunk_off */
    uint32_t lcd_save[16];
    FIL fil;
    DIR dir;
    FILINFO info;
};

_Static_assert(offsetof(struct bt_state, blk) % 4u == 2u, "YMODEM block data must be 4-byte aligned");
_Static_assert(offsetof(struct bt_state, chunk) % 4u == 0u, "chunk must be 4-byte aligned");

/* Cleared by Reset_Handler (__bt_ram_start..__bt_ram_end); all-zero is the idle state. */
static struct bt_state bt __attribute__((section(".bt_ram"), aligned(4)));

static inline void barrier(void)
{
    __asm volatile("" ::: "memory");
}

static uint32_t now(void)
{
    return bt.ticks;
}

/* ---- CRC-16/XMODEM -------------------------------------------------------------------------------- */

static uint16_t crc16_byte(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)((uint16_t)b << 8);
    for (uint8_t k = 0; k < 8; k++) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint16_t crc16(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0;

    while (n-- != 0) {
        crc = crc16_byte(crc, *p++);
    }
    return crc;
}

/* ---- rings ---------------------------------------------------------------------------------------- */

static void rx_push_isr(uint8_t b)
{
    uint16_t next = (uint16_t)((bt.rx_head + 1u) & (RX_SIZE - 1u));

    if (next != bt.rx_tail) {           /* full: the byte is lost (CRCs catch it) */
        bt.rx_buf[bt.rx_head] = b;
        barrier();
        bt.rx_head = next;
    }
}

static uint16_t rx_count(void)
{
    return (uint16_t)((bt.rx_head - bt.rx_tail) & (RX_SIZE - 1u));
}

static uint8_t rx_peek(uint16_t i)
{
    barrier();
    return bt.rx_buf[(bt.rx_tail + i) & (RX_SIZE - 1u)];
}

static void rx_drop(uint16_t n)
{
    barrier();
    bt.rx_tail = (uint16_t)((bt.rx_tail + n) & (RX_SIZE - 1u));
}

static uint8_t rx_get(void)
{
    uint8_t b = rx_peek(0);

    rx_drop(1);
    return b;
}

static void rx_flush(void)
{
    bt.rx_tail = bt.rx_head;
}

static uint8_t tx_idle(void)
{
    return bt.tx_head == bt.tx_tail && bt.tx_active == 0;
}

static void tx_put(uint8_t b)
{
    uint16_t next = (uint16_t)((bt.tx_head + 1u) & (TX_SIZE - 1u));

    while (next == bt.tx_tail) {        /* full: TIM2 drains it */
        if (g_ble_enabled == 0) {
            return;
        }
    }
    bt.tx_buf[bt.tx_head] = b;
    barrier();
    bt.tx_head = next;
}

static void tx_write(const uint8_t *p, uint16_t n)
{
    for (uint16_t i = 0; i < n; i++) {
        tx_put(p[i]);
    }
}

/* ---- interrupt hooks ------------------------------------------------------------------------------- */

/*
 * USART2 RXNE: 1 when the byte belongs to this module (the stock parser must not see it). stock_index is
 * g_ble_rx_index: a command frame can only start where the stock parser is idle, and 0xF6 is a byte the
 * stock parser would drop there.
 */
uint8_t bt_rx_isr(uint8_t byte, uint8_t stock_index)
{
    if (bt.mode != 0) {
        rx_push_isr(byte);
        return 1;
    }
    if (bt.frame_left != 0) {
        rx_push_isr(byte);
        if (bt.frame_left == 0xFFu) {
            bt.frame_left = (byte > FRAME_MAX) ? 0 : (uint8_t)(byte + 4u);   /* SEQ CMD payload CRC */
        } else {
            bt.frame_left--;
        }
        return 1;
    }
    if (stock_index == 0 && byte == SOF_CMD) {
        rx_push_isr(byte);
        bt.frame_left = 0xFFu;
        return 1;
    }
    return 0;
}

/* USART2 TXE: 1 when a paced burst owns the transmitter (the stock packet code must not run) */
uint8_t bt_tx_isr(void)
{
    if (bt.tx_active == 0) {
        return 0;
    }
    if (bt.tx_left != 0 && bt.tx_tail != bt.tx_head) {
        barrier();
        usart_send_byte(USART2, bt.tx_buf[bt.tx_tail]);
        bt.tx_tail = (uint16_t)((bt.tx_tail + 1u) & (TX_SIZE - 1u));
        bt.tx_left--;
    } else {
        USART2->CR1 &= ~USART_CR1_TXEIE;
        bt.tx_active = 0;
    }
    return 1;
}

/* TIM2 (about 100 Hz): time base and the paced bursts */
void bt_tick(void)
{
    if (bt.active == 0) {
        return;
    }
    bt.ticks++;
    if (bt.tx_wait < 0xFFu) {
        bt.tx_wait++;
    }
    if (bt.tx_active != 0 || bt.sd_busy != 0 || g_ble_enabled == 0 || bt.tx_wait < (uint8_t)(BURST_TICKS * (1u + bt.gap))
        || bt.tx_head == bt.tx_tail || (USART2->CR1 & USART_CR1_TXEIE) != 0) {
        return;
    }
    bt.tx_wait = 0;
    bt.tx_left = (bt.pace != 0) ? bt.pace : PACE_DEFAULT;
    bt.tx_active = 1;
    usart_send_byte(USART2, bt.tx_buf[bt.tx_tail]);
    bt.tx_tail = (uint16_t)((bt.tx_tail + 1u) & (TX_SIZE - 1u));
    bt.tx_left--;
    USART2->CR1 |= USART_CR1_TXEIE;
}

/* TIM2: hold the 0xF2 status packet during a transfer and while replies are going out */
uint8_t bt_status_paused(void)
{
    return bt.mode != 0 || bt.tx_head != bt.tx_tail || bt.tx_active != 0;
}

uint8_t bt_xfer_active(void)
{
    return bt.state != X_IDLE;
}

/* ---- small helpers ------------------------------------------------------------------------------------ */

static uint8_t str_eq(const char *a, const char *b)
{
    while (*a != 0 && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static uint8_t name_char_ok(uint8_t c)
{
    static const char extra[] = "$%'-_@~`!(){}^#&";

    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
        return 1;
    }
    for (const char *p = extra; *p != 0; p++) {
        if ((uint8_t)*p == c) {
            return 1;
        }
    }
    return 0;
}

/* An 8.3 name (1-8 characters, optionally '.' and 1-3 more), upper-cased into out; 0 if not valid */
static uint8_t name_83(const uint8_t *s, uint16_t n, char out[13])
{
    uint8_t base = 0;
    uint8_t ext = 0;
    uint8_t dot = 0;
    uint8_t o = 0;

    if (n == 0 || n > 12) {
        return 0;
    }
    for (uint16_t i = 0; i < n; i++) {
        uint8_t c = s[i];

        if (c == '.') {
            if (dot != 0 || base == 0) {
                return 0;
            }
            dot = 1;
            out[o++] = '.';
            continue;
        }
        if (name_char_ok(c) == 0) {
            return 0;
        }
        if (c >= 'a' && c <= 'z') {
            c = (uint8_t)(c - 0x20u);
        }
        if (dot != 0) {
            if (++ext > 3) {
                return 0;
            }
        } else if (++base > 8) {
            return 0;
        }
        out[o++] = (char)c;
    }
    if (dot != 0 && ext == 0) {
        return 0;
    }
    out[o] = 0;
    return 1;
}

static uint8_t is_leap(uint32_t y)
{
    return (y % 4u == 0 && y % 100u != 0) || y % 400u == 0;
}

static const uint16_t days_before_month[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

/* FAT date/time (taken as UTC) to seconds since 1970; 0 if the date is not valid */
static uint32_t fat_to_unix(uint16_t fdate, uint16_t ftime)
{
    uint32_t y = 1980u + (fdate >> 9);
    uint32_t m = (fdate >> 5) & 15u;
    uint32_t d = fdate & 31u;
    uint32_t days = 0;

    if (m < 1 || m > 12 || d < 1) {
        return 0;
    }
    for (uint32_t k = 1970; k < y; k++) {
        days += is_leap(k) ? 366u : 365u;
    }
    days += days_before_month[m - 1] + ((m > 2 && is_leap(y)) ? 1u : 0u) + d - 1u;
    return days * 86400u + (uint32_t)(ftime >> 11) * 3600u + ((ftime >> 5) & 63u) * 60u + (ftime & 31u) * 2u;
}

/* seconds since 1970 to FAT date/time; 0 if outside FAT's 1980..2107 */
static uint8_t unix_to_fat(uint32_t t, uint16_t *fdate, uint16_t *ftime)
{
    uint32_t days = t / 86400u;
    uint32_t secs = t % 86400u;
    uint32_t y = 1970;
    uint32_t m = 0;

    while (days >= (is_leap(y) ? 366u : 365u)) {
        days -= is_leap(y) ? 366u : 365u;
        y++;
    }
    if (y < 1980 || y > 2107) {
        return 0;
    }
    while (m < 11) {
        uint32_t next = days_before_month[m + 1] + ((m + 1 >= 2 && is_leap(y)) ? 1u : 0u);

        if (days < next) {
            break;
        }
        m++;
    }
    days -= days_before_month[m] + ((m >= 2 && is_leap(y)) ? 1u : 0u);
    *fdate = (uint16_t)(((y - 1980u) << 9) | ((m + 1u) << 5) | (days + 1u));
    *ftime = (uint16_t)(((secs / 3600u) << 11) | (((secs / 60u) % 60u) << 5) | ((secs % 60u) / 2u));
    return 1;
}

/* ---- card sessions (as the stock logging: HSE clock, card power-up and mount, then back) ------------ */

static uint8_t sd_begin(void)
{
    while (tx_idle() == 0) {            /* the clock switch changes the UART's baud rate */
        if (g_ble_enabled == 0) {
            break;
        }
    }
    bt.sd_busy = 1;
    clock_init(1);
    if (sd_open_retry(10) != 0) {       /* as the stock card paths; 2 failed intermittently on a real card */
        sd_power_off();
        clock_init(0);
        bt.sd_busy = 0;
        return 1;
    }
    f_mount(&g_fatfs, "", 0);
    return 0;
}

static void sd_end(void)
{
    sd_power_off();
    clock_init(0);
    bt.sd_busy = 0;
}

/* ---- replies ------------------------------------------------------------------------------------------- */

static void send_reply(uint8_t seq, uint8_t status, const uint8_t *p, uint8_t n, uint8_t keep)
{
    uint8_t f[REPLY_MAX + 6];
    uint16_t crc;

    f[0] = SOF_REPLY;
    f[1] = n;
    f[2] = seq;
    f[3] = status;
    for (uint8_t i = 0; i < n; i++) {
        f[4 + i] = p[i];
    }
    crc = crc16(f, (uint16_t)(4u + n));
    f[4 + n] = (uint8_t)(crc >> 8);
    f[5 + n] = (uint8_t)crc;
    tx_write(f, (uint16_t)(n + 6u));
    if (keep != 0 && n + 6u <= sizeof bt.start_reply) {
        for (uint8_t i = 0; i < n + 6u; i++) {
            bt.start_reply[i] = f[i];
        }
        bt.start_reply_len = (uint8_t)(n + 6u);
    }
}

/* ---- transfer start / end -------------------------------------------------------------------------- */

static void xfer_begin(uint8_t state, uint8_t seq, uint8_t cmd)
{
    if (tx_idle() != 0) {               /* custom: re-measure MSI (temperature drift) while nothing is going out */
        (void)msi_cal_set_baud(USART2, 19200u);
    }
    bt.mode = 1;
    bt.state = state;
    bt.start_seq = seq;
    bt.start_cmd = cmd;
    bt.retries = 0;
    bt.cans = 0;
    bt.expect = 0;
    bt.file_open = 0;
    bt.eots = 0;
    bt.temp_made = 0;
    bt.drain = 0;
    bt.chunk_ok = 0;
    bt.final_blk = 0;
    bt.t_mark = now();
}

/*
 * Back to idle. The receive path stays in transfer mode (every byte to the ring, none to the stock parser) until
 * the line has been quiet for T_QUIET: the rest of a canceled block must not reach the stock F4/F8 parser (a
 * stray 0xF4 there can inject a key), and a command frame sent right after the transfer is still read (bt_poll).
 */
static void xfer_end(void)
{
    bargraph_progress_end();            /* the progress bar goes; the next reading redraws the normal bar */
    bt.state = X_IDLE;
    bt.prompting = 0;
    bt.pace = 0;
    bt.gap = 0;
    bt.seen_tick = now();
}

static void remove_temp(void)
{
    if (bt.temp_made != 0 && sd_begin() == 0) {
        (void)f_unlink(TMP_NAME);
        sd_end();
    }
    bt.temp_made = 0;
}

/*
 * Why the last transfer was canceled, returned by INFO after its first 3 bytes (little-endian): the bt_xfer.c line
 * of the xfer_cancel() call, the state and retry count then, bytes ACKed (sender) and written (receiver), cancels
 * and failed card reads (each retried by load_chunk) since reset. Kept until the next cancel; .bt_ram: zero after reset.
 */
static struct {
    uint16_t line;
    uint8_t state;
    uint8_t retries;
    uint32_t pos;
    uint32_t done;
    uint16_t cancels;
    uint16_t read_retries;
} bt_diag __attribute__((section(".bt_ram"), aligned(4)));

/* CAN CAN CAN, drop the temporary file, back to idle */
static void xfer_cancel_at(uint16_t line)
{
    bt_diag.line = line;
    bt_diag.state = (uint8_t)bt.state;
    bt_diag.retries = bt.retries;
    bt_diag.pos = bt.pos;
    bt_diag.done = bt.done;
    bt_diag.cancels++;
    tx_put(Y_CAN);
    tx_put(Y_CAN);
    tx_put(Y_CAN);
    remove_temp();
    xfer_end();
}
#define xfer_cancel() xfer_cancel_at((uint16_t)__LINE__)

static uint8_t *put_le(uint8_t *d, uint32_t v, uint8_t n)
{
    while (n-- != 0) {
        *d++ = (uint8_t)v;
        v >>= 8;
    }
    return d;
}

/* ---- commands --------------------------------------------------------------------------------------- */

static void cmd_info(uint8_t seq)
{
    uint8_t info[30];                   /* every byte written below (an initializer would call memset) */
    uint8_t mst;
    uint16_t mper;
    uint32_t mtot, mhse;
    uint8_t *d = &info[3];

    info[0] = PROTO_VERSION;
    info[1] = FRAME_MAX;
    info[2] = FEATURES;

    d = put_le(d, bt_diag.line, 2);
    *d++ = bt_diag.state;
    *d++ = bt_diag.retries;
    d = put_le(d, bt_diag.pos, 4);
    d = put_le(d, bt_diag.done, 4);
    d = put_le(d, bt_diag.cancels, 2);
    d = put_le(d, bt_diag.read_retries, 2);
    d = put_le(d, msi_cal_last_hz(), 4); /* bytes 19..22: measured MSI (Hz), 0 = not measured */
    msi_cal_last(&mst, &mper, &mtot, &mhse);
    *d++ = mst;                         /* bytes 23..29: the last attempt: status, pairs, summed counts */
    d = put_le(d, mper, 2);
    (void)put_le(d, (mst == MSI_ST_NO_HSE) ? mhse : mtot, 4);   /* NO_HSE: the HSE wait polls instead */
    send_reply(seq, ST_OK, info, sizeof info, 0);
}

static void cmd_list(uint8_t seq, const uint8_t *p, uint8_t len)
{
    uint16_t index;
    uint16_t n = 0;
    uint8_t status = ST_END;
    uint8_t out[9 + 12];
    uint8_t out_len = 0;

    if (len != 2) {
        send_reply(seq, ST_BAD_ARG, 0, 0, 0);
        return;
    }
    index = (uint16_t)((p[0] << 8) | p[1]);
    if (sd_begin() != 0) {
        send_reply(seq, ST_SD_ERROR, 0, 0, 0);
        return;
    }
    if (f_opendir(&bt.dir, "") != FR_OK) {
        status = ST_SD_ERROR;
    } else {
        for (;;) {
            if (f_readdir(&bt.dir, &bt.info) != FR_OK) {
                status = ST_SD_ERROR;
                break;
            }
            const char *fname;
            uint32_t size;

            if (bt.info.fname[0] == 0) {
                break;
            }
            fname = fil_short_name(&bt.info);   /* the 8.3 name, as with R0.08a; exFAT: the name (first 12 chars) */
            if (fname[0] == '.') {        /* R0.16's f_readdir skips the volume label itself (R0.08a: AM_VOL) */
                continue;
            }
            if (n++ != index) {
                continue;
            }
            size = (bt.info.fsize > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)bt.info.fsize;   /* exFAT: 64-bit */
            out[0] = bt.info.fattrib;
            out[1] = (uint8_t)(size >> 24);
            out[2] = (uint8_t)(size >> 16);
            out[3] = (uint8_t)(size >> 8);
            out[4] = (uint8_t)size;
            out[5] = (uint8_t)(bt.info.fdate >> 8);
            out[6] = (uint8_t)bt.info.fdate;
            out[7] = (uint8_t)(bt.info.ftime >> 8);
            out[8] = (uint8_t)bt.info.ftime;
            out_len = 9;
            for (uint8_t i = 0; i < 12 && fname[i] != 0; i++) {
                out[out_len++] = (uint8_t)fname[i];
            }
            status = ST_OK;
            break;
        }
    }
    sd_end();
    send_reply(seq, status, out, (status == ST_OK) ? out_len : 0, 0);
}

static void cmd_receive(uint8_t seq, uint8_t len)
{
    if (len != 0) {
        send_reply(seq, ST_BAD_ARG, 0, 0, 0);
        return;
    }
    if (g_logging_active != 0) {
        send_reply(seq, ST_BUSY, 0, 0, 0);
        return;
    }
    xfer_begin(X_RX_HDR, seq, CMD_RECEIVE);
    send_reply(seq, ST_OK, 0, 0, 1);
    tx_put(Y_C);
}

static void cmd_send(uint8_t seq, const uint8_t *p, uint8_t len)
{
    FRESULT r;
    uint8_t status = ST_OK;
    uint8_t out[4];

    if (len < 3 || name_83(p + 2, (uint16_t)(len - 2u), bt.name) == 0 || (p[0] & 0xFEu) != 0) {
        send_reply(seq, ST_BAD_ARG, 0, 0, 0);
        return;
    }
    if (g_logging_active != 0) {
        send_reply(seq, ST_BUSY, 0, 0, 0);
        return;
    }
    if (sd_begin() != 0) {
        send_reply(seq, ST_SD_ERROR, 0, 0, 0);
        return;
    }
    r = f_stat(bt.name, &bt.info);
    sd_end();
    if (r == FR_OK && (bt.info.fattrib & AM_DIR) == 0 && bt.info.fsize > 0xFFFFFFFFu) {
        status = ST_BAD_ARG;            /* exFAT file of 4 GiB or more: YMODEM sizes here are 32-bit */
    } else if (r == FR_OK && (bt.info.fattrib & AM_DIR) == 0) {
        status = ST_OK;
    } else if (r == FR_OK || r == FR_NO_FILE || r == FR_NO_PATH || r == FR_INVALID_NAME) {
        status = ST_NOT_FOUND;
    } else {
        status = ST_SD_ERROR;
    }
    if (status != ST_OK) {
        send_reply(seq, status, 0, 0, 0);
        return;
    }
    xfer_begin(X_TX_WAIT_C0, seq, CMD_SEND);
    bt.size = (uint32_t)bt.info.fsize;
    bt.fdate = bt.info.fdate;
    bt.ftime = bt.info.ftime;
    bt.block_1k = p[0] & 1u;
    bt.pace = ((p[1] & 0x1Fu) > PACE_MAX) ? PACE_MAX : (uint8_t)(p[1] & 0x1Fu);   /* bits 4:0: bytes per burst */
    bt.gap = (uint8_t)(p[1] >> 5);      /* bits 7:5: extra 100 ms gaps between bursts (0..7) */
    out[0] = (uint8_t)(bt.size >> 24);
    out[1] = (uint8_t)(bt.size >> 16);
    out[2] = (uint8_t)(bt.size >> 8);
    out[3] = (uint8_t)bt.size;
    send_reply(seq, ST_OK, out, 4, 1);
}

static void handle_command(uint8_t seq, uint8_t cmd, const uint8_t *p, uint8_t len)
{
    if (bt.state != X_IDLE) {
        if (seq == bt.start_seq && cmd == bt.start_cmd) {
            tx_write(bt.start_reply, bt.start_reply_len);   /* the PC lost the reply: repeat it */
        } else if (cmd == CMD_ABORT) {
            if (bt.prompting != 0) {
                bt.prompt_abort = 1;    /* the prompt returns, the transfer is canceled there */
            } else {
                xfer_cancel();
            }
            send_reply(seq, ST_OK, 0, 0, 0);
        } else if (cmd == CMD_INFO) {
            cmd_info(seq);
        } else {
            send_reply(seq, ST_BUSY, 0, 0, 0);
        }
        return;
    }
    switch (cmd) {
    case CMD_INFO:
        cmd_info(seq);
        break;
    case CMD_RECEIVE:
        cmd_receive(seq, len);
        break;
    case CMD_SEND:
        cmd_send(seq, p, len);
        break;
    case CMD_LIST:
        cmd_list(seq, p, len);
        break;
    case CMD_ABORT:
        send_reply(seq, ST_OK, 0, 0, 0);
        break;
    default:
        send_reply(seq, ST_BAD_CMD, 0, 0, 0);
        break;
    }
}

static void rx_progress(void)
{
    if (bt.rx_head != bt.seen_head) {
        bt.seen_head = bt.rx_head;
        bt.seen_tick = now();
    }
}

/*
 * A command frame at the head of the RX ring (first byte 0xF6): 1 while it is incomplete, else 0 (executed,
 * or dropped for a bad length or CRC). An incomplete frame is dropped after 1 s without new bytes.
 */
static uint8_t take_frame(void)
{
    uint16_t have = rx_count();
    uint8_t len;
    uint16_t crc;

    rx_progress();
    if (have >= 2) {
        len = rx_peek(1);
        if (len > FRAME_MAX) {
            rx_drop(1);
            return 0;
        }
        if (have >= len + 6u) {
            for (uint16_t i = 0; i < len + 6u; i++) {
                bt.frame[i] = rx_get();
            }
            crc = crc16(bt.frame, (uint16_t)(len + 4u));
            if (bt.frame[len + 4u] == (uint8_t)(crc >> 8) && bt.frame[len + 5u] == (uint8_t)crc) {
                handle_command(bt.frame[2], bt.frame[3], &bt.frame[4], len);
            }
            return 0;
        }
    }
    if (now() - bt.seen_tick > T_FRAME) {
        rx_drop(1);
        return 0;
    }
    return 1;
}

/* ---- overwrite prompt -------------------------------------------------------------------------------- */

static void prompt_draw_name(uint8_t start)
{
    char text[16];
    uint8_t n = 0;

    for (uint8_t i = 0; bt.name[i] != 0; i++) {
        text[n++] = (bt.name[i] == '.') ? '-' : bt.name[i];
    }
    if (n <= 5) {
        for (uint8_t i = 0; i < 5; i++) {
            lcd_put_char((uint8_t)((i < n) ? text[i] : ' '), i);
        }
        return;
    }
    text[n++] = ' ';
    text[n++] = ' ';
    for (uint8_t i = 0; i < 5; i++) {
        lcd_put_char((uint8_t)text[(start + i) % n], i);
    }
}

/* during the prompt: an ABORT frame or CAN CAN cancels; everything else (repeats of block 0) is dropped */
static void prompt_service_rx(void)
{
    while (rx_count() != 0) {
        uint8_t b = rx_peek(0);

        if (b == SOF_CMD) {
            bt.cans = 0;
            if (take_frame() != 0) {
                return;
            }
            continue;
        }
        rx_drop(1);
        if (b != Y_CAN) {
            bt.cans = 0;
        } else if (++bt.cans >= 2) {
            bt.prompt_abort = 1;
        }
    }
}

/*
 * The meter owner's key press for an overwrite: main "OUEr", secondary the file name (scrolling, '.' as '-').
 * 1 = SETUP pressed. Any other key, 30 s without a key, an ABORT frame or Bluetooth switched off: 0.
 * Keys held when the prompt appears are ignored until released; the accepting key is waited for until
 * released (max 5 s) so the stock key handling does not see it afterwards. Measurement pauses meanwhile.
 */
static uint8_t prompt(void)
{
    static const char txt_over[5] = { ' ', 'O', 'U', 'E', 'r' };
    uint32_t start = now();
    uint32_t drawn = now();
    uint32_t pressed_at = 0;
    uint8_t phase = 0;
    uint8_t prev = 0xFFu;
    uint8_t stable = 0;
    uint8_t result = 0;
    uint8_t scroll = 0;

    for (uint8_t i = 0; i < 16; i++) {
        bt.lcd_save[i] = g_lcd_shadow[i];
    }
    lcd_clear_shadow();
    lcd_put_text5(0, txt_over);
    prompt_draw_name(0);
    lcd_update();
    bt.prompting = 1;
    bt.prompt_abort = 0;

    for (;;) {
        uint8_t keys;

        if (g_ble_enabled == 0) {
            result = 0;
            break;
        }
        prompt_service_rx();
        if (bt.prompt_abort != 0) {
            result = 0;
            break;
        }
        if (g_task_flag_a == 0) {
            continue;
        }
        g_task_flag_a = 0;
        keys = (uint8_t)~(GPIOG->IDR >> 8);
        if (keys == prev) {
            if (stable < 0xFFu) {
                stable++;
            }
        } else {
            prev = keys;
            stable = 0;
        }
        if (stable == 2) {              /* three equal samples */
            if (phase == 0 && keys == 0) {
                phase = 1;
            } else if (phase == 1 && keys != 0) {
                result = (keys == KEY_SETUP_BIT) ? 1u : 0u;
                phase = 2;
                pressed_at = now();
            } else if (phase == 2 && keys == 0) {
                break;
            }
        }
        if (phase == 2 && now() - pressed_at > T_RELEASE) {
            break;
        }
        if (phase < 2 && now() - start >= T_PROMPT) {
            result = 0;
            break;
        }
        if (now() - drawn >= T_SCROLL) {
            drawn = now();
            prompt_draw_name(++scroll);
            lcd_update();
        }
    }
    bt.prompting = 0;
    for (uint8_t i = 0; i < 16; i++) {
        g_lcd_shadow[i] = bt.lcd_save[i];
    }
    lcd_update();
    g_display_dirty = 1;
    rx_flush();
    return result;
}

/* ---- YMODEM receiver ------------------------------------------------------------------------------- */

static void rx_purge_then_nak(void)
{
    if (++bt.retries > MAX_RETRIES) {
        xfer_cancel();
        return;
    }
    bt.state = X_RX_PURGE;
    bt.t_mark = now();
}

static void rx_block0(uint16_t len)
{
    const uint8_t *d = &bt.blk[2];
    uint16_t n = 0;
    uint16_t s = 0;
    uint16_t k;
    uint8_t bad = 0;
    uint8_t eev;
    uint8_t confirm;
    FRESULT r;

    while (n < len && d[n] != 0) {
        n++;
    }
    if (n == 0) {                       /* null block 0: end of the batch */
        tx_put(Y_ACK);
        xfer_end();
        return;
    }
    if (n == len) {
        xfer_cancel();
        return;
    }
    for (k = 0; k < n; k++) {
        if (d[k] == '/' || d[k] == '\\') {
            s = (uint16_t)(k + 1u);
        }
    }
    if (name_83(d + s, (uint16_t)(n - s), bt.name) == 0 || str_eq(bt.name, "CAL.BIN") || str_eq(bt.name, TMP_NAME)) {
        xfer_cancel();
        return;
    }
    bt.have_size = 0;
    bt.size = 0;
    bt.have_mtime = 0;
    bt.mtime = 0;
    for (k = (uint16_t)(n + 1u); k < len && d[k] >= '0' && d[k] <= '9'; k++) {
        if (bt.size > 429496728u) {
            xfer_cancel();
            return;
        }
        bt.size = bt.size * 10u + (uint32_t)(d[k] - '0');
        bt.have_size = 1;
    }
    if (k < len && d[k] == ' ') {
        for (k++; k < len && d[k] >= '0' && d[k] <= '7'; k++) {
            if (bt.mtime > 0x1FFFFFFFu) {
                bt.have_mtime = 0;
                break;
            }
            bt.mtime = (bt.mtime << 3) | (uint32_t)(d[k] - '0');
            bt.have_mtime = 1;
        }
    }
    eev = str_eq(bt.name, "EEVBLOG.BIN");

    /* nothing is written before the prompt: a rejected or timed-out overwrite leaves the card as it was */
    if (sd_begin() != 0) {
        xfer_cancel();
        return;
    }
    r = f_stat(bt.name, &bt.info);
    if (r == FR_OK) {
        bt.exists = 1;
        if ((bt.info.fattrib & (AM_DIR | AM_RDO)) != 0) {
            bad = 1;
        }
    } else if (r == FR_NO_FILE) {
        bt.exists = 0;
    } else {
        bad = 1;
    }
    confirm = bad == 0 && (bt.exists != 0 || eev != 0);
    if (confirm != 0) {
        sd_end();
        if (prompt() == 0) {
            xfer_cancel();
            return;
        }
        if (sd_begin() != 0) {
            xfer_cancel();
            return;
        }
    }
    if (bad == 0) {                     /* a fresh BTRECV.TMP (also replaces one an earlier power loss left) */
        if (f_open(&bt.fil, TMP_NAME, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            bad = 1;
        } else {
            bt.temp_made = 1;
            if (f_close(&bt.fil) != FR_OK) {
                bad = 1;
            }
        }
    }
    sd_end();
    if (bad != 0) {
        xfer_cancel();
        return;
    }
    bt.file_open = 1;
    bt.expect = 1;
    bt.done = 0;
    bargraph_progress(0, bt.have_size ? bt.size : 0, 0);   /* empty bar: the transfer has started */
    bt.eots = 0;
    bt.retries = 0;
    tx_put(Y_ACK);
    tx_put(Y_C);
    bt.t_mark = now();
}

static void rx_data(uint16_t len)
{
    uint32_t n = len;
    UINT bw = 0;
    FRESULT r;

    if (bt.have_size != 0 && n > bt.size - bt.done) {
        n = bt.size - bt.done;          /* padding past the declared size */
    }
    if (n != 0) {
        if (sd_begin() != 0) {
            xfer_cancel();
            return;
        }
        r = f_open(&bt.fil, TMP_NAME, FA_WRITE | FA_OPEN_EXISTING);
        if (r == FR_OK) {
            FRESULT c;

            r = f_lseek(&bt.fil, bt.done);
            if (r == FR_OK) {
                r = f_write(&bt.fil, &bt.blk[2], (UINT)n, &bw);
            }
            c = f_close(&bt.fil);
            if (r == FR_OK) {
                r = c;
            }
        }
        sd_end();
        if (r != FR_OK || bw != n) {
            xfer_cancel();
            return;
        }
        bt.done += n;
        if (bt.have_size != 0) {
            bargraph_progress(bt.done, bt.size, 0);   /* upload: fill from the left */
        }
    }
    bt.expect++;
    tx_put(Y_ACK);
    bt.t_mark = now();
}

/* replace (or create) the target with BTRECV.TMP; 0 when done */
static uint8_t rx_commit(void)
{
    FRESULT r = FR_OK;

    if (sd_begin() != 0) {
        return 1;
    }
    if (bt.exists != 0) {
        r = f_unlink(bt.name);
        if (r == FR_NO_FILE) {
            r = FR_OK;
        }
    }
    if (r == FR_OK) {
        r = f_rename(TMP_NAME, bt.name);
        if (r != FR_OK && bt.exists != 0) {
            bt.temp_made = 0;           /* the old file is gone: keep the new data in BTRECV.TMP */
        }
    }
    if (r == FR_OK) {
        if (bt.have_mtime == 0 || unix_to_fat(bt.mtime, &bt.info.fdate, &bt.info.ftime) == 0) {
            bt.info.fdate = (uint16_t)(((g_rtc_set_reg2 + 20u) << 9) | ((uint32_t)g_rtc_set_reg4 << 5) | g_rtc_set_reg6);
            bt.info.ftime = (uint16_t)(((uint32_t)g_rtc_set_reg10 << 11) | ((uint32_t)g_rtc_set_reg12 << 5)
                                       | (g_rtc_set_reg14 >> 1));
        }
        (void)f_utime(bt.name, &bt.info);
        bt.temp_made = 0;
    }
    sd_end();
    return r != FR_OK;
}

static void rx_eot(void)
{
    if (bt.file_open == 0) {            /* a repeated EOT: the sender lost our ACK */
        tx_put(Y_ACK);
        tx_put(Y_C);
        bt.t_mark = now();
        return;
    }
    if (bt.eots == 0) {                 /* first EOT: NAK, the sender repeats it */
        bt.eots = 1;
        tx_put(Y_NAK);
        bt.t_mark = now();
        return;
    }
    if ((bt.have_size != 0 && bt.done != bt.size) || rx_commit() != 0) {
        xfer_cancel();
        return;
    }
    bt.file_open = 0;
    bt.expect = 0;
    bt.eots = 0;
    bt.retries = 0;
    tx_put(Y_ACK);
    tx_put(Y_C);                        /* next file of the batch */
    bt.t_mark = now();
}

static void rx_block(void)
{
    uint16_t len = (uint16_t)(bt.blk_need - 4u);
    uint8_t n = bt.blk[0];
    uint16_t crc;

    bt.state = X_RX_HDR;
    crc = crc16(&bt.blk[2], len);
    if ((uint8_t)(n ^ bt.blk[1]) != 0xFFu || bt.blk[2 + len] != (uint8_t)(crc >> 8)
        || bt.blk[3 + len] != (uint8_t)crc) {
        rx_purge_then_nak();
        return;
    }
    if (n == bt.expect) {
        bt.retries = 0;
        if (bt.file_open == 0) {
            rx_block0(len);
        } else {
            rx_data(len);
        }
    } else if (n == (uint8_t)(bt.expect - 1u)) {
        tx_put(Y_ACK);                  /* the previous block again: our ACK was lost */
        if (n == 0 && bt.done == 0) {   /* block 0 itself, not block 256, 512, ... */
            tx_put(Y_C);
        }
        bt.t_mark = now();
    } else {
        xfer_cancel();
    }
}

static void rx_poll(void)
{
    if (bt.state == X_RX_PURGE) {
        if (rx_count() != 0) {
            rx_flush();
            bt.t_mark = now();
        } else if (now() - bt.t_mark >= T_QUIET) {
            tx_put(Y_NAK);
            bt.state = X_RX_HDR;
            bt.t_mark = now();
        }
        return;
    }
    if (bt.state == X_RX_BLOCK) {
        while (rx_count() != 0 && bt.blk_have < bt.blk_need) {
            bt.blk[bt.blk_have++] = rx_get();
            bt.t_mark = now();
        }
        if (bt.blk_have == bt.blk_need) {
            rx_block();
        } else if (now() - bt.t_mark > T_INBLOCK) {
            rx_purge_then_nak();
        }
        return;
    }
    while (bt.state == X_RX_HDR && rx_count() != 0) {
        uint8_t b = rx_peek(0);

        if (b == Y_SOH || b == Y_STX) {
            rx_drop(1);
            bt.cans = 0;
            bt.blk_need = (uint16_t)(((b == Y_SOH) ? 128u : 1024u) + 4u);
            bt.blk_have = 0;
            bt.state = X_RX_BLOCK;
            bt.t_mark = now();
            rx_poll();
            return;
        }
        if (b == SOF_CMD) {
            bt.cans = 0;
            if (take_frame() != 0) {
                break;
            }
            continue;
        }
        rx_drop(1);
        if (b == Y_CAN) {
            if (++bt.cans >= 2) {       /* the sender canceled */
                remove_temp();
                xfer_end();
                return;
            }
            continue;
        }
        bt.cans = 0;
        if (b == Y_EOT) {
            rx_eot();
        }
    }
    if (bt.state == X_RX_HDR && now() - bt.t_mark >= ((bt.file_open != 0) ? T_BLOCK : T_C)) {
        bt.t_mark = now();
        if (++bt.retries > MAX_RETRIES) {
            xfer_cancel();
            return;
        }
        tx_put((bt.file_open != 0) ? Y_NAK : Y_C);
    }
}

/* ---- YMODEM sender --------------------------------------------------------------------------------- */

static void tx_block_start(uint8_t header, uint8_t num)
{
    tx_put(header);
    tx_put(num);
    tx_put((uint8_t)~num);
}

static void tx_sent(uint8_t state)
{
    bt.state = state;
    bt.drain = 1;                       /* answers count once the block has left */
}

/* block 0: name, size (decimal) and modification time (octal) */
static void send_block0(uint8_t null_block)
{
    uint8_t d[128];
    uint8_t i = 0;
    uint16_t crc;

    for (uint8_t k = 0; k < sizeof d; k++) {
        d[k] = 0;
    }
    if (null_block == 0) {
        char digits[12];
        uint8_t nd = 0;
        uint32_t v = bt.size;
        uint32_t t = fat_to_unix(bt.fdate, bt.ftime);

        for (uint8_t k = 0; bt.name[k] != 0; k++) {
            d[i++] = (uint8_t)bt.name[k];
        }
        i++;                            /* NUL */
        do {
            digits[nd++] = (char)('0' + v % 10u);
            v /= 10u;
        } while (v != 0);
        while (nd != 0) {
            d[i++] = (uint8_t)digits[--nd];
        }
        if (t != 0) {
            d[i++] = ' ';
            do {
                digits[nd++] = (char)('0' + (t & 7u));
                t >>= 3;
            } while (t != 0);
            while (nd != 0) {
                d[i++] = (uint8_t)digits[--nd];
            }
        }
    }
    bt.blknum = 0;
    bt.hdr_phase = 1;
    bt.final_blk = null_block;
    tx_block_start(Y_SOH, 0);
    tx_write(d, sizeof d);
    crc = crc16(d, sizeof d);
    tx_put((uint8_t)(crc >> 8));
    tx_put((uint8_t)crc);
    tx_sent(X_TX_WAIT_ACK);
}

/* 1 KB of the file at off into the chunk buffer (512-byte reads, as written for the 2.05 disk layer, which read
   one sector per call whatever the count) */
static uint8_t load_chunk_once(uint32_t off)
{
    uint32_t want = bt.size - off;
    uint16_t got = 0;
    UINT br = 0;
    FRESULT r;

    if (want > sizeof bt.chunk) {
        want = sizeof bt.chunk;
    }
    if (sd_begin() != 0) {
        return 1;
    }
    r = f_open(&bt.fil, bt.name, FA_READ);
    if (r == FR_OK) {
        r = f_lseek(&bt.fil, off);
        while (r == FR_OK && got < want) {
            r = f_read(&bt.fil, &bt.chunk[got], 512, &br);
            got = (uint16_t)(got + br);
            if (br < 512) {
                break;
            }
        }
        (void)f_close(&bt.fil);
    }
    sd_end();
    if (r != FR_OK || got < want) {
        bt.chunk_ok = 0;
        return 1;
    }
    bt.chunk_off = off;
    bt.chunk_len = got;
    bt.chunk_ok = 1;
    return 0;
}

/* A failed card read is repeated with a fresh card session (power, init, mount, open) before the download is
 * canceled: one intermittent card error in a 40-minute transfer should not end it. */
#define LOAD_TRIES 4u

static uint8_t load_chunk(uint32_t off)
{
    for (uint8_t i = 0; i < LOAD_TRIES; i++) {
        if (load_chunk_once(off) == 0) {
            return 0;
        }
        bt_diag.read_retries++;
    }
    return 1;
}

static void send_data(void)
{
    uint32_t rem = bt.size - bt.pos;
    uint16_t len = (bt.block_1k != 0 && rem > 128u) ? 1024u : 128u;
    uint32_t coff = bt.pos & ~1023u;
    uint16_t crc = 0;

    if (bt.chunk_ok == 0 || bt.chunk_off != coff) {
        if (load_chunk(coff) != 0) {
            xfer_cancel();
            return;
        }
    }
    bt.cur_len = len;
    bt.final_blk = 0;
    tx_block_start((len == 1024u) ? Y_STX : Y_SOH, bt.blknum);
    for (uint16_t i = 0; i < len; i++) {
        uint32_t idx = bt.pos - coff + i;
        uint8_t c = (idx < bt.chunk_len) ? bt.chunk[idx] : Y_PAD;

        crc = crc16_byte(crc, c);
        tx_put(c);
    }
    tx_put((uint8_t)(crc >> 8));
    tx_put((uint8_t)crc);
    tx_sent(X_TX_WAIT_ACK);
}

static void send_eot(void)
{
    tx_put(Y_EOT);
    tx_sent(X_TX_EOT);
}

static void tx_resend(void)
{
    if (++bt.retries > MAX_RETRIES) {
        xfer_cancel();
        return;
    }
    if (bt.state == X_TX_EOT) {
        send_eot();
    } else if (bt.hdr_phase != 0) {
        send_block0(bt.final_blk);
    } else {
        send_data();
    }
}

static void tx_acked(void)
{
    bt.retries = 0;
    if (bt.final_blk != 0) {            /* null block 0 ACKed: batch done */
        xfer_end();
        return;
    }
    if (bt.hdr_phase != 0) {
        bt.state = X_TX_WAIT_C1;
        bt.t_mark = now();
        return;
    }
    bt.pos += bt.cur_len;
    bargraph_progress(bt.pos, bt.size, 1);          /* download: fill from the right */
    bt.blknum++;
    if (bt.pos >= bt.size) {
        send_eot();
    } else {
        send_data();
    }
}

static void tx_byte_in(uint8_t b)
{
    switch (bt.state) {
    case X_TX_WAIT_C0:
        if (b == Y_C) {
            bt.retries = 0;
            send_block0(0);
        }
        break;
    case X_TX_WAIT_ACK:
        if (b == Y_ACK) {
            tx_acked();
        } else if (b == Y_NAK || (b == Y_C && bt.hdr_phase != 0)) {
            tx_resend();
        }
        break;
    case X_TX_WAIT_C1:
        if (b == Y_C) {
            bt.retries = 0;
            if (bt.size == 0) {
                send_eot();
            } else {
                bt.pos = 0;
                bt.blknum = 1;
                bt.hdr_phase = 0;
                bargraph_progress(0, bt.size, 1);   /* download starts: empty bar, bounce from the right */
                send_data();
            }
        }
        break;
    case X_TX_EOT:
        if (b == Y_NAK) {
            tx_resend();
        } else if (b == Y_ACK) {
            bt.retries = 0;
            bt.state = X_TX_WAIT_CN;
            bt.t_mark = now();
        }
        break;
    case X_TX_WAIT_CN:
        if (b == Y_C) {
            bt.retries = 0;
            send_block0(1);
        }
        break;
    default:
        break;
    }
}

static void tx_poll(void)
{
    if (bt.drain != 0) {
        if (tx_idle() == 0) {
            return;
        }
        bt.drain = 0;
        rx_flush();                     /* anything older than the block is stale */
        bt.t_mark = now();
    }
    while (rx_count() != 0 && bt.state != X_IDLE && bt.drain == 0) {
        uint8_t b = rx_peek(0);

        if (b == SOF_CMD) {
            if (take_frame() != 0) {
                return;
            }
            continue;
        }
        rx_drop(1);
        if (b == Y_CAN) {
            if (++bt.cans >= 2) {       /* the receiver canceled */
                xfer_end();
                return;
            }
            continue;
        }
        bt.cans = 0;
        tx_byte_in(b);
    }
    if (bt.state == X_IDLE || bt.drain != 0) {
        return;
    }
    switch (bt.state) {
    case X_TX_WAIT_C0:
    case X_TX_WAIT_C1:
        if (now() - bt.t_mark >= T_START) {
            xfer_cancel();
        }
        break;
    case X_TX_WAIT_CN:
        if (now() - bt.t_mark >= T_START) {
            xfer_end();                 /* the file itself is through */
        }
        break;
    default:
        if (now() - bt.t_mark >= T_ANSWER) {
            bt.t_mark = now();
            if (bt.state == X_TX_WAIT_ACK && bt.final_blk != 0) {
                xfer_end();             /* no ACK for the null block: the file itself was acknowledged */
            } else {
                tx_resend();
            }
        }
        break;
    }
}

/* ---- main loop ------------------------------------------------------------------------------------- */

static void __attribute__((noinline)) bt_poll_work(void);

/* main loop: the idle check stays as short as possible (no frame setup), so the stock loop's timing barely moves */
void bt_poll(void)
{
    if (bt.active == 0 && bt.rx_head == bt.rx_tail) {
        return;
    }
    bt_poll_work();
}

static void __attribute__((noinline)) bt_poll_work(void)
{
    if (bt.active == 0) {
        bt.active = 1;
        bt.seen_head = bt.rx_head;
        bt.seen_tick = now();
    }
    if (g_ble_enabled == 0) {           /* Bluetooth switched off: drop everything */
        if (bt.state != X_IDLE) {
            remove_temp();
            xfer_end();
        }
        __disable_irq();
        bt.mode = 0;
        bt.tx_tail = bt.tx_head;
        bt.tx_active = 0;
        bt.frame_left = 0;
        bt.rx_tail = bt.rx_head;
        bt.active = 0;
        __enable_irq();
        return;
    }
    rx_progress();
    if (bt.state == X_IDLE) {
        if (bt.mode != 0 && now() - bt.seen_tick > T_QUIET) {
            __disable_irq();            /* quiet after a transfer: the stock parser gets the line back */
            bt.mode = 0;
            bt.frame_left = 0;
            __enable_irq();
        }
        if (bt.frame_left != 0 && now() - bt.seen_tick > T_FRAME) {
            __disable_irq();            /* a frame that never completed: give the stock parser its bytes back */
            bt.frame_left = 0;
            __enable_irq();
        }
        while (rx_count() != 0) {
            if (rx_peek(0) != SOF_CMD) {
                rx_drop(1);
            } else if (take_frame() != 0) {
                break;
            }
        }
    } else if (bt.state <= X_RX_PURGE) {
        rx_poll();
    } else {
        tx_poll();
    }
    if (bt.state == X_IDLE && bt.mode == 0 && rx_count() == 0 && tx_idle() != 0 && bt.frame_left == 0) {
        bt.active = 0;
    }
}
