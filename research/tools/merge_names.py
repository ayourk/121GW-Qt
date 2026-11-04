#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Merges naming proposals (ADDR\tName\tf\tcomment) into names.tsv after checks: the address is a function entry in
# out/functions.tsv, not named yet, the name is a C identifier and unique (against names.tsv and the other
# proposals), and the comment carries a confidence word. Rejected lines are printed with the reason.
#   merge_names.py [--dry-run] HEADER PROPOSALS.tsv ...
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
args = sys.argv[1:]
dry = args and args[0] == "--dry-run"
if dry: args = args[1:]
header, files = args[0], args[1:]
entries = {l.split("\t")[0] for l in open(os.path.join(ROOT, "out/functions.tsv")).read().splitlines()[1:]}
names_path = os.path.join(ROOT, "names.tsv")
have_addr, have_name = set(), set()
for l in open(names_path):
    if l.strip() and not l.startswith("#"):
        c = l.rstrip("\n").split("\t"); have_addr.add(c[0]); have_name.add(c[1])
ok, bad = [], []
for f in files:
    for n, l in enumerate(open(f), 1):
        if not l.strip() or l.startswith("#"): continue
        c = l.rstrip("\n").split("\t")
        why = None
        if len(c) != 4: why = f"{len(c)} fields"
        else:
            a, name, flags, com = c
            a = a.lower().removeprefix("0x"); c[0] = a
            if a not in entries: why = "not a function entry"
            elif a in have_addr: why = "already named"
            elif not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name): why = "bad identifier"
            elif name in have_name: why = f"name {name} taken"
            elif flags != "f": why = "flags not f"
            elif not re.search(r"\b(high|medium|low|library)\b", com): why = "no confidence word"
        if why: bad.append(f"{os.path.basename(f)}:{n}: {why}: {l.strip()[:100]}")
        else:
            ok.append("\t".join(c)); have_addr.add(c[0]); have_name.add(c[1])
print(f"accepted {len(ok)}, rejected {len(bad)}")
for b in bad: print("  " + b)
if not dry and ok:
    with open(names_path, "a") as out: out.write(f"# {header}\n" + "\n".join(ok) + "\n")
