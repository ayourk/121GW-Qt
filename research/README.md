# Research: EEVblog 121GW firmware 2.05

This folder documents how the EEVblog 121GW multimeter's firmware works: the hardware it drives, the firmware's structure, every published version, the defects in the last one, and the tools used to find all this. Most findings are marked with how they were established: on a real meter, in the emulator, or read from the code.

## Documents

| Document | Contents |
|---|---|
| [hardware/hy3131.md](hardware/hy3131.md) | The HY3131 measurement chip: bus, registers, switch network, every mode and range configuration, the reserved bits measured on a meter |
| [hardware/board.md](hardware/board.md) | The board: MCU pins, front end, jack sense, dial, power, clock chip, SD card wiring, board-variant markers |
| [firmware/architecture.md](firmware/architecture.md) | Startup, main loop, timers and interrupts, measurement core, modes, display, power-off |
| [firmware/calibration.md](firmware/calibration.md) | Calibration records, calibration mode, current-zero trims, CAL.BIN |
| [firmware/storage.md](firmware/storage.md) | Data EEPROM layout and settings, SD card driver, FatFs, logging format and limits |
| [firmware/bluetooth.md](firmware/bluetooth.md) | The Bluetooth packet, remote keys, the clock-set frame, throughput |
| [buttons/README.md](buttons/README.md) | Every key, short and long, on every dial position; MODE cycles, SETUP pages, boot keys |
| [history.md](history.md) | The 121GW's history, 2015-2026: design with UEi, Kickstarter, shipping, hardware fixes, apps, firmware, the 2026 reconstructions |
| [versions.md](versions.md) | All 21 releases, 1.00 to 2.05: changelog, what changed by emulation and code comparison, removals |
| [bugs.md](bugs.md) | Every known 2.05 defect (ours and zombu4's OF-nn list), fixes, status, the roadmap |
| [tools/README.md](tools/README.md) | The toolchain (Ghidra pipeline, emulator and models, coverage, comparison, patching) and the fixes the tools needed |
| [research-log.md](research-log.md) | Chronological log: methods, milestones, corrections, dead ends |
| [decompile/d205-g122.c](decompile/d205-g122.c) | Ghidra 12.2 decompile of 2.05 with all 790 functions named and commented |
| [figures/](figures/) | Hand-drawn SVG figures embedded in the documents above |

Other files here: `names.tsv` (the names and comments applied to the decompile), `factory-eeprom.bin` (a factory-default data-EEPROM image with neutral calibration, for the emulator), `patches/` (binary patches and the C patch framework), and `tools/` (the scripts).

## Running the emulator

### What you need

- **The stock image.** `EEVBlog2_05.bin` is not included. Download it from EEVblog's 121GW product page. The tools check its SHA-256 (it begins `8f885f12f567c2cc`).
- **Python 3 with Unicorn 2.1.4 plus upstream PR #2344**, or a later Unicorn release that includes it. Unpatched 2.1.4 loses the Thumb IT state under memory hooks and gives wrong results ([tools/README.md](tools/README.md#unicorn-loses-the-thumb-it-state-under-memory-hooks)).
- **`binutils-arm-none-eabi`**, for the coverage and version tools.
- **An STM32L1 SVD file**: `STM32L1xx.svd` from the cmsis-svd-data project. Put it at `svd/STM32L1xx.svd`.
- **The LCD segment map** `lcdmap.json` from EEVblog/121GW-Firmware-Claude (MIT, `tools/re/extracted/`). Set its path in `tools/lcd_decode.py`.
- **A calibration image (optional).** By default the emulator uses `factory-eeprom.bin`, whose neutral calibration makes AD1 20000 show 2.0000 V. With a real meter's data EEPROM (`--eeprom FILE`; a meter's CAL.BIN padded to 12 KB works), inputs must be raw counts: about 61 per displayed DCV count.

### Examples

From this folder:

```sh
# boot on the V position, show the screen at 3 s
python3 -I tools/emu_run.py EEVBlog2_05.bin svd/STM32L1xx.svd --rotary V --ms 3500 \
    --hy-only --eeprom factory-eeprom.bin --ad1 20000 --glass --screens 3000

# a key sequence: MODE at 2.5 s, long REL at 4 s (1 kHz LPF in ACV), a 50 Hz signal
python3 -I tools/emu_run.py EEVBlog2_05.bin svd/STM32L1xx.svd --rotary V --ms 8000 \
    --eeprom factory-eeprom.bin --ad1 2000 --freq 50 --keys 2500:MODE,4000:REL:800 --glass --screens 7500

# with an SD card image (built by tools/make_sd_images.zsh) and a long MEM to start logging
python3 -I tools/emu_run.py EEVBlog2_05.bin svd/STM32L1xx.svd --rotary V --ms 12000 \
    --eeprom factory-eeprom.bin --ad1 20000 --sd out/sd/fat32-mbr.img --sd-out /tmp/card.img --keys 3000:MEM:800

# compare two images (stock and a build) over a scenario file
python3 -I tools/compare_images.py EEVBlog2_05.bin app.bin jobs.txt --seeds 4
```

`python3 -I tools/emu_run.py --help` lists every input and model option. The emulator models no analog front end: AD1 is a fixed or scheduled value, so LowZ coupling, true RMS and VA products cannot be tested in it.

## Credits

The meter, the firmware and the C reconstruction of 2.05 are EEVblog's (David L. Jones). EEVblog's reconstruction settled the update format, the bootloader and the FatFs version. The LCD segment map and the reference HY3131 configurations come from EEVblog/121GW-Firmware-Claude (MIT). zombu4's issue list (121GW-Open-Firmware) is credited item by item in [bugs.md](bugs.md); we wrote our own fixes from its descriptions.
