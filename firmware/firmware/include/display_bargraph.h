/*
 * Display: bar graph.
 */
#ifndef DISPLAY_BARGRAPH_H
#define DISPLAY_BARGRAPH_H

#include <stdint.h>

extern uint8_t g_bar_length;     /* 0x20008041  lit segments after segment 0 (0..25) */
extern uint32_t g_bar_limit;     /* 0x20007D60  overload bar threshold */

void bargraph_clear(uint8_t from, uint8_t to);
void bargraph_compute(uint8_t fine);
void bargraph_compute_for_function(void);
void bargraph_draw(void);
void bargraph_update(void);
void bargraph_off(void);
void bargraph_reset(void);
void bargraph_progress(uint32_t done, uint32_t total, uint8_t from_right);   /* custom: transfer progress + bounce */
void bargraph_progress_end(void);                        /* transfer over: readings own the bar again */

#endif /* DISPLAY_BARGRAPH_H */
