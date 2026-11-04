#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Builds the factory-default data-EEPROM image (12 KB at 0x08080000) for emulated key-press runs: every multi-key
# sequence starts from this (memory feedback_keypress_tests_factory_defaults). 2.05 has no factory-reset routine,
# so the values are chosen: the manual's stated defaults (APO on, 30 min; logging once per second; buzzer usable),
# the firmware's loader fallbacks otherwise, both board-variant markers 0xA5 (as factory calibration writes them),
# empty last-mode slots, and neutral calibration (offset 0, gain 1.0) in every record so readings are not 0.
#   factory_eeprom.py EEVBlog2_05.bin factory-eeprom.bin
import sys, struct

img = open(sys.argv[1], "rb").read()
BASE = 0x08006000
T = struct.unpack_from("<26H", img, 0x08028ee8 - BASE)        # cal record table T[mode]
ee = bytearray(0x3000)

def u8(off, v): ee[off] = v & 0xFF
def u16(off, v): ee[off:off + 2] = struct.pack(">H", v)
def u32(off, v): ee[off:off + 4] = struct.pack(">I", v & 0xFFFFFFFF)

# calibration first (records may overlap the low setting bytes for no mode: T starts at 0x30)
for m, t in enumerate(T):
    for r in range(7):
        a = t + 16 * r
        if a + 8 <= 0x2e0:
            ee[a:a + 8] = struct.pack(">iI", 0, 1_000_000)
u32(0x104, 49_152_050)        # frequency crystal constant (firmware default, 4.9152 MHz x 10)
u16(0x2e0, 0)                 # temperature trim (slot 200)
# settings (docs/eeprom-settings.md)
u16(0x01, 1708)               # date stamp YYMM (firmware default)
u32(0x03, 0)                  # meter ID
for off in range(0x08, 0x0F): u8(off, 0)    # last mode per rotary position: empty
u8(0x10, 1)                   # temperature unit: 1 = C (0 = F: Mode_Temp converts when 0; the manual's default is C)
u8(0x12, 0)                   # diode test: 3 V
u8(0x14, 0)                   # continuity mode 0
u8(0x16, 0)                   # APO on (manual: powers off after 30 minutes)
u16(0x18, 1)                  # log interval 1 s (manual: once per second)
u8(0x1A, 14)                  # LCD contrast (firmware fallback)
u8(0x1B, 0)                   # VA burden display off
u8(0x1E, 1)                   # buzzer on (blank EEPROM would leave it off)
u8(0x1F, 0xA5)                # board-variant marker (ACV lowest range)
u8(0x26, 0xA5)                # board-variant marker (Ohm range 6, switch PS0)
open(sys.argv[2], "wb").write(bytes(ee))
print(f"wrote {sys.argv[2]}: {len(ee)} bytes; cal records for {len(T)} modes x 7 ranges")
