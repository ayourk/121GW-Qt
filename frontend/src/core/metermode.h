// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Protocol/MeterMode.cs, MeterKey.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#pragma once

#include <QByteArray>
#include <cstdint>

namespace readout {

// What the meter is measuring. The main display uses 0-26. The sub display uses the same
// values, plus 100 and up for the SETUP pages (battery, date, auto power-off and so on).
// Values and names follow the official EEVblog app's Packet121GW.eMode.
enum class MeterMode : std::uint8_t {
    LowZ = 0,
    DcVolts = 1,
    AcVolts = 2,
    DcMillivolts = 3,
    AcMillivolts = 4,
    // Temperature before the °C/°F bits are applied; MeterPacket::mode() resolves it.
    Temperature = 5,
    Frequency = 6,
    PulseWidth = 7,   // milliseconds
    Duty = 8,
    Resistance = 9,
    Continuity = 10,
    Diode = 11,
    Capacitance = 12,
    AcMicroVoltAmps = 13,
    AcMilliVoltAmps = 14,
    AcVoltAmps = 15,
    AcMicroamps = 16,
    DcMicroamps = 17,
    AcMilliamps = 18,
    DcMilliamps = 19,
    AcAmps = 20,
    DcAmps = 21,
    DcMicroVoltAmps = 22,
    DcMilliVoltAmps = 23,
    DcVoltAmps = 24,
    TemperatureC = 25,
    TemperatureF = 26,
    // Sub display only: the SETUP pages.
    SetupTemperatureC = 100,
    SetupTemperatureF = 105,
    Battery = 110,
    AutoPowerOffOn = 120,
    AutoPowerOffOff = 125,
    Year = 130,
    Date = 135,
    Time = 140,
    BurdenVoltage = 150,
    LcdContrast = 160,
    Dbm = 180,
    Interval = 190,
};

// The AC/DC annunciators, from status byte 1.
enum class Coupling : std::uint8_t { None = 0, Dc = 1, Ac = 2, AcPlusDc = 3 };

// Full scale of the bar graph, from the bar status byte.
enum class BarScale : std::uint8_t { Five = 0, Fifty = 1, FiveHundred = 2, OneThousand = 3 };

// MIN/MAX state, from status byte 2. The official app lights MAX, MIN and AVG together for All.
enum class MinMaxState : std::uint8_t { Off = 0, Max = 1, Min = 2, Average = 3, All = 4 };

// HOLD state, from status byte 3. AutoHold shows "A-HOLD" on the LCD.
enum class HoldState : std::uint8_t { Off = 0, AutoHold = 1, Hold = 2 };

// The meter's front-panel buttons, in the numbering its remote-key command uses.
enum class MeterKey : std::uint8_t {
    Range = 1,
    Hold = 2,
    Rel = 3,    // REL, which is also ▲ inside SETUP
    Peak = 4,   // 1ms PEAK, which is also ▼ inside SETUP
    Mode = 5,
    MinMax = 6,
    Mem = 7,
    Setup = 8,
};

// "RANGE", "HOLD"… as the meter's panel labels them; empty for a value that isn't a key.
const char *keyName(MeterKey key);
// The key's plain id, as Readout's MCP tools report it: RANGE, HOLD, REL, PEAK, MODE, MINMAX, MEM, SETUP.
const char *keyId(MeterKey key);

// The key for a name ("range", "MinMax", "1ms peak"…), case-insensitive; false if none.
bool keyFromName(const QString &name, MeterKey *key);

// The five-byte command that presses a button remotely: F4 k c k c, where k is ASCII '0' for a
// short press or '8' for a long one, and c is the key's number as an ASCII digit.
QByteArray keyPressCommand(MeterKey key, bool longPress = false);

// Sets the meter's clock (firmware 2.05, undocumented): F8, then year (2 digits), month, day,
// hour, minute and second as two ASCII hex digits each, then their XOR the same way; 15 bytes.
// The meter takes it once per Bluetooth session.
QByteArray setClockCommand(int year, int month, int day, int hour, int minute, int second);

} // namespace readout
