#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# List every Thumb-2 bl in a 2.05 image whose target is one of the given addresses.  find_bl.py IMAGE ADDR...
import sys
BASE = 0x08006000
img = open(sys.argv[1], "rb").read(); want = {int(x, 16) & ~1 for x in sys.argv[2:]}
for i in range(0, len(img) - 3, 2):
    hi = int.from_bytes(img[i:i + 2], "little"); lo = int.from_bytes(img[i + 2:i + 4], "little")
    if hi >> 11 != 0x1E or lo >> 14 != 3 or not lo & 0x1000: continue
    s = hi >> 10 & 1; j1 = lo >> 13 & 1; j2 = lo >> 11 & 1
    i1 = ~(j1 ^ s) & 1; i2 = ~(j2 ^ s) & 1
    off = (s << 24) | (i1 << 23) | (i2 << 22) | ((hi & 0x3FF) << 12) | ((lo & 0x7FF) << 1)
    if s: off -= 1 << 25
    t = BASE + i + 4 + off
    if t in want: print(f"{BASE + i:08x} bl {t:08x}")
