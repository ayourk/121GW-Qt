#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Calls one function of a 121GW image in Unicorn with chosen r0..r7 (the OF-01..06 defects all depend on a register the
# function never sets), optional RAM/peripheral preloads, and prints r0 on return plus requested memory.
#   call_regs.py IMAGE FN [rN=VAL ...] [--mem ADDR=HEXBYTES ...] [--dump ADDR:LEN ...] [--at ADDR ...]
# --at ADDR prints r0..r3 and the 8 bytes at r1 each time execution enters ADDR (e.g. GPIO_Init's init structure).
# Peripheral space 0x40000000..0x4003FFFF is plain memory here (no device model).
import sys, struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7

REGS = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6,
        UC_ARM_REG_R7]

def num(s):
    if s.startswith("f:"): return struct.unpack("<I", struct.pack("<f", float(s[2:])))[0]
    return int(s, 0) & 0xFFFFFFFF

def main():
    a = sys.argv[1:]
    img = open(a[0], "rb").read()
    fn = int(a[1], 0)
    regs, mem, dump, at = {}, [], [], []
    i = 2
    while i < len(a):
        if a[i] == "--mem": k, v = a[i + 1].split("="); mem.append((int(k, 0), bytes.fromhex(v))); i += 2
        elif a[i] == "--dump": k, v = a[i + 1].split(":"); dump.append((int(k, 0), int(v, 0))); i += 2
        elif a[i] == "--at": at.append(int(a[i + 1], 0)); i += 2
        else: k, v = a[i].split("="); regs[int(k[1:])] = num(v); i += 1
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x08000000, 0x80000); uc.mem_write(0x08006000, img)
    uc.mem_map(0x20000000, 0xC000)
    uc.mem_map(0x40000000, 0x40000)
    trap = 0x0807F000; uc.mem_write(trap, b"\xfe\xe7")
    for k, v in mem: uc.mem_write(k, v)
    for n, v in regs.items(): uc.reg_write(REGS[n], v)
    if at:
        def hook(u, addr, size, _):
            if addr in at:
                r = [u.reg_read(REGS[j]) for j in range(4)]
                st = bytes(u.mem_read(r[1], 8)) if 0x20000000 <= r[1] < 0x2000C000 else b""
                print(f"at {addr:08x}: r0={r[0]:08x} r1={r[1]:08x} r2={r[2]:08x} r3={r[3]:08x} [r1]={st.hex()}")
        uc.hook_add(UC_HOOK_CODE, hook)
    uc.reg_write(UC_ARM_REG_SP, 0x2000B000); uc.reg_write(UC_ARM_REG_LR, trap | 1)
    uc.emu_start(fn | 1, trap, count=200000)
    r0 = uc.reg_read(UC_ARM_REG_R0)
    print(f"r0=0x{r0:08x} ({r0 if r0 < 0x80000000 else r0 - 0x100000000}) as float "
          f"{struct.unpack('<f', struct.pack('<I', r0))[0]:g}")
    for k, n in dump: print(f"mem {k:08x}: {bytes(uc.mem_read(k, n)).hex()}")

main()
