#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Static code diff between consecutive firmware images without a Ghidra project per version: linear Thumb
# disassembly (arm-none-eabi-objdump), split into pieces at every push {..., lr} prologue (plus prologues the linear
# decode swallowed into data before them, re-decoded from their own address), keeping only the
# instructions reachable from the prologue by fall-through and branches (literal pools and data dropped), normalized
# (mnemonic + registers; every number, address and branch target replaced by '#'). A piece of version N with no
# similar piece (difflib ratio >= THRESH) in version N+1 is reported as gone/rewritten, and the reverse as new.
# Each piece is named through its best match in 2.05 (functions.tsv + names.tsv), so "gone" code gets a 2.05 name
# when 2.05 still has it, or "no 2.05 counterpart" when it is gone for good.
#   codediff.py RELEASE_DIR [--thresh 0.6] [--min 40] [--pair A,B]
import difflib, glob, hashlib, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
REL = sys.argv[1]
THRESH = float(sys.argv[sys.argv.index("--thresh") + 1]) if "--thresh" in sys.argv else 0.6
MINB = int(sys.argv[sys.argv.index("--min") + 1]) if "--min" in sys.argv else 40
PAIR = sys.argv[sys.argv.index("--pair") + 1].split(",") if "--pair" in sys.argv else None
BASE = 0x08006000
VERS = sorted((os.path.basename(f)[7:11] for f in glob.glob(os.path.join(REL, "EEVBlog*.bin"))),
              key=lambda v: tuple(map(int, v.split("_"))))
RX = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{4} ?){1,2})\s+(\S+)\s*(.*)$")

def decode(img, lo=None, hi=None):
    cmd = ["/usr/bin/arm-none-eabi-objdump", "-D", "-b", "binary", "-m", "arm", "-M", "force-thumb", f"--adjust-vma={BASE:#x}"]
    if lo is not None: cmd += [f"--start-address={lo:#x}", f"--stop-address={hi:#x}"]
    ins = {}
    for ln in subprocess.run(cmd + [img], capture_output=True, text=True).stdout.splitlines():
        m = RX.match(ln)
        if not m: continue
        a, mn, raw = int(m.group(1), 16), m.group(3), m.group(4).split(";")[0].split("@")[0]
        op = re.sub(r"<[^>]*>", "", raw); op = re.sub(r"0x[0-9a-f]+|\b\d+\b", "#", op)
        ins[a] = (mn, raw.strip(), mn + " " + op.strip(), len(m.group(2).replace(" ", "")) // 2)
    return ins

def is_push_lr(ins, a):
    mn, raw = ins[a][0], ins[a][1]
    return mn.startswith("push") and "lr" in raw or mn.startswith("stmdb") and "sp!" in raw and "lr" in raw

def walk(ins, a0, end):              # instructions reachable from a0 (fall-through, branches); drops pools and data
    seen, work = set(), [a0]
    while work:
        a = work.pop()
        while a0 <= a < end and a in ins and a not in seen:
            seen.add(a); mn, raw, _, n = ins[a]
            tgt = re.match(r"(0x[0-9a-f]+)$", raw)
            base = re.sub(r"\.(n|w)$", "", mn)
            if base == "b" and tgt: work.append(int(tgt.group(1), 16)); break
            if re.fullmatch(r"b(eq|ne|cs|cc|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|hs|lo)", base) and tgt:
                work.append(int(tgt.group(1), 16))
            if base in ("cbz", "cbnz"):
                t2 = re.search(r"(0x[0-9a-f]+)$", raw)
                if t2: work.append(int(t2.group(1), 16))
            if (base == "pop" and "pc" in raw) or (base == "bx" and raw == "lr") or base in ("tbb", "tbh", "udf") \
                    or (base == "ldr" and raw.startswith("pc")) or (base.startswith("ldmia") and "pc" in raw):
                break
            a += n
    return seen

def mk(ins, a0, seen):
    toks = [ins[a][2] for a in sorted(seen)]
    return dict(addr=a0, size=sum(ins[a][3] for a in seen), toks=toks, h=hashlib.sha1("\n".join(toks).encode()).hexdigest())

def pieces(v):
    img = os.path.join(REL, f"EEVBlog{v}.bin"); raw = open(img, "rb").read()
    ins = decode(img)
    starts = [a for a in sorted(ins) if is_push_lr(ins, a)]
    res, covered = [], set()
    for i, a0 in enumerate(starts):
        seen = walk(ins, a0, starts[i + 1] if i + 1 < len(starts) else a0 + 4096)
        covered |= {x for a in seen for x in range(a, a + ins[a][3])}
        res.append(mk(ins, a0, seen))
    # resync: a push {.., lr} hidden inside a 32-bit "instruction" the linear decode made of the data before it
    hidden = []
    for o in range(0, len(raw) - 3, 2):
        h, h2 = raw[o] | raw[o + 1] << 8, raw[o + 2] | raw[o + 3] << 8
        a = BASE + o
        if ((h & 0xFF00) == 0xB500 or (h == 0xE92D and h2 & 0x4000)) and a not in ins and a not in covered:
            hidden.append(a)
    allst = sorted(set(starts) | set(hidden))
    for a0 in hidden:
        nxt = next((x for x in allst if x > a0), a0 + 4096)
        sub = decode(img, a0, min(nxt, a0 + 4096))
        if a0 not in sub or not is_push_lr(sub, a0): continue
        seen = walk(sub, a0, min(nxt, a0 + 4096))
        if len(seen) >= 4: res.append(mk(sub, a0, seen))
    return sorted(res, key=lambda p: p["addr"])

def load_names():
    f = {}
    for ln in open(os.path.join(ROOT, "out", "functions.tsv")).read().splitlines()[1:]:
        p = ln.split("\t"); f[int(p[0], 16)] = (p[1], int(p[2]))
    for ln in open(os.path.join(ROOT, "names.tsv")):
        if ln.startswith("#") or not ln.strip(): continue
        p = ln.rstrip("\n").split("\t")
        if len(p) > 2 and "f" in p[2]:
            a = int(p[0], 16)
            if a in f: f[a] = (p[1], f[a][1])
    return sorted((a, n, s) for a, (n, s) in f.items())

NAMES = load_names()
def name205(addr):
    best = None
    for a, n, s in NAMES:
        if a <= addr < a + max(s, 2): best = n
    return best or f"(in 2.05 at {addr:#010x}, unnamed)"

CACHE = {}
def P(v):
    if v not in CACHE: CACHE[v] = pieces(v)
    return CACHE[v]

def best(p, cands, byhash):
    if p["h"] in byhash: return 1.0, byhash[p["h"]]
    sm = difflib.SequenceMatcher(None, p["toks"], autojunk=False); r0, q0 = 0.0, None
    for q in cands:
        if not (0.5 * len(p["toks"]) <= len(q["toks"]) <= 2 * len(p["toks"]) + 4): continue
        sm.set_seq2(q["toks"])
        if sm.real_quick_ratio() < max(r0, THRESH * 0.9) or sm.quick_ratio() < max(r0, THRESH * 0.9): continue
        r = sm.ratio()
        if r > r0: r0, q0 = r, q
    return r0, q0

def side(a, b, label):
    byhash = {q["h"]: q for q in P(b)}
    for p in P(a):
        if p["size"] < MINB: continue
        r, q = best(p, P(b), byhash)
        if r < THRESH:
            r5, q5 = best(p, P("2_05"), {q["h"]: q for q in P("2_05")}) if a != "2_05" else (1.0, p)
            nm = name205(q5["addr"]) if q5 and r5 >= THRESH else "no 2.05 counterpart"
            print(f"  {label} {a} {p['addr']:#010x} {p['size']:5d} B  best in {b} {r:.2f}  -> 2.05: {nm} ({r5:.2f})")

pairs = [PAIR] if PAIR else list(zip(VERS, VERS[1:]))
for a, b in pairs:
    print(f"## {a} -> {b}: {len(P(a))} -> {len(P(b))} pieces")
    side(a, b, "GONE")
    side(b, a, "NEW ")
    sys.stdout.flush()
