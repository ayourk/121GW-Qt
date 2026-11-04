#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# ble_frames.py OUT.txt: groups the USART2.DR writes logged by emu_watch.py (--ww 40004404) into frames and prints
# time, the raw bytes and, for 0xF2 packets, mode / range (+0x80 overload) / 17-bit value (byte 5 bits 7..6 + 7..8).
import re, sys
frames, cur, last = [], None, -99
for line in open(sys.argv[1]):
    m = re.match(r"\s+WW t=(\d+) pc (\w+) \[40004404\]/\d <- 0x(\w+)", line)
    if not m: continue
    t, b = int(m.group(1)), int(m.group(3), 16) & 0xFF
    if cur is None or t - last > 3: cur = [t, m.group(2), []]; frames.append(cur)
    cur[2].append(b); last = t
for t, pc, bs in frames:
    d = ""
    if len(bs) == 19 and bs[0] == 0xF2:
        val = ((bs[5] >> 6) << 16) | (bs[7] << 8) | bs[8]
        d = f"mode {bs[5] & 0x3F:2d} range 0x{bs[6]:02x} value {val}"
    print(f"t={t:6d} n={len(bs):2d} {bytes(bs).hex()} {d}")
