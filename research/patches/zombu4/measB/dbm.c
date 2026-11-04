/* SPDX-License-Identifier: MIT */
/* OF-46: ACV_ComputeDbm (0x0800a74c) gives 10 log10(V^2 / 0.6); for a reading of exactly 0 the library log10 returns
   -infinity. LCD_SubDbm (0x08018590) converts the magnitude with __aeabi_f2uiz (infinity -> 0xFFFFFFFF) and splits
   it into five digits, so the dBm page shows "- 7295" (and stores +1 in the BLE copy 0x20007e98). Any magnitude of
   100000 or more (only infinity occurs) cannot be drawn with five digits.
   Fix (behavior change in what the page shows): such a value is drawn as overload on the sub line, " OFL" with the
   sub minus for -infinity ("- OFL", the meter's form of a negative overload); the BLE copy becomes -65535, so the
   frame carries the negative flag and the largest magnitude the 16-bit field holds, instead of +1. */
#include "measb.h"

/* hook 0x08017B66: the SETUP page 11 draw's bl LCD_SubDbm (ACV) */
void sub_dbm(void)
{
    uint32_t b = DBM_FLOAT;
    if (G_OL || (b & 0x7fffffffu) < 0x47c35000u) {     /* OL handled by the stock code; |dBm| < 100000.0f */
        stock_LCD_SubDbm();
        return;
    }
    int neg = (b & 0x80000000u) && (b & 0x7fffffffu) <= 0x7f800000u;   /* negative, not NaN */
    stock_LCD_ShowOFL(1);                               /* " OFL", clears the sub minus */
    if (neg) LCD_SUB_MINUS_WORD |= LCD_SUB_MINUS_BIT;
    SUB_DEC = 0;
    DBM_SHOWN = neg ? -65535 : 65535;
    SUB_DRAWN = 1;
}
