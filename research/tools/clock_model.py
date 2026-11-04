# SPDX-License-Identifier: MIT
"""Clock-accuracy models for emu_run.py (all off unless --msi-error is given; a run without it is unchanged).

The emulator's time base is instructions: one 1 ms slice = 4194 instructions = 1 ms at the nominal MSI range 6
(4.194304 MHz). --msi-error PCT says how far the real MSI is from nominal, relative to the 4 MHz HSE crystal (X2):
everything clocked from MSI (the CPU, the timers, the UART's bit timing) runs at nominal * (1 + PCT/100) in real
time, while crystal-derived clocks do not. Three things follow, and are modeled here:

- RCC_CR.RTCPRE (bits 30:29, HSE_RTC = HSE / 2, 4, 8, 16): RM0038 6.3.1, "This prescaler cannot be modified if HSE
  is enabled (HSEON = 1)". A write while HSEON is set keeps the old RTCPRE (the firmware must set it with HSE off).
- SYSCLK follows RCC_CFGR.SW (the emulator reports SWS = SW): MSI (nominal x (1 + error)), HSI 16 MHz, HSE 4 MHz, or
  the PLL (PLLSRC HSI/HSE, PLLMUL 3..48, PLLDIV 2..4). One instruction = one SYSCLK cycle, whatever the source.
- TIM11 input capture (RM0038 6.2.14, 19.4.x; TIM11_OR.TI1_RMP: 00/11 GPIO, 01 MSI, 10 HSE_RTC; TI1_RMP_RI = bit 3
  overrides to the routing interface, modeled as no input). The counter counts timer clocks = SYSCLK cycles /
  (PSC + 1) (the APB2 timer clock is SYSCLK here: no APB2 prescaler on this firmware), wraps at ARR.
  HSE_RTC reaches TIM11 only with --hse-rtc-tim11: on the real meter (2026-11-03, RTCSEL = LSI) TIM11 never
  captured it although HSE was ready, so by default it is not connected. With CC1S = 01
  (TI1) and CC1E set, every ICPSC-th rising edge of the selected input (1, 2, 4, 8) latches the counter into CCR1
  and sets CC1IF; an edge while CC1IF is still set also sets CC1OF. Reading CCR1 clears CC1IF (RM0038 19.4.5);
  SR is rc_w0. HSE_RTC has edges only while HSE is on (HSEON; the emulator already reports HSERDY with it).
  Edge times are exact (fractional instruction positions); "has the edge happened" is decided at block
  granularity, which is what a polling loop can see anyway.
- USART2 bit timing: the meter sends at f_MSI_real / BRR (oversampling 16; BRR is USARTDIV * 16). A byte to the BLE
  module is corrupted (one or two bit errors) with a probability that rises linearly from 0 at |error| = tol_lo to 1
  at tol_hi (percent, --uart-tol, default 1.5:4.0): a baud-mismatched receiver mis-samples late bits. Seeded
  (--clock-seed), so a run repeats.
"""
import random

RCC_CR, RCC_CFGR, RCC_APB2ENR = 0x40023800, 0x40023808, 0x40023820
SDIO = 0x40012C00
TIM11 = 0x40011000
USART2_BRR = 0x40004408
MSI_NOMINAL = 4194304.0
HSE_HZ = 4000000.0
NOMINAL_BAUD = 19200.0


class ClockModel:
    def __init__(self, msi_error_pct, uart_tol="1.5:4.0", seed=1, hse_hz=HSE_HZ, log=print, rd32=None,
                 hse_rtc_tim11=False, read_skew=6.0):
        self.err = msi_error_pct / 100.0
        self.f_msi = MSI_NOMINAL * (1.0 + self.err)
        self.hse_hz = hse_hz
        lo, hi = (float(x) for x in uart_tol.split(":"))
        self.tol_lo, self.tol_hi = lo / 100.0, hi / 100.0
        self.rng = random.Random(seed)
        self.log = log
        self.rd32 = rd32                                # RCC_CFGR for the SYSCLK source (None: always MSI)
        self.hse_rtc_tim11 = hse_rtc_tim11
        self.read_skew = read_skew                      # SYSCLK cycles that pass per TIM11 register read
        self.rtcpre = 0                                 # RCC_CR bits 30:29 as the hardware holds them
        self.hseon = 0
        self.icount = 0.0                               # instructions counted while TIM11 runs
        self.t = {"CR1": 0, "DIER": 0, "SR": 0, "CCMR1": 0, "CCER": 0, "PSC": 0, "ARR": 0xFFFF, "CCR1": 0, "OR": 0,
                  "CNT0": 0}
        self.cnt_base_ic = 0.0                          # icount at which CNT had the value CNT0
        self.next_edge = None                           # icount of the next counted (prescaled) input edge
        self.captures = 0
        self.tx_bytes = self.tx_corrupt = 0
        self.rtcpre_blocked = 0
        self.sw_switches = 0
        self.sdio_no_pll = 0                            # SDIO register accesses while RCC_CR.PLLON is clear

    # ---- time ------------------------------------------------------------------------------------------------
    def running(self):
        return self.t["CR1"] & 1

    def add_instructions(self, n):
        """Called per executed basic block while TIM11 runs (n = instructions in the block)."""
        self.icount += n
        self._edges()

    PLLMUL = (3, 4, 6, 8, 12, 16, 24, 32, 48)

    def sysclk_hz(self):
        if self.rd32 is None:
            return self.f_msi
        cfgr = self.rd32(RCC_CFGR)
        sw = cfgr & 3
        if sw == 0:
            return self.f_msi
        if sw == 1:
            return 16000000.0
        if sw == 2:
            return self.hse_hz
        src = self.hse_hz if cfgr >> 16 & 1 else 16000000.0
        mul = self.PLLMUL[min(cfgr >> 18 & 0xF, 8)]
        div = (cfgr >> 22 & 3) + 1
        return src * mul / max(div, 2)

    def _input_hz(self):
        rmp = self.t["OR"]
        if rmp & 0x8:
            return None                                 # routing interface: not modeled
        sel = rmp & 3
        if sel == 1:
            return self.f_msi
        if sel == 2:
            return self.hse_hz / (2 << self.rtcpre) if (self.hseon and self.hse_rtc_tim11) else None
        return None                                     # GPIO: nothing connected in the emulator

    def _capturing(self):
        return (self.t["CCMR1"] & 3) == 1 and (self.t["CCER"] & 1) and self.running()

    def _edge_period(self):
        """Instructions between two counted input edges (input period x ICPSC), or None."""
        hz = self._input_hz()
        if hz is None:
            return None
        psc_in = 1 << ((self.t["CCMR1"] >> 2) & 3)
        return self.sysclk_hz() / hz * psc_in           # instructions = SYSCLK cycles

    def _cnt_at(self, ic):
        ticks = int((ic - self.cnt_base_ic) // (self.t["PSC"] + 1))
        arr = self.t["ARR"] or 0xFFFF
        return (self.t["CNT0"] + ticks) % (arr + 1)

    def _arm(self):
        """Schedule the first counted edge when capture (or its input) has just become active: the input's phase
        is unrelated to that moment."""
        per = self._edge_period() if self._capturing() else None
        if per is None:
            self.next_edge = None
        elif self.next_edge is None:
            self.next_edge = self.icount + self.rng.uniform(0.0, per)
        return per

    def _edges(self):
        per = self._arm()
        if per is None:
            return
        while self.next_edge <= self.icount:
            if self.t["SR"] & 0x2:
                self.t["SR"] |= 0x200                   # CC1OF: a capture while CC1IF was still set
            self.t["CCR1"] = self._cnt_at(self.next_edge)
            self.t["SR"] |= 0x2                         # CC1IF
            self.captures += 1
            self.next_edge += per

    # ---- registers ---------------------------------------------------------------------------------------------
    OFF = {0x00: "CR1", 0x0C: "DIER", 0x10: "SR", 0x18: "CCMR1", 0x20: "CCER", 0x28: "PSC", 0x2C: "ARR",
           0x34: "CCR1", 0x50: "OR"}

    def on_read(self, x, rd32, wr32):
        """Fix up a register the firmware is about to read; True when this model owns it."""
        if x == RCC_CR:
            v = rd32(x)
            wr32(x, (v & ~(3 << 29)) | (self.rtcpre << 29))
            return False
        if SDIO <= x < SDIO + 0x100 and not (rd32(RCC_CR) >> 24 & 1):
            self.sdio_no_pll += 1                       # RM0038 6.2: the SDIO clock comes from the PLL VCO                                # emu_run still adds the ready flags
        if TIM11 <= x < TIM11 + 0x54:
            off = x - TIM11
            if self.running() and self.read_skew > 0:
                # the block hook counts a block's instructions at its start; time also passes between two reads in
                # the same block (a bus access takes cycles), so a capture can land between reading SR and CCR1
                self.icount += self.rng.uniform(0.0, self.read_skew)
            self._edges()
            if off == 0x24:
                wr32(x, self._cnt_at(self.icount) if self.running() else self.t["CNT0"])
            elif off in self.OFF:
                v = self.t[self.OFF[off]]
                if off == 0x34:
                    self.t["SR"] &= ~0x2                # reading CCR1 clears CC1IF
                wr32(x, v)
            else:
                wr32(x, 0)
            return True
        return False

    def on_write(self, x, v, rd32):
        """Track a register write before it lands; True when this model owns the register."""
        if x == RCC_CFGR:
            if (v & 3) != (rd32(x) & 3):
                self.next_edge = None                   # SYSCLK source switch: edge spacing in cycles changes
                self.sw_switches += 1
            return False
        if x == RCC_CR:
            old = rd32(x)
            hseon_old = old >> 16 & 1
            want = v >> 29 & 3
            if want != self.rtcpre:
                if hseon_old:
                    self.rtcpre_blocked += 1
                    self.log(f"  CLOCK RCC_CR.RTCPRE write {want} ignored: HSE is on (RM0038: change it with HSEON = 0)")
                else:
                    self.rtcpre = want
            self.hseon = v >> 16 & 1
            if not self.hseon:
                self.next_edge = None                   # HSE_RTC stops; it restarts with a new phase
            self._arm()
            return False
        if TIM11 <= x < TIM11 + 0x54:
            off = x - TIM11
            self._edges()
            if off == 0x10:
                self.t["SR"] &= v                       # rc_w0
            elif off == 0x14:
                if v & 1:                               # UG: counter reset (PSC applied)
                    self.t["CNT0"], self.cnt_base_ic = 0, self.icount
                if v & 2:                               # CC1G: software capture
                    self.t["CCR1"] = self._cnt_at(self.icount)
                    self.t["SR"] |= 0x2
            elif off == 0x24:
                self.t["CNT0"], self.cnt_base_ic = v & 0xFFFF, self.icount
            elif off == 0x00:
                if (v & 1) and not self.running():
                    self.cnt_base_ic = self.icount      # counting starts now
                elif not (v & 1) and self.running():
                    self.t["CNT0"] = self._cnt_at(self.icount)
                self.t["CR1"] = v & 0x3FF
            elif off in self.OFF:
                self.t[self.OFF[off]] = v & (0xFFFF if off in (0x28, 0x2C, 0x34) else 0xFFFFFFFF)
            if off in (0x18, 0x50, 0x28):
                self.next_edge = None                   # input, prescaler or timer clock changed: new phase
            self._arm()
            return True
        return False

    # ---- UART ------------------------------------------------------------------------------------------------
    def baud_error(self, brr):
        if brr == 0:
            return None
        return (self.f_msi / brr) / NOMINAL_BAUD - 1.0

    def tx_byte(self, b, brr):
        """A byte the meter sends to the BLE module, as the module receives it."""
        self.tx_bytes += 1
        e = self.baud_error(brr)
        if e is None:
            return b
        p = (abs(e) - self.tol_lo) / (self.tol_hi - self.tol_lo)
        if p <= 0 or self.rng.random() >= min(p, 1.0):
            return b
        self.tx_corrupt += 1
        bits = 1 if self.rng.random() < 0.7 else 2
        for _ in range(bits):
            b ^= 1 << self.rng.randrange(4, 8)          # late bits are the ones a mismatched receiver gets wrong
        return b

    def summary(self, brr):
        e = self.baud_error(brr)
        es = "n/a" if e is None else f"{e * 100:+.3f}%"
        return (f"CLOCK MSI {self.f_msi:.0f} Hz ({self.err * 100:+.3f}%), USART2 BRR {brr} -> baud error {es}; "
                f"bytes to BLE {self.tx_bytes}, corrupted {self.tx_corrupt}; TIM11 captures {self.captures}; "
                f"RTCPRE {self.rtcpre} (blocked writes {self.rtcpre_blocked}); SYSCLK switches {self.sw_switches}; "
                f"SDIO accesses with the PLL off {self.sdio_no_pll}")
