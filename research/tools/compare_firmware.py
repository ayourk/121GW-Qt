#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Runs one scenario list on two firmware images in the emulator and reports where their firmware-independent outputs
# differ: the LCD glass (controller RAM) at fixed times, the final HY3131 R20..R33 image, buzzer edges/on-time, BLE TX
# byte count, SD card files (name, size, created/modified) and their CSV/CAL contents, data-EEPROM changes, faults.
#   compare_firmware.py A.bin B.bin OUT.json [--only SUBSTR]
import concurrent.futures, json, os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
A, B, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
ONLY = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
WORK = tempfile.gettempdir()
POS = ["LowZ", "V", "mV", "Hz", "Ohm", "VA", "uA", "A"]
KEYS = ["SETUP", "MINMAX", "MEM", "RANGE", "HOLD", "REL", "PEAK"]

def sc(name, pos, ms, keys="2500:NONE", screens=None, extra=(), sd=None):
    return dict(name=name, pos=pos, ms=ms, keys=keys, screens=screens or [ms - 100], extra=list(extra), sd=sd)

S = []
for p in POS:
    S.append(sc(f"{p} baseline", p, 6000, screens=[3000, 5900]))
    S.append(sc(f"{p} MODE x1-4", p, 12000, ",".join(f"{2500 + 2000 * i}:MODE" for i in range(4)),
                screens=[2500 + 2000 * i + 1500 for i in range(4)]))
    for k in KEYS:
        S.append(sc(f"{p} {k} short", p, 5000, f"2500:{k}", screens=[4900]))
        S.append(sc(f"{p} {k} long", p, 5500, f"2500:{k}:800", screens=[5400]))
    S.append(sc(f"{p} SETUP walk", p, 2500 + 1200 * 13 + 600, ",".join(f"{2500 + 1200 * i}:SETUP" for i in range(13)),
                screens=[2500 + 1200 * i + 900 for i in range(13)]))
SD = "out/sd"
S += [
    sc("boot SD cal save", "V", 6000, "50:MEM:400,800:REL:300,1600:MODE", [1500, 3000, 5900], sd=f"{SD}/fat32-mbr.img"),
    sc("boot SD cal load", "V", 6000, "50:MEM:400,800:PEAK:300,1600:MODE", [1500, 3000, 5900], sd=f"{SD}/fat32-cal.img"),
    sc("boot SD cal save, no card", "V", 6000, "50:MEM:400,800:REL:300,1600:MODE", [3000], extra=["--no-sd"]),
    sc("boot all segments (RANGE)", "V", 4000, "50:RANGE:400", [800]),
    sc("boot SETUP (factory)", "V", 6000, "50:SETUP:400", [1000, 5900]),
    sc("cal mode AC capture", "V", 18000, "50:MODE:300,500:REL:300,3500:MODE,5000:SETUP,9000:MEM", [4500, 8500, 13000, 17900],
       extra=["--ad1", "0", "--ad1-at", "7500:10000", "--freq", "1000"]),
    sc("cal mode DC capture", "V", 16000, "50:MODE:300,500:REL:300,5000:SETUP,9000:MEM", [4500, 8500, 15900],
       extra=["--ad1", "0", "--ad1-at", "7500:20000"]),
    sc("logging 12 s", "V", 16000, "2500:MEM:800,14000:MEM:800", [5000, 15900], sd=f"{SD}/fat32-mbr.img"),
    sc("logging FAT16 card", "Ohm", 14000, "2500:MEM:800,12000:MEM:800", [13900], sd=f"{SD}/fat16-flat.img"),
    sc("logging blank card", "V", 9000, "2500:MEM:800", [8900]),
    sc("8 GiB card cal save", "V", 6000, "50:MEM:400,800:REL:300,1600:MODE", [3000],
       extra=["--sd-virtual-mb", "8192", "--sd-place", f"8390656:{SD}/big8g-part.img"], sd=f"{SD}/big8g-head.img"),
    sc("BLE on + remote keys", "V", 10000, "2500:PEAK:800,4800:NONE", [9900],
       extra=["--ble", "5000:f40101,6000:f40202,7000:f40404"]),
    sc("BLE clock set", "V", 9000, "2500:PEAK:800,4800:NONE", [8900],
       extra=["--ble", "5000:f8314130423032304631453030" + "0E".encode().hex()]),
    sc("A jack in V", "V", 6000, screens=[5900], extra=["--jack", "A"]),
    sc("mA jack in uA", "uA", 6000, screens=[5900], extra=["--jack", "mA"]),
    sc("A jack in A", "A", 6000, screens=[5900], extra=["--jack", "A"]),
    sc("Hz 1234.5 duty 25%", "Hz", 12000, "3000:MODE,5000:MODE,7000:MODE", [2900, 4900, 6900, 8900], extra=["--freq", "1234.5", "--duty", "0.25"]),
    sc("ACV 1 kHz LPF at 50 Hz", "V", 10000, "2500:MODE,4500:REL:800", [9900], extra=["--ad1", "2000", "--freq", "50"]),
    sc("ACV 1 kHz LPF at 1 kHz", "V", 10000, "2500:MODE,4500:REL:800", [9900], extra=["--ad1", "2000", "--freq", "1000"]),
    sc("ACV 1 kHz LPF at 10 Hz", "V", 10000, "2500:MODE,4500:REL:800", [9900], extra=["--ad1", "2000", "--freq", "10"]),
    sc("ACV PEAK", "V", 9000, "2500:MODE,4100:PEAK", [8900]),
    sc("AC+DC", "V", 10000, "2500:MODE,4100:MODE", [5000, 9900]),
    sc("DCV negative -20000", "V", 6000, screens=[5900], extra=["--ad1", "-20000"]),
    sc("DCV negative -30001", "V", 6000, screens=[5900], extra=["--ad1", "-30001"]),
    sc("DCV overload", "V", 6000, screens=[5900], extra=["--ad1", "200000"]),
    sc("Ohm small", "Ohm", 6000, screens=[5900], extra=["--ad1", "100"]),
    sc("low battery", "V", 6000, "2500:SETUP", [5900], extra=["--battery-raw", "1100"]),
    sc("Fahrenheit", "mV", 9000, "2500:MODE,4100:MODE", [8900], extra=["--ee", "10=00"]),
    sc("temperature hot thermistor", "mV", 9000, "2500:MODE,4100:MODE", [8900], extra=["--thermistor-raw", "3000"]),
    sc("dial V->mV->Ohm->A->V", "V", 12000, "2500:NONE", [3500, 5500, 7500, 9500, 11900],
       extra=["--rotary-at", "3000:mV,5000:Ohm,7000:A,9000:V"]),
    sc("auto-hold, step below 999", "V", 10000, "2500:HOLD,4000:HOLD", [5500, 9900], extra=["--ad1-at", "6000:500"]),
    sc("auto-hold, step 0.5 V", "V", 10000, "2500:HOLD,4000:HOLD", [5500, 9900], extra=["--ad1-at", "6000:5000"]),
    sc("uA double long REL", "uA", 8000, "2500:REL:800,5000:REL:800", [3500, 7900]),
    sc("diode 15 V toggle", "Ohm", 9000, "2500:MODE,4100:MODE,5700:RANGE", [8900]),
    sc("continuity", "Ohm", 6000, "2500:MODE", [5900], extra=["--ad1", "100"]),
    sc("MIN/MAX cycle", "V", 12000, "2500:MINMAX,4100:MINMAX,5700:MINMAX,7300:MINMAX,8900:MINMAX:800", [3500, 5100, 6700, 8300, 11900]),
    sc("REL then step", "V", 9000, "2500:REL", [3500, 8900], extra=["--ad1-at", "5000:25000"]),
    sc("manual range up x3", "V", 10000, "2500:RANGE,4000:RANGE,5500:RANGE", [3500, 5000, 6500, 9900]),
    sc("VA AC", "VA", 9000, "2500:MODE", [8900], extra=["--jack", "A", "--ad1", "52000"]),
]
if ONLY: S = [s for s in S if ONLY.lower() in s["name"].lower()]

def run(img, s):
    with tempfile.TemporaryDirectory(dir=WORK) as t:
        ee, card = os.path.join(t, "ee.bin"), os.path.join(t, "card.img")
        cmd = [sys.executable, "-I", os.path.join(HERE, "emu_run.py"), img, "svd/STM32L1xx.svd", "--rotary", s["pos"],
               "--ms", str(s["ms"]), "--hy-only", "--glass", "--eeprom", "factory-eeprom.bin", "--keys", s["keys"],
               "--screens", ",".join(map(str, s["screens"])), "--ee-out", ee] + s["extra"]
        if not any(x.startswith("--ad1") and x == "--ad1" for x in s["extra"]): cmd += ["--ad1", "20000"] if s["pos"] != "LowZ" else ["--ad1", "3000"]
        if s["sd"]: cmd += ["--sd", s["sd"], "--sd-out", card]
        r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=900)
        out = r.stdout + r.stderr
        res = {"screens": re.findall(r"SCREEN t=(\d+): (.*)", out),
               "hy": (re.search(r"config image R20..R33 now (\w+)", out) or [None, None])[1],
               "buzzer": (re.search(r"BUZZER: (.*)", out) or [None, None])[1],
               "ble": (re.search(r"BLE TX total (\d+)", out) or [None, "0"])[1],
               "faults": re.findall(r"fault[^\n]*|Traceback[^\n]*|handler \w+ faulted[^\n]*", out)}
        if os.path.exists(ee):
            base = open(os.path.join(ROOT, "factory-eeprom.bin"), "rb").read(); e = open(ee, "rb").read()
            res["eeprom"] = [f"{i:03X}:{e[i]:02X}" for i in range(len(e)) if e[i] != base[i]][:40]
        if s["sd"] and os.path.exists(card):
            if "big8g" in s["sd"]:
                res["sd"] = re.findall(r"SD: blocks touched.*", out)
            else:
                ft = subprocess.run([sys.executable, "-I", os.path.join(HERE, "fat_times.py"), card], capture_output=True, text=True).stdout \
                     if "fat32" in s["sd"] else ""
                ls = subprocess.run(["/usr/bin/mdir", "-b", "-i", card + ("@@1M" if "fat32" in s["sd"] else ""), "::/"],
                                    capture_output=True, text=True, env={**os.environ, "MTOOLS_SKIP_CHECK": "1"}).stdout
                files = {}
                for f in ls.split():
                    name = f.split("/")[-1]
                    data = subprocess.run(["/usr/bin/mtype", "-i", card + ("@@1M" if "fat32" in s["sd"] else ""), "::/" + name],
                                          capture_output=True, env={**os.environ, "MTOOLS_SKIP_CHECK": "1"}).stdout
                    files[name] = data.decode("latin-1")[:4000]
                res["sd"] = {"times": ft.strip().splitlines(), "files": files}
        return res

def both(s):
    return s["name"], run(A, s), run(B, s)

results = []
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as ex:
    for name, ra, rb in ex.map(both, S):
        diff = sorted(k for k in ra if ra.get(k) != rb.get(k))
        results.append({"name": name, "diff": diff, "a": ra, "b": rb})
        print(f"{'DIFF' if diff else 'same'}  {name}  {','.join(diff)}", flush=True)
json.dump(results, open(OUT, "w"), indent=1)
print(f"{sum(1 for r in results if r['diff'])} of {len(results)} scenarios differ; details in {OUT}")
