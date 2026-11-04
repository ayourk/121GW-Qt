# SPDX-License-Identifier: MIT
"""STM32L1 SDIO controller + DMA2 channel 4 + an SDHC card, register level, for the 121GW emulator.

The stock firmware uses ST's StdPeriph sdio_sd.c (only the parts it links): CMD0, CMD8, CMD55+ACMD41, CMD2, CMD3,
CMD9, CMD7, CMD55+ACMD51 (SCR, FIFO polling), CMD55+ACMD6 (bus width), CMD16+CMD17 (single-block read, DMA),
CMD16+CMD55+ACMD23+CMD25+CMD12 (multi-block write, DMA), CMD13. There is no card-detect pin (FUN_080226d6 returns
1), so the card is always present. Everything here is in the SD Association's Simplified Physical Layer spec.

Model:
- A write to SDIO.CMD with CPSMEN (bit 10) answers at once: RESPCMD/RESP1..4 are stored, STA gets CMDSENT (no
  response) or CMDREND; unknown commands get CTIMEOUT.
- A read command (CMD17, ACMD51) queues its data in the receive FIFO when DCTRL.DTEN is set. FIFO reads pop words
  (bytes in card order, little-endian words, as the driver's byte swaps expect). When the FIFO empties, DATAEND and
  DBCKEND are set.
- Enabling DMA2 channel 4 (CCR4.EN) with DIR = 0 drains the FIFO to CMAR; with DIR = 1 it copies CNDTR words from
  CMAR to the card at the pending CMD24/CMD25 address. Both set DMA2 TCIF4/GIF4 and SDIO DATAEND/DBCKEND and raise
  the SDIO (45) and DMA2_Channel4 (53) interrupts; the emulator stops the slice and calls them right away (the
  driver's completion wait polls only ~1000 times).
- The card image is a bytearray (block addressing, SDHC): writes stay in memory unless the caller saves them.
- DMA addressing as on the STM32 (RM0038 DMA section): CNDTR counts data items, MSIZE (CCR bits 11:10) / PSIZE
  (bits 9:8) give the memory / peripheral item size (8, 16, 32 bit), and a 16-bit or 32-bit memory access ignores
  the low address bits of CMAR (CMAR & ~1, CMAR & ~3); MINC (bit 7) steps the memory address by one item. Items
  narrower than the other side are zero-extended, wider ones truncated to their low bytes. A transfer that starts at
  an unaligned CMAR is noted in the log ("DMA ... unaligned CMAR"). dma_exact = True restores the earlier
  simplification: 4 x CNDTR bytes copied byte-exact from or to CMAR (--dma-exact). Aligned 32/32-bit transfers with
  MINC, the only kind stock 2.05 makes, move the same bytes either way.

Options (all off by default; without them the model behaves exactly as before):
- card_type: "sdhc" (default: SD 2.0 high capacity, CSD 2.0, block addresses), "sdsc" (SD 2.0 standard capacity:
  CMD8 answered, ACMD41 without CCS, CSD 1.0, byte addresses), "sdv1" (SD 1.x: CMD8 times out, CSD 1.0, byte
  addresses), "mmc" (CMD8 and CMD55 time out, as an MMC card does).
- scr: the 8-byte SCR ACMD51 returns (default 0235800000000000: 1-bit and 4-bit bus).
- faults: list of fault specs (parse_fault), each "WHAT[@N|@N-M|@N+]:KIND". WHAT is a command (CMD13, ACMD41, ...;
  counted separately, from 1 at power-on of the model) or a data phase: READ (CMD17/18 data), WRITE (the DMA write
  of CMD24/25), SCR (ACMD51 data). Without @ the fault applies to every occurrence. Command KINDs:
    timeout     CTIMEOUT, no response (as a card that ignores the command)
    silent      no status flag at all (the driver's poll loop runs out)
    crc         the response arrives with CCRCFAIL instead of CMDREND
    respcmd=N   RESPCMD reads N (wrong response index)
    r1=HEX      OR these bits into the short response (R1 card status, R6, R3/OCR)
    resp=HEX    replace the short response (e.g. CMD13 resp=0xE00 = card busy in the prg state)
  Data KINDs (the status flags the transfer ends with instead of DATAEND; data is not moved):
    dtimeout (DTIMEOUT), dcrc (DCRCFAIL), rxoverr (RXOVERR), txunderr (TXUNDERR), stbiterr (STBITERR),
    nodata (no flag, no data: polling loops run out), dflags=HEX (any STA flag set)
  A data fault raises the SDIO interrupt when an enabled MASK bit is among the flags.
  Further selectors (each after its own @, combinable; with any of them the occurrence numbers N count only the
  occurrences that pass the other selectors, per fault):
    tT0-T1 / tT0+   only while the emulated time is in [T0, T1] ms (or from T0 on)
    bLBA            only commands / data phases on this card block: CMD17/18/24/25 (argument), READ (the block
                    read), WRITE (any block of the multi-block write); e.g. READ@b8226:dcrc = every read of the
                    FAT sector at 8226 fails, READ@b8226@2:dcrc only its second read, WRITE@t9000+@1:dtimeout the
                    first data write from 9 s on
- removal (remove() / insert()): a card that is pulled out answers nothing (every command CTIMEOUT, as the SDIO
  host flags a missing response); a card pushed back in (the same contents or another image) has gone through a
  power cycle: until CMD3 it answers only the identification commands (CMD0, CMD8, CMD55, ACMD41, CMD2, CMD3) and
  ignores the rest (CTIMEOUT), as the spec's idle/ready/ident states do; its bus is 1 bit wide again.
"""
import struct

SDIO = 0x40012C00
POWER, CLKCR, ARG, CMD, RESPCMD, RESP1, DTIMER, DLEN, DCTRL, DCOUNT, STA, ICR, MASK, FIFOCNT = (
    0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x48)
FIFO_LO, FIFO_HI = 0x80, 0xFC
DMA2 = 0x40026400
DMA2_ISR, DMA2_IFCR = DMA2 + 0x00, DMA2 + 0x04
CCR4, CNDTR4, CPAR4, CMAR4 = DMA2 + 0x44, DMA2 + 0x48, DMA2 + 0x4C, DMA2 + 0x50
IRQ_SDIO, IRQ_DMA2_CH4 = 45, 53
# STA bits
CCRCFAIL, CTIMEOUT, CMDREND, CMDSENT, DATAEND, DBCKEND = 1 << 0, 1 << 2, 1 << 6, 1 << 7, 1 << 8, 1 << 10
DCRCFAIL, DTIMEOUT, TXUNDERR, RXOVERR, STBITERR = 1 << 1, 1 << 3, 1 << 4, 1 << 5, 1 << 9
TXFIFOHE, RXFIFOHF, TXFIFOE, RXFIFOE, TXDAVL, RXDAVL = 1 << 14, 1 << 15, 1 << 18, 1 << 19, 1 << 20, 1 << 21
STATIC_MASK = 0x7FF | (1 << 22)                  # flags cleared through ICR
RCA = 0x1234
CID = bytes.fromhex("035344534430384780123456780141")   # manufacturer 03, "SD", "SD08G", rev 8.0, serial, 2020-01
SCR = bytes.fromhex("0235800000000000")                 # SD spec 2.0, CMD_SUPPORT 0, bus widths 1 + 4 bit


def csd_v2(blocks):
    """CSD version 2.0 (SDHC) as a 128-bit integer: capacity = (C_SIZE + 1) x 512 KiB."""
    c_size = blocks // 1024 - 1
    f = [(127, 2, 1), (119, 8, 0x0E), (111, 8, 0), (103, 8, 0x32), (95, 12, 0x5B5), (83, 4, 9), (69, 22, c_size),
         (46, 1, 1), (45, 7, 0x7F), (28, 3, 2), (25, 4, 9), (14, 1, 1), (0, 1, 1)]
    v = 0
    for top, width, val in f:
        v |= (val & ((1 << width) - 1)) << (top - width + 1)
    return v


def csd_v1(blocks):
    """CSD version 1.0 (SDSC) as a 128-bit integer: capacity = (C_SIZE + 1) x 2^(C_SIZE_MULT + 2) x 2^READ_BL_LEN."""
    nbytes = blocks * 512
    for bl_len in (9, 10, 11):
        c_size = nbytes // ((1 << (7 + 2)) << bl_len) - 1
        if c_size <= 0xFFF: break
    c_size = max(0, min(c_size, 0xFFF))
    f = [(127, 2, 0), (119, 8, 0x26), (111, 8, 0), (103, 8, 0x32), (95, 12, 0x5F5), (83, 4, bl_len), (79, 1, 1), (73, 12, c_size),
         (61, 3, 6), (58, 3, 6), (55, 3, 6), (52, 3, 6), (49, 3, 7), (46, 1, 1), (45, 7, 0x7F), (28, 3, 2),
         (25, 4, 9), (14, 1, 1), (0, 1, 1)]
    v = 0
    for top, width, val in f:
        v |= (val & ((1 << width) - 1)) << (top - width + 1)
    return v


DATA_KINDS = {"dtimeout": DTIMEOUT, "dcrc": DCRCFAIL, "rxoverr": RXOVERR, "txunderr": TXUNDERR, "stbiterr": STBITERR,
              "nodata": 0}


def parse_fault(spec):
    """'WHAT[@N|@N-M|@N+]:KIND[=VAL]' -> dict(what, lo, hi, kind, val); see the module docstring."""
    what, _, kind = spec.partition(":")
    lo, hi = 1, None                              # every occurrence
    t0 = t1 = blk = None
    if "@" in what:
        what, *sels = what.split("@")
        for occ in sels:
            if occ[:1] == "t":
                if occ.endswith("+"): t0 = float(occ[1:-1])
                else: t0, t1 = (float(x) for x in occ[1:].split("-", 1))
            elif occ[:1] == "b": blk = int(occ[1:], 0)
            elif occ.endswith("+"): lo, hi = int(occ[:-1]), None
            elif "-" in occ: lo, hi = (int(x) for x in occ.split("-", 1))
            else: lo = hi = int(occ)
    what = what.upper()
    kind, _, val = kind.partition("=")
    kind = kind.lower()
    data = what in ("READ", "WRITE", "SCR")
    ok = (kind in DATA_KINDS or kind == "dflags") if data else kind in ("timeout", "silent", "crc", "respcmd", "r1",
                                                                         "resp")
    if not ok or not (data or what.startswith(("CMD", "ACMD"))):
        raise ValueError(f"bad SD fault spec {spec!r}")
    v = int(val, 0) if val else 0
    if data and kind != "dflags": v = DATA_KINDS[kind]
    f = {"what": what, "lo": lo, "hi": hi, "kind": kind, "val": v, "spec": spec}
    if t0 is not None or blk is not None: f.update(t0=t0, t1=t1, blk=blk, n=0)   # filtered: own occurrence count
    return f


class SDCard:
    def __init__(self, image=None, size_mb=64, log=None, virtual_mb=None, place=None, card_type="sdhc", scr=None,
                 faults=None):
        """image: the card from block 0 (bytearray, saved by --sd-out). virtual_mb: report a larger card (CSD); blocks
        past the image are sparse (zeros until written). place: {lba: bytes} preloads sparse blocks (e.g. a partition
        beyond 4 GiB). card_type, scr, faults: see the module docstring (defaults = an SDHC card with no faults)."""
        if card_type not in ("sdhc", "sdsc", "sdv1", "mmc"): raise ValueError(f"bad SD card type {card_type!r}")
        self.card_type, self.hc = card_type, card_type == "sdhc"
        self.scr = bytes(scr) if scr is not None else SCR
        self.faults = [parse_fault(f) if isinstance(f, str) else f for f in (faults or [])]
        self.count = {}                           # occurrences per command / data phase (for faults)
        self.data_fault = 0                       # pending data-phase flags (READ via DMA)
        self.late_flags = 0                       # pending data-phase flags (SCR): set at the first STA read after ICR
        self.late_armed = False
        self.data = bytearray(image) if image is not None else bytearray(size_mb << 20)
        if len(self.data) % (512 * 1024): self.data.extend(bytes(512 * 1024 - len(self.data) % (512 * 1024)))
        self.blocks = max(len(self.data) // 512, (virtual_mb or 0) * 2048)
        self.sparse = {}
        for lba, b in (place or {}).items():
            for i in range(0, len(b), 512): self.sparse[lba + i // 512] = bytes(b[i:i + 512]).ljust(512, b"\0")
        self.touched = set()                      # blocks the firmware read or wrote
        self.sta = 0
        self.rx = bytearray()
        self.app = False                          # the previous command was CMD55
        self.pending_write = None                 # block address of CMD24/CMD25
        self.irq = set()
        self.log = log if log is not None else []
        self.reads = self.writes = 0
        self.bus_width = 1
        self.present, self.uninit = True, False   # remove() / insert(): card out, card fresh after a power cycle
        self.dma_exact = False                    # True: old byte-exact DMA from CMAR (emu_run --dma-exact)

    # helpers the emulator provides: rd32(addr), wr32(addr, v), mem_read(addr, n), mem_write(addr, bytes), now()
    def attach(self, rd32, wr32, mem_read, mem_write, now):
        self.rd32, self.wr32, self.mem_read, self.mem_write, self.now = rd32, wr32, mem_read, mem_write, now

    def note(self, s): self.log.append((self.now(), s))

    def fault(self, what, lba=None, nblk=1):
        """Count one occurrence of `what` (on blocks lba .. lba+nblk-1 when known) and return the fault that applies
        to it (or None)."""
        if not self.faults: return None
        n = self.count[what] = self.count.get(what, 0) + 1
        hit = None
        for f in self.faults:
            if f["what"] != what: continue
            k = n
            if "n" in f:                              # filtered fault: count only the occurrences that pass
                if f["t0"] is not None and not (f["t0"] <= self.now() and (f["t1"] is None or self.now() <= f["t1"])): continue
                if f["blk"] is not None and (lba is None or not lba <= f["blk"] < lba + nblk): continue
                f["n"] += 1; k = f["n"]
            if hit is None and k >= f["lo"] and (f["hi"] is None or k <= f["hi"]):
                self.note(f"  fault {f['spec']} (occurrence {k})"); hit = f
        return hit

    def remove(self):
        """The card is pulled out: nothing answers any more."""
        self.present = False; self.rx = bytearray(); self.pending_write = None; self.note("card removed")

    def insert(self, image=None):
        """A card is pushed in (this one again, or a new image): powered up fresh, in the idle state."""
        if image is not None:
            self.data = bytearray(image)
            if len(self.data) % (512 * 1024): self.data.extend(bytes(512 * 1024 - len(self.data) % (512 * 1024)))
            self.blocks, self.sparse = len(self.data) // 512, {}
        self.present, self.uninit, self.app, self.bus_width, self.pending_write = True, True, False, 1, None
        self.rx = bytearray(); self.note("card inserted")

    def respond(self, idx, short=None, long=None):
        self.wr32(SDIO + RESPCMD, idx)
        if long is not None:
            for i in range(4): self.wr32(SDIO + RESP1 + 4 * i, (long >> (96 - 32 * i)) & 0xFFFFFFFF)
        else:
            self.wr32(SDIO + RESP1, short or 0)
        self.sta |= CMDREND

    def command(self, v):
        idx, waitresp = v & 0x3F, (v >> 6) & 3
        arg = self.rd32(SDIO + ARG)
        app, self.app = self.app, False
        name = f"{'A' if app and idx != 55 else ''}CMD{idx}"
        self.note(f"{name} arg {arg:08x}")
        R1 = 0x900                                # state tran, ready for data
        if not self.present or (self.uninit and idx not in (0, 8, 55, 41, 2, 3)):
            if waitresp: self.sta |= CTIMEOUT     # no card / not in the transfer states: no response
            else: self.sta |= CMDSENT
            if idx == 0: self.pending_write = None
            self.note("  no response (card " + ("out)" if not self.present else "not initialized)")); return
        if waitresp == 0:
            self.sta |= CMDSENT
            if idx == 0: self.pending_write = None
            return
        f = self.fault(name, self.lba(arg) if idx in (17, 18, 24, 25) else None)
        if f and f["kind"] in ("timeout", "silent"):
            if f["kind"] == "timeout": self.sta |= CTIMEOUT
            return
        if self.card_type != "sdhc" and self.type_command(idx, app, arg):
            pass
        else:
            self.answer(idx, app, arg, name, R1)
        if f:
            if f["kind"] == "crc": self.sta = (self.sta & ~CMDREND) | CCRCFAIL
            elif f["kind"] == "respcmd": self.wr32(SDIO + RESPCMD, f["val"])
            elif f["kind"] == "r1": self.wr32(SDIO + RESP1, self.rd32(SDIO + RESP1) | f["val"])
            elif f["kind"] == "resp": self.wr32(SDIO + RESP1, f["val"])

    def type_command(self, idx, app, arg):
        """Answers that differ for the non-SDHC card types; True when handled here."""
        if idx == 8 and self.card_type in ("sdv1", "mmc"):
            self.note("  CMD8: no answer (SD 1.x / MMC)"); self.sta |= CTIMEOUT; return True
        if idx == 55 and self.card_type == "mmc":
            self.note("  CMD55: no answer (MMC)"); self.sta |= CTIMEOUT; return True
        if app and idx == 41:
            self.respond(0x3F, 0x80FF8000); return True   # powered up, no CCS: standard capacity
        if idx == 9:
            self.respond(0x3F, long=csd_v1(self.blocks)); return True
        return False

    def lba(self, arg):
        """Block number of a data command's argument: SDHC cards take blocks, standard capacity cards bytes."""
        return arg if self.hc else arg // 512

    def answer(self, idx, app, arg, name, R1):
        if idx == 55:
            self.app = True; self.respond(55, 0x920)
        elif idx == 8:
            self.respond(8, arg & 0xFFF)
        elif app and idx == 41:
            self.respond(0x3F, 0xC0FF8000)        # powered up, CCS (SDHC), 2.7-3.6 V
        elif idx == 2:
            self.respond(0x3F, long=int.from_bytes(CID + b"\x01", "big"))
        elif idx == 3:
            self.respond(3, RCA << 16 | 0x0500); self.uninit = False
        elif idx == 9:
            self.respond(0x3F, long=csd_v2(self.blocks))
        elif idx == 10:
            self.respond(0x3F, long=int.from_bytes(CID + b"\x01", "big"))
        elif idx == 7:
            self.respond(7, 0x700)
        elif idx == 13:
            self.respond(13, R1)
        elif app and idx == 6:
            self.bus_width = 4 if arg & 3 == 2 else 1; self.respond(6, 0x920)
        elif app and idx == 51:
            self.respond(51, 0x920); self.queue(self.scr, "SCR")
        elif app and idx == 23:
            self.respond(23, 0x920)
        elif idx in (16, 12):
            self.respond(idx, R1)
        elif idx in (17, 18):
            self.respond(idx, R1)
            n = max(self.rd32(SDIO + DLEN) & 0x1FFFFFF, 512)
            lba = self.lba(arg)
            if lba + n // 512 > self.blocks:
                self.note(f"  read past the end: block {lba}"); n = 0
            if self.queue(self.get(lba, n // 512), "READ", lba, max(1, n // 512)): self.reads += n // 512
        elif idx in (24, 25):
            self.respond(idx, R1); self.pending_write = self.lba(arg)
        else:
            self.note(f"  {name}: not modeled -> timeout"); self.sta |= CTIMEOUT

    def get(self, lba, n):
        out = bytearray()
        for k in range(lba, lba + n):
            self.touched.add(k)
            out += self.data[k * 512:(k + 1) * 512] if (k + 1) * 512 <= len(self.data) else self.sparse.get(k, bytes(512))
        return bytes(out)

    def put(self, lba, b):
        for i in range(0, len(b), 512):
            k = lba + i // 512; self.touched.add(k)
            if (k + 1) * 512 <= len(self.data): self.data[k * 512:(k + 1) * 512] = b[i:i + 512]
            else: self.sparse[k] = bytes(b[i:i + 512])

    def queue(self, b, what=None, lba=None, nblk=1):
        """Data for a read command into the receive FIFO; False when a data fault replaced it."""
        if self.rd32(SDIO + DCTRL) & 1:           # DTEN: data path armed
            f = self.fault(what, lba, nblk) if what else None
            if f:
                self.rx = bytearray()
                if what == "READ": self.data_fault = f["val"] | (1 << 31)   # flags appear when the DMA starts
                else: self.late_flags = f["val"] | (1 << 31)  # after the driver clears the command flags
                return False
            self.rx = bytearray(b)
            if not self.rx: self.sta |= DATAEND | DBCKEND
        return True

    def status(self):
        s = self.sta & ~(RXDAVL | RXFIFOE | RXFIFOHF | TXFIFOE | TXFIFOHE)
        s |= (RXDAVL | (RXFIFOHF if len(self.rx) >= 32 else 0)) if self.rx else RXFIFOE
        s |= TXFIFOE | TXFIFOHE
        return s

    def fifo_read(self):
        w = bytes(self.rx[:4]).ljust(4, b"\0"); del self.rx[:4]
        if not self.rx: self.sta |= DATAEND | DBCKEND
        return struct.unpack("<I", w)[0]

    def dma_geometry(self, ccr, n, mar):
        """(item size in memory, item size at the SDIO FIFO, aligned start address, memory step) for CCR / CMAR."""
        msz, psz = (1, 2, 4, 4)[ccr >> 10 & 3], (1, 2, 4, 4)[ccr >> 8 & 3]
        al = mar & ~(msz - 1)
        if al != mar:
            self.note(f"  DMA {'write' if ccr >> 4 & 1 else 'read'}: unaligned CMAR {mar:08x} (MSIZE {8 * msz} bit): "
                      f"the hardware uses {al:08x}")
        return msz, psz, al, msz if ccr >> 7 & 1 else 0

    def dma_mem_read(self, ccr, n, mar):
        """The byte stream the DMA hands the SDIO FIFO for n items (memory -> peripheral)."""
        if self.dma_exact: return bytes(self.mem_read(mar, 4 * n))
        msz, psz, al, step = self.dma_geometry(ccr, n, mar)
        if msz == psz and step: return bytes(self.mem_read(al, msz * n))
        out = bytearray()
        for i in range(n):
            item = bytes(self.mem_read(al + i * step, msz))
            out += item[:psz].ljust(psz, b"\0")
        return bytes(out)

    def dma_mem_write(self, ccr, n, mar, take):
        """Move n items from the FIFO byte stream (take(k) -> k bytes) to memory (peripheral -> memory)."""
        if self.dma_exact:
            b = take(4 * n); self.mem_write(mar, b); return len(b)
        msz, psz, al, step = self.dma_geometry(ccr, n, mar)
        if msz == psz and step:
            b = take(msz * n); self.mem_write(al, b); return len(b)
        moved = 0
        for i in range(n):
            item = take(psz)
            if not item: break
            self.mem_write(al + i * step, item[:msz].ljust(msz, b"\0")); moved += len(item)
        return moved

    def dma_enable(self, ccr):
        n, mar = self.rd32(CNDTR4) & 0xFFFF, self.rd32(CMAR4)
        if ccr >> 4 & 1:                          # memory -> SDIO: write to the card
            b = self.dma_mem_read(ccr, n, mar)
            if self.pending_write is None:
                self.note("  DMA write with no CMD24/25 pending"); return
            f = self.fault("WRITE", self.pending_write, max(1, len(b) // 512))
            if f:
                self.pending_write = None; self.data_error(f["val"]); return
            a = self.pending_write
            if a + len(b) // 512 > self.blocks: self.note(f"  write past the end: block {a}"); return
            self.put(a, b); self.writes += len(b) // 512
            self.note(f"  wrote {len(b) // 512} block(s) at {a}")
            self.pending_write = None
        else:                                     # SDIO -> memory: drain the receive FIFO
            if self.data_fault:
                flags, self.data_fault = self.data_fault & 0x7FFFFFFF, 0
                self.data_error(flags); return
            def take(k):
                b = bytes(self.rx[:k]); del self.rx[:k]; return b
            got = self.dma_mem_write(ccr, n, mar, take)
            self.note(f"  read {got} bytes to {mar:08x}")
        self.wr32(CNDTR4, 0)
        self.wr32(DMA2_ISR, self.rd32(DMA2_ISR) | (1 << 13) | (1 << 12))
        self.sta |= DATAEND | DBCKEND
        if ccr & 2: self.irq.add(IRQ_DMA2_CH4)
        if self.rd32(SDIO + MASK) & (DATAEND | 0x22A): self.irq.add(IRQ_SDIO)

    def data_error(self, flags):
        """End a data transfer with these STA flags instead of DATAEND (no DMA completion)."""
        self.sta |= flags
        self.note(f"  data phase ends with STA flags {flags:#x}")
        if self.rd32(SDIO + MASK) & flags: self.irq.add(IRQ_SDIO)

    # hooks: return True when the access was handled (reads: the register value was stored first)
    def on_read(self, x):
        if x == SDIO + STA:
            if self.late_flags and self.late_armed:
                self.sta |= self.late_flags & 0x7FFFFFFF; self.late_flags, self.late_armed = 0, False
            self.wr32(x, self.status()); return True
        if SDIO + FIFO_LO <= x <= SDIO + FIFO_HI: self.wr32(x, self.fifo_read()); return True
        if x == SDIO + FIFOCNT: self.wr32(x, (len(self.rx) + 3) // 4); return True
        return False

    def on_write(self, x, v):
        if x == SDIO + CMD and v & (1 << 10): self.command(v); return True
        if x == SDIO + ICR:
            self.sta &= ~(v & STATIC_MASK)
            if self.late_flags: self.late_armed = True
            return True
        if x == CCR4 and v & 1 and not self.rd32(CCR4) & 1:   # the hook runs before the store: use the new value
            self.dma_enable(v); return True
        if x == DMA2_IFCR: self.wr32(DMA2_ISR, self.rd32(DMA2_ISR) & ~v); return True
        if x == SDIO + POWER and v & 3 == 0: self.note("power off")
        return False
