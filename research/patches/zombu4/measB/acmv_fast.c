/* SPDX-License-Identifier: MIT */
/* OF-44: with the 1 kHz filter on, Reading_FastPathAC (0x08008468) divides the per-sample value g_reading_fast
   (0x20007d68) by 10 in ACV and ACmV. Mode_ACV_OnAD1 (0x08006aec) then multiplies it back by 10 and applies the
   filter compensation ACFreq_LpfCompensate (0x08006d40, 0x08006c5e..0x08006c76), as the mean path does; its test
   for mode 4 there is dead, because ACmV is dispatched to Mode_ACmV_OnAD1 (0x080069b4), whose per-sample block ends
   after ACFreq_Correct (0x08006a94). In ACmV with the filter on, the bargraph and the log record (both read
   g_reading_fast) therefore show the uncompensated sample / 10 while the main display shows the compensated mean.
   Fix: the per-sample ACFreq_Correct call in Mode_ACmV_OnAD1 is followed by the step of the ACV handler. */
#include "measb.h"

/* hook 0x08006A94: Mode_ACmV_OnAD1's bl ACFreq_Correct(g_mode, g_reading_fast) */
int acmv_fast_correct(int mode, int reading)
{
    reading = stock_ACFreq_Correct(mode, reading);
    if ((G_MODE == 2 || G_MODE == 4) && G_LPF_1KHZ == 1) reading = stock_ACFreq_LpfCompensate(reading * 10);
    return reading;
}
