#!/usr/bin/env python3
# Compare every non-function byte range (data, literal pools, padding) of 2.04 with 2.05, flash pointers normalized.
import pickle, struct, difflib, bisect
import os
T = os.environ.get('V204_WORK', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'out', 'v204', 'work')) + '/'
FW_RELEASES = os.environ.get('FW_RELEASES', os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))), 'firmware-releases')) + '/'   # stock images (not in the repo)
BASE = 0x08006000
i4 = open(FW_RELEASES + 'EEVBlog2_04.bin', 'rb').read()
i5 = open(FW_RELEASES + 'EEVBlog2_05.bin', 'rb').read()
match, rows = pickle.load(open(T + 'match.pkl', 'rb'))
info = pickle.load(open(T + 'info.pkl', 'rb'))
k4 = sorted(match)
f4 = sorted((a, info[('04', a)]['size']) for (v, a) in info if v == '04')
f5 = sorted((a, info[('05', a)]['size']) for (v, a) in info if v == '05')
def fmap(a):
    """map a 2.04 flash address to 2.05: rodata block rule, else nearest preceding matched function's delta"""
    if a >= 0x080251cc: return a + 3492
    if 0x08024bb4 <= a < 0x080251cc: return a + (900 if a < 0x08024c98 else 3492)
    i = bisect.bisect_right(k4, a) - 1
    return a + (match[k4[i]] - k4[i]) if i >= 0 else a
def words(img, s, e):
    return [struct.unpack_from('<I', img, o - BASE)[0] for o in range(s, e - 3, 4)]
def norm4(w):
    if 0x08006000 <= w < 0x08030000:
        m = fmap(w)
        return ('P', m) if m is not None else ('P?', w)
    if 0x20000000 <= w < 0x2000c000: return ('R', w + 0x30 if w >= 0x20007c5c else w)
    return w
def norm5(w):
    if 0x08006000 <= w < 0x08030000: return ('P', w)
    if 0x20000000 <= w < 0x2000c000: return ('R', w)
    return w
# gaps in 2.04 (outside function bodies)
gaps = []
ends = 0
for i, (a, s) in enumerate(f4):
    nxt = f4[i + 1][0] if i + 1 < len(f4) else BASE + len(i4)
    if a + s < nxt: gaps.append((a + s, nxt))
gaps.append((BASE, f4[0][0]))
tot = 0; diff = 0
for s, e in sorted(gaps):
    if s >= 0x08024bb4 and e <= 0x080251cc + 4 and s < 0x080251cc: continue
    m = fmap(s)
    fa = [a for a, sz in f4 if a + sz == s]
    if fa and fa[0] in match:
        b = match[fa[0]]; m = b + info[('05', b)]['size']
    n = e - s
    b4 = i4[s - BASE:e - BASE]; b5 = i5[m - BASE:m - BASE + n]
    tot += n
    if b4 == b5: continue
    # word compare with pointer normalization (align to 4)
    s0 = s & ~3; m0 = m - (s - s0)
    w4 = words(i4, s0, e); w5 = words(i5, m0, m0 + (e - s0))
    n5 = [b for b in f5 if b[0] > m][0][0] if any(b[0] > m for b in f5) else BASE + len(i5)
    w5 = words(i5, m0, n5)
    W4 = [norm4(x) for x in w4]; W5 = [norm5(y) for y in w5]
    bad = []
    for op, a1, a2, b1, b2 in difflib.SequenceMatcher(None, W4, W5, autojunk=False).get_opcodes():
        if op != 'equal': bad += [(s0 + 4 * j, w4[j], w5[b1] if b1 < len(w5) else 0) for j in range(a1, max(a2, a1 + 1)) if j < len(w4)] or [(s0 + 4 * a1, 0, w5[b1])]
    if bad:
        diff += len(bad)
        print('gap 04 %08x-%08x -> 05 %08x (%d B): %d differing words; first %s' % (
            s, e, m, n, len(bad), ', '.join('%08x:%08x/%08x' % b for b in bad[:4])))
print('gap bytes compared', tot, 'differing words', diff)
# rodata block alignment
s4, e4 = 0x08024bb4, 0x080251cc
s5, e5 = 0x08024f38, 0x08025f70
W4 = [norm4(w) for w in words(i4, s4, e4)]; W5 = [norm5(w) for w in words(i5, s5, e5)]
sm = difflib.SequenceMatcher(None, W4, W5, autojunk=False)
for op, a1, a2, b1, b2 in sm.get_opcodes():
    if op != 'equal':
        print('rodata %s 04 %08x-%08x (%d w) -> 05 %08x-%08x (%d w)' % (op, s4 + 4 * a1, s4 + 4 * a2, a2 - a1, s5 + 4 * b1, s5 + 4 * b2, b2 - b1))
# the tail of the image
print('lengths 04 %d 05 %d; tail 04 from 080251cc %d B, 05 from 08025f70 %d B' % (len(i4), len(i5), BASE + len(i4) - 0x080251cc, BASE + len(i5) - 0x08025f70))
