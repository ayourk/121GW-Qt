# Calibration in firmware 2.05

Where the 121GW keeps its calibration, how firmware 2.05 applies it, how calibration mode captures it, the persistent current-zero trims, the CAL.BIN backup on the SD card (and what it leaves out), and the board-variant markers that calibration writes.

Main findings:

- Each mode and range has a 16-byte record in data EEPROM at `T[mode] + 16 x range`: a big-endian int32 offset and a uint32 gain x 1,000,000. Reading = `trunc((average - offset) / gain + 0.5)`. A blank record (gain 0) reads 0, with no "uncalibrated" warning.
- Real calibration gains are about 61 raw AD1 counts per displayed count on DC volts, about 6 on some mV ranges, 75-125 on Ohm and current, and 1.07-1730 on diode and capacitance **[meter]**.
- AC is calibrated with a two-point fit at 10% and 100% of full scale and extrapolated to zero, with a check of the reference frequency.
- The current-zero trim (long REL on a current range) is lost after a second long REL and a power cycle; a one-byte fix keeps it.
- CAL.BIN is EEPROM 0x000-0x4AF, 1200 bytes: settings and variant markers included, current-zero trims excluded. Loading restores all of it, including another meter's markers if the file came from another meter.
- The variant markers 0x1F and 0x26 are written to 0xA5 only by the AC gain capture. A production meter carries 0x1F = 0xA5 and 0x26 = 0x00.

How each claim was established: **[code]** read in the decompile ([../decompile/d205-g122.c](../decompile/d205-g122.c)), **[emulated]** run in our emulator, changing the input between steps (`--ad1-at`), **[library]** runtime helpers identified by calling them, **[meter]** on a production meter (its saved CAL.BIN). We publish no calibration values from any meter.

Related: [architecture.md](architecture.md) (where `Cal_Apply` sits), [storage.md](storage.md) (the SD side of CAL.BIN), [../hardware/board.md](../hardware/board.md) (variants).

## 1. Record layout

`Cal_LoadRecord` reads the record at **`T[mode] + 16 x range`**, with T = 26 halfwords at 0x08028ee8 **[code]**:

| Mode | T | Mode | T | Mode | T |
|---|---|---|---|---|---|
| 0 LowZ | 0x070 + 0x30 (DC) / 0x030 + 0x30 (AC) | 9 Ohm | 0x110 | 18 AC mA | 0x230 |
| 1 DCV | 0x070 | 10 continuity | 0x120 | 19 DC mA | 0x250 |
| 2 ACV | 0x030 | 11 diode | 0x180 | 20 AC A | 0x270 |
| 3 DCmV | 0x0b0 | 12 capacitance | 0x190 | 21 DC A | 0x2a0 |
| 4 ACmV | 0x0d0 | 13 AC VA (uA) | 0x300 | 22 DC VA (uA) | 0x3c0 |
| 5 temperature | 0x0f0 | 14 AC VA (mA) | 0x340 | 23 DC VA (mA) | 0x400 |
| 6-8 Hz, pulse, duty | 0x100 | 15 AC VA (A) | 0x380 | 24 DC VA (A) | 0x440 |
| | | 16 AC uA | 0x1f0 | 25 (ACV 1 ms PEAK) | 0x2d0 |
| | | 17 DC uA | 0x210 | | |

Special cases in `Cal_LoadRecord` **[code]**: LowZ uses the 600 V rows of DCV or ACV (0x070 + 0x30 or 0x030 + 0x30, by its automatic AC/DC state); ACV AC+DC uses DCV's records (0x070 + 16 x range) in its DC phase; ACV/ACmV 1 ms PEAK use 0x2d0; continuity always uses 0x120, which is also Ohm range 1's record; the VA modes load through `Cal_LoadVaRecord`.

| Bytes | Field |
|---|---|
| +0 | offset, int32, big-endian, raw AD1 counts |
| +4 | gain x 1,000,000, uint32, big-endian (capacitance range 4: x 10,000 with the PF8 strap low, else x 100,000) |

Other calibration values **[code]**:

| Location | Content | Valid / default |
|---|---|---|
| 0x104 | frequency reference constant, u32 (the HY3131 crystal, 4.9152 MHz x 10) | 40,000,000-60,000,000, else 49,152,050 |
| 0x178 | Ohm range 6 trim, u32 | +-3000, else 0 |
| record 200 (0x2e0) | temperature (cold-junction) trim, s16 | +-1000, else 0 |
| 0x4D0 + 8 x slot | current-zero trims (section 5) | marker 0xA5 + u32 |

**Applying it.** `Cal_Apply` (0x08006418): `reading = trunc((average - offset) / gain + 0.5)` in counts, using IAR float division (`__aeabi_fdiv` at 0x08012f94) **[code + library + emulated]**. ACV and ACmV with the 1 kHz filter use gain / 10. AC offsets are subtracted and the result clamped at 0. On a production meter DCV's gain is about 61, so AD1 delivers about 61 raw counts per displayed count; with that calibration +1,220,800 raw reads 2.0000 V **[emulated with real calibration data]**.

## 2. Calibration mode

Power on holding **MODE, then REL** (boot screen) **[code, emulated]**:

- `g_cal_mode = 1`, the sub display shows "CAL", and `CurrentZero_ClearAll` (0x0801150e) writes an empty trim (0xA5, value 0) to all six current-zero slots. The mode lasts until power-off.
- **SETUP** arms the offset capture (`Cal_ArmCapture(0)`); a double beep means the mode and range have no calibration.
- **MEM** arms the gain capture (`Cal_ArmCapture(1)`), only after the offset capture finished.
- Other keys work as usual, so the operator moves through modes and ranges with MODE and RANGE.
- Prompts on the main display: `-O-nn` (offset), `-G-nn` (gain), `-4-nn`, `-5-nn`, `-3-nn` (further AC points); end texts `OFF-E`, `G-END`, `400-E`, `5KEND`. The counters show only 0-19 correctly (units digit computed as n - 10).

### DC and resistance
The offset capture averages 16 AD1 blocks. The gain capture (`Cal_DcGainCapture` 0x0800f470) sums the armed number of samples in 64 bits and averages (/16; Ohm range 5: drop 8, /8; range 6: drop 8, /4), computes the gain and **writes the record** for the present mode and range **[code]**. Ohm range 6 also stores `average / gain - 30000` at 0x178 (limited to +-3000), the trim for its correction curve on variant meters.

**Probable defect, Ohm range 6 zero [code].** The offset accumulation sums all 16 samples and divides by 4 (4 x the mean), and the subtraction uses the stored offset as is, so range 6 subtracts four times its shorted-lead mean. With Ohm negatives clamped to 0 above range 0, small resistances on the top range would read 0. The gain capture uses a different scale for the same range (drop 8, /4 = 2 x the mean), so this is not one consistent convention. Its size depends on the real shorted-lead reading; not checked on a meter.

### AC: a two-point fit
Emulated on ACV range 0 (5 V) at 1 kHz: SETUP with AD1 = 0, then MEM with AD1 = 10,000 stored offset -1111 and gain 0.222220, after which 10,000 counts read 5.0000 V **[emulated]**. That is

    gain   = (b - a) / 45,000          (0.9 x 50,000 counts)
    offset = a - 5,000 x gain

so the firmware expects the offset step at **10% of full scale** (0.5000 V on the 5 V range) and the gain step at **full scale** (5.0000 V), and extrapolates to zero, presumably because the AD8436 RMS converter is not linear near zero **[inferred]**. The reference frequency is checked (`Cal_AcFreqWindow` 0x0800f13c, counter in 0.1 Hz): 800-1200 Hz on ranges above 1, 1.8-2.2 kHz (ACV only), or above 9 kHz; outside the window the capture is not armed. The other AC points feed the frequency-response correction (`AcFreq_Correction`), which interpolates built-in tables scaled by four stored calibration points. The 1 ms PEAK gain reference is 42,426.4 counts (30,000 x sqrt 2).

### Temperature
In calibration mode, MEM on the temperature page sets the cold-junction trim so the thermistor reads 23.00 C.

### Variant markers
The two-point gain capture `Cal_AcGainCapture` (0x0800f7b8; despite the name it also serves Ohm range 6) is the only code that writes the board-variant markers: after storing the record it writes EEPROM 0x1F = 0xA5 when calibrating ACV range 0 and 0x26 = 0xA5 when calibrating Ohm range 6, each verified with up to 10 tries **[code]**. So a marker records that the factory calibrated that range with this procedure. Effects of the markers are in [../hardware/board.md](../hardware/board.md) section 9. Two quirks: after the 0x26 write the RAM flag is set to "no variant" for the rest of the session, so Ohm range 5/6 behave differently before and after the next power-on; after the 0x1F write the RAM flag stays as it was until reboot.

A production meter's CAL.BIN has **0x1F = 0xA5 and 0x26 = 0x00 [meter]**. Synthetic test images that set both do not match a real meter on 0x26.

### Calibration defects zombu4 reported, confirmed in emulation
- **OF-36:** a capture armed and then followed by a range or mode change completes in the new range and writes the wrong record.
- **OF-07:** continuity's record (0x120) is Ohm range 1's; calibrating in continuity overwrites it.
- **OF-08:** entering calibration mode clears all six current zeros without notice (possibly deliberate).
- **OF-37:** a refusal beep after a correctly started AC capture, which also zeroes a stored point.

## 3. Current-zero trims

A **long REL in any current mode (AC/DC uA, mA, A) on range 0** stores the present reading as a persistent zero (`CurrentZero_Store` 0x080114bc) **[code, emulated]**:

- slot at EEPROM **0x4D0 + 8 x slot**, slots AC uA, DC uA, AC mA, DC mA, AC A, DC A; each 0xA5 + u32 big-endian;
- added to the live trim (0x20007e0c); `CurrentZero_Load` reloads it on every mode or range change; handlers subtract it on range 0 only (AC clamps at 0);
- nothing on the display shows a trim is active, and only a calibration-mode power-on clears it.

**Defect [emulated].** The live trim accumulates (`trim += reading`) but EEPROM receives only the new reading. One long REL survives a power cycle; a second one (reading about 0 by then) stores about 0, so after the next power-on the zero is gone (20.000 uA reads 20.000 again). zombu4 list it as OF-09. Our fix `zero-trim` changes one literal-pool offset at 0x0801644a so the stored value is the accumulated trim; emulated, the zero survives two long RELs and a power cycle, and an ordinary run's peripheral trace is unchanged.

## 4. CAL.BIN

Power on holding **MEM** ("SD.CAL"), then **REL** = save ("SD.SAv") or **PEAK** = load ("SD.SET"), and **MODE** to run it. Result: "SAvEd" or "-Err-" **[code, emulated, meter]**.

- `CAL.BIN` in the card's root holds **EEPROM 0x000-0x4AF verbatim, 1200 bytes**: the settings bytes, the variant markers and every calibration record. Its timestamp comes from the clock chip.
- **What it omits:** the current-zero trims at 0x4D0-0x4FF. A restore therefore brings back the calibration but not the zeros. zombu4 list this as OF-12. Our C patch (`cal.c`) extends the file to 1280 bytes (0x000-0x4FF); older 1200-byte files still load and leave the trims alone **[emulated: save/load of both formats, no-card error, card passes fsck]**.
- **Load restores everything in the file,** settings and markers included. A CAL.BIN from another meter carries that meter's calibration and variant markers.
- Load reads the file in three pieces (512 + 512 + 176), writes the bytes one at a time (up to 10 tries each), reads them back and compares: 8 = short read, 9 = read-back mismatch, 0 = ok. A successful load also shows "SAvEd" (both paths call the same message).
- The SD card is initialized twice per save. The SD side (FatFs, card sizes, the 4 GiB address wrap that also affects CAL.BIN on large cards) is in [storage.md](storage.md).

**Before any reflash, save CAL.BIN**: a firmware image never touches the data EEPROM, but a mistake in custom firmware can, and the calibration cannot be recreated without reference sources.
