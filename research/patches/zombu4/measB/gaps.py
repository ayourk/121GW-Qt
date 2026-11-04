#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Counter re-arm gaps in an emu_run.py log: re-arm = write to CTA (0x1B).  gaps.py LOG [MIN_MS]
import sys, re
lim = float(sys.argv[2]) if len(sys.argv) > 2 else 600
ts = [float(m.group(1)) for m in re.finditer(r"hy t=\s*([\d.]+) W @1B", open(sys.argv[1]).read())]
gaps = [(a, b) for a, b in zip(ts, ts[1:]) if b - a > lim]
print(f"{len(ts)} re-arms, {len(gaps)} gaps > {lim:g} ms: " + ", ".join(f"{a:.0f}->{b:.0f}" for a, b in gaps))
