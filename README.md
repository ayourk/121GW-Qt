# 121GW-Qt

Software and research for the EEVblog 121GW multimeter: a desktop companion app, a buildable C firmware for the meter, and a detailed study of the meter's last official firmware, 2.05.

| Folder | What it is | Status |
|---|---|---|
| [frontend/](frontend/) | **121gw-qt**, a Qt 6 / QML companion for Linux. It shows the whole front panel live (LCD, dial, buttons, the jacks to use), sends remote button presses, logs readings and saves sessions. Its MCP server lets an AI assistant read the meter and guide a measurement. | Working with Bluetooth LE; tests for decoding and the MCP server |
| [firmware/](firmware/) | A fork of EEVblog's MIT C reconstruction of the SD bootloader and firmware 2.05, built in Docker. | Equivalent to stock 2.05 in emulation (all 294,266 peripheral writes identical); fixes being ported into it |
| [research/](research/) | How firmware 2.05 works: hardware, architecture, calibration, storage, Bluetooth, buttons and menus, all 21 releases compared, every known defect with its fix, the annotated decompile, and the emulator and tools. | Documentation current; defect fixes being re-verified |

## How the parts relate

- **research → frontend:** the app's dial, jack and MODE logic follows firmware 2.05 as decoded here. On the A·mA and VA positions, the current jack the lead is in picks A or mA. MODE only toggles DC and AC. The app's Bluetooth packet decoding and remote-key frames match [research/firmware/bluetooth.md](research/firmware/bluetooth.md).
- **research → firmware:** every defect in [research/bugs.md](research/bugs.md) was first fixed as a small binary patch on stock 2.05 and checked in the emulator. Those patches are the specification for source changes in `firmware/`. Each change is checked against the previous build with `research/tools/compare_images.py`.
- **firmware → frontend:** planned firmware features (events, faster data, remote control frames) will be read by the app.

Four of the fixes (lost current zero, SD cards over 4 GiB, 2006 file dates, temperature rounding) already run on a production meter as a patched 2.05 image. Firmware is flashed only through the meter's own SD bootloader, which leaves the bootloader and the calibration untouched.

## Quick start

- App: build and run per [frontend/README.md](frontend/README.md). To connect, turn on the meter's Bluetooth by holding 1ms PEAK until BT shows.
- Firmware: [firmware/README-fork.md](firmware/README-fork.md) (Docker build, SD-card flashing) and EEVblog's [firmware/README.md](firmware/README.md).
- Research: start at [research/README.md](research/README.md). The emulator needs the stock image `EEVBlog2_05.bin`, which is not included; get it from EEVblog's 121GW product page.

## Credits

- **EEVblog / David L. Jones:** the 121GW meter, its firmware, and the MIT-licensed C reconstruction of the bootloader and firmware 2.05 that `firmware/` is based on ([EEVblog/121GW-Multimeter-Firmware-RE](https://github.com/EEVblog/121GW-Multimeter-Firmware-RE)).
- **Alejandro Mora:** [Readout](https://github.com/amd989/readout) (MIT), the app 121gw-qt was ported from. Readout's protocol handling follows the official EEVblog 121GW app by David Ledger (MIT). See [frontend/THIRD_PARTY_NOTICES.md](frontend/THIRD_PARTY_NOTICES.md).
- **zombu4:** the defect list in [121GW-Open-Firmware](https://github.com/zombu4/121GW-Open-Firmware) (OF-01 to OF-56), credited item by item in [research/bugs.md](research/bugs.md). Their code is under a custom non-commercial license; none of it is used here. Our fixes are written from their descriptions.
- **EEVblog/121GW-Firmware-Claude** (MIT): the LCD segment map and reference HY3131 configurations used by the research tools.

## License

MIT ([LICENSE](LICENSE)). Third-party parts keep their own licenses: FatFs (ChaN's license, in its source files) and the ARM CMSIS and ST STM32L1 device headers (Apache-2.0) in `firmware/common/`. The frontend's notices are in [frontend/THIRD_PARTY_NOTICES.md](frontend/THIRD_PARTY_NOTICES.md).
