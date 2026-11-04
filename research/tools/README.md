# Research tools

This folder holds the tools used to take firmware 2.05 apart, run it, compare versions and patch it: the Ghidra pipeline and naming pass, the Unicorn-based emulator with its hardware models, coverage measurement, image comparison, the binary patch framework, and the Bluetooth capture tools. A [later section](#fixes-we-had-to-make-to-the-tools) lists every defect we found in the tools themselves. Each entry gives the symptom, the cause and the fix. Some of these defects changed published results.

## Main points

- **All 790 functions of 2.05 are named.** The decompile ships as [../decompile/d205-g122.c](../decompile/d205-g122.c) (Ghidra 12.2, peripherals typed from the SVD, IAR literal-pool loads folded).
- **The emulator runs the unmodified stock image** from reset to the main loop and beyond. It has pin-level or register-level models of the HY3131, the SD card (SDIO + DMA), the NJU6350 clock chip, the BLE112 serial link, the LCD controller, the ADC inputs, the keys, the dial and the jack sense.
- **Coverage is 76.2% of instruction bytes** (119,004 bytes, 539 scenarios). An earlier figure of 96.6% counted every byte of any function the emulator entered; it was wrong. Work toward 100% (executed, or proven unreachable) is in progress.
- **Unicorn 2.1.4 has a bug that loses the Thumb IT state under memory hooks.** Every result produced before the fix is being re-run. The emulator now runs on a patched library.
- **The firmware can be patched safely.** Update files carry no signature or checksum, about 224 KB of the application area is free, and the emulator can show that a patch changes only what it targets.

## Requirements

- Python 3 with Unicorn **2.1.4 plus upstream PR #2344**, or any later release that includes it ([below](#unicorn-loses-the-thumb-it-state-under-memory-hooks)). Unpatched 2.1.4 runs but gives wrong results for some code.
- `binutils-arm-none-eabi` (objdump; used by the coverage and version tools) and, for patches, `arm-none-eabi-as` / `gcc-arm-none-eabi`.
- The stock image `EEVBlog2_05.bin`, from EEVblog's 121GW product page. It is not included in this repository.
- An STM32L1 SVD file (`STM32L1xx.svd` from the cmsis-svd-data project; it includes SDIO). Not included.
- The LCD segment map `lcdmap.json` from EEVblog/121GW-Firmware-Claude (MIT, `tools/re/extracted/lcdmap.json`). `lcd_decode.py` reads it; set the path in its `REF` constant.
- Ghidra 12.2 (headless) for the decompile pipeline only.
- A calibration image is optional. `factory_eeprom.py` builds a factory-default data-EEPROM image from the stock image, with neutral calibration (offset 0, gain 1.0, so AD1 20000 shows 2.0000 V).

## Overview

| Tool | What it does |
|---|---|
| Ghidra scripts: `ApplySvd.java`, `NameVectors.java`, `FlashReadOnly.java`, `TypePoolPointers.java`, `ApplyNames.java`, `ExportDecomp.java`, `DumpAsm.java` | Decompile pipeline |
| `merge_names.py`, `understanding.py`, `callers.py`, `frontier.py` | Naming pass support |
| `emu_run.py` (+ `hy3131_model.py`, `sdcard_model.py`, `lcd_decode.py`, `ucfix.py`) | The emulator |
| `emu_reset_trace.py` | Reset-to-boot peripheral write trace |
| `emu_call.py` | Calls one function with register arguments |
| `coverage_sweep.zsh`, `coverage_gaps.py` | Coverage measurement |
| `button_map.py`, `hy_positions.zsh`, `hy_decode.py` | Button map and HY3131 configuration per dial position |
| `factory_eeprom.py`, `make_sd_images.zsh`, `fat_times.py`, `sparse_extract.py` | Test inputs (EEPROM image, SD card images) |
| `compare_images.py` | Behavioral comparison of two images, with jitter classification |
| `compare_firmware.py` | Older two-image comparison (superseded by `compare_images.py`) |
| `versions/probe.py`, `versions/matrix.py`, `versions/codediff.py`, `versions/presence.py` | All-version comparison ([../versions.md](../versions.md)) |
| `v204/match.py`, `v204/datadiff.py`, `v204/diffdecomp.py`, `v204/final.py` | 2.04 against 2.05, function by function |
| `patch_stock.py`, `../patches/c/` | Binary patches and the C patch framework |
| `ble_capture.py`, `tele_decode.py`, `tele_timeline.py` | Bluetooth capture from a real meter and telemetry decoding |

Every script states its usage in its header comment. Run them from the research folder (`python3 -I tools/emu_run.py ...`).

## Ghidra 12.2 pipeline and the naming pass

The image is imported raw at 0x08006000 as `ARM:LE:32:Cortex` (the 24 KB bootloader below it is not part of an update). Three headless passes:

1. Import with no analysis; `ApplySvd.java` applies the SVD: 53 peripherals and 600 registers. 45 peripherals become structures; overlapping ones (COMP/RI/OPAMP, ADC, NVIC/SysTick/SCB) become per-register labels. It also adds the RAM, bit-band aliases, the 12 KB data EEPROM at 0x08080000, the bootloader, option bytes and system memory.
2. Analysis with `FlashReadOnly.java` (flash marked read-only, so constant loads fold) and `NameVectors.java` (72 vectors, 66 distinct handlers named from the SVD's interrupt list).
3. `TypePoolPointers.java` types 1,734 literal-pool words as pointers, so the decompiler shows `GPIOG->IDR` instead of `*DAT_...`. Unresolved pool references in the C dropped from 7,989 to 144. `ExportDecomp.java` writes the decompile and a function table (entry, name, size, callers, callees, peripherals reached). `ApplyNames.java` applies `names.tsv`; rerun it after any rebuild.

**Naming pass.** Names live in `names.tsv` (address, name, flags, comment with a confidence word). The pass ran in rounds, largest unnamed functions first: 84 names, then 188 plus 4 library routines, then the last 229. All 790 functions are named. `merge_names.py` checks every proposal before merging it: the address must be a function entry, not already named; the name must be a unique C identifier; the comment must carry a confidence word. Proposals were spot-checked against the decompile, and many against the emulator. IAR's float helpers were identified by running them with `emu_call.py`. `understanding.py` reports the share of code bytes by tier: named with high confidence, library, named partially, unnamed.

## The emulator: `emu_run.py`

Unicorn runs the image as a Cortex-M3 in 1 ms slices of 4,194 instructions (the 4.194304 MHz MSI clock). Between slices it calls `TIM3_IRQHandler` every slice and `TIM2_IRQHandler` every tenth, once the firmware has armed them (CR1.CEN and DIER.UIE). Handlers are called as functions with the context saved and restored; on a Cortex-M3 they are plain AAPCS functions. Hardware ready flags (RCC, PWR, LCD, ADC, FLASH) read as ready, and bit-band writes are mirrored into the real register. With no options, a run is deterministic.

Inputs: the dial (`--rotary`, `--rotary-at`), keys (`--keys T:KEY[:DUR]`), current-jack sense (`--jack`), the AD1 value (`--ad1`, `--ad1-at`), signal frequency and duty (`--freq`, `--duty`), battery and thermistor ADC values, EEPROM image and bytes (`--eeprom`, `--ee`, `--cal`), memory pokes, Bluetooth bytes sent to the meter (`--ble`), and an SD card image (`--sd`, `--sd-file`). Outputs: milestones, decoded LCD screens (`--screens`; `--glass` shows the LCD controller RAM, which is what the glass shows, instead of the firmware's shadow copy), key-press state snapshots, buzzer edges, HY3131 transactions, Bluetooth TX bytes, EEPROM changes (`--ee-out`), the SD card after the run (`--sd-out`), function return values (`--ret`) and coverage (`--coverage-ranges`).

### Models

| Model | Level | What it covers |
|---|---|---|
| **HY3131** (`hy3131_model.py`) | pin | Decodes CS, CK and DI on PF0-PF2 and drives DO on PF3 (the interrupt line while CS is high), per the datasheet's bus protocol. Register file with auto-increment reads and writes, INTF cleared on read, the 0x60 soft reset. Events: AD1 at the rate set by R22 (OSR, chopping); counter with whole-cycle extension; peak, RMS, LPF and AD2 events. `--hy-reg` sets power-on values and `--hy-nolatch` makes bits not latch, for the diagnostic image. No analog front end: AD1 is a fixed or scheduled value. |
| **SD card** (`sdcard_model.py`) | register | STM32L1 SDIO controller, DMA2 channel 4 and an SDHC card, using only the commands in the SD Simplified spec that ST's driver sends. Card types SDHC, SDSC, SD 1.x and MMC; fault injection per command or data transfer; SCR override. Images are 64 MB FAT32/FAT16 files (`make_sd_images.zsh`) or sparse whole-card files (`--sd-file`, tested with 8 GiB and 2 TB cards). The card images pass `fsck.fat` after the firmware writes to them. |
| **Clock chip** (NJU6350) | pin | 3-wire bus, BCD registers (year, month, day, hour, minute, second), writes at odd addresses; the clock advances with emulated time. |
| **BLE112 link** | register | USART2: bytes injected on RX with the interrupt (IRQ 38), TX captured. |
| **LCD** | register | LCD controller RAM and status; `lcd_decode.py` turns the 8 RAM words into the main and sub display text, decimal points and icons. |
| **ADC** | register | Battery (PA5) and thermistor (PA4) conversions. |
| **Power** | core | WFE/WFI enters STOP; a dial change wakes it (EXTI). `--exti-irq` runs the EXTI handlers and lets keys wake it; `--nvic` models NVIC enable registers with set/clear semantics. |
| **Flash/EEPROM** | register | Data-EEPROM writes persist; option bytes settable (`--flash-obr`). |

`--jitter SEED` is a test-only option: each slice runs 85-115% of its normal instruction count. A result that changes with the seed depends on timing.

### Other emulator entry points

- `emu_reset_trace.py`: from the reset vector to the boot screen, every peripheral write in order, named from the SVD. Used to check the startup sequence ([../firmware/architecture.md](../firmware/architecture.md)).
- `emu_call.py IMAGE ADDR ARGS`: calls one function with integer, float (`f:`) or double (`d:`) arguments and prints r0/r1. Used to identify library helpers and to unit-test arithmetic such as rounding and temperature conversion.

## Coverage: `coverage_sweep.zsh` and `coverage_gaps.py`

`coverage_sweep.zsh` writes a job file of scenarios (`POS MS KEYS [extra emu_run args]`) and runs each from a fresh boot on the factory EEPROM image. Every multi-key sequence starts from factory defaults, because 2.05 remembers the last mode per dial position. The jobs cover every dial position, MODE cycles, every key short and long, SETUP edits on every page, boot keys, sleep and wake, Bluetooth with all remote keys and the clock frame, counter modes, PEAK, LPF, AC+DC, MIN/MAX, jack sense, Fahrenheit, low battery, the SD card paths and calibration mode. `emu_run.py --coverage-ranges` records executed basic blocks.

`coverage_gaps.py --ranges FILE [--list]` measures **instruction bytes**. It disassembles every function and counts literal-pool words (targets of PC-relative loads) as data, not code. It prints the total and every unexecuted range by function, with the first instructions of each. Baseline: 76.20% of 119,004 instruction bytes. `frontier.py` lists unexecuted functions whose callers did run, with the calling line that shows the condition not met.

## `compare_images.py`

Behavioral comparison of two firmware images, built for checking the C reconstruction against stock 2.05. Its emulated time is an instruction count, and a different compiler shifts it, so the tool compares what a user and the hardware see, not timing:

- the sequence of stable LCD screens (sampled every 100 ms; a screen counts when held for at least 2 samples);
- state snapshots after each key, with times and the `beeps` count stripped;
- buzzer rising edges, Bluetooth TX bytes, final mode and range.

`compare_images.py A.bin B.bin JOBS [-P 10] [--every N]` prints identical and different scenario counts and the first difference in each different one.

**`--seeds N`** runs each side with no jitter and with jitter seeds 0..N-1, then classifies each scenario:

| Class | Meaning |
|---|---|
| same | one result, equal on both images |
| timing | A itself varies with jitter: the scenario races the firmware. Also reports whether B's results fall inside A's set. |
| REAL? | A is stable under jitter and B falls outside it: a behavior difference to investigate |

`UCFIX_A=0` / `UCFIX_B=0` runs one side on the unpatched system Unicorn, for checking the Unicorn fix itself.

## Version comparison

`versions/probe.py` runs 261 scenarios on each of the 21 release images and stores the hardware-level outputs per run (cached; a run repeats only when its scenario definition changes). `versions/matrix.py` groups versions with identical outputs. `versions/codediff.py` splits images into code pieces at prologues, keeps only reachable instructions, normalizes them and reports pieces gone or new between versions, named through 2.05. `versions/presence.py` builds the presence matrix. For 2.04 against 2.05, the `v204/` scripts match functions by normalized instruction streams. Each match is checked by its literal words and call targets, then the two decompiles are diffed with 2.04's names translated to 2.05's. Results: [../versions.md](../versions.md).

## Binary patches: `patch_stock.py` and the C framework

`patch_stock.py [--extra SET.json] EEVBlog2_05.bin OUT.bin PATCH...` applies named in-place or appended patches. It refuses unless the input's SHA-256 is the known stock image and every site holds the expected original bytes. Built in: `zero-trim`, `big-card`, `temp-round`, `fattime`, `neg-round` ([../bugs.md](../bugs.md)). Assembly patches are in `../patches/*.s`.

The C framework (`../patches/c/`):

- `gen_stock_ld.py` turns `names.tsv` into `stock_syms.ld`, so C code calls stock functions as `stock_<Name>`.
- `patch.ld` links new code at 0x0802a000 (free flash above stock's end at 0x080294e8) and `.bss` at 0x2000a000.
- `hooks.tsv` lists each hook: the `bl` site, the stock callee expected there, the new function, and why.
- `build_c.py` compiles, links and patches the hooks, refusing if a site's `bl` does not target the expected callee.

`build_v2.py` builds the v2 image now on a production meter: the four fixes, plus Bluetooth telemetry (EEPROM, bootloader and info-page dumps, live HY3131 registers, sensors) sent in the idle window of the stock packet path. `build_hy_diag.py` builds the HY3131 power-on diagnostic image.

### Assessment: patching the stock image

Before the project moved to the C source, we assessed whether stock 2.05 could be binary-edited instead of rewritten. It could, and that is safer than a rewrite, because a patch keeps every line of stock startup, power, HY3131 and display code:

- **No signature on updates.** The SD bootloader checks only that the first word looks like a RAM stack pointer, then programs `EEVBlog.bin` (8.3 name, root directory, at most 0x5A000 bytes, whole words) with read-back verify. This agrees with EEVblog's bootloader reconstruction.
- **Room:** stock is 144,616 bytes of a 360 KB application area, leaving about 224 KB free.
- **Known code:** IAR uses the plain AAPCS calling convention, so new C or assembly code can call stock functions.
- **Patch types:** in place (a constant or an instruction; smallest risk), hook (replace one `bl` with a call to new code that runs the original and adds to it), and data (behavior that 2.05 already reads from EEPROM).
- **Rules:** back up the calibration first (boot MEM, then REL saves CAL.BIN to the card). Every patch is a patcher with refusal checks, verified in the emulator against stock. Flash a no-op patch first to prove the path. Recovery: reflash stock from SD; the bootloader is never touched.

Binary patches remain the specification for each fix. New work goes into the C source ([../../firmware/README-fork.md](../../firmware/README-fork.md)).

## Bluetooth tools

- `ble_capture.py LOG [--seconds N] [--address ...]`: connects to a 121GW through BlueZ over D-Bus (no pairing, no root), subscribes to its data characteristic, reassembles packets (they arrive split or bunched), checks the XOR byte, and logs raw bytes. To stop early, create `LOG.stop`.
- `tele_decode.py INPUT OUTDIR`: reassembles the v2 telemetry dumps from a capture or from `emu_run.py --ble-all` output, reports coverage (a dropped frame is filled by a later pass), and prints live frames.
- `tele_timeline.py LOG`: HY3131 configuration, mode, range and AD1 statistics over time, from telemetry.

Measured on a production meter: about 0.3 % of the bytes the meter sends arrive damaged (changed in place, none dropped or added), at any send rate and with stock firmware too; the PC-to-meter direction is clean. Downloads therefore use 128-byte YMODEM blocks with retries: a 146 KB file arrived byte-identical at about 84 B/s. Details: [../firmware/bluetooth.md](../firmware/bluetooth.md#6-link-errors-measured).

## Docker builds

- **Firmware** (the C reconstruction): an Ubuntu 26.04 image with `gcc-arm-none-eabi` (GCC 14.2.1), newlib, CMake, Ninja and Python 3. It runs with `--network none`, the source mounted read-write, and the build tree's ownership handed back to the calling user at the end. See [../../firmware/README-fork.md](../../firmware/README-fork.md).
- **libunicorn with the IT-state fix**: Unicorn 2.1.4 source plus the ARM part of PR #2344, built ARM-only with CMake in an Ubuntu 24.04 container ([below](#the-patched-libunicorn-must-match-the-hosts-glibc)).
- **Debian package**: the same fix as `debian/patches/0005-arm-keep-IT-state-across-memory-hooks.patch` on Debian's unicorn-engine 2.1.4-4 source, built in a clean Ubuntu 24.04 container as version 2.1.4-4+p1 (CTest 12/12, Python tests 33/33, lintian clean).

## Fixes we had to make to the tools

### Unicorn loses the Thumb IT state under memory hooks

- **Symptom:** a GCC-built 121GW image skipped a function's `push` and returned to address 0x40. Results could change after unrelated code moved.
- **Cause:** in Unicorn 2.1.4 (the upstream release and Ubuntu's package), a `UC_HOOK_MEM_READ`/`WRITE` hook on an access inside a Thumb IT block leaves a stale IT condition. The instructions after the block then run under the wrong condition. Our peripheral models are such hooks. Upstream issue [#2309](https://github.com/unicorn-engine/unicorn/issues/2309), fixed on the dev branch by [PR #2344](https://github.com/unicorn-engine/unicorn/pull/2344) (commit 2bf1098e), not yet in a release as of November 2026. A second variant with `UC_TLB_VIRTUAL` is PR #2459; we do not use that mode.
- **Fix:** the `cputlb.c` part of #2344, backported. It is packaged as unicorn-engine 2.1.4-4+p1 and also built as a private library. `ucfix.py`, imported by `emu_run.py` before Unicorn, loads the private library when the installed one lacks the fix. When the installed one has it, `ucfix.py` does nothing. `UCFIX=0` forces the system library for comparisons.
- **Consequence:** results produced before the fix are being re-run, including the zombu4 fix verifications ([../bugs.md](../bugs.md)). Stock 2.05 (IAR-built) was hit less often than GCC builds, but not never. Before trusting a newer Unicorn, confirm it includes #2344.

### Ubuntu's Unicorn bindings ignore `LIBUNICORN_PATH`

- **Symptom:** pointing `LIBUNICORN_PATH` at the fixed library still loaded the system one.
- **Cause:** Ubuntu's `python3-unicorn` loads `libunicorn.so.2` by soname. `LD_LIBRARY_PATH` is read only at process start, so setting it from inside Python is too late.
- **Fix:** `ucfix.py` sets `LD_LIBRARY_PATH` and re-executes the same command line once.

### The patched libunicorn must match the host's glibc

- **Symptom:** a shared library built in a newer container does not load on the host.
- **Cause:** the library links against the container's glibc, and symbol versions from a newer glibc are missing on an older host.
- **Fix:** build it in a container of the same Ubuntu release as the host that runs the emulator (24.04).

### WFE/WFI looked like a crash

- **Symptom:** auto power-off stopped the run with an error.
- **Cause:** Unicorn has no sleep. It stops on WFE/WFI and reports the PC *after* the instruction.
- **Fix:** on an emulation error, read the halfword at PC-2. If it is WFE (0xBF20) or WFI (0xBF30), enter the modeled STOP state and resume at PC on a wake.

### The fault handler crashed on unmapped PCs

- **Symptom:** a run that jumped outside mapped memory died with a Python traceback instead of a fault report.
- **Cause:** the WFE/WFI check read memory at PC-2, which raised a second error when PC was unmapped.
- **Fix:** catch that error and report a real fault with PC, LR and SP (`--last-pcs N` adds the last N instruction addresses).

### EEPROM writes never persisted

- **Symptom:** SETUP saves, calibration and last-mode slots never stuck in emulation.
- **Cause:** FLASH.SR's error and done flags are "write 1 to clear". The model kept them set, so the driver saw a write-protection error and gave up on every write. Every EEPROM result before the fix was wrong.
- **Fix:** FLASH.SR reads READY | ENDHV only. The button map was regenerated afterward.

### The coverage figure counted touched functions

- **Symptom:** coverage was reported as 96.6% of code bytes, but there was clearly unexecuted code inside executed functions.
- **Cause:** the metric counted every byte of any function with at least one executed block, and literal pools as code.
- **Fix:** `coverage_gaps.py` measures executed instruction bytes per basic block, with literal pools excluded: 76.20% of 119,004 bytes on the same 539 scenarios.

### `compare_firmware.py` sent malformed Bluetooth frames

- **Symptom:** its "BLE on + remote keys" and clock scenarios showed no effect on any version.
- **Cause:** its key frames (`f40101` ...) are not valid. The meter wants `F4` and two ASCII digit pairs, for example `F4 30 35 30 35` for a short MODE. Its F8 clock frame carried check byte 0x0E, but the XOR of `1A 0B 02 0F 1E 00` is 0x02. Both scenarios exercised nothing.
- **Fix:** `versions/probe.py` builds both frames correctly. `compare_images.py` replaces `compare_firmware.py` for image comparison.

### The beeps counter was tied to one compiler's write form

- **Symptom:** a GCC-built image showed different beep counts from stock with identical buzzer behavior.
- **Cause:** the counter counts halfword writes to GPIOE.BSRR that set PE7, the store IAR's code uses. Other code sets the same pin with a different store.
- **Fix:** `compare_images.py` strips the `beeps` field and compares buzzer rising edges instead.

### Atomic replace lost the execute bit

- **Symptom:** scripts updated by a tool lost their execute permission.
- **Cause:** writing a temporary file and renaming it over the original creates a new file with default mode 644.
- **Fix:** copy the original's mode onto the temporary file before the rename.

### Smaller model and analysis fixes

| Symptom | Cause | Fix |
|---|---|---|
| Boot stuck in the boot-screen countdown | no timer interrupts delivered | TIM2/TIM3 handlers called between slices once armed |
| Startup walked into the firmware's deliberate HSE-failure hang | ready bits faked as always set | ready bits follow their ON bits |
| Peripheral bits set through bit-band aliases had no effect | alias writes not mirrored | mirror bit-band writes into the register |
| Wrong low bits in 52-register HY3131 reads (writes were fine) | the model overwrote DO at a byte boundary | fixed the shift; found while tracing the measurement core |
| SDIO interrupt lost behind DMA2 channel 4 | the last NVIC ISER write won | `--nvic` models set/clear enable semantics |
| SD transfers timed out | the driver polls only about 1,000 times for completion | the model raises the interrupts and the emulator calls them at once |
| Auto-hold display disagreed with the glass | the firmware's RAM shadow is not what the LCD shows | `--glass` decodes LCD RAM; differences are printed |
| `Cal_AcFreqWindow` "missing" in 1.58 | linear disassembly swallowed a prologue into the preceding data | `codediff.py` re-decodes hidden prologues |
| 0x08012f94 named `fmul` | guessed from shape | `emu_call.py` showed it divides: `__aeabi_fdiv` |
| Ohm range-6 variant described as switch SS6 | early reading of the configuration bytes | emulated images differ only in R2A: switch PS0 |
| FatFs identified as R0.09 | early guess | checked against ChaN's archives: exactly R0.08a |
