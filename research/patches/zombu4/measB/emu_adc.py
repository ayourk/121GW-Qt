#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# emu_run.py with an ADC write monitor (OF-13 / OF-39): every write to ADC_CR2 (0x40012408) and to the sample-time
# registers ADC_SMPR1..3 (0x4001240C..0x40012414) is counted; a sample-time write while CR2.ADON = 1 is a violation
# (RM0038 12.15.6: "must be written only when ADON=0"). Environment: ADC_R6=HEX starts the CPU with that r6 (PM0056:
# R0..R12 are unknown at reset; emu_run.py starts them at 0). Summary to stderr: "ADC: ...". Same arguments as emu_run.py.
import os, sys, runpy
HERE = os.path.dirname(os.path.abspath(__file__)); SCRIPTS = os.path.join(HERE, "..", "..", "..", "scripts")
sys.path.insert(0, SCRIPTS)
import unicorn
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R6, UC_ARM_REG_PC
st = {"cr2": 0, "cr2_vals": [], "smpr_on": 0, "smpr_off": 0, "pcs_on": set()}
def on_w(uc, acc, x, size, v, u):
    if x == 0x40012408:
        st["cr2"] = v
        if v not in st["cr2_vals"]: st["cr2_vals"].append(v)
    elif 0x4001240C <= x <= 0x40012414:
        if st["cr2"] & 1: st["smpr_on"] += 1; st["pcs_on"].add(uc.reg_read(UC_ARM_REG_PC))
        else: st["smpr_off"] += 1
_Uc = unicorn.Uc
def Uc(*a):
    uc = _Uc(*a)
    uc.hook_add(UC_HOOK_MEM_WRITE, on_w, begin=0x40012400, end=0x400124FF)
    if os.environ.get("ADC_R6"): uc.reg_write(UC_ARM_REG_R6, int(os.environ["ADC_R6"], 16))
    return uc
unicorn.Uc = Uc
import atexit
atexit.register(lambda: print(f"ADC: CR2 values written {[hex(v) for v in st['cr2_vals']]}; sample-time writes with ADON=1: "
                              f"{st['smpr_on']} (pc {sorted(hex(p) for p in st['pcs_on'])}), with ADON=0: {st['smpr_off']}", file=sys.stderr))
sys.argv = [os.path.join(SCRIPTS, "emu_run.py")] + sys.argv[1:]
try: runpy.run_path(sys.argv[0], run_name="__main__")
except SystemExit: pass
