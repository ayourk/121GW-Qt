/* SPDX-License-Identifier: MIT */
/* OF-53 / OF-54: HY_Service (0x0800614E) read registers 0x00..0x33 in one burst, then wrote 0 to INTF.
   - The burst clocks the counter registers CTC/CTB/CTA (0x15..0x1D) out before INTF (0x1E): a count that completes
     in between is seen as done (CTF) together with the previous count (OF-54).
   - The data sheet (sec. 10.2 (7)) clears INTF when it is read; the write of 0 that follows clears it once more. A
     flag the chip raises after the INTF byte was clocked out (the 21 bytes 0x1F..0x33 still follow it in the burst)
     or before the write is gone without having been seen: a lost CTF leaves the counter stopped, and the frequency
     reading frozen, until the next mode/range apply re-arms it (OF-53).
   Fix: read the configuration registers 0x1F..0x33 first (they carry no events), then INTF on its own (a one-byte
   frame), then the data registers together with INTF once more (0x00..0x1E). Every data register is then clocked
   out after the flags it belongs to were taken; a flag that appears in the second INTF read was raised during the
   data read (its data may be the previous ones), so the data read is repeated (at most twice) until no new flag
   shows up. Ending the data read on INTF also means that a chip which clears INTF already when the byte after 0x1D
   is fetched cannot drop a flag unseen. The clear writes 0 only to the flags that were taken (1 elsewhere: INTF bits
   are only cleared by a 0, sec. 10.2 (7)), so it cannot discard a flag raised after the read. The mirror ends up with
   the stock layout (0x200077d0 + register, INTF = every flag taken), so the stock dispatch that follows is unchanged. */
#include "measb.h"

#define HY_CFG_N (0x34 - 0x1F)                           /* INTE, R20..R33 */

/* hook 0x08006158: HY_Service's bl HY_Read(&g_hy_mirror, 0, 0x34) */
void hy_status_read(void *unused, unsigned addr, unsigned n)
{
    (void)unused; (void)addr; (void)n;
    BYTE cfg[HY_CFG_N];
    stock_HY_Read(0, HY_INTF + 1, HY_CFG_N);
    for (unsigned i = 0; i < HY_CFG_N; i++) cfg[i] = HY_MIRROR[i];
    stock_HY_Read(0, HY_INTF, 1);
    BYTE flags = HY_MIRROR[0];
    for (int pass = 0; pass < 3; pass++) {
        stock_HY_Read(0, 0, HY_INTF + 1);                /* AD1 .. CTA, INTF */
        BYTE late = HY_MIRROR[HY_INTF] & (BYTE)~flags;
        flags |= late;
        if (!late) break;
    }
    HY_MIRROR[HY_INTF] = flags;
    for (unsigned i = 0; i < HY_CFG_N; i++) HY_MIRROR[HY_INTF + 1 + i] = cfg[i];
}

/* hook 0x0800616C: HY_Service's bl HY_Write(&zero, 0x1E, 1) */
void hy_intf_clear(const void *buf, unsigned addr, unsigned n)
{
    (void)buf; (void)n;
    BYTE keep = (BYTE)~HY_MIRROR[HY_INTF];               /* 0 = clear a flag that was taken, 1 = leave it */
    stock_HY_Write(&keep, addr, 1);
}
