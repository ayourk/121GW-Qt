#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Wrapper around scripts/emu_run.py that adds memory-write watches and PC traces (UI-area tests of the zombu4
# OF-nn ports). Leaves emu_run.py itself untouched: it replaces unicorn.Uc with a subclass that installs the extra
# hooks, then runs emu_run.py in this process.
#   emu_watch.py [--ww ADDR[:LEN],...] [--pc ADDR,...] [--pcmax N] -- <emu_run.py arguments>
# --ww   print every write that touches ADDR..ADDR+LEN-1 (time, pc, value, IPSR when in a handler)
# --pc   print each execution of these instruction addresses (time, r0..r3)
# --usart-gate   USART2.SR reads 0 while its APB1 clock (RCC_APB1ENR bit 17) is off (RM0038: unclocked registers
#                read 0), instead of emu_run's constant TXE|TC
# --pc-poke ADDR:MEM=HEX[,...]  the first time ADDR executes, write HEX (bytes as given) to MEM
# --nvic         model NVIC ISER/ICER for TIM2 (IRQ 28) and TIM3 (IRQ 29): emu_run calls their handlers only while
#                enabled there too (emu_run itself looks at the timer's CEN/UIE only)
# --tim2-at ADDR[@MS] the first time ADDR executes (at or after MS), end the 1 ms slice there with the TIM2 50-tick counter at 49, so that
#                emu_run calls TIM2_IRQHandler at that instruction and its 50-tick block runs (forces a rare phase)
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
argv = sys.argv[1:]
ww, pcs, pcmax, gate, tim2_at, nvic, pcpoke = [], [], 200, False, None, False, []
while argv and argv[0] != "--":
    k = argv.pop(0)
    if k == "--ww":
        for item in argv.pop(0).split(","):
            a, _, n = item.partition(":"); ww.append((int(a, 16), int(n or "1", 0)))
    elif k == "--pc":
        pcs += [int(x, 16) for x in argv.pop(0).split(",")]
    elif k == "--usart-gate":
        gate = True
    elif k == "--pc-poke":
        for item in argv.pop(0).split(","):
            a, rest = item.split(":"); m, h = rest.split("=")
            pcpoke.append((int(a, 16), int(m, 16), bytes.fromhex(h)))
    elif k == "--nvic":
        nvic = True
    elif k == "--tim2-at":
        x, _, t = argv.pop(0).partition("@"); tim2_at = (int(x, 16), int(t or "0"))
    elif k == "--pcmax":
        pcmax = int(argv.pop(0))
    else:
        sys.exit(f"unknown option {k}")
if argv: argv.pop(0)
import unicorn
from unicorn import UC_HOOK_MEM_WRITE, UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.arm_const import UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3
G = {"__name__": "__main__"}
counts = {}

def now():
    return G.get("ms", -1)

_orig_start = unicorn.Uc.emu_start
_done = set()

def install(self):
    for addr, n in ww:
        def cb(uc, access, address, size, value, user, addr=addr, n=n):
            if address + size <= addr or address >= addr + n: return
            pc = uc.reg_read(UC_ARM_REG_PC)
            key = ("w", addr); counts[key] = counts.get(key, 0) + 1
            if counts[key] <= pcmax:
                print(f"  WW t={now()} pc {pc:08x} [{address:08x}]/{size} <- 0x{value & ((1 << 8 * size) - 1):0{2 * size}x}")
        self.hook_add(UC_HOOK_MEM_WRITE, cb, begin=addr & ~3, end=addr + n + 3)
    for p in pcs:
        def cb(uc, address, size, user, p=p):
            key = ("p", p); counts[key] = counts.get(key, 0) + 1
            if counts[key] <= pcmax:
                r = [uc.reg_read(x) for x in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)]
                print(f"  PC t={now()} {p:08x} r0..r3 {' '.join(f'{v:08x}' for v in r)}")
        self.hook_add(UC_HOOK_CODE, cb, begin=p, end=p)

    if gate:
        def rd(uc, access, address, size, value, user):
            en = int.from_bytes(uc.mem_read(0x40023824, 4), "little") >> 17 & 1
            if not en: uc.mem_write(0x40004400, b"\0\0\0\0")
        self.hook_add(UC_HOOK_MEM_READ, rd, begin=0x40004400, end=0x40004403)
    for a, m, h in pcpoke:
        done = [False]
        def cb(uc, address, size, user, m=m, h=h, done=done):
            if not done[0]:
                done[0] = True; uc.mem_write(m, h); print(f"  PC-POKE t={now()} at {address:08x}: [{m:08x}] <- {h.hex()}")
        self.hook_add(UC_HOOK_CODE, cb, begin=a, end=a)
    if nvic:
        en = [0]
        def wr(uc, access, address, size, value, user):
            if address == 0xE000E100: en[0] |= value
            elif address == 0xE000E180: en[0] &= ~value
        self.hook_add(UC_HOOK_MEM_WRITE, wr, begin=0xE000E100, end=0xE000E183)
        def rd(uc, access, address, size, value, user):    # ISER0 / ICER0 read back the enabled set
            uc.mem_write(address & ~3, (en[0] & 0xFFFFFFFF).to_bytes(4, "little"))
        self.hook_add(UC_HOOK_MEM_READ, rd, begin=0xE000E100, end=0xE000E103)
        self.hook_add(UC_HOOK_MEM_READ, rd, begin=0xE000E180, end=0xE000E183)
        orig = G["armed"]
        G["armed"] = lambda tim: orig(tim) and (en[0] >> {0x40000000: 28, 0x40000400: 29}[tim] & 1)
    if tim2_at is not None:
        fired = [False]
        def cb(uc, address, size, user):
            if fired[0] or now() < tim2_at[1]: return
            fired[0] = True
            uc.mem_write(0x20007f04, (49).to_bytes(4, "little"))
            G["ms"] = G["ms"] - G["ms"] % 10 + 9          # emu_run calls TIM2 after slices with ms % 10 == 9
            print(f"  TIM2-AT t={G['ms']} at {address:08x}: slice ended, TIM2 50-tick block next")
            uc.emu_stop()
        self.hook_add(UC_HOOK_CODE, cb, begin=tim2_at[0], end=tim2_at[0])

def emu_start(self, *a, **k):                    # install the hooks on the first run of each engine
    if id(self) not in _done:
        _done.add(id(self)); install(self)
    return _orig_start(self, *a, **k)

unicorn.Uc.emu_start = emu_start
path = os.path.join(ROOT, "scripts", "emu_run.py")
sys.argv = [path] + argv
sys.path.insert(0, os.path.join(ROOT, "scripts"))
G["__file__"] = path
try:
    exec(compile(open(path).read(), path, "exec"), G)
finally:
    for (kind, a), c in sorted(counts.items()):
        print(f"WATCH {kind} {a:08x}: {c} hits")
