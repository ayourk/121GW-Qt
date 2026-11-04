#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
# Runs the stock 121GW image in Unicorn past the boot screen: executes in 1 ms slices (4194 instructions at the
# 4.194304 MHz MSI clock), and between slices calls TIM3_IRQHandler every slice and TIM2_IRQHandler every 10th, but
# only once the firmware has armed them (CR1.CEN and DIER.UIE). Handlers are called as functions (context saved,
# return trapped, context restored): on Cortex-M3 they are plain AAPCS functions, so this is faithful for a trace.
# Inputs: rotary + keys on GPIOG.IDR (active low; --rotary picks the position, keys released), jack sense PE10/11
# high (no current jack). Hardware "ready" flags are faked as in emu_reset_trace.py. Prints milestones, then the
# peripheral writes after the boot screen (or all with --all).
# Optional hardware models (off by default; a run without them is unchanged): SD card types and fault injection
# (--sd-type, --sd-fault, --sd-scr; details in sdcard_model.py), NVIC enable registers with set/clear semantics
# (--nvic, so the SDIO interrupt is taken), EXTI edge wake from STOP with the EXTI handlers (--exti-irq), scripted
# register read values for transient or failed hardware states (--read-seq), a scripted BLE peer on USART2 that
# answers what the firmware sends (--ble-peer, e.g. scripts/ble_peer_ymodem.py for the custom firmware's file transfer).
# Time-varying inputs and coverage knobs (2026-11-03, also off by default): separate AD1 values for the two time-shared
# channels (--va, --va-at), counter input changes and edge-synchronized gate timing (--freq-at, --ctr-real), thermistor
# and battery ADC changes (--therm-at, --battery-at), SD card removal / insertion (--sd-remove), function-return
# overrides (--fail-call, --fail-rand: e.g. a CSV field write that fails), NJU6350 register contents (--rtc), and the
# diagnostics --pc-poke, --peek, --pc-count.
#   emu_run.py EEVBlog2_05.bin svd/STM32L1xx.svd [--rotary V] [--ms 3000] [--all]
import os, sys, struct, argparse, hashlib, xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # our own scripts folder (run with -I)
from hy3131_model import HY3131, NAMES as HY_NAMES
from sdcard_model import SDCard
import lcd_decode
import ucfix   # patched libunicorn (Thumb IT state vs memory hooks); see ucfix.py
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ, UC_HOOK_CODE, UcError
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_PC, UC_ARM_REG_LR

ap = argparse.ArgumentParser(epilog="SD card faults and types: see scripts/sdcard_model.py. --nvic and --exti-irq model "
                             "interrupt hardware the default run simplifies; without them the run is unchanged.")
ap.add_argument("image"); ap.add_argument("svd")
ap.add_argument("--rotary", default="V", choices=["OFF", "LowZ", "V", "mV", "Hz", "Ohm", "VA", "uA", "A"])
ap.add_argument("--ms", type=int, default=3000)
ap.add_argument("--all", action="store_true")
ap.add_argument("--pf8-low", action="store_true", help="board-variant strap PF8 reads low (default high)")
ap.add_argument("--hy-only", action="store_true", help="print only the HY3131 transactions")
ap.add_argument("--ee", action="append", default=[], metavar="OFF=VAL", help="preload a data-EEPROM byte (hex)")
ap.add_argument("--cal", action="append", default=[], metavar="MODE:RANGE:OFFSET:GAIN",
                help="preload a calibration record at T[mode] + 16*range (offset int32, gain uint32 = x1e6)")
ap.add_argument("--ad1", type=lambda x: int(x, 0), default=0x012345, help="AD1 value the HY3131 model returns (signed)")
ap.add_argument("--watch", action="store_true", help="print display value / range every 100 ms after boot")
ap.add_argument("--screens", default="", help="T_MS,... print the decoded LCD at these times")
ap.add_argument("--coverage", help="append executed basic-block addresses to this file")
ap.add_argument("--flash-obr", help="FLASH.OBR value (hex) the option-byte reads return, e.g. 00080000 = BOR level 8 (Board_Init then reprograms BOR off)")
ap.add_argument("--jitter", type=int, help="test-only timing jitter: seed; each 1 ms slice runs 85-115%% of the normal instruction count (exposes timing-dependent results)")
ap.add_argument("--trace-irq", action="store_true", help="print every simulated interrupt with the interrupted pc, sp, lr")
ap.add_argument("--last-pcs", type=int, default=0, help="on a fault, print the last N instruction addresses (slow: hooks every instruction)")
ap.add_argument("--coverage-ranges", help="append executed basic blocks as 'ADDR SIZE' (hex) lines to this file")
ap.add_argument("--rotary-at", default="", help="T_MS:POS,... turn the dial during the run (e.g. 3000:OFF,6000:V)")
ap.add_argument("--jack", default="none", choices=["none", "mA", "A"], help="lead in a current jack (PE10 mA / PE11 A low)")
ap.add_argument("--freq", type=float, default=1000.0, help="signal frequency for the HY3131 counter (Hz)")
ap.add_argument("--duty", type=float, default=0.5, help="duty cycle for the counter")
ap.add_argument("--poke", default="", help="T_MS:ADDR=HEX,... write bytes to memory at a time (little-endian as given)")
ap.add_argument("--ble", default="", help="T_MS:HEX,... bytes the BLE module sends to the meter (USART2 RX)")
ap.add_argument("--snap-delay", type=int, default=300, help="ms after a key release to snapshot the state")
ap.add_argument("--battery-raw", type=int, default=1860, help="ADC raw for PA5 (1860 = about 6.0 V on the firmware table)")
ap.add_argument("--thermistor-raw", type=int, default=2048, help="ADC raw for PA4")
ap.add_argument("--eeprom", help="data-EEPROM image to load first (e.g. factory-eeprom.bin)")
ap.add_argument("--sd", help="SD card image (FAT); without it the slot holds a blank 64 MB card (no filesystem)")
ap.add_argument("--no-sd", action="store_true", help="no card: every command times out")
ap.add_argument("--sd-virtual-mb", type=int, help="card size the CSD reports (blocks past the image are sparse)")
ap.add_argument("--sd-file", help="whole card as a sparse file (any size, e.g. 2 TB): only its data blocks are loaded; the CSD reports the file size")
ap.add_argument("--sd-file-out", help="with --sd-file: write the card after the run as a sparse copy of --sd-file plus the blocks written")
ap.add_argument("--sd-place", default="", help="LBA:FILE: preload FILE at that block (sparse; e.g. a partition past 4 GiB)")
ap.add_argument("--sd-out", help="save the card image here after the run (writes otherwise stay in memory)")
ap.add_argument("--sd-log", action="store_true", help="print every SD command and transfer")
ap.add_argument("--ret", default="", help="ADDR,... print r0 (and time) each time these functions return")
ap.add_argument("--ee-out", help="save the 12 KB data EEPROM here after the run")
ap.add_argument("--ad1-at", default="", help="T_MS:VALUE,... change the AD1 value during the run (signed, e.g. 0 then 10000)")
ap.add_argument("--hy-reg", action="append", default=[], metavar="ADDR=VAL", help="HY3131 register power-on value (hex), e.g. 34=81")
ap.add_argument("--hy-nolatch", action="append", default=[], metavar="ADDR=MASK", help="HY3131 bits that do not latch (hex), e.g. 23=60")
ap.add_argument("--glass", action="store_true", help="SCREEN shows only the LCD controller RAM (any firmware; 2.05's RAM shadow address is firmware-specific)")
ap.add_argument("--ble-all", action="store_true", help="print every BLE TX byte (hex) at the end")
ap.add_argument("--flash-low", help="load this file at 0x08000000 (bootloader area; empty otherwise)")
ap.add_argument("--sd-type", default="sdhc", choices=["sdhc", "sdsc", "sdv1", "mmc"],
                help="card type: sdhc (default, SD 2.0 high capacity), sdsc (SD 2.0 standard capacity, CSD 1.0, byte addresses), "
                     "sdv1 (SD 1.x: no CMD8 answer), mmc (no CMD8/CMD55 answer)")
ap.add_argument("--msi-error", type=float, default=None, metavar="PCT",
                help="the real MSI is PCT percent off nominal against the 4 MHz HSE crystal: models RCC_CR.RTCPRE "
                     "(locked while HSEON), TIM11 input capture of MSI / HSE_RTC, and USART2 bit timing (bytes to the "
                     "BLE module corrupted when baud = f_MSI / BRR is off 19200); see scripts/clock_model.py")
ap.add_argument("--uart-tol", default="1.5:4.0", metavar="LO:HI",
                help="with --msi-error: baud error (percent) where byte corruption starts and where it is certain")
ap.add_argument("--hse-rtc-tim11", action="store_true",
                help="with --msi-error: HSE_RTC reaches TIM11 (the real meter showed no captures; default off)")
ap.add_argument("--tim11-read-skew", type=float, default=6.0, metavar="CYCLES",
                help="with --msi-error: up to CYCLES SYSCLK cycles pass on each TIM11 register read (so a capture can "
                     "land between two reads in one block, as on the meter)")
ap.add_argument("--clock-seed", type=int, default=1, help="with --msi-error: random seed (capture phase, corruption)")
ap.add_argument("--sd-fault", action="append", default=[], metavar="WHAT[@N|@N-M|@N+]:KIND",
                help="inject an SD fault (repeatable). WHAT = CMDn / ACMDn (counted per command from power-on) or READ / "
                     "WRITE / SCR (data phases); no @ = every occurrence. Command KIND: timeout, silent (no flag at all), "
                     "crc, respcmd=N, r1=HEX (OR into the response: card status error bits, e.g. CMD25:r1=0x04000000 = "
                     "write protect violation), resp=HEX (replace it, e.g. CMD13:resp=0xE00 = busy programming). Data KIND: "
                     "dtimeout, dcrc, rxoverr, txunderr, stbiterr, nodata, dflags=HEX")
ap.add_argument("--sd-scr", help="8-byte SCR (hex) the card returns to ACMD51 (default 0235800000000000)")
ap.add_argument("--nvic", action="store_true",
                help="NVIC ISER/ICER as set/clear registers (hardware): an interrupt stays enabled when another one is "
                     "enabled after it (default: the last ISER write wins, so SDIO IRQ 45 is lost behind DMA2 ch4 53)")
ap.add_argument("--exti-irq", action="store_true",
                help="EXTI on GPIOG (implies --nvic): in STOP an edge on an EXTI line the firmware unmasked, of the polarity "
                     "it selected in RTSR/FTSR (dial PG0-7 and keys PG8-15), sets EXTI.PR, wakes the core and runs the EXTI handler (default: only a dial change "
                     "wakes, no handler runs)")
ap.add_argument("--read-seq", action="append", default=[], metavar="[T_MS@]ADDR:HEX[xN],...",
                help="from T_MS on (default 0), the next reads of peripheral register ADDR return these values in order "
                     "(HEXxN = N times; no glob characters, so job lines need no quoting), then the normal model resumes; "
                     "repeatable. For transient or failed hardware states, e.g. 40023C18:1x40000 (FLASH busy past the "
                     "driver's timeout), 1700@40023800:00010303x30000 (HSE crystal never ready), 40007004:10x3 (PWR "
                     "voltage scaling in progress)")
ap.add_argument("--ble-peer", metavar="PATH.py",
                help="scripted BLE peer: a Python module with class Peer(args dict); every emulated ms its step(t_ms, "
                     "tx_bytes) gets the bytes the firmware sent on USART2 that ms and returns bytes for the meter "
                     "(queued, fed at --ble-peer-rate); optional finished() ends the run early, summary() is printed "
                     "at the end (e.g. scripts/ble_peer_ymodem.py)")
ap.add_argument("--ble-peer-arg", action="append", default=[], metavar="KEY=VALUE", help="argument for the peer (repeatable)")
ap.add_argument("--ble-peer-rate", type=int, default=1920, help="bytes per second from the peer into USART2 (default "
                "1920: 19200 baud)")
ap.add_argument("--va", metavar="V:I",
                help="AD1 values for the two time-shared channels (signed): V while the firmware's channel selector "
                     "g_tdm_channel (2.05: 0x200080c4) is 0, I while it is 1. In VA modes channel 0 is the voltage and 1 "
                     "the current; in the current modes with the burden display on, 0 is the current and 1 the burden "
                     "voltage (each channel switch also rewrites the HY3131 configuration). The value follows every "
                     "write of the selector; --ad1/--ad1-at values last until the next switch")
ap.add_argument("--va-at", default="", metavar="T_MS:V:I,...", help="change the two --va values from T_MS on")
ap.add_argument("--freq-at", default="", metavar="T_MS:HZ[:DUTY],...",
                help="change the counter input frequency (and duty) from T_MS on; with --ctr-real, HZ 0 = no signal "
                     "(no edges)")
ap.add_argument("--ctr-real", action="store_true",
                help="HY3131 counter with edge-synchronized gate timing (datasheet sec. 14): counting starts at the first "
                     "input edge after ENCTR is set and ends at the first edge after the gate time, so the counter flag "
                     "comes late on slow signals and never while there are no edges (--freq-at T:0); default: every gate "
                     "ends on time with whole cycles")
ap.add_argument("--therm-at", default="", metavar="T_MS:RAW,...", help="change the PA4 (thermistor) ADC raw value from T_MS on")
ap.add_argument("--battery-at", default="", metavar="T_MS:RAW,...", help="change the PA5 (battery) ADC raw value from T_MS on")
ap.add_argument("--sd-remove", action="append", default=[], metavar="T_MS[:T_BACK[:IMAGE]]",
                help="pull the card out at T_MS (it answers nothing: every command CTIMEOUT); push it back in at T_BACK "
                     "(the same card, or IMAGE), freshly powered: it answers only identification commands until CMD3 "
                     "(repeatable; see scripts/sdcard_model.py)")
ap.add_argument("--fail-call", action="append", default=[], metavar="ADDR:FROM[:COUNT[:R0]]",
                help="calls FROM .. FROM+COUNT-1 (counted from 1; COUNT default 1, 0 = every later call) of the function at "
                     "ADDR (hex) return at once with r0 = R0 (default 1) instead of running: a test knob for failures the "
                     "hardware produces below that call, e.g. 0801f604 (Log_PutBytes: one CSV field's f_write fails, as on "
                     "a card that fails or fills up at that point); prints each injected return")
ap.add_argument("--fail-rand", action="append", default=[], metavar="ADDR:PROB[:SEED[:R0]]",
                help="each call of the function at ADDR returns at once (r0 = R0, default 1) with probability PROB, seeded "
                     "(a flaky card, reproducible)")
ap.add_argument("--rtc", default="", metavar="REG=BCD,...",
                help="the NJU6350 clock chip returns this raw byte for register REG (2 year, 4 month, 6 day, 10 hour, "
                     "12 minute, 14 second; hex), e.g. garbage after its backup supply ran out")
ap.add_argument("--pc-poke", action="append", default=[], metavar="ADDR:MEM=HEX",
                help="the first time the instruction at ADDR runs, write the bytes HEX to MEM (diagnostic)")
ap.add_argument("--peek", default="", metavar="T_MS:ADDR:N,...", help="print N bytes of memory at ADDR at T_MS (diagnostic)")
ap.add_argument("--pc-count", default="", metavar="ADDR,...", help="print how often these instructions ran (diagnostic)")
ap.add_argument("--dma-exact", action="store_true",
                help="old SD DMA simplification: 4 x CNDTR bytes copied byte-exact from/to CMAR. Default (hardware): CNDTR "
                     "items of MSIZE/PSIZE, a 16/32-bit memory side ignores the low CMAR bits (CMAR & ~1 / & ~3); "
                     "--sd-log notes unaligned starts (docs/emulator-dma-alignment.md)")
ap.add_argument("--reset-reg", action="append", default=[], metavar="REG=HEX",
                help="core register value at reset, before the first instruction (r0-r12, lr; repeatable). The Cortex-M3 "
                     "leaves them UNKNOWN at reset; default 0 (as Unicorn starts). E.g. r7=80000000 for code that reads a "
                     "register nothing wrote since reset")
ap.add_argument("--keys", default="", help="T_MS:KEY[:DUR_MS],... keys MODE SETUP MINMAX MEM RANGE HOLD REL PEAK (default 150 ms)")
a = ap.parse_args()
if a.exti_irq: a.nvic = True
img = open(a.image, "rb").read()
BASE, MSI_HZ = 0x08006000, 4194304
PER_MS = MSI_HZ // 1000
import random as _random
jit = None                                         # set from --jitter after argument parsing
# rotary bit k (one-hot after Rotary_Read's reversal): LowZ 01, V 02, mV 04, Hz 08, Ohm 10, VA 20, uA 40, A 80;
# Rotary_Read tests GPIOG.IDR bit (7-k) low for position bit k
ROT = {"OFF": 0, "LowZ": 0x01, "V": 0x02, "mV": 0x04, "Hz": 0x08, "Ohm": 0x10, "VA": 0x20, "uA": 0x40, "A": 0x80}
rot = ROT[a.rotary]
pg_low = 0
for k in range(8):
    if rot >> k & 1: pg_low |= 1 << (7 - k)
GPIOG_IDR = 0xFFFF & ~pg_low                     # keys PG8..15 high = released
def rot_low(pos):
    r, low = ROT[pos], 0
    for k in range(8):
        if r >> k & 1: low |= 1 << (7 - k)
    return low
ROT_AT = sorted((int(t), p) for t, p in (i.split(':') for i in filter(None, a.rotary_at.split(','))))
def gpiog_now(ms):
    pos = a.rotary
    for t, p in ROT_AT:
        if ms >= t: pos = p
    return 0xFFFF & ~rot_low(pos)
GPIOE_IDR = 0x0C00 & ~({'none': 0, 'mA': 1 << 10, 'A': 1 << 11}[a.jack])   # PE10 mA / PE11 A sense, low = lead in

names = {}
root = ET.parse(a.svd).getroot()
periphs = {p.findtext("name"): p for p in root.find("peripherals")}
for p in periphs.values():
    src = periphs.get(p.get("derivedFrom"), p) if p.get("derivedFrom") else p
    base = int(p.findtext("baseAddress"), 0)
    for r in src.iter("register"):
        names[base + int(r.findtext("addressOffset"), 0)] = p.findtext("name") + "." + r.findtext("name")
names.update({0xE000ED08: "SCB.VTOR", 0xE000E010: "SYSTICK.CTRL", 0xE0042004: "DBGMCU.CR"})
def name(x):
    if x in names: return names[x]
    if 0x42000000 <= x < 0x44000000:
        off = x - 0x42000000; byte = off // 32
        return f"{names.get(0x40000000 + (byte & ~3), hex(0x40000000 + (byte & ~3)))}.bit{(byte & 3) * 8 + (off // 4) % 8} (bb)"
    return hex(x)

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
from clock_model import ClockModel, USART2_BRR
clk = (ClockModel(a.msi_error, a.uart_tol, a.clock_seed, rd32=lambda x: rd32(x), hse_rtc_tim11=a.hse_rtc_tim11, read_skew=a.tim11_read_skew)
       if a.msi_error is not None else None)
for base, size in ((0x08000000, 0x80000), (0x08080000, 0x4000), (0x1FF80000, 0x1000), (0x20000000, 0xC000),
                   (0x22000000, 0x1800000), (0x40000000, 0x100000), (0x42000000, 0x2000000), (0xE0000000, 0x100000)):
    uc.mem_map(base, size)
uc.mem_write(BASE, img)
CALT = struct.unpack_from('<26H', img, 0x08028ee8 - BASE) if a.cal else None   # cal record table T[mode] (2.05 address)
if a.eeprom: uc.mem_write(0x08080000, open(a.eeprom, 'rb').read())
if a.flash_low: uc.mem_write(0x08000000, open(a.flash_low, 'rb').read()[:0x6000])
KEYBIT = {'NONE': 16, 'MODE': 8, 'SETUP': 9, 'MINMAX': 10, 'MEM': 11, 'RANGE': 12, 'HOLD': 13, 'REL': 14, 'PEAK': 15}
presses = []
for item in filter(None, a.keys.split(',')):
    f = item.split(':'); presses.append((int(f[0]), f[1].upper(), int(f[2]) if len(f) > 2 else 150))
snap_at = sorted({t + d + a.snap_delay for t, k, d in presses})
for c in a.cal:
    m, r, off, gain = c.split(':')
    addr = CALT[int(m)] + 16 * int(r)
    uc.mem_write(0x08080000 + addr, struct.pack('>iI', int(off, 0), int(gain, 0)))
for kv in a.ee:                                  # data EEPROM preloads (erased = 0 otherwise)
    off, val = (int(x, 16) for x in kv.split('='))
    uc.mem_write(0x08080000 + off, bytes([val]))
TRAP = 0x0807F000                                 # unused flash: handler return lands here
uc.mem_write(TRAP, b"\xfe\xe7")                   # b . (never executed: we stop on reaching it)

def rd32(x): return struct.unpack("<I", uc.mem_read(x, 4))[0]
def wr32(x, v): uc.mem_write(x, struct.pack("<I", v & 0xFFFFFFFF))
STATIC = {0x40023834: (1 << 1) | (1 << 9), 0x40007004: 0, 0x40023C18: 0x0C,   # FLASH.SR: READY | ENDHV (flags are rc_w1)
         
          0x40012400: (1 << 6) | (1 << 1), 0x40021010: GPIOE_IDR}   # GPIOG.IDR (GPIOG base 0x40021C00), GPIOE.IDR
if a.flash_obr: STATIC[0x40023C1C] = int(a.flash_obr, 16)
if a.jitter is not None: jit = _random.Random(a.jitter)
hy = HY3131(ad1_value=a.ad1 & 0xFFFFFF, freq_hz=a.freq, duty=a.duty)
for kv in a.hy_reg:
    r, v = (int(x, 16) for x in kv.split("=")); hy.reg[r] = v
hy.nolatch = {int(r, 16): int(m, 16) for r, m in (kv.split("=") for kv in a.hy_nolatch)}
if a.ctr_real: hy.real_gate = True
place = {int(l): open(f, "rb").read() for l, f in (x.split(":", 1) for x in filter(None, [a.sd_place]))}
def sparse_blocks(path):
    """{lba: 512 bytes} for every nonzero block in the data extents of a sparse file (holes and zero blocks skipped)."""
    import os
    out, fd = {}, os.open(path, os.O_RDONLY)
    try:
        end, pos = os.fstat(fd).st_size, 0
        while pos < end:
            try: pos = os.lseek(fd, pos, os.SEEK_DATA)
            except OSError: break                         # no data past pos
            stop = os.lseek(fd, pos, os.SEEK_HOLE)
            pos -= pos % 512
            while pos < stop:
                b = os.pread(fd, min(1 << 20, stop - pos), pos)
                for i in range(0, len(b), 512):
                    if b[i:i + 512].count(0) != len(b[i:i + 512]): out[(pos + i) // 512] = bytes(b[i:i + 512]).ljust(512, b"\0")
                pos += len(b)
    finally: os.close(fd)
    return out
if a.sd_file:
    import os
    fb = sparse_blocks(a.sd_file)
    head = bytearray(1 << 20)                              # blocks 0-2047 live in the image, the rest stay sparse
    for k in [k for k in fb if k < 2048]: head[k * 512:(k + 1) * 512] = fb.pop(k)
    sd = None if a.no_sd else SDCard(head, virtual_mb=os.path.getsize(a.sd_file) >> 20, place=fb, card_type=a.sd_type,
                                     scr=bytes.fromhex(a.sd_scr) if a.sd_scr else None, faults=a.sd_fault)
else:
    sd = None if a.no_sd else SDCard(open(a.sd, "rb").read() if a.sd else None, virtual_mb=a.sd_virtual_mb, place=place,
                                     card_type=a.sd_type, scr=bytes.fromhex(a.sd_scr) if a.sd_scr else None,
                                     faults=a.sd_fault)
if sd: sd.dma_exact = a.dma_exact
if sd: sd.attach(rd32, wr32, lambda x, n: uc.mem_read(x, n), lambda x, b: uc.mem_write(x, b), lambda: now[0])
# USART2 (BLE112): SR 0x40004400 (RXNE bit 5, TC bit 6, TXE bit 7), DR 0x40004404, CR1 0x4000440C (RXNEIE 5, TXEIE 7)
BLE_RX = sorted((int(t), bytes.fromhex(h)) for t, h in (i.split(':') for i in filter(None, a.ble.split(','))))
ble_rx, ble_tx = bytearray(), bytearray()
peer = None                                      # --ble-peer: scripted BLE112 + PC
if a.ble_peer:
    import importlib.util
    _spec = importlib.util.spec_from_file_location("ble_peer", a.ble_peer)
    _mod = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(_mod)
    peer = _mod.Peer(dict(kv.split("=", 1) for kv in a.ble_peer_arg))
    peer_q, peer_credit, peer_seen = bytearray(), 0.0, 0
end_ms = a.ms
# NJU6350 RTC chip (PC13 CE, PB6 CLK, PB7 DATA): 4-bit address LSB first (bit 0 = 1: write), then 8 data bits LSB
# first, BCD. Registers (firmware use): 2 year, 4 month, 6 day, 10 hour, 12 minute, 14 second.
import datetime
class NJU6350:
    def __init__(self): self.base = datetime.datetime(2026, 11, 2, 15, 0, 0); self.at_ms = 0; self.reset(); self.ce = self.clk = self.dout = 0
    def reset(self): self.phase, self.bits, self.addr, self.byte, self.out = "addr", 0, 0, 0, 0
    def now(self, ms): return self.base + datetime.timedelta(milliseconds=ms - self.at_ms)
    def reg(self, r, ms):
        t = self.now(ms); v = {2: t.year % 100, 4: t.month, 6: t.day, 8: t.weekday(), 10: t.hour, 12: t.minute, 14: t.second}.get(r, 0)
        return (v // 10) << 4 | v % 10
    def write(self, r, bcd, ms):
        v = (bcd >> 4) * 10 + (bcd & 15); t = self.now(ms)
        f = {2: "year", 4: "month", 6: "day", 10: "hour", 12: "minute", 14: "second"}.get(r)
        if f:
            try: t = t.replace(**{f: 2000 + v if f == "year" else v})
            except ValueError: pass
        self.base, self.at_ms = t, ms
        rtc_writes.append((ms, r, bcd))
    def pins(self, ce, clk, dout, ms):
        if ce and not self.ce: self.reset()
        if ce and clk and not self.clk:                  # rising edge
            if self.phase == "addr":
                self.addr |= (dout & 1) << self.bits; self.bits += 1
                if self.bits == 4:
                    self.phase, self.bits = ("wr" if self.addr & 1 else "rd"), 0
                    self.out = self.reg(self.addr & 0xE, ms); self.byte = 0
            elif self.phase == "rd":
                self.dbit = self.out >> self.bits & 1; self.bits += 1
            elif self.phase == "wr":
                self.byte |= (dout & 1) << self.bits; self.bits += 1
                if self.bits == 8: self.write(self.addr & 0xE, self.byte, ms); self.phase = "done"
        self.ce, self.clk, self.dout = ce, clk, dout
rtc = NJU6350(); rtc.dbit = 0; rtc_writes = []
if a.rtc:                                         # --rtc: fixed raw register contents
    _rtc_regs = {int(r): int(v, 16) for r, v in (kv.split("=") for kv in a.rtc.split(","))}
    _rtc_reg = rtc.reg
    rtc.reg = lambda r, ms: _rtc_regs[r] if r in _rtc_regs else _rtc_reg(r, ms)
GPIOB_ODR, GPIOC_ODR = [0], [0]
def rtc_update():
    rtc.pins(GPIOC_ODR[0] >> 13 & 1, GPIOB_ODR[0] >> 6 & 1, GPIOB_ODR[0] >> 7 & 1, int(now[0]))
GPIOF_ODR = [0]                                  # PF0 CK, PF1 CS, PF2 DI (from BSRR writes)
def gpiof_set(bits_set, bits_reset):
    o = (GPIOF_ODR[0] & ~bits_reset) | bits_set
    GPIOF_ODR[0] = o
    hy.pins(o & 1, o >> 1 & 1, o >> 2 & 1)
def keys_low(ms):
    low = 0
    for t, k, d in presses:
        if t <= ms < t + d: low |= 1 << KEYBIT[k]
    return low
nvic_en = [0] * 8                                # --nvic: enabled interrupts (ISERn set bits, ICERn clears them)
exti_pr = [0]                                    # --exti-irq: EXTI.PR pending lines
EXTI_IMR, EXTI_PR = 0x40010400, 0x40010414
def nvic_enabled(irq):
    if a.nvic: return nvic_en[irq // 32] >> (irq % 32) & 1
    return rd32(0xE000E100 + 4 * (irq // 32)) >> (irq % 32) & 1
import collections
read_seq = []                                    # --read-seq: [from ms, address, queued values]
for spec in a.read_seq:
    t, _, rest = spec.rpartition("@")
    addr, vals = rest.split(":", 1); q = collections.deque()
    for v in vals.split(","):
        h, _, n = v.partition("x"); q.extend([int(h, 16)] * (int(n) if n else 1))
    read_seq.append((int(t) if t else 0, int(addr, 16), q))
def on_read(uc, acc, x, size, v, u):
    if clk and clk.on_read(x, rd32, wr32): return
    if read_seq:
        for t, ra, q in read_seq:
            if ra == x and q and now[0] >= t: wr32(x, q.popleft()); return
    if a.exti_irq and x == EXTI_PR: wr32(x, exti_pr[0]); return
    if sd and sd.on_read(x): return
    if x == 0x40002408:                          # LCD.SR: ENS and RDY follow CR.LCDEN; FCRSF set; UDR done
        en = rd32(0x40002400) & 1
        wr32(x, en | (en << 4) | (1 << 5)); return
    if x == 0x40020410:                          # GPIOB.IDR: PB7 = RTC data out when the chip is talking
        wr32(x, (rtc.dbit << 7) | (GPIOB_ODR[0] & 0x40)); return
    if x == 0x40004400:
        wr32(x, (1 << 7) | (1 << 6) | ((1 << 5) if ble_rx else 0)); return
    if x == 0x40004404:
        wr32(x, ble_rx.pop(0) if ble_rx else 0); return
    if x == 0x40021C10:
        wr32(x, gpiog_now(int(now[0])) & ~keys_low(int(now[0])) & 0xFFFF)
    elif x == 0x40021810:                          # GPIOF.IDR: PF3 = HY3131 DO/IRQ, PF8 strap
        wr32(x, (hy.dout() << 3) | (0 if a.pf8_low else 1 << 8) | (GPIOF_ODR[0] & 0x7))
    elif x == 0x40012458:                        # ADC.DR: battery (PA5, channel 5) or thermistor (PA4, channel 4)
        wr32(x, a.battery_raw if adc_ch[0] == 5 else a.thermistor_raw)
    elif x in STATIC: wr32(x, STATIC[x])
    elif x == 0x40023800:
        v = rd32(x); rdy = 0
        for on, ready in ((0, 1), (8, 9), (16, 17), (24, 25)):
            if v >> on & 1: rdy |= 1 << ready
        wr32(x, (v & ~0x02020202) | rdy)
    elif x == 0x40023808:
        v = rd32(x); wr32(x, (v & ~0xC) | ((v & 3) << 2))
writes = []
beeps = [0]
buz = [0, 0, 0]                                   # PE7 level, rising edges, ms high
adc_ch = [0]
in_handler = [None]
def on_write(uc, acc, x, size, v, u):
    writes.append((now[0], in_handler[0], uc.reg_read(UC_ARM_REG_PC), x, size, v))
    if a.nvic and 0xE000E100 <= x < 0xE000E120: nvic_en[(x - 0xE000E100) // 4] |= v & 0xFFFFFFFF
    elif a.nvic and 0xE000E180 <= x < 0xE000E1A0: nvic_en[(x - 0xE000E180) // 4] &= ~v & 0xFFFFFFFF
    if a.exti_irq and x == EXTI_PR: exti_pr[0] &= ~v & 0xFFFFFFFF
    if clk and clk.on_write(x, v, rd32): return
    if sd and sd.on_write(x, v):
        if sd.irq and in_handler[0] is None: uc.emu_stop()   # take the SDIO/DMA interrupt at once
        return
    if x == 0x40021018 and v & 0x80 and size == 2: beeps[0] += 1   # GPIOE.BSRR set PE7: buzzer on edge
    pe7 = buz[0]                                  # PE7 (buzzer) level from any write form (firmware-independent)
    if x == 0x40021018: pe7 = 1 if v & 0x80 else (0 if size == 4 and v >> 16 & 0x80 else pe7)
    elif x == 0x4002101A and v & 0x80: pe7 = 0
    elif x == 0x40021014: pe7 = v >> 7 & 1
    elif x == 0x42000000 + (0x40021014 - 0x40000000) * 32 + 7 * 4: pe7 = v & 1
    if pe7 and not buz[0]: buz[1] += 1
    buz[0] = pe7
    if x == 0x40012440: adc_ch[0] = v & 0x1F     # ADC.SQR5: first conversion's channel
    if x == 0x40004404:                          # USART2.DR: byte to the BLE module (bit timing: --msi-error)
        ble_tx.append(clk.tx_byte(v & 0xFF, rd32(USART2_BRR) & 0xFFFF) if clk else v & 0xFF)
    if x in (0x40020418, 0x4002041A, 0x40020818, 0x4002081A):   # GPIOB / GPIOC BSRR halves
        port = GPIOB_ODR if x < 0x40020800 else GPIOC_ODR
        if x & 2: port[0] &= ~(v & 0xFFFF)
        else:
            port[0] |= v & 0xFFFF
            if size == 4: port[0] &= ~(v >> 16)
        rtc_update()
    if x == 0x40021818:                          # GPIOF.BSRR (low half set, high half reset)
        if size == 4: gpiof_set(v & 0xFFFF, v >> 16)
        else: gpiof_set(v & 0xFFFF, 0)
    elif x == 0x4002181A: gpiof_set(0, v & 0xFFFF)
    elif x == 0x40021814: gpiof_set(v & 0xFFFF, ~v & 0xFFFF)   # GPIOF.ODR
    if 0x42000000 <= x < 0x44000000:
        off = x - 0x42000000; byte = 0x40000000 + off // 32; bit = (off // 4) % 8
        b = uc.mem_read(byte, 1)[0]
        uc.mem_write(byte, bytes([(b | (1 << bit)) if v & 1 else (b & ~(1 << bit) & 0xFF)]))
uc.hook_add(UC_HOOK_MEM_READ, on_read, begin=0x40000000, end=0x4FFFFFFF)
uc.hook_add(UC_HOOK_MEM_WRITE, on_write, begin=0x40000000, end=0x5FFFFFFF)
uc.hook_add(UC_HOOK_MEM_WRITE, on_write, begin=0xE0000000, end=0xE00FFFFF)

# milestones: first time these addresses execute
MILES = {0x08024b40: "main", 0x0801be76: "Board_Init", 0x0801c056: "Board_GPIOInit", 0x0801b450: "Board_LCDInit",
         0x0800ea3a: "BootScreen", 0x0800b974: "HY_SetMode", 0x08014a14: "Rotary_Read", 0x08014500: "Mode_Cycle?",
         0x08006124: "HY_Poll (main loop)", 0x08024eca: "App_Sleep"}
seen = {}
now = [0.0]
def on_code(uc, addr, size, u):
    if addr in MILES and addr not in seen:
        seen[addr] = now[0]; print(f"  t={now[0]:8.3f} ms  reached {MILES[addr]} (0x{addr:08x})")
for m in MILES: uc.hook_add(UC_HOOK_CODE, on_code, begin=m, end=m)
ret_watch = {}                                    # return address -> function entry, armed at entry
def on_entry(uc, addr, size, u):
    lr = uc.reg_read(UC_ARM_REG_LR) & ~1
    ret_watch[lr] = addr
    uc.hook_add(UC_HOOK_CODE, on_return, begin=lr, end=lr)
def on_return(uc, addr, size, u):
    if addr in ret_watch:
        from unicorn.arm_const import UC_ARM_REG_R0
        print(f"  t={now[0]:8.1f} ms  {ret_watch.pop(addr):08x} returned r0=0x{uc.reg_read(UC_ARM_REG_R0):08x}")
for r in filter(None, a.ret.split(',')):
    uc.hook_add(UC_HOOK_CODE, on_entry, begin=int(r, 16), end=int(r, 16))
TDM = 0x200080c4                                  # --va: g_tdm_channel (2.05)
va = [int(x, 0) for x in a.va.split(":")] if a.va else None
VA_AT = [[int(t), int(v, 0), int(i, 0), False] for t, v, i in (x.split(":") for x in filter(None, a.va_at.split(",")))]
def va_set(ch):
    if va is not None: hy.ad1_value = va[ch & 1] & 0xFFFFFF
if va is not None or VA_AT:
    def _va_wr(uc, acc, x, size, v, u):
        if x <= TDM < x + size: va_set(v >> 8 * (TDM - x))
    uc.hook_add(UC_HOOK_MEM_WRITE, _va_wr, begin=TDM & ~3, end=TDM | 3)
    va_set(0)
from unicorn.arm_const import UC_ARM_REG_R0 as _R0
for spec in a.fail_call:                          # --fail-call: forced early returns
    f = spec.split(":")
    _fa, _ff, _fc, _fr = int(f[0], 16), int(f[1]), int(f[2]) if len(f) > 2 else 1, int(f[3], 0) if len(f) > 3 else 1
    def _fail_cb(uc, addr, size, u, first=_ff, count=_fc, r0=_fr, n=[0]):
        n[0] += 1
        if n[0] >= first and (count == 0 or n[0] < first + count):
            print(f"  FAIL-CALL {addr:08x} call {n[0]}: r0 = {r0}")
            uc.reg_write(_R0, r0); uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR) | 1)
    uc.hook_add(UC_HOOK_CODE, _fail_cb, begin=_fa, end=_fa)
for spec in a.fail_rand:                          # --fail-rand: forced early returns at random (seeded)
    f = spec.split(":")
    _fa, _fp, _fs, _fr = int(f[0], 16), float(f[1]), int(f[2]) if len(f) > 2 else 1, int(f[3], 0) if len(f) > 3 else 1
    def _rand_cb(uc, addr, size, u, prob=_fp, r0=_fr, rng=_random.Random(_fs), n=[0]):
        if rng.random() < prob:
            n[0] += 1
            if n[0] <= 200: print(f"  FAIL-RAND {addr:08x} #{n[0]}: r0 = {r0}")
            uc.reg_write(_R0, r0); uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR) | 1)
    uc.hook_add(UC_HOOK_CODE, _rand_cb, begin=_fa, end=_fa)
for spec in a.pc_poke:                            # --pc-poke
    _pa, _rest = spec.split(":"); _pm, _ph = _rest.split("=")
    def _poke_cb(uc, addr, size, u, m=int(_pm, 16), h=bytes.fromhex(_ph), done=[False]):
        if not done[0]:
            done[0] = True; uc.mem_write(m, h); print(f"  PC-POKE at {addr:08x}: [{m:08x}] <- {h.hex()}")
    uc.hook_add(UC_HOOK_CODE, _poke_cb, begin=int(_pa, 16), end=int(_pa, 16))
pc_counts = {}
for _x in filter(None, a.pc_count.split(",")):
    pc_counts[int(_x, 16)] = 0
    uc.hook_add(UC_HOOK_CODE, lambda uc, addr, size, u: pc_counts.__setitem__(addr, pc_counts[addr] + 1), begin=int(_x, 16), end=int(_x, 16))
FREQ_AT = [[int(f[0]), float(f[1]), float(f[2]) if len(f) > 2 else None] for f in (x.split(":") for x in filter(None, a.freq_at.split(",")))]
THERM_AT = [[int(t), int(r, 0)] for t, r in (x.split(":") for x in filter(None, a.therm_at.split(",")))]
BATT_AT = [[int(t), int(r, 0)] for t, r in (x.split(":") for x in filter(None, a.battery_at.split(",")))]
SD_EVENTS = []                                    # --sd-remove: [time, "out"/"in", image or None]
for spec in a.sd_remove:
    f = spec.split(":", 2)
    SD_EVENTS.append([int(f[0]), "out", None])
    if len(f) > 1 and f[1]: SD_EVENTS.append([int(f[1]), "in", open(f[2], "rb").read() if len(f) > 2 else None])
SD_EVENTS.sort(key=lambda e: e[0])
PEEKS = [[int(t), int(m, 16), int(n, 0)] for t, m, n in (x.split(":") for x in filter(None, a.peek.split(",")))]
TIMED = bool(VA_AT or FREQ_AT or THERM_AT or BATT_AT or SD_EVENTS or PEEKS)
def timed_inputs(ms):
    """--va-at, --freq-at, --therm-at, --battery-at, --sd-remove, --peek events due at or before ms (each once)."""
    global va
    for e in VA_AT:
        if e[0] <= ms and e[3] is False:
            e[3] = True; va = [e[1], e[2]]; va_set(uc.mem_read(TDM, 1)[0])
    while FREQ_AT and FREQ_AT[0][0] <= ms:
        _, hz, du = FREQ_AT.pop(0); hy.freq_hz = hz
        if du is not None: hy.duty = du
    while THERM_AT and THERM_AT[0][0] <= ms: a.thermistor_raw = THERM_AT.pop(0)[1]
    while BATT_AT and BATT_AT[0][0] <= ms: a.battery_raw = BATT_AT.pop(0)[1]
    while SD_EVENTS and SD_EVENTS[0][0] <= ms and sd is not None:
        _, what, image = SD_EVENTS.pop(0)
        if what == "out": sd.remove(); print(f"  t={ms} ms: SD card removed")
        else: sd.insert(image); print(f"  t={ms} ms: SD card inserted")
    while PEEKS and PEEKS[0][0] <= ms:
        _, m, n = PEEKS.pop(0); print(f"  PEEK t={ms} [{m:08x}] {bytes(uc.mem_read(m, n)).hex()}")
FREQ_AT.sort(key=lambda e: e[0]); THERM_AT.sort(key=lambda e: e[0]); BATT_AT.sort(key=lambda e: e[0]); PEEKS.sort(key=lambda e: e[0])
blocks = set()
branges = set()
if a.coverage or a.coverage_ranges:
    from unicorn import UC_HOOK_BLOCK
    uc.hook_add(UC_HOOK_BLOCK, lambda uc, addr, size, u: (blocks.add(addr), branges.add((addr, size))), begin=BASE, end=BASE + len(img))

def handler_addr(irq): return rd32(BASE + (16 + irq) * 4) & ~1
def call(fn, label):
    ctx = uc.context_save()
    if a.trace_irq: print(f"  irq {label} t={now[0]} pc 0x{uc.reg_read(UC_ARM_REG_PC):08x} sp 0x{uc.reg_read(UC_ARM_REG_SP):08x} lr 0x{uc.reg_read(UC_ARM_REG_LR):08x}")
    sp = (uc.reg_read(UC_ARM_REG_SP) - 0x40) & ~7    # below the interrupted frame
    uc.reg_write(UC_ARM_REG_SP, sp); uc.reg_write(UC_ARM_REG_LR, TRAP | 1)
    in_handler[0] = label
    try: uc.emu_start(fn | 1, TRAP, count=200000)
    except UcError as e: print(f"  handler {label} faulted: {e} at 0x{uc.reg_read(UC_ARM_REG_PC):08x}")
    in_handler[0] = None
    uc.context_restore(ctx)
def armed(tim): return (rd32(tim) & 1) and (rd32(tim + 0x0C) & 1)    # CR1.CEN, DIER.UIE

import collections as _c
lastpcs = _c.deque(maxlen=a.last_pcs or 1)
if a.last_pcs: uc.hook_add(UC_HOOK_CODE, lambda u, ad, sz, d: lastpcs.append(ad))
print(f"image sha256 {hashlib.sha256(img).hexdigest()[:16]}  rotary {a.rotary} (GPIOG.IDR 0x{GPIOG_IDR:04x})  {a.ms} ms")
sp, reset = struct.unpack_from("<II", img, 0)
uc.reg_write(UC_ARM_REG_SP, sp)
if a.reset_reg:
    import unicorn.arm_const as _ac
    for kv in a.reset_reg:
        r, v = kv.split("=")
        uc.reg_write(getattr(_ac, "UC_ARM_REG_" + r.upper()), int(v, 16))
pc = reset | 1
if clk:                                           # TIM11 time base: instructions per executed block (Thumb widths)
    from unicorn import UC_HOOK_BLOCK as _UHB
    _ninstr = {}
    def _count_block(uc, addr, size, u):
        if not clk.running(): return
        n = _ninstr.get((addr, size))
        if n is None:
            code = bytes(uc.mem_read(addr, size)); i = n = 0
            while i < size:
                hw = code[i] | (code[i + 1] << 8) if i + 1 < size else 0
                i += 4 if (hw >> 11) in (0x1D, 0x1E, 0x1F) else 2; n += 1
            _ninstr[(addr, size)] = n
        clk.add_instructions(n)
    uc.hook_add(_UHB, _count_block)
TIM2, TIM3 = 0x40000000, 0x40000400
asleep = [False, 0]
for ms in range(a.ms):
    now[0] = float(ms); hy.tick_ms(ms)
    buz[2] += buz[0]
    if asleep[0] and a.exti_irq:                  # STOP: an edge on an unmasked EXTI line (GPIOG) wakes, handler first
        lines = gpiog_now(ms) & ~keys_low(ms) & 0xFFFF
        rising, falling = lines & ~asleep[1], ~lines & asleep[1] & 0xFFFF
        edges = ((rising & rd32(EXTI_IMR + 8)) | (falling & rd32(EXTI_IMR + 12))) & rd32(EXTI_IMR)   # RTSR, FTSR
        asleep[1] = lines
        if edges:
            exti_pr[0] |= edges
            print(f"  t={ms} ms: woken from STOP by EXTI lines 0x{edges:04x}"); asleep[0] = False
            for irq, mask in ((6, 1), (7, 2), (8, 4), (9, 8), (10, 0x10), (23, 0x3E0), (40, 0xFC00)):
                if edges & mask and nvic_enabled(irq): call(handler_addr(irq), f"EXTI{irq}")
        else:
            if a.screens and str(ms) in a.screens.split(','):
                print(f"  SCREEN t={ms} (STOP): {lcd_decode.render(bytes(uc.mem_read(0x20007790, 32)))}")
            continue
    elif asleep[0]:                               # STOP mode: nothing runs until a dial line changes (EXTI wake)
        if gpiog_now(ms) & 0xFF != asleep[1]:
            print(f"  t={ms} ms: woken from STOP by the dial (EXTI)"); asleep[0] = False
        else:
            if a.screens and str(ms) in a.screens.split(','):   # the LCD keeps its last frame in STOP
                print(f"  SCREEN t={ms} (STOP): {lcd_decode.render(bytes(uc.mem_read(0x20007790, 32)))}")
            continue
    if TIMED: timed_inputs(ms)
    try:
        uc.emu_start(pc, 0xFFFFFFFF, count=PER_MS if jit is None else int(PER_MS * jit.uniform(0.85, 1.15)))
    except UcError as e:
        fpc = uc.reg_read(UC_ARM_REG_PC)
        try: op = struct.unpack("<H", uc.mem_read(fpc - 2, 2))[0]   # Unicorn reports the PC after WFE/WFI
        except UcError: op = None                 # PC outside mapped memory: a real fault
        if op is None:
            print(f"  t={ms} ms: fault {e} at 0x{fpc:08x} (lr 0x{uc.reg_read(UC_ARM_REG_LR):08x}, sp 0x{uc.reg_read(UC_ARM_REG_SP):08x})")
            if a.last_pcs: print("  last pcs: " + " ".join(f"{x:x}" for x in lastpcs))
            break
        if op in (0xBF20, 0xBF30):                # WFE / WFI: Unicorn has no sleep; model STOP
            print(f"  t={ms} ms: {'WFE' if op == 0xBF20 else 'WFI'} at 0x{fpc - 2:08x}: STOP")
            asleep[0], asleep[1] = True, (gpiog_now(ms) & ~keys_low(ms) & 0xFFFF) if a.exti_irq else gpiog_now(ms) & 0xFF
            pc = fpc | 1
            continue
        print(f"  t={ms} ms: fault {e} at 0x{fpc:08x}"); break
    pc = uc.reg_read(UC_ARM_REG_PC) | 1
    if a.trace_irq and ms < 20: print(f"  slice t={ms} ends pc 0x{pc & ~1:08x}" + (f" last {' '.join(f'{x:x}' for x in list(lastpcs)[-3:])}" if a.last_pcs else ""))
    for _ in range(64):                           # SDIO / DMA2 channel 4 interrupts raised inside the slice
        if not sd or not sd.irq: break
        for irq in sorted(sd.irq):
            sd.irq.discard(irq)
            if nvic_enabled(irq): call(handler_addr(irq), f"IRQ{irq}")
        try: uc.emu_start(pc, 0xFFFFFFFF, count=PER_MS // 4)   # rest of the slice (time stretches slightly)
        except UcError as e: print(f"  t={ms} ms: fault {e} after an SD interrupt"); break
        pc = uc.reg_read(UC_ARM_REG_PC) | 1
    if armed(TIM3): call(handler_addr(29), "TIM3")
    if ms % 10 == 9 and armed(TIM2): call(handler_addr(28), "TIM2")
    for item in filter(None, a.ad1_at.split(',')):
        t, v = item.split(':')
        if int(t) == ms: hy.ad1_value = int(v, 0) & 0xFFFFFF
    for item in filter(None, a.poke.split(',')):
        t, rest = item.split(':'); addr, hx = rest.split('=')
        if int(t) == ms: uc.mem_write(int(addr, 16), bytes.fromhex(hx))
    for t, h in BLE_RX:
        if t == ms: ble_rx.extend(h)
    if peer is not None and peer_q:              # the peer's bytes at the UART's rate
        peer_credit = min(peer_credit + a.ble_peer_rate / 1000.0, 64.0)
        n = min(int(peer_credit), len(peer_q))
        ble_rx.extend(peer_q[:n]); del peer_q[:n]; peer_credit -= n
    cr1 = rd32(0x4000440C)
    nvic38 = nvic_enabled(38)                    # ISER1 bit 6 = IRQ 38
    for _ in range(24):                          # USART2 interrupt while enabled and pending
        cr1 = rd32(0x4000440C)
        if not (nvic38 and cr1 & (1 << 13)): break
        if (ble_rx and cr1 & (1 << 5)) or (cr1 & (1 << 7)): call(handler_addr(38), "USART2")
        else: break
    if peer is not None:
        peer_q.extend(peer.step(ms, bytes(ble_tx[peer_seen:]))); peer_seen = len(ble_tx)
        if hasattr(peer, "finished") and peer.finished() and not peer_q:
            print(f"  t={ms} ms: BLE peer finished"); end_ms = ms + 1
            break
    if any(ms == t for t, k, d in presses): beeps[0] = 0      # count beeps from the press on
    if a.screens and str(ms) in a.screens.split(','):
        shadow, glass = bytes(uc.mem_read(0x20007790, 32)), bytes(uc.mem_read(0x40002414, 32))
        if a.glass: print(f"  SCREEN t={ms}: {lcd_decode.render(glass)}")
        else:
            print(f"  SCREEN t={ms} pc {uc.reg_read(UC_ARM_REG_PC):08x}: {lcd_decode.render(shadow)}"
                  + (f"\n      LCD RAM (what the glass shows): {lcd_decode.render(glass)}" if glass != shadow else ""))
    if ms in snap_at:
        b = lambda x: uc.mem_read(x, 1)[0]
        cfg = [dt for t2, kind, st, dt in hy.log if kind == 'W' and st == 0x20 and len(dt) == 20]
        held = [f'{k}@{t}' + (f'/{d}' if d != 150 else '') for t, k, d in presses if t + d + a.snap_delay == ms]
        print(f"  STATE t={ms} after {' '.join(held)}: mode {b(0x2000803c)} range {b(0x2000803d)} manual {b(0x20008042)} "
              f"setup_page {b(0x20008037)} hold {b(0x2000804f)} autohold {b(0x20008050)} rel {b(0x20008058)} minmax {b(0x2000804e)} peak {b(0x200080b7)} "
              f"lpf {b(0x200080b9)} acdc {b(0x200080ba)} ble {b(0x2000802b)} reading {struct.unpack('<i', uc.mem_read(0x20007d64, 4))[0]} "
              f"beeps {beeps[0]} hy {cfg[-1].hex() if cfg else '-'} lcd {bytes(uc.mem_read(0x20007790, 32)).hex()} "
              f"screen {lcd_decode.render(bytes(uc.mem_read(0x20007790, 32)))}")
        beeps[0] = 0
    if a.watch and ms % 100 == 99 and 0x08006124 in seen:
        dv = struct.unpack('<i', uc.mem_read(0x20007d64, 4))[0]; fast = struct.unpack('<i', uc.mem_read(0x20007d68, 4))[0]
        print(f"  t={ms+1:5d} ms  mode {uc.mem_read(0x2000803c,1)[0]} range {uc.mem_read(0x2000803d,1)[0]}  display {dv}  fast {fast}")
for _p, _c in sorted(pc_counts.items()): print(f"PC {_p:08x}: {_c} hits")
print(f"BUZZER: {buz[1]} rising edges, {buz[2]} ms on")
if ble_tx: print(f"BLE TX total {len(ble_tx)} bytes")
if ble_tx and a.ble_all: print(f"BLE TX ALL {bytes(ble_tx).hex()}")
if peer is not None and hasattr(peer, "summary"): print(peer.summary())
if clk: print(clk.summary(rd32(USART2_BRR) & 0xFFFF))
print(f"end t={end_ms} ms at pc 0x{pc & ~1:08x}; {len(writes)} peripheral writes; g_mode={uc.mem_read(0x2000803c,1)[0]} "
      f"g_range={uc.mem_read(0x2000803d,1)[0]} g_rotary=0x{uc.mem_read(0x20008066,1)[0]:02x}")
if a.coverage:
    with open(a.coverage, 'a') as cf: cf.write(''.join(f'{b:x}\n' for b in sorted(blocks)))
if a.coverage_ranges:
    with open(a.coverage_ranges, 'a') as cf: cf.write(''.join(f'{b:x} {n:x}\n' for b, n in sorted(branges)))
if sd:
    print(f"SD: {sd.reads} block reads, {sd.writes} block writes, bus {sd.bus_width}-bit, {len(sd.log)} events")
    if sd.blocks > 8388608: print(f"SD: blocks touched {sorted(sd.touched)[:12]}{' ...' if len(sd.touched) > 12 else ''}; past 4 GiB: {sum(1 for k in sd.touched if k >= 8388608)}")
    if a.sd_log:
        for t, e in sd.log: print(f"  sd t={t:8.1f} {e}")
    if a.sd_out: open(a.sd_out, "wb").write(bytes(sd.data))
    if a.sd_file_out:
        import subprocess, os
        subprocess.run(["/usr/bin/cp", "--sparse=always", a.sd_file, a.sd_file_out], check=True)
        fd = os.open(a.sd_file_out, os.O_WRONLY)
        os.pwrite(fd, bytes(sd.data[:1 << 20]), 0)
        for k, b in sd.sparse.items(): os.pwrite(fd, b, k * 512)
        os.close(fd)
if a.ee_out: open(a.ee_out, "wb").write(bytes(uc.mem_read(0x08080000, 0x3000)))
if rtc_writes: print(f"RTC writes: {rtc_writes[:12]}")
if ble_tx: print(f"BLE TX {len(ble_tx)} bytes: {bytes(ble_tx[:76]).hex()}")
print(f"HY3131: {len(hy.log)} transactions; config image R20..R33 now {hy.config_image().hex()}")
for t, kind, start, data in hy.log:
    if kind == 'R' and start == 0 and len(data) == 0x34 and not a.hy_only: continue
    nm = HY_NAMES.get(start, 'R%02X' % start)
    print(f"  hy t={t:8.1f} {kind} @{start:02X} {nm:6s} {data.hex()}")
if a.hy_only: sys.exit(0)
boot_end = seen.get(0x08006124, None)
last = None
shown = 0
for t, h, wpc, x, size, v in writes:
    if not a.all and (boot_end is None or t < boot_end): continue
    if h is not None and x in (TIM2 + 0x10, TIM3 + 0x10): continue      # handlers clearing their flag
    key = (name(x), v, h)
    if key == last: continue
    last = key; shown += 1
    if shown > 400: print("  ..."); break
    print(f"  t={t:8.1f} {('['+h+']') if h else '':6s} pc {wpc:08x}  {name(x):32s} <- 0x{v:0{size*2}x}")
