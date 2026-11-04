# SPDX-License-Identifier: MIT
"""Scripted Bluetooth peer for emu_run.py --ble-peer: plays the PC side of the custom firmware's file transfer
(scripts/ymodem_ble.py, the same code the real tool runs) on emulated time.

  emu_run.py IMAGE svd/STM32L1xx.svd ... --ble-peer scripts/ble_peer_ymodem.py --ble-peer-arg 'ops=OP;OP;...'
  ops (run in order, each after the previous one finished):
    info | list | abort | wait:SECONDS | raw:HEX (bytes as they are, e.g. a stock F4 key frame)
    echo:HEX (raw bytes that the firmware must echo within 2 s: stock F4/F8) | cmd:CODE[:PAYLOAD] (hex) | badcrc
    put:LOCAL[:NAME]      upload with 1 KB blocks          put128:LOCAL[:NAME]  with 128-byte blocks
    get:NAME:OUT          download, 128-byte blocks        get1k:NAME:OUT       with 1 KB blocks
    pty-put:CMD           RECEIVE + bridge to a pty running CMD ('+' for spaces), e.g. sz+--ymodem+-k+FILE
    pty-get:NAME:CMD      SEND NAME + bridge to a pty running CMD, e.g. rz+--ymodem (cwd: env YMODEM_PTY_CWD)
  other args: start=SECONDS before the first op (default 3: past the boot screen), corrupt=N[,N] (data blocks
  whose first transmission of a put gets a bad CRC), cut=N (a put stops writing after N data blocks: a cut link),
  seed=N (sequence-number seed, default 1), trace=1 (print every byte exchanged).
Prints 'PEER ...' lines as it goes and, at the end, one 'PEER RESULT' line per op and the count of valid 0xF2
status packets before, during and after each op. The run ends when the ops are done (emu_run stops early).
"""
import os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ymodem_ble as Y


class Peer:
    def __init__(self, args):
        self.args = args
        random.seed(int(args.get("seed", "1")))
        self.tr = Y.EmuTransport()
        self.link = Y.Link(self.tr)
        self.meter = Y.Meter(self.link, self.log)
        self.results, self.windows, self.f2, self.scan, self.serial = [], [], [], bytearray(), None
        self.tr.start(self.run)

    def log(self, s):
        print(f"  PEER t={self.tr.t * 1000:8.0f} ms {s}", flush=True)

    # emulator side ------------------------------------------------------------------------------------------
    def step(self, t_ms, tx):
        if tx:
            self._scan_f2(t_ms, tx)
        out = self.tr.step(t_ms, tx)
        if self.args.get("trace") and (tx or out):
            print(f"  PEER TRACE t={t_ms} meter->pc {tx[:40].hex()}{'..' if len(tx) > 40 else ''} "
                  f"pc->meter {out[:40].hex()}{'..' if len(out) > 40 else ''} ({len(out)})", flush=True)
        return out

    def finished(self):
        return self.tr.finished

    def _scan_f2(self, t_ms, tx):
        """valid 19-byte 0xF2 packets (XOR, and the serial bytes 1..4 of the first one) in the meter's output"""
        self.scan += tx
        while len(self.scan) >= 19:
            if self.scan[0] != 0xF2:
                del self.scan[0]
                continue
            x = 0
            for c in self.scan[:18]:
                x ^= c
            if x == self.scan[18] and (self.serial is None or bytes(self.scan[1:5]) == self.serial):
                self.serial = bytes(self.scan[1:5])
                self.f2.append(t_ms)
                del self.scan[:19]
            else:
                del self.scan[0]

    def summary(self):
        out = []
        if self.tr.error:
            out.append(f"PEER ERROR {type(self.tr.error).__name__}: {self.tr.error}")
        for op, ok, detail in self.results:
            out.append(f"PEER RESULT {op}: {'ok' if ok else 'FAIL'} {detail}")
        if self.windows:
            first = self.windows[0][1]
            out.append(f"PEER F2 status packets before the first op: {sum(1 for t in self.f2 if t < first)}")
            for i, (op, t0, t1) in enumerate(self.windows):
                n = sum(1 for t in self.f2 if t0 <= t < t1)
                ts = [t for t in self.f2 if t0 <= t < t1]
                out.append(f"PEER F2 status packets during {op} ({t0}-{t1} ms): {n}"
                           + (f" at {ts[0]}..{ts[-1]} ms" if ts else ""))
            last = self.windows[-1][2]
            after = [t for t in self.f2 if t >= last]
            out.append(f"PEER F2 status packets after the last op: {len(after)}"
                       + (f" (first at {after[0]} ms, last at {after[-1]} ms)" if after else ""))
        return "\n".join(out)

    # protocol side (its own thread, emulated time) ----------------------------------------------------------
    def run(self):
        self.link.sleep(float(self.args.get("start", "3")))
        corrupt = {int(x) for x in self.args.get("corrupt", "").split(",") if x}
        cut = int(self.args["cut"]) if "cut" in self.args else None
        for op in filter(None, self.args.get("ops", "").split(";")):
            f = op.split(":")
            t0 = int(self.tr.t * 1000)
            self.log(f"op {op}")
            ok, detail = True, ""
            try:
                if f[0] == "info":
                    st, p = self.meter.info()
                    ok, detail = st == 0, f"{Y.STATUS.get(st, st)} {p.hex()}"
                elif f[0] == "list":
                    entries = self.meter.list()
                    detail = "; ".join(f"{n} {s} {w}" for n, s, a, w in entries)
                elif f[0] == "abort":
                    detail = Y.STATUS.get(self.meter.abort(), "?")
                elif f[0] == "wait":
                    self.link.sleep(float(f[1]))
                    self.link.read_some(0)          # drop what arrived meanwhile
                elif f[0] == "raw":
                    self.link.write(bytes.fromhex(f[1]))
                elif f[0] == "echo":                # a stock F4/F8 frame: accepted ones are echoed
                    want, seen = bytes.fromhex(f[1]), bytearray()
                    self.link.write(want)
                    end = self.tr.t + 2.0
                    while want not in seen and self.tr.t < end:
                        seen += self.link.read_some(end - self.tr.t)
                    ok, detail = want in seen, "echoed" if want in seen else "no echo"
                elif f[0] == "cmd":                 # any command code (+ payload): the reply's status
                    st, p = self.meter.command(int(f[1], 16), bytes.fromhex(f[2]) if len(f) > 2 else b"")
                    detail = f"{Y.STATUS.get(st, st)} {p.hex()}"
                elif f[0] == "badcrc":              # INFO with a damaged CRC: no reply expected
                    fr = bytearray(self.meter.frame(Y.CMD_INFO))
                    fr[-1] ^= 0x55
                    self.link.write(bytes(fr))
                    end, got = self.tr.t + 2.0, bytearray()
                    while self.tr.t < end:
                        got += self.link.read_some(end - self.tr.t)
                    ok = Y.SOF_REPLY not in got
                    detail = "no reply" if ok else f"reply bytes {got.hex()}"
                elif f[0] in ("put", "put128"):
                    data = open(f[1], "rb").read()
                    name = f[2] if len(f) > 2 else os.path.basename(f[1])
                    self.meter.start_receive()
                    ok = Y.ymodem_send(self.link, [(name, data, os.path.getmtime(f[1]))], one_k=f[0] == "put",
                                       log=self.log, corrupt=corrupt, stop_after=cut)
                    detail = f"{name} {len(data)} bytes"
                elif f[0] in ("get", "get1k"):
                    size = self.meter.start_send(f[1], one_k=f[0] == "get1k")
                    files = Y.ymodem_recv(self.link, self.log)
                    with open(f[2] + ".part", "wb") as fo:
                        for name, data, mtime in files:
                            fo.write(data)
                    os.replace(f[2] + ".part", f[2])
                    detail = f"{f[1]}: announced {size} bytes, got " + ", ".join(
                        f"{n} {len(d)} bytes mtime {m}" for n, d, m in files)
                    ok = len(files) == 1 and len(files[0][1]) == size
                elif f[0] == "pty-put":
                    self.meter.start_receive()
                    ok = Y.pty_bridge(self.link, f[1].split("+"), Y.Block0Filter(self.log), self.log)
                elif f[0] == "pty-get":
                    self.meter.start_send(f[1])
                    ok = Y.pty_bridge(self.link, f[2].split("+"), None, self.log)
                else:
                    ok, detail = False, "unknown op"
            except Y.TransferError as e:
                ok, detail = False, f"{detail} {e}".strip()
            self.results.append((op, ok, detail))
            self.windows.append((op, t0, int(self.tr.t * 1000)))
            self.log(f"op {op}: {'ok' if ok else 'FAIL'} {detail}")
