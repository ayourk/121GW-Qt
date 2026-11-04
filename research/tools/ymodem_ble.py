#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""File transfer with a 121GW running the custom firmware (C-2.05 line with bt_xfer.c) over Bluetooth LE: command
frames (0xF6 ... / replies 0xD6 ...) and YMODEM-1K, as specified in docs/ble-ymodem.md ("Wire protocol"). Our own
implementation from the public YMODEM description (no lrzsz code).

  ymodem_ble.py [--address AA:BB:..] [--name 121GW] info
  ymodem_ble.py ... list
  ymodem_ble.py ... put FILE [--as NAME.EXT] [--128]          upload (YMODEM, 1 KB blocks unless --128)
  ymodem_ble.py ... get NAME.EXT [-o OUT] [--1k] [--pace N] [--gap G]   download (128-byte blocks unless --1k;
                        pace = bytes per 100 ms burst, gap = extra 100 ms between bursts, 0..7)
  ymodem_ble.py ... pty-put [--run 'sz --ymodem -k FILE']      bridge the link to a pseudo-terminal for lrzsz:
  ymodem_ble.py ... pty-get NAME.EXT [--run 'rz --ymodem']     the meter receives (sz) / sends (rz)

Without --run the pty's name is printed: run e.g. `sz --ymodem -k FILE <PTY >PTY` yourself. The bridge holds
back sz's repeats of block 0 while the meter's overwrite prompt is up (sz retransmits after 10 s; the meter waits up
to 30 s for the key press), and ends when the transfer ends (or the --run command exits).

BLE: BlueZ over D-Bus (python3-dbus + GLib, no pairing, no root); data characteristic
E7ADD780-B042-4876-AAE1-112855353CC1: notifications in, writes out in chunks of 20 bytes (write request when the
characteristic offers it, else write command; the same choice as the Qt app's BleConnection::sendCommand).
Stop early: Ctrl-C (the link is closed, the meter cancels the transfer on its own timeouts).

The protocol classes also run inside the emulator (scripts/ble_peer_ymodem.py with emu_run.py --ble-peer) through
EmuTransport, which runs the protocol code on emulated time.
"""
import struct
import argparse, os, random, select, subprocess, sys, threading, time

CHAR = "e7add780-b042-4876-aae1-112855353cc1"
SOF_CMD, SOF_REPLY = 0xF6, 0xD6
CMD_INFO, CMD_RECEIVE, CMD_SEND, CMD_LIST, CMD_ABORT = 0x01, 0x10, 0x11, 0x12, 0x1F
STATUS = {0: "OK", 1: "BAD_CMD", 2: "BAD_ARG", 3: "BUSY", 4: "NOT_FOUND", 5: "SD_ERROR", 6: "END"}
SOH, STX, EOT, ACK, NAK, CAN, C, PAD = 0x01, 0x02, 0x04, 0x06, 0x15, 0x18, 0x43, 0x1A


def crc16(data, crc=0):
    """CRC-16/XMODEM: poly 0x1021, init 0, MSB first (check: b'123456789' -> 0x31C3)."""
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def decode_info(p):
    """INFO bytes 3..18 (FEATURES bit 1): the meter's record of its last canceled transfer."""
    if len(p) < 19:
        return f"not reported (INFO {p.hex()}: older firmware)"
    line, state, retries, pos, done, cancels, rr = struct.unpack_from("<HBBIIHH", p, 3)
    msi = ""
    if len(p) >= 23 and p[2] & 4:
        hz = struct.unpack_from("<I", p, 19)[0]
        msi = (f"; MSI measured {hz} Hz ({(hz / 4194304 - 1) * 100:+.3f}% from nominal)" if hz
               else "; MSI not measured")
        if len(p) >= 30:
            st, per, tot = struct.unpack_from("<BHI", p, 23)
            names = {0: "never tried", 1: "ok", 2: "busy (not on MSI / HSE on)", 3: "HSE never ready",
                     4: "TIM11 never captured", 5: "captures stopped", 6: "too many overcaptures", 7: "implausible",
                     8: "PLL never locked", 9: "SYSCLK did not switch to the PLL"}
            msi += (f"; last attempt: {names.get(st, st)}" +
                    (f" after {tot} HSE polls" if st == 3 else f", {per} pairs, {tot} counts" if st != 0 else ""))
    if cancels == 0:
        return f"none since power-on; failed card reads (retried): {rr}" + msi
    return (f"bt_xfer.c line {line}, state {state}, retries {retries}, sent+ACKed {pos} bytes, written {done} bytes; "
            f"cancels since power-on {cancels}, failed card reads {rr}" + msi)


class TransferError(Exception):
    pass


# ---- transports: write(bytes), take() -> bytes received so far, wait(deadline, rx=True), now() ----------------

class EmuTransport:
    """Lockstep transport for the emulator: the protocol runs in its own thread; emu_run.py calls step() once per
    emulated millisecond with the bytes the firmware sent, and the protocol thread runs (on emulated time) until it
    waits again. Deterministic: only one side runs at a time."""
    def __init__(self):
        self.cv = threading.Condition()
        self.rx, self.out = bytearray(), bytearray()
        self.t, self.turn, self.wake_at, self.wake_on_rx = 0.0, "emu", 0.0, True
        self.finished, self.error, self.thread = False, None, None

    def start(self, fn):
        def run():
            with self.cv:
                self.cv.wait_for(lambda: self.turn == "proto")
            try:
                fn()
            except BaseException as e:      # reported by the peer
                self.error = e
            with self.cv:
                self.finished, self.turn = True, "emu"
                self.cv.notify_all()
        self.thread = threading.Thread(target=run, daemon=True)
        self.thread.start()

    # protocol side
    def now(self):
        return self.t

    def write(self, b):
        self.out += b

    def take(self):
        b = bytes(self.rx)
        self.rx.clear()
        return b

    def wait(self, deadline, rx=True):
        with self.cv:
            if (rx and self.rx) or self.t >= deadline:
                return
            self.wake_at, self.wake_on_rx, self.turn = deadline, rx, "emu"
            self.cv.notify_all()
            self.cv.wait_for(lambda: self.turn == "proto")

    # emulator side
    def step(self, t_ms, tx):
        with self.cv:
            self.t = t_ms / 1000.0
            self.rx += tx
            if self.finished:
                return b""
            if (self.wake_on_rx and self.rx) or self.t >= self.wake_at:
                self.turn = "proto"
                self.cv.notify_all()
                self.cv.wait_for(lambda: self.turn == "emu")
            o = bytes(self.out)
            self.out.clear()
            return o


class BleTransport:
    """BlueZ D-Bus: notifications on the data characteristic in, 20-byte writes out."""
    def __init__(self, address=None, name="121GW", scan_seconds=30, log=print):
        import dbus, dbus.mainloop.glib
        from gi.repository import GLib
        self.dbus, self.GLib, self.log = dbus, GLib, log
        dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
        self.bus = dbus.SystemBus()
        self.ctx = GLib.MainContext.default()
        self.rx = bytearray()
        om = dbus.Interface(self.bus.get_object("org.bluez", "/"), "org.freedesktop.DBus.ObjectManager")
        self.objects = om.GetManagedObjects
        adapter_path = next(p for p, i in self.objects().items() if "org.bluez.Adapter1" in i)
        adapter = dbus.Interface(self.bus.get_object("org.bluez", adapter_path), "org.bluez.Adapter1")

        def find():
            for p, i in self.objects().items():
                d = i.get("org.bluez.Device1")
                if not d:
                    continue
                if address and str(d.get("Address", "")).lower() == address.lower():
                    return p
                if not address and str(d.get("Name", d.get("Alias", ""))) in (name, "Bluegiga"):
                    return p
            return None
        dev_path = find()
        if not dev_path:
            log(f"scanning up to {scan_seconds:.0f} s (meter Bluetooth on: long PEAK)")
            adapter.SetDiscoveryFilter({"Transport": "le"})
            adapter.StartDiscovery()
            t0 = time.monotonic()
            while not dev_path and time.monotonic() - t0 < scan_seconds:
                self.ctx.iteration(False)
                time.sleep(0.2)
                dev_path = find()
            try:
                adapter.StopDiscovery()
            except dbus.DBusException:
                pass
        if not dev_path:
            raise TransferError("meter not found")
        self.dev = dbus.Interface(self.bus.get_object("org.bluez", dev_path), "org.bluez.Device1")
        props = dbus.Interface(self.bus.get_object("org.bluez", dev_path), "org.freedesktop.DBus.Properties")
        if not props.Get("org.bluez.Device1", "Connected"):
            log("connecting")
            self.dev.Connect()
        t0 = time.monotonic()
        while not props.Get("org.bluez.Device1", "ServicesResolved") and time.monotonic() - t0 < 60:
            self.ctx.iteration(False)
            time.sleep(0.2)
        char_path, flags = None, []
        for p, i in self.objects().items():
            ch = i.get("org.bluez.GattCharacteristic1", {})
            if p.startswith(dev_path) and str(ch.get("UUID", "")).lower() == CHAR:
                char_path, flags = p, [str(f) for f in ch.get("Flags", [])]
        if not char_path:
            raise TransferError("data characteristic not found")
        self.write_type = "request" if "write" in flags else "command"
        self.char = dbus.Interface(self.bus.get_object("org.bluez", char_path), "org.bluez.GattCharacteristic1")
        self.bus.add_signal_receiver(self._on_props, dbus_interface="org.freedesktop.DBus.Properties",
                                     signal_name="PropertiesChanged", path=char_path)
        self.char.StartNotify()
        log(f"connected; writes as '{self.write_type}' (characteristic flags {','.join(flags)})")

    def _on_props(self, iface, changed, invalidated):
        if "Value" in changed:
            self.rx += bytes(changed["Value"])

    def now(self):
        return time.monotonic()

    def write(self, b):
        for i in range(0, len(b), 20):
            self.char.WriteValue(self.dbus.Array(b[i:i + 20], signature="y"), {"type": self.write_type})
            self.ctx.iteration(False)

    def take(self):
        while self.ctx.iteration(False):
            pass
        b = bytes(self.rx)
        self.rx.clear()
        return b

    def wait(self, deadline, rx=True):
        while time.monotonic() < deadline:
            while self.ctx.iteration(False):
                pass
            if rx and self.rx:
                return
            time.sleep(0.005)

    def close(self):
        for f in (self.char.StopNotify, self.dev.Disconnect):
            try:
                f()
            except self.dbus.DBusException:
                pass


# ---- byte stream with timeouts ---------------------------------------------------------------------------------

class Link:
    def __init__(self, transport):
        self.t, self.buf = transport, bytearray()

    def now(self):
        return self.t.now()

    def write(self, b):
        self.t.write(bytes(b))

    def _fill(self):
        self.buf += self.t.take()

    def read_some(self, timeout):
        deadline = self.now() + timeout
        while True:
            self._fill()
            if self.buf:
                b = bytes(self.buf)
                self.buf.clear()
                return b
            if self.now() >= deadline:
                return b""
            self.t.wait(deadline)

    def getc(self, timeout):
        deadline = self.now() + timeout
        while True:
            self._fill()
            if self.buf:
                c = self.buf[0]
                del self.buf[0]
                return c
            if self.now() >= deadline:
                return None
            self.t.wait(deadline)

    def read_exact(self, n, gap):
        """n bytes, each within `gap` seconds of the previous one; fewer if the line goes quiet."""
        out = bytearray()
        while len(out) < n:
            self._fill()
            if self.buf:
                k = min(n - len(out), len(self.buf))
                out += self.buf[:k]
                del self.buf[:k]
                continue
            c = self.getc(gap)
            if c is None:
                break
            out.append(c)
        return bytes(out)

    def unread(self, b):
        self.buf[:0] = b

    def purge(self, quiet=1.0, limit=10.0):
        end = self.now() + limit
        self._fill()
        self.buf.clear()
        while self.now() < end:
            if self.read_some(quiet) == b"":
                return

    def sleep(self, s):
        deadline = self.now() + s
        while self.now() < deadline:
            self.t.wait(deadline, rx=False)


# ---- command frames ----------------------------------------------------------------------------------------------

class Meter:
    def __init__(self, link, log=print):
        self.link, self.log, self.seq = link, log, random.randrange(256)

    def frame(self, cmd, payload=b""):
        self.seq = (self.seq + 1) & 0xFF
        f = bytes([SOF_CMD, len(payload), self.seq, cmd]) + bytes(payload)
        c = crc16(f)
        return f + bytes([c >> 8, c & 0xFF])

    def command(self, cmd, payload=b"", timeout=3.0, tries=4):
        """(status, payload) of the reply; the same frame (same SEQ) is repeated on a timeout."""
        f = self.frame(cmd, payload)
        for attempt in range(tries):
            self.link.write(f)
            end = self.link.now() + timeout
            while self.link.now() < end:
                c = self.link.getc(end - self.link.now())
                if c is None:
                    break
                if c != SOF_REPLY:
                    continue                    # status packets, echoes, stray bytes
                hdr = self.link.read_exact(3, 1.0)
                if len(hdr) < 3 or hdr[0] > 32:
                    self.link.unread(hdr)
                    continue
                rest = self.link.read_exact(hdr[0] + 2, 1.0)
                whole = bytes([SOF_REPLY]) + hdr + rest
                if len(rest) < hdr[0] + 2 or crc16(whole[:-2]) != (whole[-2] << 8 | whole[-1]):
                    self.link.unread(hdr + rest)
                    continue
                if hdr[1] != self.seq:
                    continue                    # an old reply
                return hdr[2], rest[:-2]
            self.log(f"no reply to command 0x{cmd:02X} (try {attempt + 1}/{tries})")
        raise TransferError(f"no reply to command 0x{cmd:02X}")

    def info(self):
        st, p = self.command(CMD_INFO)
        return st, p

    def list(self):
        out, i = [], 0
        while True:
            st, p = self.command(CMD_LIST, bytes([i >> 8, i & 0xFF]))
            if st == 6:
                return out
            if st != 0:
                raise TransferError(f"list: {STATUS.get(st, st)}")
            size = int.from_bytes(p[1:5], "big")
            fd, ft = int.from_bytes(p[5:7], "big"), int.from_bytes(p[7:9], "big")
            when = f"{1980 + (fd >> 9):04d}-{fd >> 5 & 15:02d}-{fd & 31:02d} {ft >> 11:02d}:{ft >> 5 & 63:02d}:{(ft & 31) * 2:02d}"
            out.append((p[9:].decode("ascii", "replace"), size, p[0], when))
            i += 1

    def start_receive(self):
        st, _ = self.command(CMD_RECEIVE)
        if st != 0:
            raise TransferError(f"receive: {STATUS.get(st, st)}")

    def start_send(self, name, one_k=False, pace=0, gap=0):
        """pace: bytes per 100 ms burst (0 = the meter's default); gap: extra 100 ms between bursts (0..7)."""
        st, p = self.command(CMD_SEND, bytes([1 if one_k else 0, (pace & 0x1F) | ((gap & 7) << 5)])
                             + name.encode("ascii"))
        if st != 0:
            raise TransferError(f"send {name}: {STATUS.get(st, st)}")
        return int.from_bytes(p[:4], "big")

    def abort(self):
        return self.command(CMD_ABORT)[0]


# ---- YMODEM ------------------------------------------------------------------------------------------------------

def _block(num, data, size, pad=PAD):
    data = bytes(data).ljust(size, bytes([pad]))
    c = crc16(data)
    return bytes([SOH if size == 128 else STX, num & 0xFF, 0xFF - (num & 0xFF)]) + data + bytes([c >> 8, c & 0xFF])


def _wait_for(link, wanted, timeout, log):
    """the first byte in `wanted` within timeout; CAN CAN raises; others are skipped."""
    end, cans = link.now() + timeout, 0
    while link.now() < end:
        c = link.getc(end - link.now())
        if c is None:
            break
        if c == CAN:
            cans += 1
            if cans >= 2:
                raise TransferError("canceled by the meter (CAN CAN)")
            continue
        cans = 0
        if c in wanted:
            return c
    return None


def ymodem_send(link, files, one_k=True, log=print, ack0_timeout=40.0, ack_timeout=10.0, corrupt=(), stop_after=None):
    """Upload files = [(name, data, mtime)] as one YMODEM batch. Block 0 is never retransmitted within
    ack0_timeout (the meter's overwrite prompt waits up to 30 s). Test hooks: corrupt = block numbers whose first
    transmission gets a bad CRC; stop_after = stop writing after that many data blocks (a cut link)."""
    if _wait_for(link, (C,), 60.0, log) is None:
        raise TransferError("no 'C' from the meter")
    for entry in list(files) + [None]:
        name, data, mtime = entry if entry else (None, b"", 0)
        if name is None:
            head = b""
        else:
            head = name.encode("ascii") + b"\0" + str(len(data)).encode() + b" " + format(int(mtime), "o").encode()
        b0 = _block(0, head, 128, pad=0)           # block 0 is NUL-padded
        for attempt in range(10):
            link.write(b0)
            r = _wait_for(link, (ACK, NAK), ack0_timeout, log)
            if r == ACK:
                break
            log(f"block 0: {'NAK' if r == NAK else 'timeout'}, again")
        else:
            raise TransferError("block 0 not acknowledged")
        if name is None:
            return True
        if _wait_for(link, (C,), 10.0, log) is None:
            raise TransferError("no 'C' after block 0")
        pos, num, sent = 0, 1, 0
        while pos < len(data):
            size = 1024 if one_k and len(data) - pos > 128 else 128
            blk = _block(num, data[pos:pos + size], size)
            for attempt in range(10):
                if stop_after is not None and sent >= stop_after:
                    log(f"test: link cut after {sent} data blocks")
                    return False
                if num in corrupt and attempt == 0:
                    bad = bytearray(blk)
                    bad[-1] ^= 0xFF
                    link.write(bytes(bad))
                    log(f"test: block {num} sent with a bad CRC")
                else:
                    link.write(blk)
                r = _wait_for(link, (ACK, NAK), ack_timeout, log)
                if r == ACK:
                    break
                log(f"block {num}: {'NAK' if r == NAK else 'timeout'}, again")
            else:
                raise TransferError(f"block {num} not acknowledged")
            pos += size
            num += 1
            sent += 1
        for attempt in range(10):
            link.write(bytes([EOT]))
            r = _wait_for(link, (ACK, NAK), ack_timeout, log)
            if r == ACK:
                break
        else:
            raise TransferError("EOT not acknowledged")
        if _wait_for(link, (C,), 15.0, log) is None:
            raise TransferError("no 'C' after the file")
        log(f"sent {name}: {len(data)} bytes in {num - 1} blocks")
    return True


def ymodem_recv(link, log=print, c_interval=5.0, block_timeout=20.0):
    """Download one YMODEM batch: [(name, data, mtime or None)]."""
    files = []
    while True:
        # block 0 (request it with 'C')
        got = None
        for attempt in range(12):
            link.write(bytes([C]))
            got = _read_block(link, c_interval if attempt < 11 else block_timeout, log)
            if got and got[0] == 0:
                break
            if got == "eot":
                link.write(bytes([ACK]))
        if not got or got[0] != 0:
            raise TransferError("no block 0")
        info = got[1]
        link.write(bytes([ACK]))
        name = info.split(b"\0", 1)[0].decode("ascii", "replace")
        if not name:
            return files
        fields = info.split(b"\0", 1)[1].split(b"\0", 1)[0].split()
        size = int(fields[0]) if fields else None
        mtime = int(fields[1], 8) if len(fields) > 1 else None
        data, expect, errors = bytearray(), 1, 0
        t0, next_log = link.now(), 8192
        link.write(bytes([C]))
        eots = 0
        while True:
            got = _read_block(link, block_timeout, log)
            if got == "eot":
                if eots == 0:
                    eots = 1
                    link.write(bytes([NAK]))
                    continue
                link.write(bytes([ACK]))
                break
            if got is None or got == "bad":
                errors += 1
                log(f"block {expect & 0xFF} ({len(data)} bytes in): {'timeout' if got is None else 'bad block'}, NAK {errors}")
                if errors > 10:
                    link.write(bytes([CAN] * 3))
                    raise TransferError("too many errors")
                link.purge(1.0)
                link.write(bytes([NAK]))
                continue
            num, payload = got
            if num == expect & 0xFF:
                data += payload
                expect += 1
                errors = 0
                if len(data) >= next_log:
                    dt = max(link.now() - t0, 0.001)
                    log(f"{len(data)} bytes" + (f" of {size}" if size else "") + f", {len(data) / dt:.0f} B/s")
                    next_log += 8192
                link.write(bytes([ACK]))
            elif num == (expect - 1) & 0xFF:
                link.write(bytes([ACK]) + (bytes([C]) if expect == 1 else b""))   # our ACK was lost (block 0 again: C too; not block 256)
            else:
                link.write(bytes([CAN] * 3))
                raise TransferError(f"block {num} out of sequence (expected {expect & 0xFF})")
        if size is not None:
            data = data[:size]
        files.append((name, bytes(data), mtime))
        log(f"received {name}: {len(data)} bytes")


def _read_block(link, timeout, log):
    """(num, data), 'eot', 'bad' or None (timeout)."""
    end = link.now() + timeout
    cans = 0
    while link.now() < end:
        c = link.getc(end - link.now())
        if c is None:
            return None
        if c == EOT:
            return "eot"
        if c == CAN:
            cans += 1
            if cans >= 2:
                raise TransferError("canceled by the meter (CAN CAN)")
            continue
        cans = 0
        if c in (SOH, STX):
            size = 128 if c == SOH else 1024
            # the meter paces its output (16 bytes per 100 ms by default): a 1 KB block takes ~6.5 s on the link
            rest = link.read_exact(size + 4, 3.0 + size / 60)
            if len(rest) < size + 4 or rest[0] ^ rest[1] != 0xFF or crc16(rest[2:-2]) != (rest[-2] << 8 | rest[-1]):
                why = (f"short: {len(rest)} of {size + 4} bytes" if len(rest) < size + 4 else
                       f"block number {rest[0]:02x}/{rest[1]:02x}" if rest[0] ^ rest[1] != 0xFF else
                       f"CRC {crc16(rest[2:-2]):04x} computed, {rest[-2] << 8 | rest[-1]:04x} sent")
                log(f"bad {size}-byte block: {why}")
                dump = os.environ.get("YMODEM_DUMP")
                if dump:
                    with open(os.path.join(dump, f"bad-{int(time.time() * 1000)}.bin"), "wb") as f:
                        f.write(bytes([c]) + bytes(rest))
                return "bad"
            return rest[0], rest[2:-2]
    return None


# ---- pty bridge for lrzsz ---------------------------------------------------------------------------------------

class Block0Filter:
    """sz -> meter: forward bytes, but hold back a repeat of block 0 until the meter has answered the first one
    (sz repeats it after 10 s; the meter's overwrite prompt may take 30 s)."""
    def __init__(self, log):
        self.pending, self.acc, self.need, self.last0, self.awaiting, self.log = bytearray(), None, 0, None, False, log

    def from_meter(self, b):
        if self.awaiting and (ACK in b or NAK in b or CAN in b):
            self.awaiting = False

    def from_sz(self, b):
        out = bytearray()
        for c in b:
            if self.acc is not None:
                self.acc.append(c)
                if len(self.acc) == self.need:
                    blk, self.acc = bytes(self.acc), None
                    if blk[1] == 0 and self.awaiting and blk == self.last0:
                        self.log("pty: holding back a repeat of block 0 (meter prompt)")
                        continue
                    if blk[1] == 0:
                        self.last0, self.awaiting = blk, True
                    out += blk
                continue
            if c in (SOH, STX):
                self.acc, self.need = bytearray([c]), (133 if c == SOH else 1029)
                continue
            out.append(c)
        return bytes(out)


def pty_bridge(link, run=None, filt=None, log=print, idle_end=None):
    """Copy bytes between the link and a new pty until `run` exits (or, without run, until the link has been idle
    for idle_end seconds after the first byte, or Ctrl-C)."""
    import pty, tty
    master, slave = pty.openpty()
    tty.setraw(slave)
    name = os.ttyname(slave)
    proc = None
    if run:
        import tempfile
        errf = tempfile.TemporaryFile()
        proc = subprocess.Popen(run, shell=False, stdin=slave, stdout=slave, stderr=errf,
                                cwd=os.environ.get("YMODEM_PTY_CWD") or None)
        log(f"pty {name}: running {' '.join(run)}")
    else:
        log(f"pty {name}: run e.g. sz --ymodem -k FILE <{name} >{name}")
    os.set_blocking(master, False)
    last = link.now()
    try:
        while True:
            try:
                b = os.read(master, 4096)
            except BlockingIOError:
                b = b""
            except OSError:
                b = b""
            if b:
                if filt:
                    b = filt.from_sz(b)
                if b:
                    link.write(b)
                last = link.now()
            m = link.read_some(0.01)
            if m:
                if filt:
                    filt.from_meter(m)
                os.write(master, m)
                last = link.now()
            if proc and proc.poll() is not None:
                # forward what the program wrote last (e.g. its final ACK), give it time to reach the meter
                end = link.now() + 1.0
                while link.now() < end:
                    try:
                        b = os.read(master, 4096)
                    except OSError:
                        b = b""
                    if b:
                        link.write(filt.from_sz(b) if filt else b)
                    link.read_some(0.01)
                errf.seek(0)
                err = errf.read().decode(errors="replace").replace("\r", "\n").strip()
                log(f"pty: {run[0]} exited with {proc.returncode}" + (f": {err[-300:]}" if err else ""))
                return proc.returncode == 0
            if not proc and idle_end and link.now() - last > idle_end:
                return True
    finally:
        os.close(master)
        os.close(slave)


# ---- command line --------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--address"); ap.add_argument("--name", default="121GW")
    ap.add_argument("--scan-seconds", type=float, default=30)
    sub = ap.add_subparsers(dest="op", required=True)
    sub.add_parser("info"); sub.add_parser("list")
    p = sub.add_parser("put"); p.add_argument("file"); p.add_argument("--as", dest="as_name"); p.add_argument("--128", dest="small", action="store_true")
    p = sub.add_parser("get"); p.add_argument("remote"); p.add_argument("-o", dest="out"); p.add_argument("--1k", dest="one_k", action="store_true")
    p.add_argument("--pace", type=int, default=0)
    p.add_argument("--gap", type=int, default=0, choices=range(8))
    p = sub.add_parser("pty-put"); p.add_argument("--run")
    p = sub.add_parser("pty-get"); p.add_argument("remote"); p.add_argument("--run"); p.add_argument("--1k", dest="one_k", action="store_true")
    p.add_argument("--pace", type=int, default=0)
    p.add_argument("--gap", type=int, default=0, choices=range(8))
    a = ap.parse_args()
    log = lambda s: print(f"{time.strftime('%H:%M:%S')} {s}", flush=True)
    tr = BleTransport(a.address, a.name, a.scan_seconds, log)
    link = Link(tr)
    meter = Meter(link, log)
    ok = True
    try:
        if a.op == "info":
            st, p = meter.info()
            log(f"info: {STATUS.get(st, st)} {p.hex(' ')}")
            if len(p) >= 19:
                log(f"last cancel: {decode_info(p)}")
        elif a.op == "list":
            for name, size, attr, when in meter.list():
                print(f"{name:12s} {size:10d}  {when}{'  <DIR>' if attr & 0x10 else ''}")
        elif a.op == "put":
            data = open(a.file, "rb").read()
            name = a.as_name or os.path.basename(a.file)
            meter.start_receive()
            log(f"meter receiving; {name}: {len(data)} bytes (a prompt on the meter needs SETUP within 30 s "
                "if the file exists or is EEVBlog.bin)")
            ok = ymodem_send(link, [(name, data, os.path.getmtime(a.file))], one_k=not a.small, log=log)
        elif a.op == "get":
            size = meter.start_send(a.remote, a.one_k, a.pace, a.gap)
            log(f"meter sending {a.remote}: {size} bytes")
            for name, data, mtime in ymodem_recv(link, log):
                out = a.out or name
                tmp = out + ".part"
                with open(tmp, "wb") as f:
                    f.write(data); f.flush(); os.fsync(f.fileno())
                os.replace(tmp, out)
                if mtime:
                    os.utime(out, (mtime, mtime))
                log(f"wrote {out}")
        elif a.op == "pty-put":
            meter.start_receive()
            ok = pty_bridge(link, a.run.split() if a.run else None, Block0Filter(log), log, idle_end=60)
        elif a.op == "pty-get":
            meter.start_send(a.remote, a.one_k, a.pace, a.gap)
            ok = pty_bridge(link, a.run.split() if a.run else None, None, log, idle_end=60)
    except (TransferError, KeyboardInterrupt) as e:
        log(f"failed: {e}")
        ok = False
        if a.op != "info" and not isinstance(e, KeyboardInterrupt):
            try:                        # the meter ignores commands for 1 s after a transfer
                link.sleep(1.5)
                st, p = meter.info()
                log(f"meter's last cancel: {decode_info(p)}")
            except Exception as e2:
                log(f"no INFO after the failure: {e2}")
    finally:
        tr.close()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
