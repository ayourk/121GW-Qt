/* SPDX-License-Identifier: MIT */
/* OF-52 (behavior change): Battery_TableToCentivolts (0x08013728) maps the PA5 code with a 16-point table at
   0x08028c9c (T[i] = 3120 - 140 i -> 1050 - 50 i centivolts), i.e. code / 2.8 - 64.3. The divider on the board is
   B+ - R69 1 M - PA5 - R70 330 k - VSS, read against VDDA = 3.300 V (NJU7200U33), so
   B+ = code x 3.3 / 4095 x 1330 / 330 = code x 1330 / 4095 volts / 100 exactly. The two lines cross near 6.4 V;
   below it the table reads low (4.00 V shows 3.75 V, and the 4.1 V low-battery flag comes on at 4.31 V).
   Fix: the hardware relation, rounded to 10 mV (printed nominal resistor values; part tolerances move it by about
   their sum, as they move the stock table). Battery_Update keeps its clamp, trimmed mean and 4.1 V threshold. */
#include "measb.h"

/* hook 0x0801351A: Battery_Update's bl Battery_TableToCentivolts(code) */
unsigned battery_centivolts(unsigned code)
{
    return (code * 1330u + 4095u / 2) / 4095u;
}
