# SPDX-License-Identifier: MIT
"""Decodes the 121GW LCD shadow RAM (0x20007790: 8 words, COM0 SEG0-31, COM0 SEG32-43, COM1 ..., as LCD_Flush copies
to LCD->RAM) into text: main display (5 digits), sub display (5 digits), decimal points and icons.

Segment, decimal-point and icon codes ((COM << 6) | SEG) are data from the EEVblog/121GW-Firmware-Claude repository
(MIT; tools/re/extracted/lcdmap.json, src/lcd.h, src/tables_gen.h), which took them from stock 2.05's icon and digit
setters; each one used here is cross-checked by decoding screens the stock image draws in the emulator.
"""
import json, os, struct

REF = os.environ.get("EEV_REF", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "reference", "121gw", "121GW-Firmware-Claude", "src"))   # EEVblog's 121GW-Firmware-Claude src/
_lm = json.load(open(os.path.join(REF, "tools/re/extracted/lcdmap.json")))
SEGMAP = {int(p): {s: tuple(cs) for s, cs in m.items()} for p, m in _lm["segmap"].items()}   # pos -> seg -> (com, seg)
# segment set -> character; digits first so ambiguous shapes read as digits
_REV = {}
for ch, segs in sorted(_lm["font"].items(), key=lambda kv: (not kv[0].isdigit(), not kv[0].isupper(), kv[0])):
    _REV.setdefault(frozenset(segs), ch)
_REV[frozenset()] = " "
_REV[frozenset("g")] = "-"

ICONS = {0x00: "REL", 0x10: "MEM", 0x50: "A-HOLD", 0x90: "HOLD", 0xd0: "1ms", 0x5e: "AVG", 0x9e: "MAX", 0xde: "MIN",
         0x1f: "BT", 0x5f: "1kHzLPF", 0xe0: "AUTO", 0xa7: "APO", 0x8b: "degU1", 0xcb: "degU2", 0x67: "main-",
         0x9d: "sub-", 0xe7: "LOWBAT", 0xca: "A", 0x20: "AC", 0x9f: "BEEP", 0xa0: "DC", 0xdf: "DIODE", 0x89: "F",
         0x88: "Hz", 0x1e: "LowZ", 0x0b: "M", 0x48: "OHM", 0x11: "%", 0x8a: "V", 0x08: "k", 0x4a: "m", 0xc0: "ms",
         0x49: "n", 0x27: "HV"}   # 0x27 high-voltage warning (COM0 SEG39; from the 2.05 naming pass, not in lcdmap.json)
BAR = [0x41, 0x81, 0xc1, 0xc2, 0x82, 0x42, 0x02, 0x03, 0x43, 0x83, 0xc3, 0xc4, 0x84, 0x44, 0x04, 0x05, 0x45,
       0x85, 0xc5, 0xc6, 0x86, 0x46, 0x06, 0x07, 0x47]      # bargraph segments 1..25 (g_bar_seg)
EXTRA = {0x01: "bar0", 0x40: "bar+", 0x80: "bar-", 0x0a: "bar500", 0x0c: "bar1000", 0x87: "scaleA", 0xc8: "scaleB"}
# 0x87 / 0xc8: bargraph scale marks swapped by 0x0801912c (Bar_ScaleMark), medium
DP_MAIN = {1: 0x0e, 2: 0x21, 3: 0x23, 4: 0x25}
DP_SUB = {1: 0x15, 2: 0x17, 3: 0x19, 4: 0x1b}

def bits(shadow):
    """shadow: 32 bytes. Returns the set of (com, seg) that are on."""
    w = struct.unpack("<8I", shadow)
    on = set()
    for com in range(4):
        lo, hi = w[2 * com], w[2 * com + 1]
        for seg in range(32):
            if lo >> seg & 1: on.add((com, seg))
        for seg in range(12):
            if hi >> seg & 1: on.add((com, 32 + seg))
    return on

def _code(c): return (c >> 6, c & 0x3F)

def _digits(on, positions, dps, minus):
    out = ""
    for i, p in enumerate(positions):
        segs = frozenset(s for s, cs in SEGMAP[p].items() if cs in on)
        out += _REV.get(segs, "?")
        k = i + 1                                     # dp after digit i (dp n sits before the n-th last digit)
    text = list(out)
    for n, c in dps.items():                          # dp n: decimal point with n digits after it
        if _code(c) in on: text.insert(len(positions) - n, ".")
    s = "".join(text)
    return ("-" if _code(minus) in on else "") + s

def decode(shadow):
    on = bits(shadow)
    main = _digits(on, [5, 6, 7, 8, 9], DP_MAIN, 0x67)
    sub = _digits(on, [0, 1, 2, 3, 4], DP_SUB, 0x9d)
    icons = [n for c, n in sorted(ICONS.items(), key=lambda kv: kv[1]) if _code(c) in on and n not in ("main-", "sub-")]
    bar = sum(1 for c in BAR if _code(c) in on)
    if bar or any(_code(c) in on for c in EXTRA):
        icons.append(f"bar={bar}" + "".join("/" + n for c, n in EXTRA.items() if _code(c) in on))
    used = {cs for m in SEGMAP.values() for cs in m.values()} | {_code(c) for c in ICONS} | \
           {_code(c) for c in list(DP_MAIN.values()) + list(DP_SUB.values()) + BAR + list(EXTRA)}
    unknown = sorted(on - used)
    return main, sub, icons, unknown

def render(shadow):
    main, sub, icons, unknown = decode(shadow)
    s = f"'{main}' [{sub}] {' '.join(icons)}"
    if unknown: s += f"  +{len(unknown)} other segments"
    return s
