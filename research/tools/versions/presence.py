#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Function presence across versions (static): every code piece of an anchor image (codediff.py's reachable-code
# pieces, named through 2.05) is looked up in every version; the cell is the best similarity (1.00 = identical
# normalized code, blank = below 0.30). Rows where a piece is present, then absent, then present again, or absent
# from 2.05 while present earlier, are the static candidates for removed features.
#   presence.py RELEASE_DIR ANCHOR [--min 40] [--addr 0xADDR,...] [--changed]
#   ANCHOR is a version such as 2_05 or 1_57; --changed prints only rows whose cells are not all >= 0.9
import difflib, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REL, ANCHOR = sys.argv[1], sys.argv[2]
MINB = int(sys.argv[sys.argv.index("--min") + 1]) if "--min" in sys.argv else 40
ADDRS = {int(x, 16) for x in sys.argv[sys.argv.index("--addr") + 1].split(",")} if "--addr" in sys.argv else None
CHANGED = "--changed" in sys.argv
sys.argv = ["codediff.py", REL]
src = open(os.path.join(HERE, "codediff.py")).read().split("pairs = [PAIR]")[0]
g = {"__file__": os.path.join(HERE, "codediff.py"), "__name__": "codediff"}; exec(src, g)
P, VERS, name205 = g["P"], g["VERS"], g["name205"]

def best(p, v):
    hs = {q["h"] for q in P(v)}
    if p["h"] in hs: return 1.0
    sm = difflib.SequenceMatcher(None, p["toks"], autojunk=False); r0 = 0.3
    for q in P(v):
        if not (0.5 * len(p["toks"]) <= len(q["toks"]) <= 2 * len(p["toks"]) + 4): continue
        sm.set_seq2(q["toks"])
        if sm.real_quick_ratio() < r0 or sm.quick_ratio() < r0: continue
        r0 = max(r0, sm.ratio())
    return r0 if r0 > 0.3 else 0.0

print("addr\tsize\tname(2.05)\t" + "\t".join(VERS))
for p in P(ANCHOR):
    if p["size"] < MINB or (ADDRS and p["addr"] not in ADDRS): continue
    cells = [best(p, v) for v in VERS]
    if CHANGED and min(cells) >= 0.9: continue
    nm = name205(p["addr"]) if ANCHOR == "2_05" else "-"
    if ANCHOR != "2_05":
        r5 = cells[VERS.index("2_05")]
        nm = "(gone by 2.05)" if r5 < 0.5 else "(see 2.05)"
    print(f"{p['addr']:#010x}\t{p['size']}\t{nm}\t" + "\t".join(f"{c:.2f}" if c else "" for c in cells), flush=True)
