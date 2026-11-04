# Research log

This is a curated, dated account of how the firmware 2.05 research went: the methods, milestones, corrections and dead ends. It is drawn from the running work log. Results are written up by subject in the other documents ([README.md](README.md)); this page explains how they were reached and what turned out wrong along the way.

## Summary

- Two days of concentrated work (2026-11-02 and 11-03) followed an earlier exploratory pass. The work took the stock image to a fully named Ghidra decompile, built an emulator that runs it with modeled hardware, compared all 21 releases, fixed four defects on a production meter, and moved to building from C source.
- Corrections worth knowing about: an emulator bug made every EEPROM write fail until it was found. The coverage figure was overstated (96.6% reported, 76.2% real). A Unicorn bug invalidated some results, which are being re-run. The Ohm range-6 variant switch is PS0, not SS6. FatFs is R0.08a, not R0.09.

## Before 2026-11-02: first pass

- An exploratory decompile with Ghidra 12.1.3 and text-substituted names established the basics: the dial and jack logic, the hidden boot keys, and the F8 Bluetooth clock-set frame. Those findings fed the dial and jack behavior of the 121gw-qt app.
- EEVblog video #1783 and its repository (EEVblog/121GW-Firmware-Claude, MIT) contained a rewrite of the firmware. Its data tables (HY3131 configurations, mode tables, LCD segment map, font) were extracted from stock 2.05 by emulation. We used that data, cross-checking one sample of each table. We did not use its behavior code for the startup state: it differs from stock in clocking, power-down and pin states.

## 2026-11-02

**Ghidra 12.2 pipeline.** We re-imported the image raw at 0x08006000, applied the STM32L1 SVD (53 peripherals, 600 registers), named the 72 vectors, marked flash read-only and typed the literal-pool words as pointers. IAR's literal loads then fold into register names, and unresolved pool references dropped from 7,989 to 144. The pipeline is rerunnable from scratch ([tools/README.md](tools/README.md#ghidra-122-pipeline-and-the-naming-pass)).

**Emulator, step by step.**
- A reset trace (`tools/emu_reset_trace.py`) confirmed the startup sequence from the image itself: the clock setup, the vector table move, the power range, the order of the rail enables, the front end, the timers, the LCD and the first HY3131 configuration. It stopped in the boot screen's countdown because no timer interrupts were delivered.
- `tools/emu_run.py` added 1 ms slices with TIM3 and TIM2 handlers called between them. The boot screen lasts exactly 2,000 ms on TIM3, and the main loop starts at 2,015 ms. With no HY3131 model, the main loop waited forever for the chip's interrupt line.
- The HY3131 pin-level model was written from the datasheet. The configurations of all 8 dial positions from a full boot matched the extracted reference data. DCV reads the chip every 25 ms, the datasheet's AD1 rate.
- Measurement core traced: AD1, a 16-sample average, calibration (offset, gain / 10^6, trunc(x + 0.5)), a stability filter, and autorange at +10% / 10%. This found two quirks with negative inputs and a bug in our own HY3131 model (DO overwritten at a byte boundary).
- IAR float helpers were identified by running them (`tools/emu_call.py`). 0x08012f94 divides; it is not the multiply its shape suggested.

**Test discipline.** Every multi-key test boots fresh from a factory-default EEPROM image. 2.05 remembers the last mode per dial position, so results that depend on earlier presses would be wrong. 2.05 has no factory-reset routine, so `tools/factory_eeprom.py` builds the image from the manual's defaults, the loaders' fallbacks, and neutral calibration. A blank EEPROM is not factory state: for one thing, it leaves the buzzer off.

**Button map.** We emulated 128 runs (8 positions x MODE cycle + 7 keys short and long) and decoded the LCD from its segment map. Then came the SETUP walk (the manual's 10 pages) and modeled ADC inputs. Results: [buttons/README.md](buttons/README.md).

**Correction: dial OFF.** The dial's OFF position cuts the battery in hardware. STOP mode is used only by auto power-off.

**Coverage, first figures.** The coverage sweeps grew from 137 to 539 scenarios, adding the SD card, Bluetooth, the clock chip, STOP/wake and calibration mode. The reported figure rose to 96.6%. That figure turned out to count whole functions ([below](#2026-11-03)).

**Models added:** the NJU6350 clock chip, USART2/BLE, the HY3131 counter and its peak/RMS/LPF/AD2 events, STOP and EXTI wake (Unicorn reports the PC after WFE/WFI), and the SD card over SDIO and DMA with real FAT images.

**Correction: EEPROM writes never persisted.** The FLASH.SR model kept its write-1-to-clear flags set, so every EEPROM write failed with a write-protection error. That included SETUP saves. Every EEPROM-dependent result was regenerated.

**SD card findings.** The bus is 1-bit because only D0 is wired. There is no card detect. Logging and CAL.BIN save and load run end to end, and the images pass `fsck.fat`. Then the defects:
- the creation-time constant (2006-02-09 22:30);
- the 4 GiB address wrap (emulated on a sparse 8 GiB card);
- the 60-row RAM buffer;
- ignored SD errors.

**Calibration mode decoded.** SETUP captures the offset and MEM the gain; AC is a two-point fit with a reference-frequency window. Found here: the second long REL loses the current zero. The fix is one byte.

**Naming pass.** Three rounds, largest first: 84 names, then 188, then the last 229. All 790 functions are named, with confidence words in the comments. Claims were spot-checked. One example: the VA correction tables have 7 points per row where the code interpolates over 8, confirmed from the table data.

**HY3131 switch network.** We built the pin-to-net table, the switch buses, and every mode and range image from the 15 flash tables.
- **Correction:** the Ohm range-6 variant marker sets PS0 (a 27 nF capacitor across the 10 MΩ reference), not SS6. The emulated images differ only in R2A.
- Reserved and test bits were listed as features to characterize on a meter.

**EEVblog's reconstruction** (video #1784 and EEVblog/121GW-Multimeter-Firmware-RE, MIT) settled several open questions: update files have no header or checksum, the bootloader never talks to the HY3131, and FatFs is R0.08a. **Correction:** we had said R0.09. Checked independently against ChaN's release archives: exactly R0.08a.

**HY3131 diagnostic image.** A small appended routine reads R34-R37 at power-on and tests whether the reserved bits latch. It was emulator-tested, then flashed through the stock bootloader and read back over Bluetooth on a production meter:
- R34-R37 power up as 00;
- R20-R33 are 0 at power-on except R33 = 0x20 (ENOSC);
- all six reserved bits latch, including the datasheet's "-" cells.

**A production meter's SD card and CAL.BIN.** The card held firmware 2.04, byte-identical to EEVblog's download. The meter's CAL.BIN showed:
- board-variant markers differing from our synthetic image: 0x1F = 0xA5, but 0x26 = 0x00;
- real calibration gains of about 61 raw counts per DCV count;
- with that calibration in the emulator, -2.0000 V reads -1.9998, the negative-reading bug on real data.

A 60-row log with no footer confirmed the power-off data loss.

**2.04 against 2.05.** Full function match: 2.05 is 2.04 plus the LPF compensation, its tables and the version digits ([versions.md](versions.md#205)).

**Bluetooth to a PC.** `tools/ble_capture.py` worked over BlueZ D-Bus with no pairing and no root. One dead end was hardware: the PC adapter heard no LE devices at all until its antenna was attached.

**v2 image on a meter.** The image carries four fixes (zero-trim, big-card, temp-round, fattime), a silent boot diagnostic, and Bluetooth telemetry: EEPROM, bootloader and info-page dumps plus live HY3131 registers, sent in the stock packet path's idle window. It was emulator-verified, then flashed through the SD bootloader. The link carries about 190 B/s in practice.

**More fixes, C framework.** `neg-round` fixes the negative readings. A C patch framework was built: stock functions get linker symbols from `names.tsv`, and hooks replace a `bl`. Its first module, `cal.c`, extends CAL.BIN to include the current-zero trims (OF-12).

**zombu4's list.** Four areas, all 56 OF items: each confirmed in the emulator, then fixed with our own patches written from zombu4's descriptions ([bugs.md](bugs.md)).

**Dead end: exFAT by binary patch.** Transplanting FatFs R0.16 into the binary needed a wrapper layer between 2.05's calls and the new library. That layer was not written. With the big-card fix, a FAT32-formatted 2 TB card works in emulation; exFAT waits for the source build.

## 2026-11-03

**All 21 releases compared.** We collected every published image (1.00-2.05; the 1.52 download boots as 1.53) and ran 261 scenarios on each, 5,481 runs. A static code diff across versions found one removed-and-restored feature (the LPF compensation in 2.04), the 1.10 Bluetooth format change, and the 1.26 compiler change ([versions.md](versions.md)). Tool corrections came out of this: `compare_firmware.py`'s Bluetooth frames were malformed, and a "missing" function in 1.58 was a disassembly artifact.

**Course change: build from source.** The project moved from binary patches to EEVblog's C reconstruction, built in Docker with GCC 14.2.1, flashed through the SD card only. With source, dead code can be dropped, fixes become source edits, and exFAT becomes a library upgrade. The binary patches became the specification.

**Unicorn IT-state bug.** The first GCC build misexecuted in the emulator: a function skipped its `push` and returned to 0x40. The cause was a Unicorn 2.1.4 bug (upstream #2309, fixed on dev by #2344, unreleased): memory hooks inside a Thumb IT block leave a stale condition. We backported the fix (unicorn-engine 2.1.4-4+p1) and added `tools/ucfix.py` to load it. Results from before the fix, including the zombu4 verifications, are being re-run.

**Coverage, corrected.** `tools/coverage_gaps.py` counts executed instruction bytes, with literal pools as data. The real figure for the same 539 scenarios is **76.20% of 119,004 bytes**, not 96.6%. Work toward 100% started: each byte either executed or listed as unreachable with a reason.

**Source build equivalence.** `tools/compare_images.py` compares what a user and the hardware see, not instruction-count timing. `--seeds` separates timing-dependent results from real ones. On 54 sampled scenarios, 47 are identical; jitter classification and a TIM2 phase check show the other 7 are timing artifacts. EEVblog's tick-synchronized harness, run on our GCC 14.2.1 build, found all 294,266 peripheral writes identical and no RAM differences outside the stack ([../firmware/README-fork.md](../firmware/README-fork.md)).

## Lessons

- **Model bugs look like firmware behavior.** Stuck flash flags, a DO shift error and the Unicorn IT bug each produced plausible results. Check the tool when a result changes for no reason.
- **Measure coverage by executed instruction bytes,** never by functions touched.
- **Start every key-press test from factory defaults.**
- **Compare across compilers by behavior, not time,** and classify differences with jitter before calling them real.
- **Check every agent claim against the decompile or an emulator run before recording it.** Several early comments were wrong.
