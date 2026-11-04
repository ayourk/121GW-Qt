#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Unit harness: call one function of an image in Unicorn with RAM preset, print r0 and chosen RAM words after return.
#   ucall.py IMAGE FUNC [r0 [r1 ...]] [--w ADDR=VAL] [--h ADDR=VAL] [--b ADDR=VAL] [--rd ADDR[:w|h|b]] [--trace-reads]
import sys, struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_READ
from unicorn.arm_const import *

def main():
    a = sys.argv[1:]
    img = open(a[0], "rb").read(); fn = int(a[1], 16)
    regs, sets, reads, trace = [], [], [], False
    i = 2
    while i < len(a):
        x = a[i]
        if x in ("--w", "--h", "--b"):
            ad, v = a[i + 1].split("="); sets.append((x[2], int(ad, 16), int(v, 0))); i += 2; continue
        if x == "--rd": reads.append(a[i + 1]); i += 2; continue
        if x == "--trace-reads": trace = True; i += 1; continue
        regs.append(int(x, 0) & 0xFFFFFFFF); i += 1
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x08000000, 0x80000); uc.mem_write(0x08006000, img)
    uc.mem_map(0x20000000, 0x10000)
    uc.mem_map(0x40000000, 0x100000); uc.mem_map(0xE0000000, 0x100000)
    TRAP = 0x0807F000; uc.mem_write(TRAP, b"\xfe\xe7")
    for k, ad, v in sets:
        uc.mem_write(ad, struct.pack({"w": "<i" if v < 0 else "<I", "h": "<h" if v < 0 else "<H", "b": "<b" if v < 0 else "<B"}[k], v))
    if trace:
        def hr(uc, acc, ad, sz, val, ud):
            if 0x08006000 <= ad < 0x0802A000 + 0x6000 and sz == 4:
                pc = uc.reg_read(UC_ARM_REG_PC)
                if not (0x08006000 <= ad < 0x08029500) or True:
                    pass
            if 0x08028000 <= ad < 0x08029500:
                print(f"  read {ad:08x} sz{sz} pc={uc.reg_read(UC_ARM_REG_PC):08x}")
        uc.hook_add(UC_HOOK_MEM_READ, hr)
    for r, v in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), regs): uc.reg_write(r, v)
    uc.reg_write(UC_ARM_REG_SP, 0x2000F000); uc.reg_write(UC_ARM_REG_LR, TRAP | 1)
    uc.emu_start(fn | 1, TRAP, count=2000000)
    r0 = uc.reg_read(UC_ARM_REG_R0)
    out = [f"r0={r0 - (1 << 32) if r0 >= 1 << 31 else r0}"]
    for s in reads:
        ad, _, k = s.partition(":"); k = k or "w"; ad = int(ad, 16)
        n = {"w": 4, "h": 2, "b": 1}[k]; v = int.from_bytes(uc.mem_read(ad, n), "little", signed=True)
        out.append(f"[{ad:08x}]={v}")
    print(" ".join(out))

main()
