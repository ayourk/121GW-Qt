#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# OF-01 regression: Ohm_R6Correction (0x08007D1E) over readings -2000..60000 (step 1) with trims -3000, 100, 3000 and
# a garbage r7, on two images; prints the number of differing results and the first few.
#   sweep_of01.py A.bin B.bin
import sys
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R7, UC_ARM_REG_SP, UC_ARM_REG_LR

def make(path):
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x08000000, 0x80000); uc.mem_write(0x08006000, open(path, "rb").read())
    uc.mem_map(0x20000000, 0xC000); uc.mem_write(0x0807F000, b"\xfe\xe7")
    return uc

def call(uc, reading, trim):
    uc.reg_write(UC_ARM_REG_R0, reading & 0xFFFFFFFF); uc.reg_write(UC_ARM_REG_R1, trim & 0xFFFFFFFF)
    uc.reg_write(UC_ARM_REG_R7, 0xA5A5A5A5)
    uc.reg_write(UC_ARM_REG_SP, 0x2000B000); uc.reg_write(UC_ARM_REG_LR, 0x0807F001)
    uc.emu_start(0x08007D1F, 0x0807F000, count=100000)
    return uc.reg_read(UC_ARM_REG_R0)

a, b = make(sys.argv[1]), make(sys.argv[2])
n, diff = 0, []
for trim in (-3000, 100, 3000):
    for r in range(-2000, 60001):
        n += 1
        x, y = call(a, r, trim), call(b, r, trim)
        if x != y: diff.append((trim, r, x, y))
print(f"{n} calls, {len(diff)} differ", diff[:5])
