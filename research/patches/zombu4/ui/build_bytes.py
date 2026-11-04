#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Writes patches/zombu4/ui/bytes.json (byte patches of the UI area, for scripts/patch_stock.py --extra) by
# assembling each replacement at its address with arm-none-eabi-as/ld, and checks every OLD against stock 2.05.
#   build_bytes.py            (run from anywhere; reads EEVBlog2_05.bin from the project root)
import json, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
BASE = 0x08006000
STOCK = open(os.path.join(ROOT, "EEVBlog2_05.bin"), "rb").read()

def asm(addr, src):
    """assemble Thumb-2 `src` placed at `addr`; returns the bytes"""
    with tempfile.TemporaryDirectory(dir=tempfile.gettempdir()) as t:
        s, o, e, b = (os.path.join(t, n) for n in ("a.s", "a.o", "a.elf", "a.bin"))
        open(s, "w").write(".syntax unified\n.thumb\n.cpu cortex-m3\n.text\n" + src + "\n")
        subprocess.run(["arm-none-eabi-as", "-mcpu=cortex-m3", "-mthumb", s, "-o", o], check=True)
        subprocess.run(["arm-none-eabi-ld", "-e", hex(addr), "-Ttext", hex(addr), o, "-o", e], check=True)
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "-j", ".text", e, b], check=True)
        return open(b, "rb").read()

def site(addr, old_hex, src):
    new = asm(addr, src)
    old = bytes.fromhex(old_hex)
    if len(new) != len(old): sys.exit(f"{addr:08x}: new {len(new)} B, old {len(old)} B")
    have = STOCK[addr - BASE:addr - BASE + len(old)]
    if have != old: sys.exit(f"{addr:08x}: stock holds {have.hex()}, expected {old_hex}")
    return [f"{addr:08X}", old.hex(), new.hex()]

def append(addr, src):
    return [f"{addr:08X}", "", asm(addr, src).hex()]

P = {}
# OF-43: Keys_Scan's two byte counters (equal-sample count 0x200080EF, hold time 0x20008072) stop at 0xFF instead of
# wrapping to 0, which re-armed the key as a new press: one long press per hold, no short press after it.
# Each `ldr r0,=x; ldrb r0,[r0]; adds r0,#1; ldr r1,=x; strb r0,[r1]` becomes
# `ldr r1,=x; ldrb r0,[r1]; cmp r0,#255; it ne; addne r0,#1; strb r0,[r1]` (same literal, same 14 bytes).
P["of43-key-saturate"] = [
    site(0x080168D0, "dff838040078401cdff830140870",
         "ldr.w r1, [pc, #0x438]\nldrb r0, [r1]\ncmp r0, #255\nit ne\naddne r0, r0, #1\nstrb r0, [r1]"),
    site(0x0801695E, "dff8b0030078401cdff8a8130870",
         "ldr.w r1, [pc, #0x3b0]\nldrb r0, [r1]\ncmp r0, #255\nit ne\naddne r0, r0, #1\nstrb r0, [r1]"),
]
# OF-37: SETUP short in calibration mode: a Cal_ArmCapture result other than 2 (capture armed, or AC point flagged)
# ends with the accept beep (the existing `bl Beep_Short; b end` at 0x08014E1E) instead of running on into the MEM
# short code (refusal beep after a good start; AC slot 2 rewritten from the slot-1 reading).
P["of37-setup-end"] = [site(0x08014DD4, "0bd1", "bne.n 0x08014E1E")]
# OF-38 (behavior change): Settings_LoadContrast accepts the editor's whole range 10..17 at power-up (was 12..17).
P["of38-contrast-boot"] = [site(0x08011264, "0c2c", "cmp r4, #10")]
# OF-50 (latent): LCD_ApplyContrast passes the CC mask itself, not LCD_FCR AND mask (ands.w -> mov.w, 7 settings).
P["of50-contrast-mask"] = [site(a, "10f4" + hi, "mov.w r0, #" + str(m))
                           for a, hi, m in [(0x0801B7AC, "8060", 0x400), (0x0801B7B8, "0060", 0x800),
                                            (0x0801B7C4, "4060", 0xC00), (0x0801B7D0, "8050", 0x1000),
                                            (0x0801B7DC, "a050", 0x1400), (0x0801B7E8, "c050", 0x1800),
                                            (0x0801B7F4, "e050", 0x1C00)]]
# OF-49 (latent): Num_SplitDigits keeps only the units of n / 10000 for the first digit word (stub in the UI area's
# free flash 0x08029FC0; the routine is a leaf, so b.w there and back, r3 is a scratch register).
P["of49-digit-mod"] = [
    append(0x08029FC0, "udiv r1, r0, r1\nmovs r2, #10\nudiv r3, r1, r2\nmls r1, r3, r2, r1\nb.w 0x0801B098"),
    site(0x0801B094, "b0fbf1f1", "b.w 0x08029FC0"),
]
# OF-23 (proposal B, owner choice): RANGE in DIODE no longer writes EEPROM 0x12; the value is never restored (every
# mode apply and the boot screen force 3 V, deliberately it seems), so the write only cost an EEPROM cycle.
P["of23-diode-nosave"] = [site(0x080159EE, "fbf7c9fc", "nop.w")]
# OF-47 (behavior change): logging stopped by a low battery sets message code 0x14 ("LoBat", as a refused start)
# instead of 100, which LCD_SubSdError drew as "SdE" and two blank digits.
P["of47-lobat-stop"] = [site(0x0801D316, "6420", "movs r0, #0x14")]

out = os.path.join(HERE, "bytes.json")
tmp = out + ".part"
with open(tmp, "w") as f:
    json.dump(P, f, indent=1); f.flush(); os.fsync(f.fileno())
os.replace(tmp, out)
for k, v in P.items(): print(k, v)
