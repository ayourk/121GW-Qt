/*
 * Display: bar graph.
 *
 * The bar graph has 26 segments, numbered from the left. Consecutive
 * segments zig-zag through the COM rows of SEG1..SEG7 (shadow words 0, 2,
 * 4 and 6).
 */
#include "display_bargraph.h"
#include "buzzer.h"
#include "display_symbols.h"
#include "dual_correction.h"
#include "hy3131_ctrl.h"
#include "lcd_driver.h"
#include "measure_derived.h"

#define BARGRAPH_SEGMENTS 26u

/* orig: 0x08019408 (code) - shadow word and bit of each segment */
static const struct {
    uint8_t word;
    uint8_t bit;
} bargraph_seg[BARGRAPH_SEGMENTS] = {
    { 0, 1 }, { 2, 1 }, { 4, 1 }, { 6, 1 },
    { 6, 2 }, { 4, 2 }, { 2, 2 }, { 0, 2 },
    { 0, 3 }, { 2, 3 }, { 4, 3 }, { 6, 3 },
    { 6, 4 }, { 4, 4 }, { 2, 4 }, { 0, 4 },
    { 0, 5 }, { 2, 5 }, { 4, 5 }, { 6, 5 },
    { 6, 6 }, { 4, 6 }, { 2, 6 }, { 0, 6 },
    { 0, 7 }, { 2, 7 },
};

/*
 * orig: 0x08019408 - clear segments from `from` down to `to` (inclusive),
 * or down to segment 0 when `to` is above `from`. Does nothing for
 * from > 25.
 */
void bargraph_clear(uint8_t from, uint8_t to)
{
    uint8_t i = from;

    if (from >= BARGRAPH_SEGMENTS) {
        return;
    }
    for (;;) {
        g_lcd_shadow[bargraph_seg[i].word] &= ~(1u << bargraph_seg[i].bit);
        if (i == 0 || i == to) {
            return;
        }
        i--;
    }
}

/* Functions that show the bar graph: all but 5..8 and 12 */
static int bargraph_function(void)
{
    return (g_function < 6 || g_function > 8) && g_function != 12 && g_function != 5;
}

/* set while a transfer owns the bar (readings do not redraw it); .bt_ram is cleared at reset, like bt_xfer.c's state */
static uint8_t progress_active __attribute__((section(".bt_ram")));

static void bar_off_raw(void);

static void bargraph_set(uint8_t i)
{
    g_lcd_shadow[bargraph_seg[i].word] |= 1u << bargraph_seg[i].bit;
}

/*
 * orig: 0x08018FA8 - bar graph sign and scale symbols, and its length
 * (|g_scaled_value| / 400 with fine == 1, else / 2000). With an overload
 * (g_flag_8046) the bar is full: always without g_minmax_mode, otherwise when
 * the value is at or above g_bar_limit (unsigned compare) or not positive.
 * Function 11 is limited to 15 segments.
 */
void bargraph_compute(uint8_t fine)
{
    int32_t v;
    int32_t mag;
    uint16_t divisor;

    if (!bargraph_function()) {
        return;
    }
    if (g_scaled_value < 0) {
        g_lcd_shadow[4] |= 1u;
        g_lcd_shadow[2] &= ~1u;
    } else {
        g_lcd_shadow[4] &= ~1u;
        g_lcd_shadow[2] |= 1u;
    }
    if (fine == 1) {
        g_lcd_shadow[0] = (g_lcd_shadow[0] & ~0x1000u) | 0x400u;
        sym_range_state_update(1);
        divisor = 400;
    } else {
        g_lcd_shadow[0] = (g_lcd_shadow[0] | 0x1000u) & ~0x400u;
        sym_range_state_update(0);
        divisor = 2000;
    }
    bargraph_set(0);
    v = g_scaled_value;
    mag = (v < 0) ? (int32_t)(0u - (uint32_t)v) : v;
    g_bar_length = (uint8_t)(mag / (int32_t)divisor);
    if (g_flag_8046 != 0) {
        if (g_minmax_mode == 0) {
            g_bar_length = BARGRAPH_SEGMENTS - 1u;
        } else if ((uint32_t)g_scaled_value >= g_bar_limit ||
                   (int32_t)(0u - (uint32_t)g_scaled_value) >= g_scaled_value) {
            g_bar_length = BARGRAPH_SEGMENTS - 1u;
        }
    }
    if (g_function == 11 && g_bar_length >= 16) {
        g_bar_length = 15;
    }
}

/* orig: 0x08018F14 - bar graph scale: fine for function 0, ranges 3 of 1..4 and 2 of 20..21 */
void bargraph_compute_for_function(void)
{
    uint8_t f = g_function;

    if (f == 0) {
        bargraph_compute(1);
    } else if (f >= 1 && f <= 4) {
        bargraph_compute(g_range == 3);
    } else if ((f >= 9 && f <= 11) || (f >= 13 && f <= 19) || (f >= 22 && f <= 24)) {
        bargraph_compute(0);
    } else if (f == 20 || f == 21) {
        bargraph_compute(g_range == 2);
    }
}

/* end of a transfer: the bar goes blank and readings own it again (custom firmware) */
void bargraph_progress_end(void)
{
    if (progress_active != 0) {
        progress_active = 0;
        bar_off_raw();
        g_display_dirty = 1;
    }
}

/* orig: 0x08018B80 - compute and draw segments 1..g_bar_length (not while a transfer shows progress) */
void bargraph_draw(void)
{
    uint8_t i;

    if (progress_active != 0) {
        return;
    }

    bargraph_compute_for_function();
    for (i = 1; i < BARGRAPH_SEGMENTS; i++) {
        if (g_bar_length < i) {
            bargraph_clear(BARGRAPH_SEGMENTS - 1u, i);
        } else {
            bargraph_set(i);
        }
    }
    g_display_dirty = 1;
}

/* orig: 0x08018B14 */
void bargraph_update(void)
{
    g_scaled_ready = 0;
    if (bargraph_function()) {
        bargraph_draw();
    }
}

/* orig: 0x08019364 - bar graph off, with its sign and scale symbols (raw: also during transfer progress) */
static void bar_off_raw(void)
{
    g_lcd_shadow[0] &= ~0x1400u;
    g_lcd_shadow[4] &= ~0x81u;
    g_lcd_shadow[6] &= ~0x180u;
    g_lcd_shadow[2] &= ~0x801u;
    bargraph_clear(BARGRAPH_SEGMENTS - 1u, 0);
}

/* orig: 0x08019364 - bar graph off, with its sign and scale symbols (not while a transfer shows progress) */
void bargraph_off(void)
{
    if (progress_active == 0) {
        bar_off_raw();
    }
}

/*
 * Custom firmware (not in 2.05): Bluetooth transfer progress on the bar graph, segments 1..25.
 * The fill shows how far the transfer is: from the left for uploads into the meter (from_right == 0), from the
 * right for downloads out of it (from_right == 1); all 25 at done >= total, none while total is unknown. One more
 * segment bounces back and forth across the unfilled part, one step per BOUNCE_STEP_BYTES (1 KB) of progress
 * whatever the block size, so a slow transfer shows activity from its first block; it stops once 3 or fewer
 * segments are unfilled. done == 0 starts the bounce again at the edge next to the fill. 32-bit math only: one fill step is ceil(total / 25) bytes.
 */
static uint8_t bounce_pos __attribute__((section(".bt_ram")));    /* 0 = not placed yet */
static uint8_t bounce_back __attribute__((section(".bt_ram")));   /* 1 = moving toward the fill */
static uint32_t bounce_mark __attribute__((section(".bt_ram")));  /* done at the last bounce step */

#define BOUNCE_STOP_FREE 3u
#define BOUNCE_STEP_BYTES 1024u

void bargraph_progress(uint32_t done, uint32_t total, uint8_t from_right)
{
    const uint8_t segs = (uint8_t)(BARGRAPH_SEGMENTS - 1u);       /* 25 */
    uint8_t n = 0;
    uint8_t lo, hi;                                               /* the unfilled segments, lo..hi */

    progress_active = 1;
    if (total != 0) {
        uint32_t step = total / segs + ((total % segs) != 0);
        n = (done >= total) ? segs : (uint8_t)(done / step);
    }
    if (from_right != 0) {
        lo = 1;
        hi = (uint8_t)(segs - n);
    } else {
        lo = (uint8_t)(n + 1u);
        hi = segs;
    }
    bar_off_raw();
    for (uint8_t i = 1; i <= segs; i++) {
        if (i < lo || i > hi) {
            bargraph_set(i);
        }
    }
    if (done == 0) {
        bounce_pos = 0;
    }
    if (total != 0 && (uint8_t)(segs - n) > BOUNCE_STOP_FREE) {
        /* the edge next to the fill is "near"; the bounce starts there and moves away from it first */
        uint8_t near = (from_right != 0) ? hi : lo, far = (from_right != 0) ? lo : hi;
        int8_t away = (from_right != 0) ? -1 : 1;

        if (bounce_pos == 0 || bounce_pos < lo || bounce_pos > hi) {
            bounce_pos = near;
            bounce_back = 0;
            bounce_mark = done;
        } else if (done - bounce_mark >= BOUNCE_STEP_BYTES) {
            int8_t d = (bounce_back != 0) ? (int8_t)-away : away;
            uint8_t next = (uint8_t)(bounce_pos + d);

            if (next < lo || next > hi) {                         /* hit an end: turn around */
                bounce_back = (uint8_t)(bounce_back == 0);
                next = (uint8_t)(bounce_pos - d);
            }
            if (next >= lo && next <= hi) {
                bounce_pos = next;
            }
            bounce_mark = done;
        }
        (void)far;
        bargraph_set(bounce_pos);
    }
    g_display_dirty = 1;
}

/* orig: 0x080192FC - empty bar graph (zero value) for bar graph functions, else off */
void bargraph_reset(void)
{
    if (progress_active != 0) {
        return;
    }
    if (bargraph_function()) {
        g_scaled_value = 0;
        g_bar_length = 0;
        bargraph_compute_for_function();
        bargraph_clear(BARGRAPH_SEGMENTS - 1u, 1);
        g_display_dirty = 1;
    } else {
        bargraph_off();
    }
}
