#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Builds the button map per dial position by emulating stock 2.05: for each position, a fresh boot from the factory
# EEPROM image (factory-eeprom.bin) per sequence (rule: feedback_keypress_tests_factory_defaults), a baseline
# snapshot, the MODE cycle (5 presses), and every other key short (150 ms) and long (800 ms). Records what changed
# against the baseline (mode, range, manual range, SETUP page, HOLD, REL, MIN/MAX, PEAK, LPF, AC+DC, BLE, beeps, the
# HY3131 configuration). Output: out/button-map.json + docs/button-map.md.
#   button_map.py            (run from firmware-re/)
import os, re, sys, json, subprocess, tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
POS = ["LowZ", "V", "mV", "Hz", "Ohm", "VA", "uA", "A"]
KEYS = ["SETUP", "MINMAX", "MEM", "RANGE", "HOLD", "REL", "PEAK"]
MODE_NAMES = "LowZ DCV ACV DCmV ACmV TEMP FREQ PULSE DUTY OHM BEEP DIODE CAP uVA(AC) mVA(AC) VA(AC) ACuA DCuA ACmA DCmA ACA DCA DCuVA DCmVA DCVA".split()
AD1 = {"LowZ": 3000}            # mid range 0 of each position (LowZ full scale is 6000)
FIELDS = ["mode", "range", "manual", "setup_page", "hold", "autohold", "rel", "minmax", "peak", "lpf", "acdc", "ble"]

SNAP = 1500
FACTORY = open(os.path.join(ROOT, "factory-eeprom.bin"), "rb").read()
# data-EEPROM areas (docs/eeprom-settings.md); 0x4D0-0x4FF: current-mode zero trims (long REL, range 0)
EE_AREAS = [(0x01, 2, "date stamp"), (0x03, 4, "meter ID"), (0x08, 7, "last mode per position"), (0x10, 1, "temp unit"),
            (0x12, 1, "diode 15/3 V"), (0x14, 1, "continuity mode"), (0x16, 1, "APO"), (0x18, 2, "log interval"),
            (0x1A, 1, "contrast"), (0x1B, 1, "VA burden display"), (0x1E, 1, "buzzer"), (0x1F, 1, "variant marker"),
            (0x26, 1, "variant marker")] + [(0x4D0 + 8 * i, 5, f"{m} zero trim") for i, m in enumerate(
            ["ACuA", "DCuA", "ACmA", "DCmA", "ACA", "DCA"])]

def ee_changes(path):
    after = open(path, "rb").read()
    out = []
    for off, n, name in EE_AREAS:
        if after[off:off + n] != FACTORY[off:off + n]: out.append(f"{name} 0x{off:03X} = {after[off:off + n].hex()}")
    known = {o + i for o, n, _ in EE_AREAS for i in range(n)}
    other = [i for i in range(len(after)) if after[i] != FACTORY[i] and i not in known]
    if other: out.append(f"{len(other)} other bytes from 0x{other[0]:03X}")
    return out

def run(pos, keys, ms=None):
    last = max(int(t) + (int(rest[0]) if rest else 150) for t, _k, *rest in (i.split(':') for i in keys.split(',')))
    ms = last + SNAP + 100
    cmd = [sys.executable, "-I", os.path.join(HERE, "emu_run.py"), "EEVBlog2_05.bin", "svd/STM32L1xx.svd",
           "--rotary", pos, "--ms", str(ms), "--hy-only", "--eeprom", "factory-eeprom.bin", "--sd", "out/sd/fat32-mbr.img", "--keys", keys,
           "--ad1", str(AD1.get(pos, 20000)), "--snap-delay", str(SNAP)]
    fd, eep = tempfile.mkstemp(prefix="bmap-ee-", dir=tempfile.gettempdir()); os.close(fd)
    out = subprocess.run(cmd + ["--ee-out", eep], cwd=ROOT, capture_output=True, text=True, timeout=600).stdout
    ee = ee_changes(eep); os.unlink(eep)
    states = []
    for line in out.splitlines():
        if "STATE" not in line: continue
        st = {f: int(re.search(rf"\b{f} (-?\d+)", line).group(1)) for f in FIELDS + ["reading", "beeps"]}
        st["hy"] = re.search(r"hy (\S+)", line).group(1)
        st["screen"] = re.search(r"screen (.*)$", line).group(1).strip()
        states.append(st)
    if states: states[-1]["ee"] = ee
    return states

WORDS = {"manual": "range locked", "hold": "HOLD", "autohold": "auto-hold", "rel": "REL", "minmax": "MIN/MAX",
         "peak": "1 ms PEAK", "lpf": "1 kHz LPF", "acdc": "AC+DC", "ble": "Bluetooth"}

def diff(a, b):
    out = []
    for f in FIELDS:
        if a[f] == b[f]: continue
        if f == "mode": out.append(f"mode -> {MODE_NAMES[b[f]] if b[f] < len(MODE_NAMES) else b[f]}")
        elif f == "range": out.append(f"range -> {b[f]}")
        elif f == "setup_page": out.append("logging on (page 100)" if b[f] == 100 else f"SETUP page {b[f]}")
        else: out.append(f"{WORDS[f]} {'on' if b[f] else 'off'}" if f != "manual" else ("range locked" if b[f] else "autorange"))
    if b["hy"] != a["hy"]: out.append("HY3131 reconfigured")
    if b.get("ee"): out.append("**EEPROM:** " + "; ".join(b["ee"]))
    scr = f" `{b['screen']}`" if b["screen"] != a["screen"] else ""
    if b["beeps"] >= 2: return ("**rejected** (double beep)" if not out else ", ".join(out) + " (double beep)") + scr
    if b["beeps"] == 1: out.append("beep")
    return (", ".join(out) or "nothing (no beep)") + scr

def position(pos):
    base = run(pos, "2500:NONE", 3200)[0]
    cyc = run(pos, ",".join(f"{2500 + 2000 * i}:MODE" for i in range(5)))
    res = {"baseline": base, "mode_cycle": [base] + cyc, "keys": {}}
    res["setup_walk"] = run(pos, ",".join(f"{2500 + 2000 * i}:SETUP" for i in range(12))) if pos == "V" else []
    for k in KEYS:
        s = run(pos, f"2500:{k}", 3200)[0]
        l = run(pos, f"2500:{k}:800", 3900)[0]
        res["keys"][k] = {"short": s, "long": l}
    return pos, res

with ThreadPoolExecutor(max_workers=8) as ex:
    results = dict(ex.map(position, POS))
json.dump(results, open(os.path.join(ROOT, "out", "button-map.json"), "w"), indent=1)

md = ["# Button map per dial position (stock 2.05, emulated)", "",
      "Each cell: what changed against the position's power-on state, from a fresh boot on the factory-default EEPROM",
      "image (factory-eeprom.bin, scripts/factory_eeprom.py) and a freshly formatted FAT32 card (out/sd/fat32-mbr.img) for every sequence, with the HY3131 model returning a steady",
      "AD1 mid range 0 (20,000; LowZ 3,000), battery ~6.0 V; snapshots 1.5 s after each release. Screens: `main` [sub] icons, decoded by scripts/lcd_decode.py (7-segment: V shows as U). Short = 150 ms, long = 800 ms. **beep** = accepted (Beep_Short 100 ms or",
      "Beep_Long 200 ms), **double beep** = key not available here (Beep_Double); every beep resets APO. Generated",
      "by scripts/button_map.py; state names are firmware variables (startup.md, measurement-core.md). Not yet covered:",
      "two-key combinations (HOLD+MEM), editing inside SETUP pages, presses after MODE.", "",
      "Emulator limits that shape some cells: **no SD card** (MEM short is likely rejected for that reason; the manual",
      "has MEM save to the card), **no lead in a current jack** (PE10/PE11 both high: uA/mA/A follow the no-jack path),",
      "a steady input (nothing to MIN/MAX or autorange against), only the first press from power-on, and the HY3131",
      "model makes AD1 events only: AC+DC, capacitance and frequency values, and the VA product, stay blank.", ""]
for pos in POS:
    r = results[pos]; b = r["baseline"]
    md.append(f"## {pos}  (power-on: {MODE_NAMES[b['mode']]}, range {b['range']}; screen `{b['screen']}`)")
    cyc = [MODE_NAMES[s["mode"]] + (" +AC+DC" if s["acdc"] else "") + (f" r{s['range']}" if s["range"] != b["range"] else "") for s in r["mode_cycle"]]
    md.append(f"MODE cycle: {' -> '.join(cyc)}")
    md.append("")
    for i, st in enumerate(r["mode_cycle"][1:], 1):
        md.append(f"- MODE x{i}: `{st['screen']}`")
    if r.get("setup_walk"):
        md.append("")
        md.append("SETUP walk (short SETUP presses, from power-on):")
        md.append("")
        for i, st in enumerate(r["setup_walk"], 1):
            md.append(f"- SETUP x{i}: page {st['setup_page']}: `{st['screen']}`")
    md.append("")
    md.append("| Key | Short press | Long press |")
    md.append("|---|---|---|")
    for k in KEYS:
        md.append(f"| {k} | {diff(b, r['keys'][k]['short'])} | {diff(b, r['keys'][k]['long'])} |")
    md.append("")
open(os.path.join(ROOT, "docs", "button-map.md"), "w").write("\n".join(md) + "\n")
print("wrote docs/button-map.md and out/button-map.json")
