#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""Behavioral comparison of two firmware images in the emulator: the same scenarios (lines 'POS MS KEYS [extra
emu_run args]', as scripts/coverage_sweep.zsh writes them) on both, compared by what a user and the hardware see,
not by timing (emulated time is instruction count, so a different compiler shifts it):
  - the sequence of stable LCD screens (sampled every 100 ms; a screen counts when held >= 2 samples)
  - STATE snapshots after each key (time and the IAR-specific 'beeps' count stripped), buzzer rising edges, BLE TX bytes, final mode/range
  compare_images.py A.bin B.bin JOBS [--every N] [--jobs-limit N] [-P 10] [--show N]
Env UCFIX_A / UCFIX_B = 0 runs that side on the system libunicorn (for checking the Unicorn fix itself)."""
import argparse, concurrent.futures as cf, os, re, shlex, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ap = argparse.ArgumentParser(); ap.add_argument("a"); ap.add_argument("b"); ap.add_argument("jobs")
ap.add_argument("--every", type=int, default=1); ap.add_argument("--jobs-limit", type=int, default=0)
ap.add_argument("--seeds", type=int, default=0, help="also run each side with emu_run --jitter 0..N-1 and classify timing dependence")
ap.add_argument("-P", type=int, default=10); ap.add_argument("--show", type=int, default=15)
a = ap.parse_args()
jobs = [l.split(None, 3) for l in open(a.jobs).read().splitlines() if l.strip()][::a.every]
if a.jobs_limit: jobs = jobs[:a.jobs_limit]
def run(img, job, ucfix, seed=None):
    pos, ms, keys = job[0], int(job[1]), job[2]; extra = shlex.split(job[3]) if len(job) > 3 else []
    scr = ",".join(str(t) for t in range(100, ms, 100))
    cmd = [sys.executable, "-I", "scripts/emu_run.py", img, "svd/STM32L1xx.svd", "--rotary", pos, "--ms", str(ms),
           "--hy-only", "--eeprom", "factory-eeprom.bin", "--ad1", "20000", "--glass", "--screens", scr, "--keys", keys] + extra + ([] if seed is None else ["--jitter", str(seed)])
    env = dict(os.environ); env["UCFIX"] = ucfix
    out = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, env=env, timeout=1800).stdout
    screens, state, other = [], [], []
    for l in out.splitlines():
        m = re.match(r"\s*SCREEN t=\S+?: (.*)", l)
        if m: screens.append(m.group(1))
        elif l.lstrip().startswith("STATE"):         # 'beeps' counts one IAR-specific write form: not comparable
            state.append(re.sub(r" beeps \d+", "", re.sub(r"t=\d+ ", "", l.strip())))
        elif l.startswith("BUZZER:"): other.append(l.split(",")[0])
        elif l.startswith("BLE TX"): other.append(l)
        elif l.startswith("end "): other.append(re.sub(r".*?(g_mode=)", r"\1", l))
        elif "fault" in l: other.append(l.strip())
    runs = []                                       # (screen, consecutive samples)
    for x in screens:
        if runs and runs[-1][0] == x: runs[-1][1] += 1
        else: runs.append([x, 1])
    stable = []                                     # screens held for >= 2 samples (200 ms): transitions drop out
    for x, n in runs:
        if n >= 2 and (not stable or stable[-1] != x): stable.append(x)
    return stable, state, other
def both(job):
    return job, run(a.a, job, os.environ.get("UCFIX_A", "1")), run(a.b, job, os.environ.get("UCFIX_B", "1"))
if a.seeds:
    # per job: signatures of A and B over no jitter + seeds 0..N-1. Classes: "same" (one signature, equal on both);
    # "timing" (A itself varies with jitter: the scenario races the firmware; B's results within A's set or not);
    # "REAL" (A stable under jitter, B outside A's set: a behavior difference)
    seeds = [None] + list(range(a.seeds))
    def sig(img, job, sd): return repr(run(img, job, "1", sd))
    with cf.ThreadPoolExecutor(a.P) as ex:
        futs = {(i, side, sd): ex.submit(sig, img, job, sd) for i, job in enumerate(jobs) for side, img in (("A", a.a), ("B", a.b)) for sd in seeds}
        for i, job in enumerate(jobs):
            A = [futs[(i, "A", sd)].result() for sd in seeds]; B = [futs[(i, "B", sd)].result() for sd in seeds]
            sa, sb = set(A), set(B)
            if len(sa) == 1 and sa == sb: cls = "same"
            elif len(sa) > 1: cls = f"timing (A {len(sa)} variants, B {len(sb)}; B inside A's set: {sb <= sa}; overlap {len(sa & sb)})"
            else: cls = f"REAL? (A stable, B {len(sb)} variant(s), B matches A in {sum(1 for x in B if x in sa)}/{len(B)})"
            print(f"{cls}: {' '.join(job)}")
    sys.exit(0)
same = diff = 0; shown = 0
with cf.ThreadPoolExecutor(a.P) as ex:
    for job, ra, rb in ex.map(both, jobs):
        if ra == rb: same += 1; continue
        diff += 1
        if shown < a.show:
            shown += 1; print(f"DIFF {' '.join(job)}")
            for name, x, y in zip(("screens", "state", "other"), ra, rb):
                if x != y:
                    i = next((k for k in range(min(len(x), len(y))) if x[k] != y[k]), min(len(x), len(y)))
                    print(f"   {name} #{i}: A {x[i] if i < len(x) else '(end)'}\n   {' ' * len(name)}     B {y[i] if i < len(y) else '(end)'}")
print(f"scenarios {same + diff}: identical {same}, different {diff}")
