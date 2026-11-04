#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""Instruction-level coverage of stock 2.05: executed bytes (out/coverage-ranges.txt, 'ADDR SIZE' per basic block from
emu_run.py --coverage-ranges) against every function in out/functions.tsv, with literal-pool words (targets of
PC-relative loads) counted as data, not code. Prints totals and every unexecuted instruction range by function.
  coverage_gaps.py [--ranges FILE] [--top N] [--list]      --list prints each range with its first instructions"""
import argparse, bisect, collections, os, re, subprocess
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ap = argparse.ArgumentParser(); ap.add_argument("--ranges", default=os.path.join(ROOT, "out/coverage-ranges.txt"))
ap.add_argument("--top", type=int, default=60); ap.add_argument("--list", action="store_true"); a = ap.parse_args()
IMG, BASE = os.path.join(ROOT, "EEVBlog2_05.bin"), 0x08006000
funcs = []
for line in open(os.path.join(ROOT, "out/functions.tsv")).read().splitlines()[1:]:
    c = line.split("\t"); funcs.append((int(c[0], 16), c[1], int(c[2])))
funcs.sort()
lo, hi = funcs[0][0], max(e + s for e, _, s in funcs)
dis = subprocess.run(["arm-none-eabi-objdump", "-D", "-b", "binary", "-marm", "-Mforce-thumb", f"--adjust-vma={BASE:#x}",
                      f"--start-address={lo:#x}", f"--stop-address={hi:#x}", IMG], capture_output=True, text=True).stdout
insn = {}                                           # addr -> text (objdump's linear view; data may decode as insns)
data = set()                                        # literal-pool bytes
for m in re.finditer(r"^ *([0-9a-f]+):\t([0-9a-f ]+?)\s*\t(.*)$", dis, re.M):
    ad, txt = int(m.group(1), 16), m.group(3); insn[ad] = txt
    t = re.search(r"\[pc, #-?\d+\].*@ 0x([0-9a-f]+)", txt)
    if t:
        n = 8 if txt.startswith(("ldrd", "vldr")) else 4
        data.update(range(int(t.group(1), 16), int(t.group(1), 16) + n))
ex = bytearray(hi - lo)
for line in open(a.ranges):
    p = line.split()
    if len(p) == 2:
        s, n = int(p[0], 16), int(p[1], 16)
        for x in range(max(s, lo), min(s + n, hi)): ex[x - lo] = 1
tot = code = done = 0
gaps = collections.defaultdict(list)                # func -> [(start, end)]
per = {}
for e, name, sz in funcs:
    c = d = 0; run = None
    for x in range(e, e + sz):
        if x in data: continue
        c += 1
        if ex[x - lo]: d += 1; run = None if run is None else (gaps[(e, name)].append(run), None)[1]
        else: run = (x, x + 1) if run is None else (run[0], x + 1)
    if run: gaps[(e, name)].append(run)
    tot += sz; code += c; done += d; per[(e, name)] = (c, d)
print(f"functions {len(funcs)}, bytes {tot:,}; literal data {tot - code:,}; instruction bytes {code:,}; executed {done:,} "
      f"= {100 * done / code:.2f}%; unexecuted {code - done:,} in {sum(len(v) for v in gaps.values())} ranges, "
      f"{sum(1 for k in gaps if per[k][1] == 0)} functions never entered")
rows = sorted(gaps.items(), key=lambda kv: -sum(b - a for a, b in kv[1]))
for (e, name), rs in rows[:a.top]:
    c, d = per[(e, name)]; miss = sum(b - a for a, b in rs)
    print(f"{miss:6d}B {e:08x} {name:34s} {'never' if d == 0 else f'{d}/{c}'}  " + " ".join(f"{s:x}+{t - s}" for s, t in rs[:6]) + (" ..." if len(rs) > 6 else ""))
    if a.list:
        for s, t in rs:
            first = [f"{x:x} {insn[x]}" for x in sorted(insn) if s <= x < t][:3]
            print("        " + " | ".join(first))
