#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Callers of functions in the decompile (out/d205-g122.c), with the calling line.
#   callers.py NAME_OR_ADDR ...      (an address like 0800f7b8 is looked up in the "// ==== NAME @ ADDR" headers)
import os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(ROOT, "out/d205-g122.c")).read().split("\n// ==== ")
heads = {}
for chunk in src:
    m = re.match(r"(\S+) @ ([0-9a-f]{8})", chunk)
    if m: heads[m.group(2)] = m.group(1)
for want in sys.argv[1:]:
    name = heads.get(want.lower(), want)
    print(f"== {name}")
    for chunk in src:
        m = re.match(r"(\S+) @ ([0-9a-f]{8})", chunk)
        if not m or m.group(1) == name: continue
        for line in chunk.split("\n")[1:]:
            if re.search(r"\b%s\(" % re.escape(name), line):
                print(f"  {m.group(2)} {m.group(1):28s} | {line.strip()[:110]}")
