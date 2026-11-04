#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Feature-probe suite across firmware versions: runs every scenario below on every image given, in the emulator
# (scripts/emu_run.py), and stores one JSON per (version, scenario) with the firmware-independent outputs only:
# LCD glass (controller RAM, decoded) at fixed times, buzzer, every BLE TX byte, final HY3131 R20..R33 image,
# data-EEPROM bytes changed against factory-eeprom.bin, SD card files (names, sizes, times, contents), faults.
# Results are cached: an existing JSON whose stored scenario matches the current definition is not re-run.
#   probe.py OUTDIR IMAGE.bin... [--only SUBSTR] [--jobs N] [--list]
# Then scripts/versions/matrix.py OUTDIR prints, per scenario, which versions agree.
import concurrent.futures, json, os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
def opt(name, default=None):
    if name in args:
        i = args.index(name); v = args[i + 1]; del args[i:i + 2]; return v
    return default
ONLY, JOBS = opt("--only"), int(opt("--jobs", "8"))
LIST = "--list" in args
if LIST: args.remove("--list")
OUT, IMAGES = (args[0], args[1:]) if args else (None, [])
WORK = tempfile.gettempdir()
POS = ["LowZ", "V", "mV", "Hz", "Ohm", "VA", "uA", "A"]
KEYS = ["SETUP", "MINMAX", "MEM", "RANGE", "HOLD", "REL", "PEAK"]
BLEKEY = dict(RANGE=1, HOLD=2, REL=3, PEAK=4, MODE=5, MINMAX=6, MEM=7, SETUP=8)
SD = "out/sd"

def sc(name, pos, ms, keys="2500:NONE", screens=None, extra=(), sd=None):
    return dict(name=name, pos=pos, ms=ms, keys=keys, screens=screens or [ms - 100], extra=list(extra), sd=sd)

def ble_key(k, long=False):  # protocol: F4 k c k c, k = '0' short / '8' long, c = key number as an ASCII digit
    kc = ("8" if long else "0") + str(BLEKEY[k]); return "f4" + (kc + kc).encode().hex()

def ble_clock(*f):  # F8 + six bytes (YY MM DD hh mm ss) and their XOR, each as two upper-case ASCII hex digits
    x = 0
    for b in f: x ^= b
    return "f8" + "".join(f"{b:02X}" for b in f + (x,)).encode().hex()

def walk(pre_keys, n, t0, step=1200):  # n short SETUP presses after the given keys, screen 900 ms after each
    keys = pre_keys + [f"{t0 + step * i}:SETUP" for i in range(n)]
    return ",".join(keys), [t0 + step * i + 900 for i in range(n)], t0 + step * n + 300

S = []
# --- per dial position: power-on screen, MODE cycle, every key short/long, SETUP walk (as compare_firmware.py)
for p in POS:
    S.append(sc(f"{p} baseline", p, 6000, screens=[3000, 5900]))
    S.append(sc(f"{p} MODE x1-5", p, 14000, ",".join(f"{2500 + 2000 * i}:MODE" for i in range(5)),
                screens=[2500 + 2000 * i + 1500 for i in range(5)]))
    for k in KEYS:
        S.append(sc(f"{p} {k} short", p, 5000, f"2500:{k}", screens=[4900]))
        S.append(sc(f"{p} {k} long", p, 5500, f"2500:{k}:800", screens=[5400]))
    k, scr, ms = walk([], 12, 2500)
    S.append(sc(f"{p} SETUP walk", p, ms, k, scr))
# --- SETUP walks in sub-modes (pages that exist only there: ACV Hz/dBm, continuity mode, diode, temperature, AC current)
for name, pos, pre in [("ACV", "V", ["2500:MODE"]), ("AC+DC", "V", ["2500:MODE", "4500:MODE"]),
                       ("ACmV", "mV", ["2500:MODE"]), ("TEMP", "mV", ["2500:MODE", "4500:MODE"]),
                       ("continuity", "Ohm", ["2500:MODE"]), ("diode", "Ohm", ["2500:MODE", "4500:MODE"]),
                       ("CAP", "Ohm", ["2500:MODE", "4500:MODE", "6500:MODE"]), ("ACuA", "uA", ["2500:MODE"]),
                       ("ACA", "A", ["2500:MODE"]), ("AC VA", "VA", ["2500:MODE"]), ("pulse", "Hz", ["2500:MODE"])]:
    t0 = 2500 + 2000 * len(pre)
    k, scr, ms = walk(pre, 13, t0)
    S.append(sc(f"{name} SETUP walk", pos, ms, k, [t0 - 100] + scr))
# --- SETUP edits: reach page n, long SETUP (edit), REL (up) x1, long SETUP (save); EEPROM diff shows what was saved
for n in range(1, 13):
    pre = [f"{2500 + 1200 * i}:SETUP" for i in range(n)]; t = 2500 + 1200 * n
    keys = ",".join(pre + [f"{t}:SETUP:800", f"{t + 1800}:REL", f"{t + 3000}:SETUP:800", f"{t + 4800}:SETUP:800"])
    S.append(sc(f"V SETUP page x{n} edit REL", "V", t + 7000, keys, [t - 300, t + 1500, t + 2700, t + 4500, t + 6900]))
for name, pos, pre in [("continuity", "Ohm", ["2500:MODE"]), ("uA", "uA", []), ("VA", "VA", []), ("ACV", "V", ["2500:MODE"])]:
    for n in (10, 11, 12):
        t0 = 2500 + 2000 * len(pre); pg = [f"{t0 + 1200 * i}:SETUP" for i in range(n)]; t = t0 + 1200 * n
        keys = ",".join(pre + pg + [f"{t}:SETUP:800", f"{t + 1800}:REL", f"{t + 3000}:SETUP:800"])
        S.append(sc(f"{name} SETUP page x{n} edit", pos, t + 5000, keys, [t - 300, t + 1500, t + 2700, t + 4900]))
# --- boot-time key functions (held from power-on), every key
for k in ["MODE", "SETUP", "MINMAX", "MEM", "RANGE", "HOLD", "REL", "PEAK"]:
    S.append(sc(f"boot {k} held", "V", 6000, f"50:{k}:400", [700, 1500, 3000, 5900]))
S += [
    sc("boot SD cal save", "V", 6000, "50:MEM:400,800:REL:300,1600:MODE", [1500, 3000, 5900], sd=f"{SD}/fat32-mbr.img"),
    sc("boot SD cal load", "V", 6000, "50:MEM:400,800:PEAK:300,1600:MODE", [1500, 3000, 5900], sd=f"{SD}/fat32-cal.img"),
    sc("boot SD cal save, no card", "V", 6000, "50:MEM:400,800:REL:300,1600:MODE", [3000], extra=["--no-sd"]),
    sc("boot all segments (RANGE)", "V", 4000, "50:RANGE:400", [800]),
    sc("boot SETUP (factory)", "V", 6000, "50:SETUP:400", [1000, 5900]),
    sc("boot MODE+REL cal mode", "V", 8000, "50:MODE:300,500:REL:300", [1000, 2500, 4500, 7900]),
    sc("cal mode AC capture", "V", 18000, "50:MODE:300,500:REL:300,3500:MODE,5000:SETUP,9000:MEM", [4500, 8500, 13000, 17900],
       extra=["--ad1", "0", "--ad1-at", "7500:10000", "--freq", "1000"]),
    sc("cal mode DC capture", "V", 16000, "50:MODE:300,500:REL:300,5000:SETUP,9000:MEM", [4500, 8500, 15900],
       extra=["--ad1", "0", "--ad1-at", "7500:20000"]),
    # logging
    sc("logging 12 s", "V", 16000, "2500:MEM:800,14000:MEM:800", [5000, 15900], sd=f"{SD}/fat32-mbr.img"),
    sc("logging ACV 8 s", "V", 14000, "2500:MODE,4500:MEM:800,12500:MEM:800", [7000, 13900], sd=f"{SD}/fat32-mbr.img"),
    sc("logging FAT16 card", "Ohm", 14000, "2500:MEM:800,12000:MEM:800", [13900], sd=f"{SD}/fat16-flat.img"),
    sc("logging blank card", "V", 9000, "2500:MEM:800", [8900]),
    sc("logging then MEM short (recall)", "V", 16000, "2500:MEM:800,9000:MEM:800,11000:MEM,12500:RANGE,14000:MEM",
       [10500, 12000, 13500, 15900], sd=f"{SD}/fat32-mbr.img"),
    sc("MEM short with card", "V", 8000, "2500:MEM,4500:MEM", [3500, 5500, 7900], sd=f"{SD}/fat32-mbr.img"),
    sc("8 GiB card cal save", "V", 6000, "50:MEM:400,800:REL:300,1600:MODE", [3000],
       extra=["--sd-virtual-mb", "8192", "--sd-place", f"8390656:{SD}/big8g-part.img"], sd=f"{SD}/big8g-head.img"),
    # Bluetooth: packet format and rate, remote keys, clock
    sc("BLE on 10 s", "V", 13000, "2500:PEAK:800", [12900]),
    sc("BLE on Ohm", "Ohm", 9000, "2500:PEAK:800", [8900]),
    sc("BLE on ACV", "V", 9000, "2500:MODE,4100:PEAK:800", [8900]),
    sc("BLE then off", "V", 12000, "2500:PEAK:800,6000:PEAK:800", [5000, 11900]),
    sc("BLE remote MODE short", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", f"5000:{ble_key('MODE')}"]),
    sc("BLE remote HOLD short", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", f"5000:{ble_key('HOLD')}"]),
    sc("BLE remote SETUP short", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", f"5000:{ble_key('SETUP')}"]),
    sc("BLE remote REL short", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", f"5000:{ble_key('REL')}"]),
    sc("BLE remote RANGE short", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", f"5000:{ble_key('RANGE')}"]),
    sc("BLE remote MINMAX short", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", f"5000:{ble_key('MINMAX')}"]),
    sc("BLE remote MEM long", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", f"5000:{ble_key('MEM', True)}"]),
    sc("BLE remote PEAK long", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", f"5000:{ble_key('PEAK', True)}"]),
    sc("BLE remote SETUP long", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", f"5000:{ble_key('SETUP', True)}"]),
    sc("BLE remote key, bytes differ", "V", 9000, "2500:PEAK:800", [8900], extra=["--ble", "5000:f430353032"]),
    sc("BLE remote key while BLE off", "V", 9000, "", [8900], extra=["--ble", f"5000:{ble_key('MODE')}"]),
    sc("BLE clock set + SETUP walk", "V", 21000, ",".join(["2500:PEAK:800"] + [f"{6000 + 1200 * i}:SETUP" for i in range(12)]),
       [6000 + 1200 * i + 900 for i in range(12)],
       extra=["--ble", "5000:" + ble_clock(26, 11, 2, 15, 30, 0)]),
]
for hdr in ["f0", "f1", "f3", "f5", "f9", "ff"]:
    S.append(sc(f"BLE frame header {hdr}", "V", 8000, "2500:PEAK:800", [7900], extra=["--ble", f"5000:{hdr}" + "30" * 14]))
S += [
    # measurement features
    sc("A jack in V", "V", 6000, screens=[5900], extra=["--jack", "A"]),
    sc("mA jack in uA", "uA", 6000, screens=[5900], extra=["--jack", "mA"]),
    sc("A jack in A", "A", 6000, screens=[5900], extra=["--jack", "A"]),
    sc("Hz 1234.5 duty 25%", "Hz", 12000, "3000:MODE,5000:MODE,7000:MODE", [2900, 4900, 6900, 8900], extra=["--freq", "1234.5", "--duty", "0.25"]),
    sc("ACV 1 kHz LPF at 50 Hz", "V", 10000, "2500:MODE,4500:REL:800", [6500, 9900], extra=["--ad1", "2000", "--freq", "50"]),
    sc("ACV 1 kHz LPF at 1 kHz", "V", 10000, "2500:MODE,4500:REL:800", [6500, 9900], extra=["--ad1", "2000", "--freq", "1000"]),
    sc("ACV 1 kHz LPF at 10 Hz", "V", 10000, "2500:MODE,4500:REL:800", [6500, 9900], extra=["--ad1", "2000", "--freq", "10"]),
    sc("ACmV 1 kHz LPF", "mV", 10000, "2500:MODE,4500:REL:800", [6500, 9900], extra=["--ad1", "2000", "--freq", "50"]),
    sc("ACV LPF off again", "V", 12000, "2500:MODE,4500:REL:800,7500:REL:800", [6500, 11900], extra=["--freq", "50"]),
    sc("ACV PEAK", "V", 9000, "2500:MODE,4100:PEAK", [5500, 8900]),
    sc("ACmV PEAK", "mV", 9000, "2500:MODE,4100:PEAK", [5500, 8900]),
    sc("DCV PEAK", "V", 7000, "2500:PEAK", [6900]),
    sc("ACV PEAK twice", "V", 11000, "2500:MODE,4100:PEAK,6500:PEAK", [5500, 8000, 10900]),
    sc("AC+DC", "V", 10000, "2500:MODE,4100:MODE", [5000, 9900]),
    sc("AC+DC mV", "mV", 10000, "2500:MODE,4100:MODE,6100:MODE", [5000, 7000, 9900]),
    sc("ACV dBm page", "V", 10000, "2500:MODE,4500:SETUP,6000:SETUP", [5500, 7000, 9900], extra=["--freq", "1000"]),
    sc("DCV negative -20000", "V", 6000, screens=[5900], extra=["--ad1", "-20000"]),
    sc("DCV negative -30001", "V", 6000, screens=[5900], extra=["--ad1", "-30001"]),
    sc("DCV overload", "V", 6000, screens=[5900], extra=["--ad1", "200000"]),
    sc("Ohm small", "Ohm", 6000, screens=[5900], extra=["--ad1", "100"]),
    sc("low battery", "V", 6000, "2500:SETUP", [3000, 5900], extra=["--battery-raw", "1100"]),
    sc("Fahrenheit", "mV", 9000, "2500:MODE,4100:MODE", [8900], extra=["--ee", "10=00"]),
    sc("temperature hot thermistor", "mV", 9000, "2500:MODE,4100:MODE", [8900], extra=["--thermistor-raw", "3000"]),
    sc("dial V->mV->Ohm->A->V", "V", 12000, "2500:NONE", [3500, 5500, 7500, 9500, 11900],
       extra=["--rotary-at", "3000:mV,5000:Ohm,7000:A,9000:V"]),
    sc("mode memory per position", "V", 12000, "2500:MODE", [3500, 6500, 11900], extra=["--rotary-at", "4500:Ohm,8500:V"]),
    sc("auto-hold, step below 999", "V", 10000, "2500:HOLD,4000:HOLD", [5500, 9900], extra=["--ad1-at", "6000:500"]),
    sc("auto-hold, step 0.5 V", "V", 10000, "2500:HOLD,4000:HOLD", [5500, 9900], extra=["--ad1-at", "6000:5000"]),
    sc("HOLD x3", "V", 9000, "2500:HOLD,4000:HOLD,5500:HOLD", [3500, 5000, 8900]),
    sc("uA double long REL", "uA", 8000, "2500:REL:800,5000:REL:800", [3500, 7900]),
    sc("diode 15 V toggle", "Ohm", 9000, "2500:MODE,4100:MODE,5700:RANGE", [5600, 8900]),
    sc("diode 15 V via SETUP", "Ohm", 10000, "2500:MODE,4100:MODE,5700:SETUP:800", [5600, 7500, 9900]),
    sc("continuity", "Ohm", 6000, "2500:MODE", [5900], extra=["--ad1", "100"]),
    sc("continuity open", "Ohm", 6000, "2500:MODE", [5900], extra=["--ad1", "20000"]),
    sc("continuity RANGE", "Ohm", 8000, "2500:MODE,4100:RANGE", [5000, 7900], extra=["--ad1", "100"]),
    sc("buzzer off then continuity", "Ohm", 9000, "", [8900], extra=["--ee", "1E=00", "--ad1", "100", "--keys", "2500:MODE,4500:REL"]),
    sc("MIN/MAX cycle", "V", 12000, "2500:MINMAX,4100:MINMAX,5700:MINMAX,7300:MINMAX,8900:MINMAX:800", [3500, 5100, 6700, 8300, 11900]),
    sc("MIN/MAX with step", "V", 12000, "2500:MINMAX,7000:MINMAX,8500:MINMAX,10000:MINMAX", [6500, 8000, 9500, 11900],
       extra=["--ad1-at", "4000:30000,5000:10000"]),
    sc("REL then step", "V", 9000, "2500:REL", [3500, 8900], extra=["--ad1-at", "5000:25000"]),
    sc("REL long", "V", 7000, "2500:REL:800", [6900]),
    sc("manual range up x3", "V", 10000, "2500:RANGE,4000:RANGE,5500:RANGE", [3500, 5000, 6500, 9900]),
    sc("VA AC", "VA", 9000, "2500:MODE", [8900], extra=["--jack", "A", "--ad1", "52000"]),
    sc("VA DC jack A", "VA", 9000, "", [5000, 8900], extra=["--jack", "A"]),
    sc("uA burden display", "uA", 9000, "", [5000, 8900], extra=["--jack", "mA"]),
    sc("LowZ AC signal", "LowZ", 9000, "", [5000, 8900], extra=["--ad1", "3000", "--freq", "50"]),
    sc("cap measurement", "Ohm", 12000, "2500:MODE,4100:MODE,5700:MODE", [8000, 11900], extra=["--freq", "100"]),
]

def run(img, s):
    with tempfile.TemporaryDirectory(dir=WORK) as t:
        ee, card = os.path.join(t, "ee.bin"), os.path.join(t, "card.img")
        extra = list(s["extra"]); keys = s["keys"]
        if "--keys" in extra:
            i = extra.index("--keys"); keys = extra[i + 1]; del extra[i:i + 2]
        cmd = [sys.executable, "-I", os.path.join(ROOT, "scripts", "emu_run.py"), img, "svd/STM32L1xx.svd", "--rotary", s["pos"],
               "--ms", str(s["ms"]), "--hy-only", "--glass", "--eeprom", "factory-eeprom.bin", "--ble-all",
               "--screens", ",".join(map(str, s["screens"])), "--ee-out", ee] + extra
        if keys: cmd += ["--keys", keys]
        if "--ad1" not in extra: cmd += ["--ad1", "3000" if s["pos"] == "LowZ" else "20000"]
        if s["sd"]: cmd += ["--sd", s["sd"], "--sd-out", card]
        r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=1800)
        out = r.stdout + r.stderr
        res = {"screens": re.findall(r"SCREEN t=(\d+): (.*)", out),
               "hy": (re.search(r"config image R20..R33 now (\w+)", out) or [None, None])[1],
               "buzzer": (re.search(r"BUZZER: (.*)", out) or [None, None])[1],
               "ble": (re.search(r"BLE TX ALL (\w+)", out) or [None, ""])[1],
               "sdsum": (re.search(r"SD: (\d+ block reads, \d+ block writes)", out) or [None, None])[1],
               "faults": re.findall(r"fault[^\n]*|Traceback[^\n]*|handler \w+ faulted[^\n]*|Error[^\n]*", out)[:5],
               "rc": r.returncode}
        if os.path.exists(ee):
            base = open(os.path.join(ROOT, "factory-eeprom.bin"), "rb").read(); e = open(ee, "rb").read()
            res["eeprom"] = [f"{i:03X}:{e[i]:02X}" for i in range(len(e)) if e[i] != base[i]][:60]
        if s["sd"] and os.path.exists(card):
            if "big8g" in s["sd"]:
                res["sd"] = re.findall(r"SD: blocks touched.*", out)
            else:
                part = card + ("@@1M" if "fat32" in s["sd"] else "")
                env = {**os.environ, "MTOOLS_SKIP_CHECK": "1"}
                ft = subprocess.run([sys.executable, "-I", os.path.join(ROOT, "scripts", "fat_times.py"), card],
                                    capture_output=True, text=True).stdout if "fat32" in s["sd"] else ""
                ls = subprocess.run(["/usr/bin/mdir", "-b", "-i", part, "::/"], capture_output=True, text=True, env=env).stdout
                files = {}
                for f in ls.split():
                    name = f.split("/")[-1]
                    data = subprocess.run(["/usr/bin/mtype", "-i", part, "::/" + name], capture_output=True, env=env).stdout
                    files[name] = data.decode("latin-1")[:6000]
                res["sd"] = {"times": ft.strip().splitlines(), "files": files}
        return res

def slug(n): return re.sub(r"[^A-Za-z0-9]+", "_", n).strip("_")

if __name__ == "__main__":
    if ONLY: S = [s for s in S if ONLY.lower() in s["name"].lower()]
    if LIST:
        for s in S: print(s["name"])
        print(len(S), "scenarios"); sys.exit(0)
    os.makedirs(WORK, exist_ok=True)
    jobs = []                                    # scenario-major order: each scenario finishes on every version together
    for s in S:
        for img in IMAGES:
            ver = os.path.basename(img).replace("EEVBlog", "").replace(".bin", "")
            os.makedirs(os.path.join(OUT, ver), exist_ok=True)
            path = os.path.join(OUT, ver, slug(s["name"]) + ".json")
            if not os.path.exists(path) or json.load(open(path)).get("scenario") != s:   # missing or stale
                jobs.append((os.path.abspath(img), s, path))
    print(f"{len(jobs)} runs", flush=True)
    def one(j):
        img, s, path = j
        res = run(img, s); res["scenario"] = s
        tmp = path + ".part"
        with open(tmp, "w") as f: json.dump(res, f, indent=0); f.flush(); os.fsync(f.fileno())
        os.replace(tmp, path); return path
    done = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=JOBS) as ex:
        for p in ex.map(one, jobs):
            done += 1
            if done % 50 == 0: print(f"{done}/{len(jobs)}", flush=True)
    print("done", flush=True)
