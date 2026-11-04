#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Calls one function of the stock 121GW image in Unicorn with register arguments and prints r0 (and r1) on return:
# for identifying runtime-library helpers by behavior (float/double ops, conversions). Arguments: f:<float> passes a
# float's bits, d:<double> passes a double in r0:r1 (and the next in r2:r3), anything else is an integer.
#   emu_call.py EEVBlog2_05.bin 0x08012f94 f:6.0 f:2.0
import sys, struct
import os as _os, sys as _sys; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import ucfix   # patched libunicorn (Thumb IT state vs memory hooks); see ucfix.py
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR

img = open(sys.argv[1], "rb").read()
fn = int(sys.argv[2], 0)
regs = []
for a in sys.argv[3:]:
    if a.startswith("f:"): regs.append(struct.unpack("<I", struct.pack("<f", float(a[2:])))[0])
    elif a.startswith("d:"):
        lo, hi = struct.unpack("<II", struct.pack("<d", float(a[2:]))); regs += [lo, hi]
    else: regs.append(int(a, 0) & 0xFFFFFFFF)
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0x08000000, 0x80000); uc.mem_write(0x08006000, img)
uc.mem_map(0x20000000, 0xC000)
TRAP = 0x0807F000; uc.mem_write(TRAP, b"\xfe\xe7")
for r, v in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), regs): uc.reg_write(r, v)
uc.reg_write(UC_ARM_REG_SP, 0x2000B000); uc.reg_write(UC_ARM_REG_LR, TRAP | 1)
uc.emu_start(fn | 1, TRAP, count=100000)
r0, r1 = uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R1)
f = struct.unpack("<f", struct.pack("<I", r0))[0]
d = struct.unpack("<d", struct.pack("<II", r0, r1))[0]
print(f"r0=0x{r0:08x} ({r0 if r0 < 0x80000000 else r0 - 0x100000000})  as float {f:g}  r0:r1 as double {d:g}")
