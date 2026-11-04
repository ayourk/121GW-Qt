#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Reads the per-(version, scenario) JSON of scripts/versions/probe.py and prints, for each scenario, the groups of
# versions in release order whose normalized outputs agree, with each group's outputs (screens, buzzer, BLE summary,
# HY3131 image, EEPROM changes, SD files). The boot-screen version text is masked; BLE bytes are split into packets
# (19-byte binary F2 ... XOR, or F2 + ASCII hex text + CR LF) and echoed frames.
#   matrix.py OUTDIR [--only SUBSTR] [--all] [--keys screens,ble,...]
import glob, json, os, re, sys

OUT = sys.argv[1]
ONLY = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
SHOWALL = "--all" in sys.argv
DELTA = "--delta" in sys.argv   # after the first group, print only the lines that differ from the previous group
FIELDS = (sys.argv[sys.argv.index("--keys") + 1].split(",") if "--keys" in sys.argv
          else ["screens", "buzzer", "ble", "hy", "eeprom", "sd", "faults"])
VERS = sorted(os.listdir(OUT), key=lambda v: tuple(int(x) for x in v.split("_")))

def ble_parse(h):
    b = bytes.fromhex(h); i = 0; pk, echo, junk = [], [], 0
    while i < len(b):
        if b[i] == 0xF2 and i + 19 <= len(b):
            x = 0
            for c in b[i:i + 18]: x ^= c
            if x == b[i + 18] and not all(0x30 <= c < 0x47 for c in b[i + 1:i + 19]):
                pk.append(("bin", b[i:i + 19].hex())); i += 19; continue
        if b[i] == 0xF2:
            j = b.find(b"\r\n", i)
            if j > 0 and all(chr(c) in "0123456789ABCDEF" for c in b[i + 1:j]):
                pk.append(("txt%d" % (j - i - 1), b[i + 1:j].decode())); i = j + 2; continue
        if b[i] in (0xF4, 0xF8) or (0xF0 <= b[i] <= 0xFF and b[i] != 0xF2):
            j = i + 1
            while j < len(b) and 0x30 <= b[j] < 0x47 and j - i < 15: j += 1
            echo.append(b[i:j].hex()); i = j; continue
        junk += 1; i += 1
    return pk, echo, junk

def ble_summary(h):
    if not h: return "none"
    pk, echo, junk = ble_parse(h)
    kinds = sorted({k for k, _ in pk}); distinct = []
    for k, v in pk:
        if v not in distinct: distinct.append(v)
    return f"{len(pk)} pkts {'/'.join(kinds)} echo={echo} junk={junk} distinct={distinct[:6]}{'...' if len(distinct) > 6 else ''}"

def norm(d, f):
    v = d.get(f)
    if f == "screens":
        return [(t, re.sub(r"'[UV]-\d\.\d\d'", "'<ver>'", s)) for t, s in v]
    if f == "ble": return ble_summary(v)
    return v

def main():
    names = {}
    for v in VERS:
        for p in glob.glob(os.path.join(OUT, v, "*.json")):
            names.setdefault(os.path.basename(p), []).append(v)
    order = []
    for v in VERS:
        for p in sorted(glob.glob(os.path.join(OUT, v, "*.json"))):
            n = os.path.basename(p)
            if n not in order: order.append(n)
    for n in order:
        rows = {}
        for v in VERS:
            p = os.path.join(OUT, v, n)
            if os.path.exists(p): rows[v] = json.load(open(p))
        if not rows: continue
        sc = next(iter(rows.values()))["scenario"]["name"]
        if ONLY and ONLY.lower() not in sc.lower(): continue
        sig = {v: json.dumps([norm(d, f) for f in FIELDS]) for v, d in rows.items()}
        groups = []
        for v in VERS:
            if v not in sig: continue
            if groups and groups[-1][1] == sig[v]: groups[-1][0].append(v)
            else: groups.append(([v], sig[v]))
        if len(groups) == 1 and not SHOWALL: continue
        print(f"### {sc}  ({len(groups)} groups)")
        prev = None
        for vs, s in groups:
            print(f"  [{vs[0]}..{vs[-1]}] ({len(vs)})")
            lines = []
            for f, val in zip(FIELDS, json.loads(s)):
                if f == "screens":
                    for t, txt in val: lines.append(f"      t={t}: {txt}")
                elif f == "sd" and val:
                    for k, c in (val.get("files") or {}).items(): lines.append(f"      sd {k}: {c[:700]!r}")
                    if val.get("times"): lines.append(f"      sd times {val['times']}")
                elif val not in (None, [], "none", ""):
                    lines.append(f"      {f}: {val}")
            for ln in lines:
                if not (DELTA and prev is not None and ln in prev): print(ln)
            prev = set(lines)
        print()

main()
