#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Coverage frontier: unexecuted functions (out/coverage.txt) whose callers ran, with the caller and the decompiled
# line that calls them (the condition that was not met), grouped by bytes reachable behind each. Also lists
# unexecuted functions with no executed caller (deeper, or reached only through pointers/vectors).
#   frontier.py [N]        prints the N largest frontier entries (default 60)
import bisect, collections, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
funcs = []
for line in open(os.path.join(ROOT, "out/functions.tsv")).read().splitlines()[1:]:
    c = line.split("\t"); funcs.append((int(c[0], 16), c[1], int(c[2])))
funcs.sort(); starts = [f[0] for f in funcs]; byaddr = {f[0]: f for f in funcs}
blocks = {int(x, 16) for x in open(os.path.join(ROOT, "out/coverage.txt")).read().split()}
ran = set()
for b in blocks:
    i = bisect.bisect_right(starts, b) - 1
    if i >= 0: ran.add(funcs[i][0])
# call graph from the decompile: "// ==== NAME @ ADDR" headers, calls by name
src = open(os.path.join(ROOT, "out/d205-g122.c")).read().split("\n// ==== ")
name2addr = {f[1]: f[0] for f in funcs}
calls = collections.defaultdict(list)          # callee -> [(caller, line)]
for chunk in src:
    m = re.match(r"(\S+) @ ([0-9a-f]{8})", chunk)
    if not m: continue
    caller = int(m.group(2), 16)
    for line in chunk.split("\n")[1:]:
        for callee in re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\(", line):
            if callee in name2addr and name2addr[callee] != caller:
                calls[name2addr[callee]].append((caller, line.strip()[:150]))
def reach(a, seen):
    """bytes of unexecuted code reachable from a (through unexecuted callees)"""
    if a in seen: return 0
    seen.add(a)
    total = byaddr[a][2]
    for callee, lst in calls.items():
        if callee not in ran and any(c == a for c, _ in lst): total += reach(callee, seen)
    return total
front = []
for e, n, sz in funcs:
    if e in ran: continue
    callers = [(c, l) for c, l in calls.get(e, []) if c in ran]
    if callers: front.append((reach(e, set()), e, n, callers))
front.sort(reverse=True)
N = int(sys.argv[1]) if len(sys.argv) > 1 else 60
unran = sum(sz for e, n, sz in funcs if e not in ran)
print(f"unexecuted: {sum(1 for f in funcs if f[0] not in ran)} functions, {unran:,} bytes; frontier entries {len(front)}")
for tot, e, n, callers in front[:N]:
    c, l = callers[0]
    print(f"{tot:6d}B behind {e:08x} {n:24s} <- {byaddr[c][1]:24s} | {l}")
