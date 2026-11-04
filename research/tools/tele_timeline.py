#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Timeline of v2 telemetry from a ble_capture.py log: per received 'S' frame (mode, range, AD1 average, reading), the
# HY3131 R20..R33 configuration from the most recent complete set of 'R' frames. Prints each distinct
# (mode, range, configuration) with its time span and AD1 statistics.
#   tele_timeline.py LOG.txt
import re, statistics, struct, sys

buf, cur_r, have = bytearray(), bytearray(52), [False] * 4
rows = []
for line in open(sys.argv[1]):
    m = re.match(r"([\d.]+) RAW ([0-9a-f]+)", line)
    if not m: continue
    t = float(m.group(1)); buf.extend(bytes.fromhex(m.group(2)))
    while len(buf) >= 19:
        if buf[0] not in (0xF2, 0xD9): del buf[0]; continue
        f = bytes(buf[:19]); x = 0
        for c in f[:18]: x ^= c
        if x != f[18]: del buf[0]; continue
        del buf[:19]
        if f[0] != 0xD9: continue
        if f[1] == ord("R"):
            off = f[3]; n = min(14, 52 - off); cur_r[off:off + n] = f[4:4 + n]; have[off // 14] = True
        elif f[1] == ord("S"):
            mode, rng = f[4], f[5]; bat, avg, rd, gpg = struct.unpack_from("<HiiH", f, 6)
            rows.append((t, mode, rng, avg, rd, bytes(cur_r[0x20:0x34]).hex() if all(have) else None, gpg))

groups = []
for r in rows:
    key = (r[1], r[2], r[5])
    if groups and groups[-1]["key"] == key: groups[-1]["rows"].append(r)
    else: groups.append({"key": key, "rows": [r]})
t0 = rows[0][0] if rows else 0
for g in groups:
    (mode, rng, cfg), rs = g["key"], g["rows"]
    avgs = [r[3] for r in rs]; rds = [r[4] for r in rs]
    sd = statistics.pstdev(avgs) if len(avgs) > 1 else 0
    print(f"{rs[0][0] - t0:7.1f}-{rs[-1][0] - t0:7.1f} s  mode {mode:2d} range {rng}  n={len(rs):3d}  "
          f"AD1 avg {statistics.mean(avgs):12.1f} sd {sd:9.1f}  reading {statistics.mean(rds):10.1f}  "
          f"GPIOG {rs[-1][6]:04x}  cfg {cfg}")
