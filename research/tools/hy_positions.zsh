#!/bin/zsh
# Boots stock 2.05 in the emulator once per rotary position and records the HY3131 configuration written after the
# dial is read (the last 20-byte write to R20), then compares it with the video repo's function-level extraction
# (hycfg.json, key none/<mode>/<range>). Output: out/hy-positions.txt.
set -euo pipefail
cd ${0:A:h:h}
ref=${EEV_REF:-${0:A:h:h:h:h}/reference/121gw/121GW-Firmware-Claude/src}/tools/re/extracted/hycfg.json
out=out/hy-positions.txt
: >| $out
for pos in LowZ V mV Hz Ohm VA uA A; do
    python3 -I scripts/emu_run.py EEVBlog2_05.bin svd/STM32L1xx.svd --rotary $pos --ms 2400 --hy-only >| out/emu-hy-$pos.txt 2>&1 || true
    python3 -I - $pos out/emu-hy-$pos.txt $ref >>| $out <<'PY'
import sys, re, json
pos, f, ref = sys.argv[1], sys.argv[2], json.load(open(sys.argv[3]))
txt = open(f).read()
m = re.search(r"g_mode=(\d+) g_range=(\d+) g_rotary=0x([0-9a-f]+)", txt)
mode, rng = int(m.group(1)), int(m.group(2))
cfgs = re.findall(r"hy t=\s*([\d.]+) W @20 R20\s+([0-9a-f]{40})\n", txt)
after = [c for t, c in cfgs if float(t) > 2015]
mine = after[-1] if after else None
theirs = [h for r, h in ref.get(f"none/{mode}/{rng}", []) if r == 32]
verdict = "MATCH" if mine and theirs and mine == theirs[-1] else "DIFF"
print(f"{pos:5s} mode {mode:2d} range {rng}  emu {mine}  ref {theirs[-1] if theirs else None}  {verdict}")
PY
done
cat $out
