# Defects and design decisions in firmware 2.05, and the roadmap

This document lists every defect we know of in the 121GW's last firmware, 2.05, and the behaviors that look odd but are, or may be, deliberate. Not everything that surprises a reverse engineer is a bug: a meter's designers make choices for safety and usability that only look wrong out of context ([Classification](#classification)). Our firmware keeps stock's deliberate behavior by default; where we offer a change, it is an option, not a fix.

It includes the defects we found and zombu4's list (OF-01 to OF-56, from [zombu4/121GW-Open-Firmware](https://github.com/zombu4/121GW-Open-Firmware), `firmware/ORIGINAL_FIRMWARE_ISSUES.md`). For each defect it gives how it was confirmed, the fix as a binary patch on stock 2.05, the planned change in the C source, and its status. The fixes that change intended behavior, and so need a decision before they ship, are collected in [their own section](#design-decisions-and-behavior-changes). The roadmap is at the end.

## Main findings

- **Not every oddity is a bug.** Items first listed as bugs are now sorted into bugs, design decisions and unclear items ([Classification](#classification)); the functional beeps that ignore "buzzer off" are deliberate.
- **Four fixes run on a production meter today**, as a binary-patched 2.05 ("v2") flashed through the stock SD bootloader: the lost current zero, SD cards over 4 GiB, the 2006 file dates and the temperature rounding on the sub display.
- **Two defects are fixed by neither public firmware** (EEVblog's faithful reconstruction or zombu4's rewrite): addresses wrap past 4 GiB on large SD cards, and every file is created on 2006-02-09 22:30.
- **Every item on zombu4's list has a binary fix written** (our own code, written from their descriptions), except the SD driver item, which belongs to the FatFs upgrade. Those fixes were tested on an emulator that was later found to have a Unicorn bug that loses the Thumb IT state ([tools/README.md](tools/README.md#unicorn-loses-the-thumb-it-state-under-memory-hooks)). Treat them as provisional: the binary patches are superseded by the C source rebuild, and each fix is re-implemented and re-verified there, on the fixed emulator.
- **Every known 2.05 defect except the LPF items was already in 2.04.** 2.05 introduced one: a 32-bit overflow in the restored LPF compensation ([versions.md](versions.md#205)).
- **The project now builds from C source.** The base is EEVblog's MIT reconstruction of 2.05, and the binary patches serve as the specification: each one records the root cause, a scenario that shows the bug and the expected fixed output. Fixes go into the source one at a time.

## Classification

Every item is in one of three groups. "Bug" is reserved for the first.

| Group | Meaning | Items |
|---|---|---|
| **Bug** | Wrong results, lost or corrupted data, or a hang; nothing plausible was intended | B-01 negative readings 2 counts off, B-02 temperature rounding, B-03 LPF overflow, B-05 stability filters, B-06 lost current zero, B-07 4 GiB wrap, B-08 2006 file dates, B-11 SD errors ignored, B-12 single-sector reads; OF-22 C to F for negatives, OF-25 AC+DC wraparound, OF-26 LPF x10 below 20 Hz, OF-36 calibration capture surviving a range change, OF-45 the Bluetooth send hang, OF-52 battery gauge reads 0.3-0.4 V low, OF-53/54 lost frequency flags, and the other OF items marked as bugs below |
| **Design decision** | Deliberate, or defensible as deliberate; kept by default | B-15 functional beeps ignore "buzzer off", B-14 calibration mode clears the current zeros (weak evidence), B-10 logging buffered 60 rows, B-13 the 600,000-row log limit; OF-30 LowZ shows 0.0 while it settles, OF-07 BEEP-mode calibration accepted, OF-08 (as B-14) |
| **Unclear** | Needs the designers' intent or a bench measurement | B-04 Ohm range 6 zero divided by 4, B-09 CAL.BIN coverage of the current-zero trims, OF-23 DIODE resets to 3 V at entry, OF-27 VA table column 7, OF-29 VA DC sign, OF-38 contrast forced to 14 below 12 |

**Why the buzzer item moved.** "Buzzer off" silences key-press and dial-change feedback. The beeps that ignore it carry information: continuity is "beep on low resistance", the diode test beep serves several functions, auto-hold must confirm a capture audibly, and a high-voltage warning should never be silenceable. A forum reader pointed this out after our first write-up listed it as a bug; we agree.

**Whose intent.** UEi (Kane) designed the meter and wrote its firmware; EEVblog passed requests and answers on and said in 2018 that it had "almost no real visibility into their internal firmware environment". The evidence for each verdict (quotes from manuals, forum posts by EEVblog staff and Dave's videos, with dates) is collected in [history.md](history.md) and its sources. Highlights: firmware 1.04-1.09 *did* silence continuity with buzzer off, users objected ("should... be on all the time", 2018-03-07) and 1.10 changed it back; the 60-row log buffer follows ST's recommendation passed on by UEi (2020-02-10); the manual puts LOW BAT "below approx. 4.2 V", while meters show it at about 4.3 V and read the battery 0.3-0.4 V low, so OF-52 is a bug.

**B-16 is withdrawn.** It described a variant-flag mismatch after an Ohm range-6 calibration. The coverage work proved the variant-marker write it rests on is dead code (it can never execute), so the mismatch cannot happen.

## Key

**Confirmed:**

| Code | Meaning |
|---|---|
| **M** | Seen on a production meter (its SD card, its CAL.BIN, or Bluetooth telemetry from a patched image). |
| **E** | Reproduced in the emulator with a scenario. |
| **E\*** | Reproduced in the emulator before the Unicorn IT-state fix; re-verified when ported to the source rebuild. |
| **U** | Shown by calling the function directly in the emulator (`emu_call.py` or a unit harness). |
| **C** | Read from the disassembly or decompile only. |

**Status:**

| Status | Meaning |
|---|---|
| **on meter** | In the v2 binary image running on a production meter. |
| **binary, emulated** | Binary patch built and emulator-verified (fixed output plus an unchanged-elsewhere trace), not flashed. |
| **binary, re-run** | Binary patch built and verified on the pre-fix emulator; superseded by the source rebuild, where the fix is re-implemented and re-verified. |
| **planned** | Fix designed, not built. |
| **deferred** | Not reachable from any 2.05 caller, or needs a bench check first. A patch may exist. |
| **later** | Understood, not scheduled. |

"Source" names the file in the C reconstruction ([firmware/](../firmware/)) that holds the function. The planned source change is the binary fix's logic written as C in that function, unless the row says otherwise.

## Defects we found

Items marked with an OF number were found independently by zombu4 too.

### Wrong or misleading readings

| # | Defect | Confirmed | Cause (2.05 address) | Binary fix | Source change | Status |
|---|---|---|---|---|---|---|
| B-01 | **Negative readings come out 2 counts toward zero.** With a real calibration, -1,220,800 raw reads -1.9998 V instead of -2.0000 V. (OF-14, OF-15) | E (with a real meter's calibration record) | The 24-bit sign conversion is off by one (`HY_AD1Signed` 0x080062f8), and `trunc(x + 0.5)` rounds negatives toward zero (`Cal_DivideGain` 0x0800f3b8) | `neg-round`: raw - 0x1000000 including 0x800000, and round half away from zero (patches/neg_round.s at 0x08029740). Neutral calibration: -20000 gives -2.0000 (stock -1.9998) | measure_ad1.c, measure_scale.c | binary, emulated |
| B-02 | **Temperature tenths rounded on the wrong digit.** The shown tenth is off by 0.1 either way (77.45 F shows 77.4, 77.50 shows 77.6). (OF-21) | E, U | Rounds on `(x / 10) % 10` instead of the dropped digit `x % 10` (`Temp_ToDisplayTenths` 0x08018ad8; main TEMP value in `Mode_Temp_OnAD1` 0x0800788a-0x0800789e) | `temp-round`: 18 bytes in place | display_symbols.c, measure_funcs.c | **on meter** (sub display); main TEMP value still open |
| B-03 | **1 kHz LPF: the 32-bit product overflows on large over-range** (new in 2.05). Shows "-OFL", or a plausible 2.0503 V at 2 kHz instead of OFL. | E | `reading * k` as a signed 32-bit product (`ACFreq_LpfCompensate` 0x08006d40) | part of `lpf-comp` (see OF-26): `smull` and a 64-bit divide by 10000 | ac_response.c | binary, re-run |
| B-04 | **Unclear: Ohm range 6 zero possibly over-corrected.** 4x the shorted-lead mean is subtracted; the divide by 4 could be intended scaling. | C | The offset capture divides a 16-sample sum by 4 (`Ohm_R6Correction` 0x08007d1e) | none yet | measure_funcs.c | planned: bench check first (calibrate, read a low value on range 6) |
| B-05 | **Stability filters count the newest value twice.** They write the new value to history[0] before shifting, so the previous newest value is dropped. Found by the measurement-area agent. | C | both stability filters (11 call sites) | none | filters.c | later (triage) |

### Data loss and storage

| # | Defect | Confirmed | Cause | Binary fix | Source change | Status |
|---|---|---|---|---|---|---|
| B-06 | **Second long REL loses the saved current zero.** A long REL on the lowest µA/mA/A range stores a zero trim in EEPROM. The running trim accumulates, but EEPROM gets only the latest residual (~0), so the next power-on forgets the zero. (OF-09) | E | `Key_LongPress` 0x0801622c stores `g_reading` instead of the accumulated trim (0x20007e0c) | `zero-trim`: 1 byte at 0x0801644a (load the trim's literal) | key_handlers.c | **on meter** |
| B-07 | **SD cards over 4 GiB wrap to the start of the card.** Every access past 4 GiB lands in the MBR, FAT or first files. On an emulated 2 TB FAT32 card, stock 2.05 writes log data into the wrong cluster about 3.2 GiB in. | E (8 GiB and 2 TB card images) | `disk_read` 0x08023f7e and `disk_write` 0x08023fb8 build the byte address as `sector << 9` in 32 bits with the high word 0 | `big-card`: 2 x 2 bytes, high word = `sector >> 23`. Exact for single-sector writes (all the logger does); a multi-sector write straddling 4 GiB stays wrong | diskio.c: a full 64-bit byte address (or block addressing for SDHC) | **on meter** |
| B-08 | **Every file is created on 2006-02-09 22:30.** A log cut short by power-off keeps a 2006 modified time too. | M (files on a meter's card), E | `get_fattime` 0x0802402e returns FatFs's sample constant; `File_SetTimestamp` fixes only the modified time, and only on a normal close | `fattime`: entry branches to 44 bytes appended at 0x08029700 that pack the clock-chip time | diskio.c | **on meter** |
| B-09 | **Unclear: CAL.BIN misses the current-zero trims** (EEPROM 0x4D0-0x4FF), so a restore loses them. Whether CAL.BIN was meant to cover them is not documented; the long-REL zero itself appears in no manual. (OF-12) | M (a production meter's CAL.BIN), E | `CalFile_Save` 0x0801cd14 / `CalFile_Load` 0x0801cf06 copy 1,200 bytes (0x000-0x4AF) | patches/c/cal.c: 1,280-byte CAL.BIN; old 1,200-byte files still load and leave the trims alone | sd_log.c | binary, emulated |
| B-10 | **Design decision: logging is buffered 60 rows at a time**, so a power-off while logging loses up to 59 rows and the MAX/MIN footer (the dial's OFF position cuts the battery). Buffering spares the card and the battery; a shorter flush interval would be an option, not a fix. | M (a 60-row log with no footer on a meter's card) | rows are buffered 60 at a time in RAM | none | sd_log.c: flush every N rows (a setting; default about 10) | planned |
| B-11 | **SD errors are ignored.** `SD_InitMount` discards `SD_Init`'s result (failures surface later as mount errors), and `disk_write` ignores the driver's return values, so a flaky card fails silently. | C | `SD_InitMount` 0x0801f8ba, `disk_write` | none | sd_card.c, diskio.c: report as an event | planned |
| B-12 | **`disk_read` reads one sector whatever count is asked.** No caller asks for more. (OF-42) | C | `disk_read` | none | part of the FatFs upgrade | later |
| B-13 | **Design decision: logging stops at 600,000 rows.** After 20 failed writes it continues in a new `MMDDNNPP.csv` without a header (the missing header is the one bug-like part). | C | row counter 0x20007d40; `Log_WriteRetry` | none | sd_log.c | later |
| B-14 | **Design decision: calibration mode clears all six current-zero trims.** A fresh calibration should not inherit old zeros; a warning would help. Weak evidence: no manual or post addresses it. (OF-08) | C | boot MODE+REL clears 0x4D0-0x4FF | see OF-08 | modes.c, main.c | binary, re-run (design decision) |

### UI and annoyances

| # | Defect | Confirmed | Cause | Binary fix | Source change | Status |
|---|---|---|---|---|---|---|
| B-15 | **Design decision: functional beeps ignore "buzzer off":** continuity, diode, the auto-hold capture beep, the high-voltage warning and others. "Buzzer off" silences key and dial feedback only ([Classification](#classification)). | C | they call `Buzzer_OnAlways` 0x0800eed0, which skips the setting | none | none: stock behavior kept | kept |
| B-16 | **Withdrawn.** Described a variant-flag mismatch after an Ohm range-6 calibration; the write it rests on is dead code ([Classification](#classification)). | C | — | none | none | withdrawn |

### Notes that are not defects

- **CAL.BIN carries the board-variant markers** (EEPROM 0x1F and 0x26, factory-set). Loading another meter's CAL.BIN copies its markers along with its calibration, and the markers change the front-end setup (0x26 = 0xA5 switches HY3131 PS0 on the top Ohm range). Only restore a meter's own file. Production meters differ here: one meter we read has 0x1F = 0xA5 and 0x26 = 0x00 (M). See [hardware/hy3131.md](hardware/hy3131.md).
- **The HY3131's own RMS result is read and discarded.** True RMS comes from the AD8436. zombu4 lists the wasted computation as OF-16.

## zombu4's list (OF-01 to OF-56)

Credit: zombu4, 121GW-Open-Firmware, `firmware/ORIGINAL_FIRMWARE_ISSUES.md`. Their repository is under a custom non-commercial license, so we read their descriptions for the facts and wrote every patch ourselves. No code was copied. Each item was first confirmed on stock 2.05 in our emulator, then fixed as a patch on a base image (stock + zero-trim, big-card, temp-round, fattime, neg-round, and cal.c for the C items). Each fix was checked with the same scenarios on the base and the fixed image (LCD glass, buzzer, Bluetooth bytes, HY3131 traffic): they differ only as intended.

The binary patches are byte patches (`tools/patch_stock.py --extra bytes.json`, which refuses to apply unless every site holds the expected bytes) and C hooks (`patches/c/build_c.py`: replace one `bl` in stock code with a call to new C code linked at 0x0802a000).

| OF | Defect | Confirmed | Binary fix | Source | Status |
|---|---|---|---|---|---|
| 01 | Ohm r6 table search with no match returns garbage | U (186,003 calls, 0 differences after the patch) | failed search goes to the existing return-0 exit | measure_funcs.c | deferred: unreachable (the function returns early for readings >= 56,000; every lower reading matches) |
| 02 | Counter overflow above 1 MHz stores a never-assigned register | U | store 0, the firmware's own "no frequency" value | measure_counter.c | binary, re-run |
| 03 | BLE VA code from an unset register for non-VA modes | C | other modes send code 0 | ble_packet.c | deferred: unreachable (the caller tests the same mode byte) |
| 04 | dBm with range byte > 3 | C | use the range-3 formula | display_text.c | deferred: unreachable |
| 05 | Contrast outside 10..17 ORs garbage into LCD_FCR | C | take the setting-10 path | lcd_driver.c | deferred: unreachable |
| 06 | Rotary mask with a multi-bit argument | C | arg-0 mask 0xFF | keys.c | deferred: unreachable |
| 07 | Calibration capture accepted in BEEP (writes Ohm r1's record) | E\* | `Cal_CaptureRejected` also rejects mode 10 | key_handlers.c | binary, re-run; **behavior change** |
| 08 | Entering calibration clears all six current-zero trims (B-14) | E\* | clear only the slot being calibrated | main.c, key_handlers.c | binary, re-run; **design decision** |
| 09 | Second long REL loses the zero (B-06) | E | `zero-trim` | key_handlers.c | **on meter** |
| 10 | BLE packet build and transmit not synchronized: a status packet lands inside a key echo | E\* | wait for the frame in flight; mask TIM2 while echoing and building | ble_command.c, irq.c | binary, re-run |
| 11 | First frame after Bluetooth turns on carries the old reading | E\* | build the packet at once on BT on | key_handlers.c | binary, re-run |
| 12 | CAL.BIN misses the current-zero trims (B-09) | M, E | cal.c | sd_log.c | binary, emulated |
| 13 | ADC sample time configured while the ADC is on | E\* (40 of 40 writes with ADON = 1) | ADC off, channel and 384-cycle sample time, ADC on, wait | battery.c | binary, re-run (analog effect not emulated) |
| 14, 15 | Negative readings 2 counts toward zero (B-01) | E | `neg-round` | measure_ad1.c, measure_scale.c | binary, emulated |
| 16 | HY3131 RMS read and discarded | C | skip the computation (the SPI burst is unchanged) | measure_rms.c | binary, re-run |
| 17 | PEAK + 1 kHz LPF together blank the display | E\* | the second key is refused with the double beep | key_handlers.c | binary, re-run |
| 18 | Pulse width truncated: duty kept in per mille (0.2499 instead of 0.2500 ms) | E\* | W = CTC x 10^8 / (ref x CTB), 64-bit, rounded | measure_counter.c | binary, re-run |
| 19 | TEMP converted again in the MEM view (25.0 becomes 33.0) | E\* | convert only on a new mean | measure_funcs.c | binary, re-run |
| 20 | Overload hysteresis survives a mode change (OFL in BEEP after a DCV overload) | E\* | mode change also clears it | range_select.c | binary, re-run |
| 21 | Temperature tenths rounding (B-02) | E | `temp-round` | display_symbols.c | **on meter** (sub display) |
| 22 | `Temp_CToF` negatives as ones' complement (-40.0 C shows -39.9 F; 12,745 of 24,315 values wrong in a -274..1400 C sweep) | U | F = round((9x + 16000) / 5), signed | temperature.c | binary, re-run |
| 23 | RANGE in DIODE saves the 15 V option to EEPROM | E\* | stop saving (option B) | modes.c | binary, re-run; **decision** |
| 24 | Cold junction keeps its last value below -20 C | E\* | bound ±100.0 C both signs | temperature.c | binary, re-run |
| 25 | AC+DC sum of squares wraps at 2^32: large readings shown small, no OL (high) | U, E\* | squares as doubles, sqrt, back to int | measure_derived.c | binary, re-run |
| 26 | 1 kHz LPF reads x10 below 20 Hz or before the first frequency result | E | clamp the frequency to 20 Hz (+ B-03's 64-bit product) | ac_response.c | binary, re-run |
| 27 | VA AC (A jack) ranges 2/3: rows hold 7 points, code interpolates over 8; up to ~700 counts error between 50,000 and 60,000 | C, table data, U | voltage bin 7 uses column 6 (flat above 50,000) | dual_correction.c | binary, re-run; **behavior change**; real VA data needed |
| 28 | Stability filter state shared across 11 call sites (AC+DC of small values wrong: 0.0034 for a true 0.0042) | E\* | per-site state (972 B of RAM) | filters.c | binary, re-run (needs RAM) |
| 29 | VA DC sign handling | E\* | negate, correct, negate back for negative V and I | dual_correction.c | binary, re-run; **behavior change** |
| 30 | LowZ shows 0.0 V for 1-2 s when a voltage appears (high) | E\* (and a 5-update hold-off seen in our own runs) | blank while unconfirmed instead of 0.0 | modes.c, display_update.c | binary, re-run; **behavior change** |
| 31 | Main loop busy-waits on the LCD update request; no sleep per pass | C | proposal only: skip and retry, WFI per pass | lcd_driver.c, main.c | deferred: changes all display timing; needs a meter |
| 32 | BEEP and Hz show "0.00" / "0.000" on entry before the first reading | E\* | settle count 1 for modes 6, 7, 8, 10 (blank first) | range_select.c | binary, re-run |
| 33 | Capacitance range jump shows a wrong value briefly (10.000 nF as 1.0000 nF) | E\* | blank the display on the jump | measure_counter.c | binary, re-run |
| 34 | BLE frames carry reading 0 while the display is blank | E\* | no frame while blank | ble_packet.c | binary, re-run; **behavior change** |
| 35 | Log records store the per-sample value, not the displayed one | E\* | the record loads `g_reading` | sd_log.c | binary, re-run; **behavior change** |
| 36 | A calibration capture survives a mode or range change and writes the wrong record (high) | E\* | a mode change cancels the capture with the refusal beep | range_select.c | binary, re-run |
| 37 | AC calibration: refusal beep after a correct start, and the slot is zeroed | E\* | branch to the existing accept beep | key_handlers.c | binary, re-run |
| 38 | Contrast setting 10 loads as 14 at power-up | E\* | accept 10 | settings.c | binary, re-run; **behavior change** |
| 39 | ADC init structure partly uninitialized (garbage in CR2) | E\* | zero the structure | system_init.c | binary, re-run |
| 40 | TIM2 can set the buzzer pin after the power-off beep | E\* | TIM2/TIM3 off before sleep, buzzer pin low | power.c | binary, re-run |
| 41 | The clock page is drawn from TIM2 and collides with the main loop (0.0000 instead of 1.2345) | E\* | TIM2 only requests; main draws | irq.c, main.c | binary, re-run |
| 42 | `disk_read` reads one sector whatever the count (B-12) | C | none | diskio.c | later (FatFs upgrade) |
| 43 | Held key repeats its long press every ~2.6 s (counters wrap) | E\* | counters saturate at 0xFF | keys.c | binary, re-run |
| 44 | ACmV with LPF: the per-sample value lacks the compensation, so display and bargraph disagree | E\* | apply the ACV step for ACmV too | measure_funcs.c | binary, re-run |
| 45 | Unbounded USART wait inside TIM2 can hang the meter | E\* | bounded send that skips an unclocked USART2 | irq.c, ble_command.c | binary, re-run |
| 46 | dBm of zero shows "- 7295" (log10(0)) | E\* | show OFL beyond ±100,000 | display_update.c | binary, re-run; **display form to choose** |
| 47 | "SdEnn" and calibration prompts right only to 19; low battery shows "SdE" + blanks | E\* | show "LoBAT" | sd_log.c, display_text.c | binary, re-run; **behavior change** (text) |
| 48 | Auto-repeat adds one queued step after release | E\* | drop the queued press once repeat starts | keys.c | binary, re-run; minor **behavior change** |
| 49 | Digit split of values >= 100,000 (latent) | U | reduce the first word mod 10 | display_text.c | binary, re-run |
| 50 | Boot contrast ANDs LCD->FCR instead of setting it (latent; overwritten later) | E\* | `and` to `mov` | lcd_driver.c | binary, re-run |
| 51 | Float/integer helper edge cases (fdiv overflow result, divide by zero) | U | none | iar_math.c | deferred: no 2.05 caller reaches them |
| 52 | Battery gauge linear fit wrong (3.7 V shown for 4.00 V; LOW BAT up to 4.31 V) | E\* | volts = code x 1330 / 4095 (nominal resistors) | battery.c | binary, re-run; **behavior change**; check on a meter |
| 53 | HY3131 interrupt flag raised during the status read is lost (frozen frequency, slow re-arm) | E\* (7 of 42 counter flags lost in 30 s) | read INTF alone, then data; clear only the flags taken | hy3131_ctrl.c | binary, re-run (partly model-dependent) |
| 54 | Counter registers read stale when a flag arrives mid-burst | E\* | same as 53 | hy3131_ctrl.c | binary, re-run |
| 55 | Two MODE long presses needed after wake | E\* | clear the backlight state before sleep | power.c | binary, re-run |
| 56 | Type K table entry at 270 C off by 1 µV (0.02 C) | U | 10970 to 10971 | temperature.c (table) | binary, re-run (optional) |

## Design decisions and behavior changes

Each item changes what the meter visibly does. **Bug fix** items correct a defect and ship by default (listed so the visible change is conscious); **design decision** items keep stock behavior by default and any change is an option; **unclear** items wait for intent or a measurement.

| Item | Class | Choice |
|---|---|---|
| B-01 / OF-14, 15 negative rounding | bug fix | Displayed values change (intended). |
| B-15 buzzer off | design decision | Kept: functional beeps (continuity, diode, auto-hold, high voltage) stay audible. |
| OF-07 | design decision | Refuse calibration in BEEP. |
| OF-08 | design decision | Clear only the slot being calibrated, or keep stock's clear-all with a warning. |
| OF-23 | unclear | Stop saving the diode 15 V option (option B), or restore the saved value on entry. Forcing 3 V on entry looks deliberate (15 V on the leads). |
| OF-27 | unclear | VA top bin flat above 50,000 counts. Better: real VA reference data for the missing point. |
| OF-29 | unclear | VA DC sign correction for negative inputs. |
| OF-30 | design decision | LowZ shows blank instead of 0.0 while unconfirmed. A wrong coupling still shows 0.0 until AC/DC toggles. |
| OF-34 | option | No Bluetooth frames while the display is blank (apps see a gap instead of zeros). |
| OF-35 | option | Logs record the displayed value instead of the per-sample value. |
| OF-38 | unclear | Accept contrast 10 at power-up, or limit the editor to 12-17. |
| OF-46 | bug fix | How dBm of zero is shown (" OFL" with the sub minus). |
| OF-47 | bug fix | Low battery during logging shows "LoBAT". |
| OF-48 | option | Drop the extra queued step after auto-repeat (zombu4 kept 2.05's behavior here). |
| OF-52 | bug fix | Battery gauge from nominal resistors; needs a check against a real meter's battery voltage. |

## How fixes are verified

Every fix needs four things:

1. A scenario that shows the bug on stock 2.05 in the emulator, or a stated reason why it cannot be emulated.
2. The fixed output for the same scenario.
3. A comparison against the previous image showing that nothing else changed. For binary patches this is a trace comparison. For source builds it is `tools/compare_images.py` with `--seeds N`, which separates real differences from timing-dependent ones ([tools/README.md](tools/README.md#compare_imagespy)).
4. For anything flashed, a check on the meter over Bluetooth.

The emulator does not model the analog front end, so OF-13, OF-27, OF-31, OF-52 and B-04 need the bench as well.

## Roadmap

The project changed course on 2026-11-03: the base is now the C source (EEVblog's MIT reconstruction, built in Docker), not binary patches on stock 2.05. The v2 binary stays on the meter until a source build has been flashed and checked. Flashing is only through the stock SD bootloader (`EEVBlog.bin` on the card), which leaves the bootloader and the calibration EEPROM untouched. Stock 2.05 can always be reflashed the same way.

1. **Re-run the zombu4 fixes on the fixed emulator.** Their results become confirmed, changed or not reproduced. This table is updated to match.
2. **Source build equal to stock (done).** Our GCC 14.2.1 build of the reconstruction matches stock 2.05 in EEVblog's tick-synchronized harness: 294,266 peripheral writes identical, no RAM differences outside the stack. In our behavioral comparison, 47 of 54 sampled scenarios are identical and the other 7 are timing artifacts ([firmware/README-fork.md](../firmware/README-fork.md)).
3. **Port the fixes one at a time into the source**, each checked against the previous build with `compare_images.py --seeds`. Order: the four v2 fixes, then B-01, B-09, B-03/OF-26, then the confirmed high-severity bugs (OF-25, OF-36, OF-45, OF-53/54), then the rest. Design decisions keep stock behavior; changes to them come only as options.
4. **FatFs upgrade** (R0.08a to R0.16), done: exFAT, so SDXC cards work as shipped; also closes OF-42 and the multi-sector case of B-07. It runs in the custom firmware on a real meter (card listing, reads, a new file written and read back byte-identical) **[meter]**, and exFAT 64 GB and 2 TB cards pass the card tests in emulation **[emulated]**. The SD bootloader keeps R0.08a and its 4 GiB address wrap, so a firmware update still needs `EEVBlog.bin` on a FAT32 card, below 4 GiB.
5. **Features (v3 plan):**
   - Bluetooth telemetry and file transfer sized for what the meter-to-module line delivers: about 0.3 % of bytes arrive damaged at any rate, stock firmware included, so everything sent needs a checksum and a retry ([firmware/bluetooth.md](firmware/bluetooth.md#6-link-errors-measured)).
   - Events: overload, range and mode changes, jack warnings, HOLD/REL/MIN-MAX, low battery, logging errors.
   - Per-interval min/max/avg of the fast reading.
   - Probe-placement warnings: a lead in a current jack off a current position, a current position with no current lead, an A-jack lead on µA, AC on DCV or DC on ACV, voltage on Ohm.
   - Remote control frames (go to mode/range, settings, one-shot queries).
   - Logging triggers (on change, on threshold) and more frequent flushes (B-10).
6. **Later:** B-04 after a bench check, B-05, B-13, OF-31, and the VA table once real VA reference data exists.
