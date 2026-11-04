#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# unit_check.py IMAGE: calls two routines of a 2.05 image directly in Unicorn (OF-49, OF-50 checks):
#  Num_SplitDigits (0x0801B090) for several n -> the five digit words 0x20007DB0..C0
#  LCD_ApplyContrast (0x0801B77A) for contrast 10..17 with LCD_FCR.CC = 7 and = 0 before -> CC after
import sys, struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR
img = open(sys.argv[1], "rb").read()
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0x08000000, 0x80000); uc.mem_write(0x08006000, img)
uc.mem_map(0x20000000, 0xC000); uc.mem_map(0x40000000, 0x30000)
TRAP = 0x0807F000; uc.mem_write(TRAP, b"\xfe\xe7")
def call(fn, r0=0):
    uc.reg_write(UC_ARM_REG_R0, r0); uc.reg_write(UC_ARM_REG_SP, 0x2000B000); uc.reg_write(UC_ARM_REG_LR, TRAP | 1)
    uc.emu_start(fn | 1, TRAP, count=100000)
for n in (0, 12345, 99999, 100000, 123456, 256000, 999999):
    call(0x0801B090, n)
    w = struct.unpack("<5I", uc.mem_read(0x20007DB0, 20))
    print(f"Num_SplitDigits({n}) = {list(w)}")
for cc0 in (7, 0):
    out = []
    for c in range(10, 18):
        uc.mem_write(0x40002404, struct.pack("<I", 0x00C05440 & ~(7 << 10) | cc0 << 10))
        uc.mem_write(0x20000237, bytes([c]))
        call(0x0801B77A)
        out.append(struct.unpack("<I", uc.mem_read(0x40002404, 4))[0] >> 10 & 7)
    print(f"LCD_ApplyContrast CC before {cc0}: settings 10..17 -> CC {out}")
