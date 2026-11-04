#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# emu_run.py with the HY3131 model instrumented for OF-53 / OF-54: every INTF flag the model raises is tracked until
# the firmware has read it out (INTF byte clocked out on DO); a flag cleared before it was read out is LOST. A counter
# completion that changes CTA/CTB/CTC while a read frame is in progress after those registers were clocked out but
# before INTF was is STALE (the firmware sees CTF with the previous count). Same arguments as emu_run.py; the summary
# goes to stderr as "FLAGS: ...". Environment: FREQ_AT=T_MS:HZ,... changes the counter input frequency during the
# run; CT_STALL=FROM_MS:TO_MS keeps every count started in that window from completing; FLAGS_DEBUG=1 prints every CTF raised and read out.
import os, sys, runpy
HERE = os.path.dirname(os.path.abspath(__file__)); SCRIPTS = os.path.join(HERE, "..", "..", "..", "scripts")
sys.path.insert(0, SCRIPTS)
import hy3131_model as M
NAMES = {M.CTF: "CTF", M.AD2F: "AD2F", M.AD1F: "AD1F", M.LPFF: "LPFF", M.RMSF: "RMSF"}
DEBUG = bool(os.environ.get("FLAGS_DEBUG"))
st = {"unseen": 0, "lost": [], "raised": {}, "stale": [], "ctr_done_ms": None}
_tick, _load, _write, _pins = M.HY3131.tick_ms, M.HY3131._load_out, M.HY3131._write, M.HY3131.pins

def _cleared(self, before, why):
    gone = before & ~self.reg[M.INTF] & st["unseen"]
    for b, n in NAMES.items():
        if gone & b:
            st["lost"].append((self.now, n, why))
            if b == M.CTF: print(f"  FLAG lost t={self.now:.0f} {n} ({why})", file=sys.stderr)
    st["unseen"] &= ~gone

CT_STALL = [int(x) for x in os.environ.get("CT_STALL", "").split(":") if x] or None
FREQ_AT = sorted((int(t), float(f)) for t, f in (i.split(":") for i in filter(None, os.environ.get("FREQ_AT", "").split(","))))

def tick_ms(self, ms):
    for t, f in FREQ_AT:
        if ms == t: self.freq_hz = f
    if CT_STALL and CT_STALL[0] <= ms < CT_STALL[1] and self.reg[0x20] & 0x02:
        self._ct_end = False                              # count never completes (e.g. a capacitor too large)
    before = self.reg[M.INTF]; ctr = bytes(self.reg[0x15:0x1E])
    _tick(self, ms)
    new = self.reg[M.INTF] & ~before
    st["unseen"] |= new
    for b, n in NAMES.items():
        if new & b: st["raised"][n] = st["raised"].get(n, 0) + 1
    if new & M.CTF and DEBUG: print(f"  FLAG raised t={ms} CTF (cs {self.cs} phase {self.phase} addr {self.addr:02x})", file=sys.stderr)
    if new & M.CTF and self.cs == 0 and self.phase == "rdata" and self.start <= 0x15 and self.addr > 0x1D - 0 \
            and self.addr <= M.INTF and bytes(self.reg[0x15:0x1E]) != ctr:
        st["stale"].append(ms); print(f"  FLAG stale t={ms} CTF with old counter bytes", file=sys.stderr)
    elif new & M.CTF and self.cs == 0 and self.phase == "rdata" and self.start <= 0x15 and 0x15 < self.addr <= M.INTF:
        st["stale"].append(ms); print(f"  FLAG stale t={ms} CTF mid counter read (addr {self.addr:02x})", file=sys.stderr)

def _load_out(self):
    _load(self)
    if self.addr % len(self.reg) == M.INTF:
        if DEBUG and self.reg[M.INTF] & M.CTF: print(f"  FLAG seen t={self.now:.0f} INTF {self.reg[M.INTF]:02x}", file=sys.stderr)
        st["unseen"] &= ~self.reg[M.INTF]

def _write_(self, a, v):
    before = self.reg[M.INTF]; _write(self, a, v)
    if a % len(self.reg) == M.INTF: _cleared(self, before, "write")

def pins(self, ck, cs, di):
    before = self.reg[M.INTF]; _pins(self, ck, cs, di)
    if self.reg[M.INTF] != before: _cleared(self, before, "read-clear at frame end")

M.HY3131.tick_ms, M.HY3131._load_out, M.HY3131._write, M.HY3131.pins = tick_ms, _load_out, _write_, pins
import atexit
def summary():
    lost = {}
    for t, n, w in st["lost"]: lost[n] = lost.get(n, 0) + 1
    print(f"FLAGS: raised {st['raised']} lost {lost} stale-CTF {len(st['stale'])}", file=sys.stderr)
atexit.register(summary)
sys.argv = [os.path.join(SCRIPTS, "emu_run.py")] + sys.argv[1:]
try: runpy.run_path(sys.argv[0], run_name="__main__")
except SystemExit: pass
