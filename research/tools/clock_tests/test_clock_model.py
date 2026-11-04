#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Unit tests of scripts/clock_model.py: TIM11 capture of HSE_RTC with an MSI-clocked counter recovers the MSI
# error; CC1OF on a missed capture; RTCPRE locked while HSE is on; UART corruption only past the tolerance.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from clock_model import ClockModel, RCC_CR, TIM11, MSI_NOMINAL

fails = 0
def check(name, ok, detail=""):
    global fails
    print(("PASS " if ok else "FAIL ") + name + (f"  ({detail})" if detail else ""))
    fails += not ok

class Mem(dict):
    def rd32(self, x): return self.get(x, 0)
    def wr32(self, x, v): self[x] = v & 0xFFFFFFFF

def setup(err, rtcpre=3, rtcpre_after_hse=False):
    m, logs = Mem(), []
    c = ClockModel(err, log=logs.append, seed=7, hse_rtc_tim11=True)
    def w(x, v):
        c.on_write(x, v, m.rd32); m.wr32(x, v)
    if rtcpre_after_hse:
        w(RCC_CR, 1 << 16); w(RCC_CR, (1 << 16) | (rtcpre << 29))
    else:
        w(RCC_CR, rtcpre << 29); w(RCC_CR, (rtcpre << 29) | (1 << 16))
    w(TIM11 + 0x28, 0); w(TIM11 + 0x2C, 0xFFFF); w(TIM11 + 0x50, 2)          # PSC 0, ARR, TI1_RMP = HSE_RTC
    w(TIM11 + 0x18, 1 | (3 << 2)); w(TIM11 + 0x20, 1); w(TIM11 + 0x00, 1)    # CC1S = TI1, ICPSC /8, CC1E, CEN
    return c, m, w, logs

def rd(c, m, x):
    c.on_read(x, m.rd32, m.wr32); return m.rd32(x)

def measure(c, m, n=2000, poll=12):
    """A firmware-like loop: poll SR every `poll` instructions, read CCR1 on CC1IF, sum the differences."""
    first = last = None; total = 0; got = 0; ovf = 0
    while got < n + 1:
        c.add_instructions(poll)
        sr = rd(c, m, TIM11 + 0x10)
        if sr & 0x200: ovf += 1
        if sr & 2:
            v = rd(c, m, TIM11 + 0x34)
            if last is not None: total += (v - last) & 0xFFFF
            last = v; got += 1
    return total, ovf

for err in (-3.0, -1.2, 0.0, 0.7, 2.5):
    c, m, w, logs = setup(err)
    total, ovf = measure(c, m)
    hse_rtc = 4e6 / 16
    est = total / (2000 * 8 / hse_rtc)
    e_est = (est / MSI_NOMINAL - 1) * 100
    check(f"msi error {err:+.1f}%: estimated {e_est:+.4f}%", abs(e_est - err) < 0.002 and ovf == 0, f"{total} ticks")

c, m, w, logs = setup(0.0)
c.add_instructions(1000)                                  # ~7 captures without reading CCR1
sr = rd(c, m, TIM11 + 0x10)
check("missed captures set CC1OF", sr & 0x200 and sr & 2)
w(TIM11 + 0x10, ~0x200 & 0xFFFF); sr = rd(c, m, TIM11 + 0x10)
check("SR is rc_w0 (CC1OF cleared, CC1IF kept)", not sr & 0x200 and sr & 2)
rd(c, m, TIM11 + 0x34); sr = rd(c, m, TIM11 + 0x10)
check("reading CCR1 clears CC1IF", not sr & 2)

c, m, w, logs = setup(0.0, rtcpre=3, rtcpre_after_hse=True)
check("RTCPRE write with HSE on is ignored", c.rtcpre == 0 and c.rtcpre_blocked == 1 and logs)
check("RCC_CR read shows the effective RTCPRE", (rd(c, m, RCC_CR) >> 29) & 3 == 0)

w(RCC_CR, 0)                                              # HSE off: HSE_RTC has no edges
n0 = c.captures; c.add_instructions(5000)
check("no HSE_RTC edges with HSE off", c.captures == n0)

# HSE_RTC is not connected by default (the real meter: no captures with RTCSEL = LSI)
m0 = Mem(); c0 = ClockModel(0.0, seed=7)
for x, v in ((RCC_CR, 3 << 29), (RCC_CR, (3 << 29) | (1 << 16)), (TIM11 + 0x50, 2), (TIM11 + 0x18, 1 | (3 << 2)),
             (TIM11 + 0x20, 1), (TIM11 + 0x00, 1)):
    c0.on_write(x, v, m0.rd32); m0.wr32(x, v)
c0.add_instructions(20000)
check("HSE_RTC gives no captures by default (as on the meter)", c0.captures == 0)

# the firmware's method: TIM11 captures MSI / 8 while SYSCLK is the PLL (HSE 4 MHz x 12 / 3 = 16 MHz)
RCC_CFGR = 0x40023808
for err in (-3.0, 0.0, 1.3, 2.5):
    m = Mem(); c = ClockModel(err, seed=5, rd32=m.rd32)
    def w(x, v):
        c.on_write(x, v, m.rd32); m.wr32(x, v)
    w(RCC_CR, 1 << 16); w(RCC_CFGR, 0x00910000); w(RCC_CR, (1 << 16) | (1 << 24))
    w(TIM11 + 0x50, 1); w(TIM11 + 0x18, 1 | (3 << 2)); w(TIM11 + 0x20, 1)
    w(RCC_CFGR, 0x00910003)                                      # SW = PLL
    check(f"SYSCLK = PLL 16 MHz (err {err:+.1f}%)", abs(c.sysclk_hz() - 16e6) < 1)
    w(TIM11 + 0x00, 1)
    first = last = None; total = got = ovf = 0
    while got < 1001:
        c.add_instructions(8)
        sr = rd(c, m, TIM11 + 0x10)
        if sr & 0x200: ovf += 1
        if sr & 2:
            v = rd(c, m, TIM11 + 0x34)
            if last is not None: total += (v - last) & 0xFFFF
            last = v; got += 1
    f = 1000 * 8 * 16e6 / total
    e = (f / MSI_NOMINAL - 1) * 100
    check(f"MSI on TI1, timer on the PLL: msi {err:+.1f}% estimated {e:+.4f}%", abs(e - err) < 0.01 and ovf == 0, f"{total} counts")
    w(RCC_CFGR, 0x00910000)                                      # back to MSI
    check("back on MSI after the switch", abs(c.sysclk_hz() - MSI_NOMINAL * (1 + err / 100)) < 1 and c.sw_switches == 2)

for err, brr, want in ((2.5, 218, True), (0.0, 218, False), (2.5, 224, False)):
    c = ClockModel(err, seed=3)
    bad = sum(c.tx_byte(0x55, brr) != 0x55 for _ in range(2000))
    e = c.baud_error(brr) * 100
    check(f"UART msi {err:+.1f}% BRR {brr} (baud error {e:+.2f}%): {'corrupts' if want else 'clean'}", (bad > 0) == want, f"{bad}/2000")
sys.exit(1 if fails else 0)
