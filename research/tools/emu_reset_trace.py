#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Emulates the stock 121GW image from its reset vector (Unicorn, Cortex-M3 Thumb) and prints every write to the
# peripheral space (0x40000000-0x5FFFFFFF, plus the Cortex-M SCS) in order, named from the SVD, until an instruction
# budget runs out or the code waits on hardware. Ready/status flags are faked so the wait loops pass: reads of
# RCC.CR / RCC.CSR / PWR.CSR / LCD.SR / ADC.SR return the "ready" bits, and RCC.CFGR's SWS follows SW. A check of docs/startup.md, not a simulator.
#   emu_reset_trace.py EEVBlog2_05.bin svd/STM32L1xx.svd [max-instructions]
import sys, struct, xml.etree.ElementTree as ET
import os as _os, sys as _sys; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import ucfix   # patched libunicorn (Thumb IT state vs memory hooks); see ucfix.py
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ, UcError
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_PC

img = open(sys.argv[1], "rb").read()
budget = int(sys.argv[3]) if len(sys.argv) > 3 else 200000
BASE = 0x08006000

# register names from the SVD: address -> PERIPH.REG
names = {}
root = ET.parse(sys.argv[2]).getroot()
periphs = {p.findtext("name"): p for p in root.find("peripherals")}
for p in periphs.values():
    src = periphs.get(p.get("derivedFrom"), p) if p.get("derivedFrom") else p
    base = int(p.findtext("baseAddress"), 0)
    for r in src.iter("register"):
        names[base + int(r.findtext("addressOffset"), 0)] = p.findtext("name") + "." + r.findtext("name")
names.update({0xE000ED08: "SCB.VTOR", 0xE000ED0C: "SCB.AIRCR", 0xE000E010: "SYSTICK.CTRL", 0xE0042004: "DBGMCU.CR"})
def name(a):
    if a in names: return names[a]
    if 0x42000000 <= a < 0x44000000:                       # bit-band alias -> register.bit
        off = a - 0x42000000; byte = off // 32; reg = 0x40000000 + (byte & ~3); bit = (byte & 3) * 8 + (off // 4) % 8
        return f"{names.get(reg, hex(reg))}.bit{bit} (bit-band)"
    return hex(a)

READY = {0x40023834: (1 << 1) | (1 << 9),                            # RCC.CSR: LSIRDY LSERDY
         0x40007004: 0,                                              # PWR.CSR: VOSF clear, no WU/SB
         0x40002408: (1 << 0) | (1 << 5) | (1 << 4),                 # LCD.SR (offset 0x08): ENS RDY FCRSF
         0x40012400: (1 << 6) | (1 << 1)}                            # ADC.SR: ADONS EOC

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0x08000000, 0x00080000)                 # flash (bootloader area left zero)
uc.mem_write(BASE, img)
uc.mem_map(0x08080000, 0x4000)                     # data EEPROM, erased (reads 0 -> defaults)
uc.mem_map(0x1FF80000, 0x1000)                     # option bytes (BOR level 0: no option-byte relaunch)
uc.mem_map(0x20000000, 0xC000)                     # RAM
uc.mem_map(0x22000000, 0x0180_0000)                # RAM bit-band
uc.mem_map(0x40000000, 0x0010_0000)                # peripherals
uc.mem_map(0x42000000, 0x0200_0000)                # peripheral bit-band
uc.mem_map(0xE0000000, 0x0010_0000)                # system control space

writes = []
def on_write(uc, access, addr, size, value, user):
    writes.append((uc.reg_read(UC_ARM_REG_PC), addr, size, value))
    if 0x42000000 <= addr < 0x44000000:                  # bit-band alias: set/clear the bit in the real register
        off = addr - 0x42000000; byte = 0x40000000 + off // 32; bit = (off // 4) % 8
        b = uc.mem_read(byte, 1)[0]
        uc.mem_write(byte, bytes([(b | (1 << bit)) if value & 1 else (b & ~(1 << bit) & 0xFF)]))
def on_read(uc, access, addr, size, value, user):
    if addr in READY: uc.mem_write(addr, struct.pack("<I", READY[addr])[:4])
    elif addr == 0x40023800:                             # RCC.CR: each RDY bit follows its ON bit
        v = struct.unpack("<I", uc.mem_read(addr, 4))[0]
        rdy = 0
        for on, ready in ((0, 1), (8, 9), (16, 17), (24, 25)):   # HSI, MSI, HSE, PLL
            if v >> on & 1: rdy |= 1 << ready
        uc.mem_write(addr, struct.pack("<I", (v & ~((1 << 1) | (1 << 9) | (1 << 17) | (1 << 25))) | rdy))
    elif addr == 0x40023808:                             # RCC.CFGR: SWS follows SW (clock switch done)
        v = struct.unpack("<I", uc.mem_read(addr, 4))[0]
        uc.mem_write(addr, struct.pack("<I", (v & ~0xC) | ((v & 3) << 2)))
uc.hook_add(UC_HOOK_MEM_WRITE, on_write, begin=0x40000000, end=0x5FFFFFFF)
uc.hook_add(UC_HOOK_MEM_WRITE, on_write, begin=0xE0000000, end=0xE00FFFFF)
uc.hook_add(UC_HOOK_MEM_READ, on_read, begin=0x40000000, end=0x4FFFFFFF)

sp, reset = struct.unpack_from("<II", img, 0)
uc.reg_write(UC_ARM_REG_SP, sp)
print(f"SP 0x{sp:08x}  Reset_Handler 0x{reset:08x}  budget {budget} instructions")
try:
    uc.emu_start(reset | 1, 0xFFFFFFFF, count=budget)
except UcError as e:
    print(f"stopped: {e} at pc 0x{uc.reg_read(UC_ARM_REG_PC):08x}")
print(f"stopped at pc 0x{uc.reg_read(UC_ARM_REG_PC):08x} after the budget or an exception; {len(writes)} peripheral writes")
lcd_sr = struct.unpack("<I", uc.mem_read(0x40002408, 4))[0]
print(f"LCD.SR at stop: 0x{lcd_sr:08x}")
last = None
for pc, a, sz, v in writes:
    key = (name(a), v)
    if key == last: continue                             # collapse repeats
    last = key
    print(f"  pc {pc:08x}  {name(a):34s} <- 0x{v:0{sz*2}x}")
