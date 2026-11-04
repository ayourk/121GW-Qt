# SPDX-License-Identifier: MIT
"""HY3131 pin-level model for the 121GW emulator (from DS-HY3131-V10_EN, sections 7-10).

Bus (sec. 7): CS low starts a frame; CK idles low; DI is latched on the CK rising edge, DO changes on the falling
edge (we present the next bit at the rising edge, which is when the stock driver samples it: CK high, read PF3).
First byte = ADDRESS<6:0>, R/W (1 = read). Write: data bytes follow, address auto-increments. Read: one blank clock,
then data from ADDRESS, auto-incrementing. CS high ends the frame. While CS is high, DO is the IRQ output: high when
(INTF & INTE) != 0 (sec. 10). INTF (0x1E) is cleared when read and can only be written to 0 (bits cleared).
Registers: 0x00-0x1D data (AD1 24-bit, AD2, LPF, RMS 40-bit, PKHMIN/MAX, CTSTA, CTC/CTB/CTA), 0x1E INTF, 0x1F
INTE, 0x20-0x36 configuration (R20..R36), 0x37 test (write 0 only).

The model logs every transaction (time, kind, address, bytes) and produces events: AD1F at the AD1 output rate
(from R22: 4.9152 MHz / 30 / OSR1, halved when chopping) while ENAD1 (R23 bit 7) is set, with a fixed AD1 value.
CTA<23:8> is writable while ENCTR = 0 and ENCTR 1 -> 0 clears the counters (sec. 14); 0x60 to 0x37 resets the
chip (sec. 11). It does not model the analog side, AD2/LPF/RMS events or the counter's counting.
"""

INTF, INTE = 0x1E, 0x1F
AD1F, AD2F, CTF, LPFF, RMSF = 0x04, 0x02, 0x01, 0x08, 0x10
NAMES = {0x00: "AD1", 0x03: "AD2", 0x06: "LPF", 0x09: "RMS", 0x0E: "PKHMIN", 0x11: "PKHMAX", 0x14: "CTSTA",
         0x15: "CTC", 0x18: "CTB", 0x1B: "CTA", 0x1E: "INTF", 0x1F: "INTE"}
NAMES.update({r: "R%02X" % r for r in range(0x20, 0x38)})


class HY3131:
    def __init__(self, ad1_value=0x012345, freq_hz=1000.0, duty=0.5):
        self.freq_hz, self.duty = freq_hz, duty
        self.reg = bytearray(0x38)
        self.cs = 1; self.ck = 0; self.di = 0
        self.log = []                 # (time_ms, "W"/"R", start address, bytes)
        self.now = 0.0
        self.ad1_value = ad1_value & 0xFFFFFF
        self._frame_reset()

    # --- bus -----------------------------------------------------------------------------------------------
    def _frame_reset(self):
        self.bits = 0; self.byte = 0; self.phase = "addr"; self.addr = 0; self.rw = 0
        self.out_byte = 0; self.out_bits = 0; self.do = 0; self.frame = bytearray(); self.start = 0
        self.read_intf = False

    def pins(self, ck, cs, di):
        """Call after any change of the three MCU output pins (PF0 CK, PF1 CS, PF2 DI)."""
        if cs != self.cs:
            if cs == 0:                                   # frame start
                self._frame_reset()
            else:                                         # frame end
                if self.phase in ("wdata", "rdata") and self.frame:
                    self.log.append((self.now, "W" if self.rw == 0 else "R", self.start, bytes(self.frame)))
                if self.read_intf: self.reg[INTF] = 0     # INTF cleared after being read
                self._frame_reset()
            self.cs = cs
        if self.cs == 0 and ck == 1 and self.ck == 0:     # rising edge
            self._rise()
        self.ck = ck; self.di = di

    def _rise(self):
        if self.phase == "addr":
            self.byte = (self.byte << 1) | (self.di & 1); self.bits += 1
            if self.bits == 8:
                self.addr = self.byte >> 1; self.rw = self.byte & 1; self.start = self.addr
                self.bits = 0; self.byte = 0
                self.phase = "blank" if self.rw else "wdata"
        elif self.phase == "blank":                       # the read's empty ninth clock
            self.phase = "rdata"; self._load_out(); self.do = (self.out_byte >> 7) & 1
        elif self.phase == "wdata":
            self.byte = (self.byte << 1) | (self.di & 1); self.bits += 1
            if self.bits == 8:
                self._write(self.addr, self.byte); self.frame.append(self.byte)
                self.addr = (self.addr + 1) & 0x3F; self.bits = 0; self.byte = 0
        elif self.phase == "rdata":
            self.do = (self.out_byte >> (7 - self.out_bits)) & 1
            self.out_bits += 1
            if self.out_bits == 8:
                self.frame.append(self.out_byte)
                self.addr = (self.addr + 1) & 0x3F; self._load_out()

    def _load_out(self):
        # DO is NOT changed here: at the 8th rising edge it still holds bit 0 of the byte being read, which the MCU
        # samples while CK is high; the next byte's bit 7 appears at the next rising edge.
        a = self.addr % len(self.reg)
        if a == INTF: self.read_intf = True
        self.out_byte = self.reg[a]; self.out_bits = 0

    def _write(self, a, v):
        a %= len(self.reg)
        v &= ~getattr(self, 'nolatch', {}).get(a, 0) & 0xFF   # test knob: reserved bits that do not latch
        if a == INTF: self.reg[INTF] &= v                 # writable to 0 only
        elif a in (0x1C, 0x1D):                           # CTA<23:8>: SPI-writable while ENCTR = 0 (sec. 14)
            if not self.reg[0x20] & 0x02: self.reg[a] = v
        elif a == 0x20:
            was = self.reg[0x20] & 0x02
            self.reg[0x20] = v
            if was and not v & 0x02:                      # ENCTR 1 -> 0: clear CTA<7:0>, CTB, CTC, CTBOV
                self.reg[0x1B] = 0
                for r in range(0x15, 0x1B): self.reg[r] = 0
                self.reg[0x14] &= ~0x01
        elif a == 0x37 and v == 0x60:                     # chip reset (sec. 11 (7))
            self.reg = bytearray(0x38); self.resets = getattr(self, 'resets', 0) + 1
        elif a >= 0x21 or a == INTE: self.reg[a] = v      # other data registers are read-only
        # 0x37 test register: stored but meaningless

    def dout(self):
        """Level of DO (PF3) as the MCU sees it now."""
        if self.cs:                                       # IRQ output mode
            return 1 if (self.reg[INTF] & self.reg[INTE] & 0x1F) else 0
        return self.do

    # --- events --------------------------------------------------------------------------------------------
    def ad1_period_ms(self):
        """AD1 output period from R22 (sec. 13.1 + 11): AD1CLK = 4.9152 MHz / 30; rate = AD1CLK / OSR1, halved when
        AD1CHOP = 1x (chopping)."""
        r22 = self.reg[0x22]
        osr = 256 << (r22 & 7)
        chop = (r22 >> 3) & 3
        rate = 4_915_200 / 30 / osr / (2 if chop >= 2 else 1)
        return 1000.0 / rate

    def _put(self, addr, n, v):
        for i in range(n): self.reg[addr + i] = (v >> (8 * i)) & 0xFF

    def tick_ms(self, ms):
        """Advance time. While ENAD1 (R23 bit 7): an AD1 conversion every ad1_period_ms(), and with it the blocks that
        are enabled: LPF (R29 bit 6) -> LPFF, RMS (R29 bit 7) -> RMSF, AD2 (R26 bit 7) -> AD2F, peak hold (R29 bit 2)
        values. Counter (R20 ENCTR, sec. 14): gate = (0x1000000 - CTA preset) / 4.9152 MHz; at its end CTB = input
        cycles, CTC = high time in SYSCLK counts, CTF set, counting stops."""
        self.now = float(ms)
        if getattr(self, "real_gate", False):
            self._tick_real_gate(ms)
        elif self.reg[0x20] & 0x02:
            if getattr(self, "_ct_end", None) is None:
                preset = self.reg[0x1B] | self.reg[0x1C] << 8 | self.reg[0x1D] << 16
                gate_s = (0x1000000 - preset) / 4_915_200
                self._ct_end, self._gate = ms + gate_s * 1000, gate_s
            elif self._ct_end is not False and ms >= self._ct_end:
                f, duty = self.freq_hz, self.duty
                import math
                cycles = max(1, math.ceil(f * self._gate))         # the chip counts on to a whole input cycle (sec. 14)
                t_total = cycles / f
                extra = round((t_total - self._gate) * 4_915_200)  # SYSCLK counts past the CTA overflow
                self._put(0x18, 3, cycles & 0xFFFFFF)                                 # CTB: whole cycles
                self._put(0x15, 3, int(duty * t_total * 4_915_200) & 0xFFFFFF)        # CTC: high time
                self._put(0x1B, 3, extra & 0xFFFFFF)                                  # CTA final
                self.reg[INTF] |= CTF
                self._ct_end = False                                                  # done until re-armed
        else:
            self._ct_end = None
        if not self.reg[0x23] & 0x80 and not self.reg[0x26] & 0x80:
            self._next_ad1 = None
            return
        if getattr(self, "_next_ad1", None) is None:
            self._next_ad1 = ms + self.ad1_period_ms()
        if ms >= self._next_ad1:
            self._next_ad1 += self.ad1_period_ms()
            v = self.ad1_value
            if self.reg[0x23] & 0x80:
                self._put(0x00, 3, v); self.reg[INTF] |= AD1F
                self.ad1_events = getattr(self, "ad1_events", 0) + 1
            if self.reg[0x26] & 0x80:
                self._put(0x03, 3, v >> 5); self.reg[INTF] |= AD2F
            if self.reg[0x29] & 0x40:
                self._put(0x06, 3, v >> 5); self.reg[INTF] |= LPFF
            if self.reg[0x29] & 0x80:
                self._put(0x09, 5, (v * v) & 0xFFFFFFFFFF); self.reg[INTF] |= RMSF
            if self.reg[0x29] & 0x04:
                self._put(0x0E, 3, (-(v >> 5)) & 0xFFFFFF); self._put(0x11, 3, (v >> 5) & 0xFFFFFF)

    def _tick_real_gate(self, ms):
        """Counter with edge-synchronized gate timing (real_gate = True; emu_run --ctr-real). The input is a free-running
        signal whose phase advances with freq_hz (changeable at any time, 0 = no signal: no edges). ENCTR 0 -> 1 arms
        the counter; counting (CTA and CTB) starts at the next rising input edge, the CTA preset runs out after the gate
        time, then counting goes on to the next rising edge (sec. 14 step 3: "until CTB records the whole cycle
        amount") and only then sets CTF. So CTF comes late on slow signals, and never while there are no edges.
        CTB = whole cycles, CTA final = SYSCLK counts past the overflow, CTC = duty x total counting time (duty at the
        end), CTBOV (CTSTA bit 0) when CTB overflows 24 bits. Edge times are exact within the 1 ms tick (frequency
        constant within a tick); the flag is raised at the first tick at or after the closing edge."""
        import math
        prev_t = getattr(self, "_ph_t", None)
        ph = getattr(self, "_ph", 0.0)
        f = max(0.0, float(self.freq_hz))
        if prev_t is None: prev_t = float(ms)
        dt = (ms - prev_t) / 1000.0
        ph_new = ph + f * dt
        self._ph, self._ph_t = ph_new, float(ms)

        def edge_time(k):                                   # time (ms) of rising edge k inside (prev_t, ms]
            return prev_t + (k - ph) / f * 1000.0
        if not self.reg[0x20] & 0x02:
            self._rg = None
            return
        st = getattr(self, "_rg", None)
        if st is None:                                      # just armed: wait for the first edge
            preset = self.reg[0x1B] | self.reg[0x1C] << 8 | self.reg[0x1D] << 16
            self._rg = st = {"gate": (0x1000000 - preset) / 4_915_200, "k0": None, "t0": None, "done": False}
            return                                          # edges from the next tick on (arming happens this tick)
        if st["done"] or f <= 0 or ph_new <= ph: return
        if st["k0"] is None:
            k = math.floor(ph) + 1
            if k > ph_new: return
            st["k0"], st["t0"] = k, edge_time(k)
        t_gate = st["t0"] + st["gate"] * 1000.0
        if ms < t_gate: return
        k = max(st["k0"] + 1, math.floor(ph) + 1)           # first edge at or after the gate end
        while k <= ph_new and edge_time(k) < t_gate: k += 1
        if k > ph_new: return
        t_end = edge_time(k)
        cycles = k - st["k0"]
        total_s = (t_end - st["t0"]) / 1000.0
        extra = max(0, round((total_s - st["gate"]) * 4_915_200))
        self._put(0x18, 3, cycles & 0xFFFFFF)
        self._put(0x15, 3, int(self.duty * total_s * 4_915_200) & 0xFFFFFF)
        self._put(0x1B, 3, extra & 0xFFFFFF)
        if cycles > 0xFFFFFF: self.reg[0x14] |= 0x01           # CTBOV
        self.reg[INTF] |= CTF
        st["done"] = True

    def config_image(self):
        return bytes(self.reg[0x20:0x34])
