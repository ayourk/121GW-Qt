# 121gw-qt

A Qt 6 companion for the EEVblog 121GW multimeter on Linux: the whole front panel live (LCD, rotary dial, buttons, input jacks), remote button presses, logging and saved sessions, and an MCP server so an AI assistant can read the meter and guide you through a measurement: which dial position, which jacks for the probe leads, which buttons to press.

Ported from [Readout](https://github.com/amd989/readout) (MIT); see THIRD_PARTY_NOTICES.md.

## Build

Ubuntu 24.04 packages: `qt6-base-dev qt6-declarative-dev qt6-connectivity-dev qt6-svg-dev cmake ninja-build g++` (Qt Widgets, in qt6-base-dev, gives the file dialogs), and at run time `qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtqml-models qml6-module-qtquick qml6-module-qtquick-templates qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-window`.

    cmake -S . -B ../build -G Ninja
    cmake --build ../build
    ctest --test-dir ../build

A Debian package (Ubuntu 24.04, Debian 12): `dpkg-buildpackage -b -us -uc` here builds `../121gw-qt_*.deb` with debian/, running the tests. Packaging for other systems is in packaging/ (RPM, Arch, the macOS bundle's Info.plist).

On macOS the app asks for Bluetooth permission the first time it scans, and identifies the meter by a UUID that macOS assigns per computer instead of its address. Windows needs Windows 10 or later with a Bluetooth LE adapter.

## Run

    121gw-qt            # scans for the last meter used and connects to it
    121gw-qt --demo     # a simulated meter: click the dial legends to turn it, a current jack to plug its lead

Turn on the meter's Bluetooth by holding **1ms PEAK** until **BT** shows, then press **Scan**.

## The front panel

The knob follows the meter: its mode tells which dial position it is on. The jacks light up where the probe leads go for the current mode: red, black, or (VA) red for current and red for voltage. On the A·mA and mVA/VA positions the meter itself picks A or mA (VA or mVA) by which current jack the red lead is in, and MODE only toggles DC/AC; the app follows the same rules (from firmware 2.05).

### Layout and artwork

QML draws the whole face; an SVG can take over any part of it.

- **Ctrl+E** edits the layout: click an element, move it with the arrow keys (Shift: 10 px), R puts it back, Esc ends. Offsets are saved to `~/.config/121gw-qt/layout.json`.
- `~/.config/121gw-qt/face.svg` (or `--face-svg FILE`), viewBox 0 0 500 1000, layered bottom to top:
  - group `artwork`: the base (boot, face plate); replaces QML's
  - QML's live elements, each placed by name: an SVG element with that id (`btn-mode`, `jack-v`, `dial-legend-va`, `dial-knob`…) centers it there
  - `art-<name>`: that element's own artwork instead of QML's, drawn in its layer (`art-dial-knob` still turns, `art-btn-mode` still flashes); an empty `art-<name>` group just hides QML's
  - group `overlay`: drawn above everything, clicks pass through (glare on the LCD glass)
- `121gw-qt --export-svg FILE` writes a starter: the base, and one named rect per element (52) in a group `elements` that the app never draws.

Qt renders SVG Tiny 1.2: no filters; convert text to paths.

Customize the QML: `121gw-qt --export-qml` copies the built-in front panel to `~/.config/121gw-qt/qml/`, which the app then loads (falling back to the built-in one if it fails).

## MCP

The server listens on `http://127.0.0.1:47121/mcp` (this computer only, Bearer token). Register it with Claude Code:

    $(121gw-qt --print-mcp-command)

Tools: `get_status`, `get_reading`, `get_recent_readings`, `press_button`, `get_dial`, `suggest_dial_position` (lights a dial position and the jacks to use, with a reason; can wait until the meter is there), `clear_dial_suggestion`, `set_clock`, `set_logging`, `clear_log`, `save_session`, `list_sessions`, `get_session`, and for the SD card `list_files`, `download_file`, `upload_file`, `get_transfer_status`, `cancel_transfer` (see File transfer). Holding 1ms PEAK is refused: it would turn the meter's Bluetooth off.

`set_clock` uses the meter's F8 command (firmware 2.05; not in the manual): the meter takes it once per Bluetooth connection.

## File transfer

With the custom firmware's Bluetooth file transfer, the **SD card** tab reaches the meter's card. The stock firmware ignores the commands; the app says so after about 12 seconds (four tries).

- **Refresh** lists the card (name, size, date). **Download** saves a file where you choose.
- **Upload…** copies a file to the card under an 8.3 name (suggested from the file's own name; the meter's FAT driver has no long names), and warns first when the name is already on the card.
- **Update firmware…** uploads a chosen image as `EEVBLOG.BIN`, after a warning: the meter's bootloader flashes `EEVBLOG.BIN` the next time you run its firmware update procedure.

The meter decides what it accepts. Overwriting a file, and every write of `EEVBLOG.BIN`, waits for you to press **SETUP on the meter** within 30 seconds (any other key, or none, refuses); the app shows "Confirm on the meter: press SETUP" while it waits, in either tab. `CAL.BIN` (the calibration backup) and `BTRECV.TMP` (the meter's temporary file) are never written over Bluetooth; the app refuses them before asking, and handles the meter's refusal too.

The link carries about 190 bytes per second: a 92 KB image takes about 8 minutes, the stock 2.05 (141 KB) about 13. The panel shows progress, the rate and the time left; **Cancel** stops the transfer (YMODEM's CAN, plus the firmware's ABORT, which also ends the meter's prompt). The meter's readings pause during a transfer, and the buttons and the clock command wait until it ends and for a second after: the meter drops key and clock frames until nothing has reached it for a second.

How it works: a command frame (`F6 LEN SEQ CMD payload CRC-16`, answered by a `D6` reply frame) lists the card one entry at a time or starts a transfer; the file then moves with YMODEM (batch, CRC-16, the name and size in block 0, 1 KB blocks up, 128-byte blocks down, the size from block 0 cutting the padding). Commands without a reply within 3 seconds are sent again with the same sequence number (up to four tries); the meter answers a repeat of the command that started its transfer from its cache, so nothing starts twice. Writes go out in 20-byte pieces on the meter's data characteristic. The YMODEM code (`src/core/ymodem.*`) is this project's own, written from the public protocol description, and `src/core/cardprotocol.*` holds everything specific to the firmware's frames.

The demo meter (`--demo`) has an in-memory card with two logs and a `CAL.BIN`, at the meter's speed, and shows its SETUP prompt for overwrites and `EEVBLOG.BIN`: it accepts after 3 seconds, or refuses with "Don't press SETUP" ticked. `settings.conf` keys: `demo/cardBytesPerSecond` (default 190), `demo/rejectConfirmations`.

Over MCP, `upload_file` needs `overwrite: true` to replace a file and `firmwareUpdate: true` for `EEVBLOG.BIN`; the on-meter confirmation applies either way. `download_file` saves to `saveTo` or the Downloads folder and returns a text file (the meter's CSV logs) up to 32 KB inline. Both wait up to `waitSeconds` (default 120) and then report progress; `get_transfer_status` follows a longer one.

## Files

- `~/.config/121gw-qt/settings.conf`: the remembered meter, MCP token and port (mode 600)
- `~/.local/share/121gw-qt/sessions/`: saved sessions, `{id}.csv` + `{id}.json`, the same files Readout writes
