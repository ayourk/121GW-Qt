#!/usr/bin/env python3
# Match 2.04 functions to 2.05 functions: normalized instruction streams from objdump, exact hashes, order alignment,
# call-graph propagation, fuzzy similarity. Writes match.tsv and a pickle of per-function token lists.
import sys, re, struct, hashlib, difflib, pickle, collections
import os
T = os.environ.get('V204_WORK', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'out', 'v204', 'work')) + '/'
FW_RELEASES = os.environ.get('FW_RELEASES', os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))), 'firmware-releases')) + '/'   # stock images (not in the repo)
R = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/'
BASE = 0x08006000
img = {v: open(FW_RELEASES + 'EEVBlog2_%s.bin' % v, 'rb').read() for v in ('04', '05')}

def word(v, a):
    o = a - BASE
    if 0 <= o <= len(img[v]) - 4: return struct.unpack_from('<I', img[v], o)[0]
    return None

def load_funcs(path):
    out = []
    for ln in open(path).read().splitlines()[1:]:
        f = ln.split('\t'); out.append((int(f[0], 16), f[1], int(f[2])))
    return sorted(out)

F = {'04': load_funcs(R + 'out/v204/functions.tsv'), '05': load_funcs(R + 'out/functions.tsv')}
names05 = {}
for ln in open(R + 'names.tsv'):
    if ln.startswith('#') or not ln.strip(): continue
    p = ln.rstrip('\n').split('\t')
    if len(p) >= 2 and 'f' in (p[2] if len(p) > 2 else 'f'): names05[int(p[0], 16)] = p[1]
for a, n, s in F['05']:
    names05.setdefault(a, n)

def load_dis(v):
    ins = {}
    rx = re.compile(r'^\s*([0-9a-f]+):\s+([0-9a-f]{4}(?: [0-9a-f]{4})?)\s+(\S+)\s*(.*)$')
    for ln in open(T + 'dis%s.txt' % v):
        m = rx.match(ln)
        if m: ins[int(m.group(1), 16)] = (m.group(2), m.group(3), m.group(4))
    return ins
D = {v: load_dis(v) for v in ('04', '05')}

def classify_val(x):
    if x is None: return 'N'
    if 0x08006000 <= x < 0x08040000: return 'FL'
    if 0x20000000 <= x < 0x2000c000: return 'RAM'   # RAM layout may shift between versions
    return '%x' % x

def tokens(v, start, end):
    """Normalized tokens; also callees (absolute) and literal values; pool words skipped."""
    ins = D[v]; toks = []; calls = []; lits = []; pool = set()
    a = start
    while a < end:
        if a in pool: a += 2; continue
        e = ins.get(a)
        if e is None: a += 2; continue
        hexw, mn, ops = e
        size = 4 if ' ' in hexw else 2
        ops = ops.split('@')[0].strip() if mn.startswith('ldr') or mn.startswith('add') or mn.startswith('adr') else ops
        m = re.search(r'\[pc, #(-?\d+)\]', e[2])
        if m and (mn.startswith('ldr') or mn.startswith('vldr')):
            tgt = ((a + 4) & ~3) + int(m.group(1))
            val = word(v, tgt)
            if mn.startswith('ldr') and not mn.startswith('ldrb') and not mn.startswith('ldrh') and not mn.startswith('ldrd'):
                pool.update((tgt, tgt + 2))
            lits.append(val)
            toks.append('%s %s =%s' % (mn, ops.split(',')[0], classify_val(val)))
        elif mn.startswith('ldrd') and 'pc' in ops:
            toks.append(mn + ' pcrel')
        elif mn in ('bl', 'blx') or mn.startswith('bl.'):
            mm = re.search(r'0x([0-9a-f]+)', ops)
            t = int(mm.group(1), 16) if mm else None
            calls.append(t); toks.append('bl F')
        elif re.match(r'^(b|cbz|cbnz|b[a-z]{2})(\.[nw])?$', mn) and re.search(r'0x[0-9a-f]+', ops):
            mm = re.search(r'0x([0-9a-f]+)', ops); t = int(mm.group(1), 16)
            r0 = ops.split(',')[0] if mn.startswith('cb') else ''
            base = mn.split('.')[0]
            if start <= t < end: toks.append('%s %s L' % (base, r0))
            else: calls.append(t); toks.append('%s %s X' % (base, r0))
        elif mn.startswith('adr') or (mn.startswith('add') and 'pc' in ops):
            toks.append(mn + ' adr')
        else:
            toks.append(mn + ' ' + re.sub(r'\s+', '', ops))
        a += size
    return toks, calls, lits

info = {}
for v in ('04', '05'):
    fl = F[v]
    for i, (a, n, s) in enumerate(fl):
        end = fl[i + 1][0] if i + 1 < len(fl) else BASE + len(img[v])
        end = min(end, a + max(s, 2) + 0x400)   # bound tail data
        # include pool words after the body up to next function (already via end)
        t, c, l = tokens(v, a, end)
        bodyend = a + s
        tb, cb, lb = tokens(v, a, bodyend)
        info[(v, a)] = dict(name=n, size=s, toks=tb, calls=cb, lits=lb,
                            raw=img[v][a - BASE:bodyend - BASE],
                            h=hashlib.sha1('\n'.join(tb).encode()).hexdigest())
pickle.dump(info, open(T + 'info.pkl', 'wb'))

A04 = [a for a, _, _ in F['04']]; A05 = [a for a, _, _ in F['05']]
def sim(a4, a5):
    x, y = info[('04', a4)], info[('05', a5)]
    if x['h'] == y['h']: return 1.0
    sm = difflib.SequenceMatcher(None, x['toks'], y['toks'], autojunk=False)
    return sm.ratio()

# 1. unique exact hashes
h4 = collections.defaultdict(list); h5 = collections.defaultdict(list)
for a in A04: h4[info[('04', a)]['h']].append(a)
for a in A05: h5[info[('05', a)]['h']].append(a)
match = {}
for h, l in h4.items():
    if len(l) == 1 and len(h5.get(h, [])) == 1: match[l[0]] = h5[h][0]
print('unique exact', len(match))
# 2. order alignment between anchors: functions between consecutive anchors paired in order if sizes/sim close
def gaps():
    ks = sorted(match)
    prev4, prev5 = None, None
    for k in ks + [None]:
        n4 = k; n5 = match[k] if k is not None else None
        g4 = [a for a in A04 if (prev4 is None or a > prev4) and (n4 is None or a < n4) and a not in match]
        g5 = [a for a in A05 if (prev5 is None or a > prev5) and (n5 is None or a < n5) and a not in match.values()]
        yield g4, g5
        prev4, prev5 = n4, n5
mono = all(match[a] < match[b] for a, b in zip(sorted(match), sorted(match)[1:]))
print('anchors monotone', mono)
changed = True
while changed:
    changed = False
    for g4, g5 in list(gaps()):
        if not g4 or not g5: continue
        # DP alignment by similarity
        n, m = len(g4), len(g5)
        S = [[sim(a, b) for b in g5] for a in g4]
        dp = [[0.0] * (m + 1) for _ in range(n + 1)]
        for i in range(n - 1, -1, -1):
            for j in range(m - 1, -1, -1):
                dp[i][j] = max(dp[i + 1][j], dp[i][j + 1], dp[i + 1][j + 1] + (S[i][j] if S[i][j] >= 0.5 else -1))
        i = j = 0
        while i < n and j < m:
            if S[i][j] >= 0.5 and dp[i][j] == dp[i + 1][j + 1] + S[i][j]:
                match[g4[i]] = g5[j]; changed = True; i += 1; j += 1
            elif dp[i][j] == dp[i + 1][j]: i += 1
            else: j += 1
print('after order alignment', len(match))
# 3. remaining: global best fuzzy above 0.4, prefer call-graph agreement
rem4 = [a for a in A04 if a not in match]; rem5 = [a for a in A05 if a not in match.values()]
cand = sorted(((sim(a, b), a, b) for a in rem4 for b in rem5), reverse=True)
for s, a, b in cand:
    if s < 0.4: break
    if a in match or b in match.values(): continue
    match[a] = b
print('after fuzzy', len(match))
# sanity: callee agreement
def callee_agree(a4, a5):
    c4 = info[('04', a4)]['calls']; c5 = info[('05', a5)]['calls']
    if not c4 and not c5: return None
    ok = sum(1 for x, y in zip(c4, c5) if x in match and match[x] == y)
    return ok, max(len(c4), len(c5))
inv = {b: a for a, b in match.items()}
rows = []
for a in A04:
    if a in match:
        b = match[a]; x, y = info[('04', a)], info[('05', b)]
        if x['raw'] == y['raw'] and a == b: cls = 'identical'
        elif x['h'] == y['h']: cls = 'moved'
        else: cls = 'changed'
        s = sim(a, b); ca = callee_agree(a, b)
        rows.append((a, b, names05.get(b, y['name']), cls, s, ca, x['size'], y['size']))
    else:
        rows.append((a, None, '', 'removed', 0, None, info[('04', a)]['size'], 0))
for b in A05:
    if b not in inv:
        rows.append((None, b, names05.get(b, ''), 'added', 0, None, 0, info[('05', b)]['size']))
pickle.dump((match, rows), open(T + 'match.pkl', 'wb'))
with open(T + 'match-raw.tsv', 'w') as f:
    for r in rows:
        f.write('\t'.join([('%08x' % r[0]) if r[0] else '-', ('%08x' % r[1]) if r[1] else '-', r[2], r[3], '%.3f' % r[4],
                           str(r[5]), str(r[6]), str(r[7])]) + '\n')
print(collections.Counter(r[3] for r in rows))
