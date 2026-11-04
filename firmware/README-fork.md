# About this fork

This folder is a fork of EEVblog's [121GW-Multimeter-Firmware-RE](https://github.com/EEVblog/121GW-Multimeter-Firmware-RE) (MIT): a C reconstruction of the 121GW's SD bootloader and of application firmware 2.05, made by EEVblog / David L. Jones. EEVblog's [README.md](README.md) is kept unchanged. It describes the reconstruction, its build and its own verification. This page describes what the fork adds, and how the fork is meant to be used.

## Main points

- **Our GCC 14.2.1 build is equivalent to stock 2.05.** EEVblog's tick-synchronized emulator harness finds every one of 294,266 peripheral writes identical and no RAM differences outside the stack, the same result EEVblog documents for their GCC 9 build. Our own behavioral comparison agrees: the scenarios that differ do so only through timing artifacts.
- **This source is the base for fixed and extended firmware.** The binary patches in [../research/](../research/) serve as the specification for each fix ([../research/bugs.md](../research/bugs.md)). The fixes go into this source one at a time.
- **Flashing is through the SD card only.**

## What the fork adds

### Docker build

The build runs in a clean container, so the host needs only Docker:

- Image: Ubuntu 26.04 with `gcc-arm-none-eabi` (GCC 14.2.1), `libnewlib-arm-none-eabi`, `binutils-arm-none-eabi`, CMake, Ninja and Python 3.
- The container runs with `--network none` and this folder mounted at `/src`. It runs the two CMake builds from [README.md](README.md) (`bootloader`, then `firmware`) and hands ownership of `build/` back to the calling user.
- Output: `build/firmware/app.bin`, linked at 0x08006000, a plain image like EEVblog's `EEVBlog*.bin` releases. Also `build/bootloader/bootloader.bin`, built for comparison only and never flashed.

A minimal Dockerfile:

```dockerfile
FROM ubuntu:26.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        gcc-arm-none-eabi libnewlib-arm-none-eabi binutils-arm-none-eabi cmake ninja-build python3 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
```

```sh
docker build -t 121gw-fw-build docker/
docker run --rm --network none -v "$PWD":/src 121gw-fw-build sh -c \
  'cmake -G Ninja -S bootloader -B build/bootloader && cmake --build build/bootloader &&
   cmake -G Ninja -S firmware -B build/firmware && cmake --build build/firmware'
```

Save the Dockerfile outside the source tree (for example as `docker/Dockerfile` beside this folder) and build the image from that directory.

### SD-card flashing only

EEVblog's README also shows SWD programming. This project flashes only through the meter's own SD bootloader:

1. Back up the calibration first. On the boot screen press MEM, then REL; the meter saves `CAL.BIN` to the card. Keep that file: it holds this meter's calibration and its board-variant markers. Restore only a meter's own file.
2. Copy `app.bin` to the root of a FAT-formatted card as `EEVBlog.bin` and follow the update procedure in the 121GW manual.

The SD bootloader checks only that the first word looks like a RAM stack pointer. It erases and programs the application area with read-back verify, and never touches the bootloader or the data EEPROM (calibration). Recovery is always possible the same way: put stock `EEVBlog2_05.bin` on the card. SWD is the last resort and is not part of this workflow.

### Custom firmware C-2.06

On top of the stock-equivalent build, this tree adds the following. The boot screen shows **C-2.06** instead of V-2.05. The SD bootloader is unchanged.

- **Bluetooth file transfer** over YMODEM, in both directions, with new command frames (`F6`: length, sequence, command, CRC-16, answered by `D6` status replies): INFO, LIST, SEND (download), RECEIVE (upload), ABORT. Stock `F4` key presses and the `F8` clock command are unchanged. Overwriting an existing file, or writing `EEVBlog.bin` at all, needs a SETUP press on the meter; `CAL.BIN` is never written over Bluetooth **[meter, emulated]**.
- **Reliable downloads** over the meter's noisy meter-to-module line: 128-byte blocks, 20-byte bursts every 100 ms, up to 10 retries per block. A 146,484-byte file arrived byte-identical across 1,145 blocks (the block number wraps past 255 four times) **[meter]**. Background: [../research/firmware/bluetooth.md](../research/firmware/bluetooth.md#6-link-errors-measured).
- **A new disk layer:** aligned DMA buffers (an unaligned buffer shifted uploads by one byte on the meter), 64-bit addresses (no 4 GiB wrap), every transfer checked, and a failed card read retried with a fresh card session **[meter, emulated]**.
- **FatFs R0.16 with exFAT** for the application, so cards over 32 GB work as shipped **[meter for FAT32, emulated for exFAT 64 GB and 2 TB]**.
- **Diagnostics in INFO:** the reason for the last canceled transfer, failed card reads, and the MSI clock measured against the 4 MHz crystal with TIM11, which also sets the UART divisor **[meter]**.
- **Transfer progress on the bar graph:** it fills left to right for uploads and right to left for downloads, with one segment bouncing every 1 KB **[emulated]**.

## Emulator equivalence with stock 2.05

Two checks, both on the emulator with Unicorn 2.1.4 patched for the Thumb IT-state bug (upstream PR #2344; [../research/tools/README.md](../research/tools/README.md#unicorn-loses-the-thumb-it-state-under-memory-hooks)). That bug made earlier GCC builds misexecute in emulation.

**1. EEVblog's harness, tick-synchronized.** `tools/emu/compare_app.py 20000000 0 --points --ticks=40000` runs in a Docker image with the fixed Unicorn and `binutils-arm-none-eabi`. It compares our build against stock 2.05, with the rebuilt bootloader on both sides and a synthetic factory-default EEPROM:

| Stream | Peripheral writes | Identical |
|---|---:|---|
| main | 250,266 | yes |
| TIM3 interrupt (IRQ 29) | 40,000 | yes |
| TIM2 interrupt (IRQ 28) | 4,000 | yes |
| **total** | **294,266** | **yes** |

RAM differences outside the stack: 0. EEVblog documents the same result for their GCC 9.2.1 build.

**2. Our behavioral comparison.** [`research/tools/compare_images.py`](../research/tools/README.md#compare_imagespy) runs whole scenarios from the coverage sweep (dial, keys, SD card, Bluetooth) on both images. It compares stable LCD screens, key-press state, buzzer edges and Bluetooth bytes. On 54 sampled scenarios, 47 are identical and 7 differ. All 7 are timing artifacts, not behavior:

- Emulated time is an instruction count, so a different compiler shifts when things happen. TIM2 first fires at 19 ms in stock and at 9 ms in the rebuild.
- Jitter classification (`--seeds`) shows these scenarios already vary with timing on stock alone. They race the firmware.

The tick-synchronized harness removes that timing difference, which is why it finds every write identical.

## Licenses

MIT, as upstream ([LICENSE](LICENSE)). Third-party code in `common/` keeps its own license: the ARM CMSIS and ST STM32L1 device headers are Apache-2.0, and FatFs R0.08a is under ChaN's license (in its source files).
