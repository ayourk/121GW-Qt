import pickle, difflib, re, collections
import os
T = os.environ.get('V204_WORK', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'out', 'v204', 'work')) + '/'
match, rows = pickle.load(open(T+'match.pkl','rb')); info = pickle.load(open(T+'info.pkl','rb'))
W = lambda ts: [re.sub(r'^(\w+)\.[nw]\b', r'\1', t) for t in ts]
out = []
STUB4, NEW5 = 0x08006d44, 0x08006d40
for r in rows:
    a4, a5, name, cls = r[0], r[1], r[2], r[3]
    note = ''
    if a4 == STUB4 or (a4 is None and a5 == NEW5): continue
    if cls in ('moved', 'changed', 'identical'):
        x, y = W(info[('04', a4)]['toks']), W(info[('05', a5)]['toks'])
        s = 1.0 if x == y else difflib.SequenceMatcher(None, x, y, autojunk=False).ratio()
        if cls == 'changed':
            if name == 'LCD_DrawVersion': note = 'version number 204 -> 205 (movs r4,#0xcc -> #0xcd)'
            elif name == '__iar_program_start': note = 'call to exit() after main dropped (main never returns); exit chain removed'
            elif name in ('Mode_ACV_OnAD1',): cls, note = 'moved', 're-encoded only (ldr narrow/wide by pool distance); decompile identical; calls the LPF hook slot (stub in 2.04)'
            else: cls, note = 'moved', 're-encoded only (ldr narrow/wide by pool distance' + (', block order' if name == 'Mode_DCV_OnAD1' else '') + '); decompile identical'
        elif cls == 'moved' and name in ('Mode_ACmV_OnAD1',): note = 'calls the LPF hook slot (stub in 2.04)'
        elif cls == 'moved' and a5 - a4 not in (0, 900, 1032, 3492) and name.startswith('__aeabi'): note = 'library module link order changed'
    elif cls == 'removed':
        s = 0.0; name = {0x08028386: '(exit, IAR runtime)', 0x0802838c: '(exit loop -> __exit)', 0x08028398: '(__exit: semihosting bkpt 0xAB, SYS_EXIT)'}.get(a4, '')
        note = 'dead after main (never returns); dropped by the linker in 2.05'
    else:
        s = 0.0
    out.append((a4, a5, name, cls, s, note))
s = difflib.SequenceMatcher(None, info[('04', STUB4)]['toks'], info[('05', NEW5)]['toks'], autojunk=False).ratio()
out.append((STUB4, NEW5, 'ACFreq_LpfCompensate', 'changed', s, 'rewritten: 2.04 identity stub (return x, 6 B) -> 856 B frequency/amplitude LPF compensation with 3 new tables (2,592 B) and 12 new RAM words; same slot in link order, same 3 call sites'))
out.sort(key=lambda r: (r[0] or r[1]))
with open(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'out', 'v204', 'match.tsv'), 'w') as f:
    f.write('# 2.04 vs 2.05 function match (job 2026-11-02): addr204\taddr205\tname205\tclass\tsimilarity\tnote\n')
    f.write('# class: identical = same address and bytes; moved = same code at a new address (relocations, literal-pool distance re-encoding); changed = different code; removed = only in 2.04; added = only in 2.05\n')
    f.write('# similarity: normalized instruction tokens (branch targets, literal addresses masked; ldr .n/.w folded), difflib ratio\n')
    f.write('addr204\taddr205\tname205\tclass\tsimilarity\tnote\n')
    for a4, a5, n, c, s, note in out:
        f.write('%s\t%s\t%s\t%s\t%.3f\t%s\n' % ('%08x' % a4 if a4 else '-', '%08x' % a5 if a5 else '-', n, c, s, note))
print(collections.Counter(r[3] for r in out))
