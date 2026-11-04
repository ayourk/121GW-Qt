#!/usr/bin/env python3
# Rename the 2.04 decompile into 2.05 terms (functions via match, RAM via +0x30 above 0x20007c5c, flash data via the
# nearest matched function's delta or +3492 in the rodata block) and diff the changed functions.
import re, sys, pickle, difflib, bisect
import os
T = os.environ.get('V204_WORK', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'out', 'v204', 'work')) + '/'
R = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/'
match, rows = pickle.load(open(T + 'match.pkl', 'rb'))
names = {}; gl = {}
for ln in open(R + 'names.tsv'):
    if ln.startswith('#') or not ln.strip(): continue
    p = ln.rstrip('\n').split('\t')
    names[int(p[0], 16)] = p[1]
def split_funcs(path):
    txt = open(path).read()
    out = {}
    # functions separated by header comments from ExportDecomp; find "// ... @ addr" or by signature line
    parts = re.split(r'\n(?=// [=-]+|/\* Function)', txt)
    return txt
d04 = open(R + 'out/v204/d204-g122.c').read()
d05 = open(R + 'out/d205-g122.c').read()
k4 = sorted(match);
def fl_map(a):
    if 0x08024bb4 <= a < 0x08026000 or a >= 0x080251cc: return a + 3492
    i = bisect.bisect_right(k4, a) - 1
    if i < 0: return a
    return a + (match[k4[i]] - k4[i])
def ram_map(a):
    return a + 0x30 if a >= 0x20007c5c else a
fun05 = {}
for a4, a5 in match.items(): fun05[a4] = a5
def nm(a5, default):
    return names.get(a5, default)
def ren(m):
    kind, h = m.group(1), m.group(2); a = int(h, 16)
    if kind == 'FUN_':
        b = fun05.get(a)
        return nm(b, 'FUN05_%08x' % b) if b else 'REMOVED_%08x' % a
    if 0x20000000 <= a < 0x20010000:
        b = ram_map(a); return nm(b, '%s%08x' % (kind, b))
    if 0x08000000 <= a < 0x08040000:
        b = fl_map(a); return nm(b, '%s%08x' % (kind, b))
    return m.group(0)
d04r = re.sub(r'\b(FUN_|DAT_|UNK_|PTR_[A-Za-z0-9_]*?_|LAB_|s_[A-Za-z0-9_]*?_|uRam|iRam|cRam|bRam|sRam|pcRam|puRam|piRam)([0-9a-f]{8})\b', ren, d04)
open(T + 'd204-renamed.c', 'w').write(d04r)
def body(txt, name):
    m = re.search(r'^[^\n;]*\b' + re.escape(name) + r'\([^\n]*\)\n\n?\{', txt, re.M)
    if not m: return None
    i = m.start(); j = txt.find('\n}\n', i)
    return txt[i:j + 3]
for n in sys.argv[1:]:
    a = body(d04r, n); b = body(d05, n)
    print('=' * 20, n)
    if a is None or b is None: print('missing', a is None, b is None); continue
    norm = lambda t: re.sub(r'\s+', ' ', t).replace('( ', '(').replace(' )', ')').replace(') )', '))').replace(';', ';\n').replace('{', '{\n').replace('}', '}\n').splitlines()
    na, nb = [x.strip() for x in norm(a)], [x.strip() for x in norm(b)]
    if na == nb: print('decompile identical'); continue
    for l in difflib.unified_diff(na, nb, '2.04', '2.05', n=1, lineterm=''): print(l)
