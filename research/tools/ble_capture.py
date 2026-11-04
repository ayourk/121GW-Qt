#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Connects to a 121GW over Bluetooth LE through BlueZ (D-Bus; no pairing, no root), subscribes to its data
# characteristic, and logs every received byte. Packets are reassembled (they can arrive split or bunched): 0xF2 =
# the normal 19-byte reading packet, 0xD9 = our HY3131 diagnostic frame (patches/hy_diag.s). XOR (byte 18 = XOR of
# bytes 0..17) is checked on both. The adapter is left as it was; the meter is disconnected on exit.
#   ble_capture.py LOG.txt [--seconds N] [--scan-seconds N] [--name 121GW] [--address AA:BB:..]
# Stop early: create LOG.txt.stop (removed on exit). Ctrl-C works too (finishes the current line, disconnects).
import argparse, os, signal, sys, time
import dbus, dbus.mainloop.glib
from gi.repository import GLib

CHAR = "e7add780-b042-4876-aae1-112855353cc1"
BLUEZ, OM, PROPS = "org.bluez", "org.freedesktop.DBus.ObjectManager", "org.freedesktop.DBus.Properties"

ap = argparse.ArgumentParser()
ap.add_argument("log"); ap.add_argument("--seconds", type=float, default=120); ap.add_argument("--scan-seconds", type=float, default=30)
ap.add_argument("--name", default="121GW"); ap.add_argument("--address")
a = ap.parse_args()
stopfile = a.log + ".stop"
if os.path.exists(stopfile): os.remove(stopfile)
log = open(a.log, "a", buffering=1)
def note(s):
    line = f"{time.strftime('%H:%M:%S')} {s}"; print(line, flush=True); log.write(line + "\n")

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SystemBus(); loop = GLib.MainLoop()
om = dbus.Interface(bus.get_object(BLUEZ, "/"), OM)
stop = [False]
def on_signal(*_): stop[0] = True
signal.signal(signal.SIGINT, on_signal); signal.signal(signal.SIGTERM, on_signal)

def objects(): return om.GetManagedObjects()
adapter_path = next(p for p, i in objects().items() if "org.bluez.Adapter1" in i)
adapter = dbus.Interface(bus.get_object(BLUEZ, adapter_path), "org.bluez.Adapter1")

def find_device():
    for p, i in objects().items():
        d = i.get("org.bluez.Device1")
        if not d: continue
        if a.address and str(d.get("Address", "")).lower() == a.address.lower(): return p, d
        if not a.address and str(d.get("Name", d.get("Alias", ""))) in (a.name, "Bluegiga"): return p, d
    return None, None

dev_path, d = find_device()
if not dev_path:
    note(f"scanning up to {a.scan_seconds:.0f} s for '{a.name}' (turn the meter's Bluetooth on: long PEAK)")
    adapter.SetDiscoveryFilter({"Transport": "le"})
    adapter.StartDiscovery()
    t0 = time.time()
    while time.time() - t0 < a.scan_seconds and not stop[0] and not dev_path:
        GLib.MainContext.default().iteration(False); time.sleep(0.2); dev_path, d = find_device()
    try: adapter.StopDiscovery()
    except dbus.DBusException: pass
if not dev_path: note("meter not found"); sys.exit(1)
note(f"found {d.get('Name', d.get('Alias'))} {d.get('Address')} RSSI {d.get('RSSI', '?')} at {dev_path}")
dev = dbus.Interface(bus.get_object(BLUEZ, dev_path), "org.bluez.Device1")
devprops = dbus.Interface(bus.get_object(BLUEZ, dev_path), PROPS)
char_path = None
for attempt in range(1, 4):                     # the meter may still be rebooting: retry the connection
    try:
        if not devprops.Get("org.bluez.Device1", "Connected"):
            note(f"connecting (attempt {attempt})"); dev.Connect()
    except dbus.DBusException as e:
        note(f"connect failed: {e.get_dbus_name()}"); time.sleep(3); continue
    t0 = time.time()
    while not devprops.Get("org.bluez.Device1", "ServicesResolved") and time.time() - t0 < 60 and not stop[0]:
        GLib.MainContext.default().iteration(False); time.sleep(0.2)
    char_path = next((p for p, i in objects().items() if p.startswith(dev_path) and
                      str(i.get("org.bluez.GattCharacteristic1", {}).get("UUID", "")).lower() == CHAR), None)
    if char_path: break
    note("services not resolved; disconnecting to retry")
    try: dev.Disconnect()
    except dbus.DBusException: pass
    time.sleep(3)
if not char_path: note("data characteristic not found"); sys.exit(1)
note(f"subscribing to {CHAR} ({char_path})")

buf = bytearray(); stats = {"F2": 0, "D9": 0, "bad": 0, "bytes": 0}
def frames():
    while len(buf) >= 19:
        if buf[0] not in (0xF2, 0xD9): del buf[0]; continue
        f = bytes(buf[:19]); x = 0
        for c in f[:18]: x ^= c
        if x != f[18]: stats["bad"] += 1; del buf[0]; continue
        del buf[:19]
        if f[0] == 0xF2:
            stats["F2"] += 1
            val = (f[5] >> 6 << 16 | f[7] << 8 | f[8]) * (-1 if f[6] & 0x40 else 1)
            note(f"F2 {f.hex(' ')}  mode {f[5] & 0x1F} range {f[6] & 0xF} value {val}{' OFL' if f[6] & 0x80 else ''}")
        else:
            stats["D9"] += 1
            if f[1] == ord("A"):
                note(f"D9 A {f.hex(' ')}  R34..R37 {f[2:6].hex(' ')}  latch mask 0x{f[6]:02X}  nonzero {f[7]}  power-on R20..R29 {f[8:18].hex(' ')}")
            else:
                note(f"D9 B {f.hex(' ')}  power-on R2A..R37 {f[2:16].hex(' ')}")

def on_props(iface, changed, invalidated, path=None):
    if "Value" in changed:
        b = bytes(changed["Value"]); stats["bytes"] += len(b); log.write(f"{time.time():.3f} RAW {b.hex()}\n"); buf.extend(b); frames()
bus.add_signal_receiver(on_props, dbus_interface=PROPS, signal_name="PropertiesChanged", path=char_path, path_keyword="path")
ch = dbus.Interface(bus.get_object(BLUEZ, char_path), "org.bluez.GattCharacteristic1")
ch.StartNotify()
end = time.time() + a.seconds
def tick():
    if stop[0] or os.path.exists(stopfile) or time.time() > end: loop.quit(); return False
    return True
GLib.timeout_add(250, tick)
note(f"capturing up to {a.seconds:.0f} s (stop: touch {stopfile})")
loop.run()
try: ch.StopNotify()
except dbus.DBusException: pass
try: dev.Disconnect()
except dbus.DBusException: pass
note(f"done: {stats['bytes']} bytes, {stats['F2']} F2 packets, {stats['D9']} D9 diagnostic frames, {stats['bad']} bad XOR")
if os.path.exists(stopfile): os.remove(stopfile)
