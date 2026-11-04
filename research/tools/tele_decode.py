#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Decodes the v2 Bluetooth telemetry (patches/telemetry.s) from a byte stream: emu_run.py --ble-all output ("BLE TX
# ALL <hex>") or a scripts/ble_capture.py log ("<time> RAW <hex>" lines). Reassembles the dumps into files, reports
# their coverage (a dropped frame is filled in by a later pass), and prints the latest live frames.
#   tele_decode.py INPUT OUTDIR
import os, re, struct, sys

src, outdir = sys.argv[1], sys.argv[2]
os.makedirs(outdir, exist_ok=True)
text = open(src).read()
m = re.search(r"BLE TX ALL ([0-9a-f]+)", text)
stream = bytes.fromhex(m.group(1)) if m else b"".join(bytes.fromhex(h) for h in re.findall(r" RAW ([0-9a-f]+)", text))

REG = {"E": ("eeprom.bin", 0x3000), "F": ("bootloader.bin", 0x6000), "O": ("info-page-1ff80000.bin", 0x100)}
dump = {k: bytearray(size) for k, (_, size) in REG.items()}
have = {k: bytearray(size) for k, (_, size) in REG.items()}
live, counts, bad, i = {}, {}, 0, 0
while i + 19 <= len(stream):
    f = stream[i:i + 19]
    if f[0] not in (0xF2, 0xD9): i += 1; continue
    x = 0
    for c in f[:18]: x ^= c
    if x != f[18]: bad += 1; i += 1; continue
    i += 19
    kind = "F2" if f[0] == 0xF2 else chr(f[1])
    counts[kind] = counts.get(kind, 0) + 1
    if f[0] == 0xF2: live["F2"] = f; continue
    off = f[2] << 8 | f[3]
    if kind in REG:
        size = REG[kind][1]
        n = min(14, size - off)
        if n > 0:
            dump[kind][off:off + n] = f[4:4 + n]; have[kind][off:off + n] = b"\x01" * n
    elif kind == "R":
        live.setdefault("R", bytearray(52))
        n = min(14, 52 - off); live["R"][off:off + n] = f[4:4 + n]
    else:
        live[kind] = f

print(f"stream {len(stream)} bytes; frames {dict(sorted(counts.items()))}; bad XOR {bad}")
for k, (name, size) in REG.items():
    got = sum(have[k]); path = os.path.join(outdir, name)
    open(path, "wb").write(bytes(dump[k]))
    print(f"  {name:26s} {got:6d} / {size} bytes received ({100 * got / size:5.1f}%) -> {path}")
if "A" in live:
    f = live["A"]; print(f"  A: R34..R37 {f[4:8].hex(' ')}  latch mask 0x{f[8]:02X}  nonzero {f[9]}  power-on R20..R27 {f[10:18].hex(' ')}")
if "B" in live:
    print(f"  B: power-on R28..R35 {live['B'][4:18].hex(' ')}")
if "R" in live:
    r = live["R"]; ad1 = r[0] | r[1] << 8 | r[2] << 16
    print(f"  R: HY3131 R00..R33 {bytes(r).hex()}  (AD1 raw 0x{ad1:06x} = {ad1 - (1 << 24) if ad1 & 0x800000 else ad1})")
if "S" in live:
    f = live["S"]; mode, rng = f[4], f[5]
    bat, ad1avg, rd, gpg = struct.unpack_from("<HiiH", f, 6)
    print(f"  S: mode {mode} range {rng}  battery {bat / 10:.1f} V  AD1 average {ad1avg}  reading {rd}  GPIOG.IDR 0x{gpg:04x}")
if "T" in live:
    f = live["T"]; y, mo, d, h, mi, s = f[4:10]
    fast = struct.unpack_from("<i", f, 14)[0]
    print(f"  T: clock 20{y:02d}-{mo:02d}-{d:02d} {h:02d}:{mi:02d}:{s:02d}  GPIOE.IDR hi 0x{f[10]:02x} (jacks)  PF8 low {f[11]}  "
          f"variant_ohm6 {f[12]}  variant_acv0 {f[13]}  reading_fast {fast}")
if "F2" in live: print(f"  last stock packet: {live['F2'].hex(' ')}")
