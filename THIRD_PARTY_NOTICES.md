# Third-party notices

This repository is MIT-licensed (see LICENSE). The parts below come from others and keep their own licenses.

| Where | What | License |
|---|---|---|
| `firmware/` | EEVblog's reverse-engineered C reconstruction of the 121GW bootloader and firmware 2.05 (our fork) | MIT, Copyright (c) 2026 Dave Jones (`firmware/LICENSE`) |
| `firmware/common/cmsis/` | ARM CMSIS core and ST STM32L1 device headers | Apache-2.0 (license files in that folder) |
| `firmware/common/fatfs/` | ChaN FatFs R0.08a, as used by the original firmware | FatFs license (in its source files) |
| `research/patches/c/fatfs/` | ChaN FatFs R0.16 (with patch 2) | FatFs license (`LICENSE-FatFs.txt`) |
| `frontend/` | Port of Readout (packet decoding, display model, logging, sessions, demo meter, LCD drawing, MCP tools); protocol handling after the official EEVblog 121GW app | MIT; see `frontend/THIRD_PARTY_NOTICES.md` |

Not included, and not covered by this repository's license: EEVblog's stock firmware images (download them from EEVblog), ST's STM32L1 SVD file (download it from ST), and any data from a real meter (calibration, EEPROM dumps).

Findings credited to zombu4's issue list (121GW-Open-Firmware, OF-nn numbers) are facts restated in our own words; no code from that project is included.
