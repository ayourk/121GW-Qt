// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Display/MeterReading.cs, ReadingDecoder.cs, ModeInfo.cs, SiFormat.cs),
// Copyright (c) 2026 Alejandro Mora, MIT; which follow the official EEVblog 121GW app (MultimeterScreen.Update,
// mRangeLookup), Copyright (c) 2018 David Ledger, MIT.
#pragma once

#include "meterpacket.h"

#include <QDateTime>
#include <QString>
#include <optional>

namespace readout {

// The small display at the top right of the LCD.
struct SubDisplay {
    QString text;      // the digits, decimal point included, or "OFL"
    bool negative = false;
    QString unit;      // "Hz", "°C", "mA"… or empty
    QString label;     // what a SETUP page shows ("Battery", "Year"); empty for live readings
    QString line() const;
};

// The analog bar graph under the main digits.
struct BarGraph {
    static constexpr int TotalSegments = 26;
    bool on = false;
    int segments = 0;  // lit segments, 0 to TotalSegments
    bool negative = false;
    BarScale scale = BarScale::Five;
    bool ticks150 = false;  // tick labels 0-150 instead of 0-5
    int fullScale() const;
};

// The LCD's symbols and labels around the digits.
struct Annunciators {
    bool lowZ = false, ac = false, dc = false, acPlusDc = false, autoRange = false, autoPowerOff = false;
    bool lowBattery = false, bluetooth = false, relative = false, decibels = false;
    bool max = false, min = false, average = false, test = false, memory = false;
    bool hold = false, autoHold = false;   // autoHold: the "A-" before HOLD
    bool beep = false, diode = false;
    bool lowPassFilter = false;            // the "1kHz" low-pass filter symbol
    bool oneMsPeak = false;                // the "1ms" PEAK symbol
    bool arrow = false;
    // The lit symbols as words ("AUTO", "HOLD", "MAX"…), for text displays and the MCP tools.
    QStringList names() const;
};

// Everything the meter's LCD shows for one packet, decoded for display and logging.
struct MeterReading {
    MeterPacket packet;
    QDateTime receivedAt;
    MeterMode mode = MeterMode::DcVolts;
    QString modeName;          // "DC voltage", "Resistance"…
    QString mainText;          // the main digits as the LCD shows them ("12.106", " 0.0005"), or "OFL"
    bool mainNegative = false;
    bool mainOverload = false;
    QString mainUnit;          // the unit next to the main digits ("mV", "kΩ", "°C")
    std::optional<double> value;      // in mainUnit; nullopt on overload or an unknown range
    std::optional<double> baseValue;  // in baseUnit ("12.106 mV" is 0.012106 V)
    QString baseUnit;          // the SI unit for logging and graphs ("V", "Ω", "A")
    SubDisplay sub;
    BarGraph bar;
    Annunciators icons;
    // Right after a mode change the meter's display is blank until its first measurement, while
    // the packets say 0. Such a reading shows nothing and isn't logged.
    bool blank = false;

    int serial() const { return packet.serial(); }
    // The main reading as one line: "-12.106 mV", "OFL kΩ".
    QString mainLine() const;
};

namespace decoder {

// "DC voltage", "Resistance"… for a main mode.
QString modeName(MeterMode mode);

// The meter's own temperature: where the small display rests in most modes.
inline const QString MeterTemperaturePage = QStringLiteral("Meter temperature");

// Decodes a packet into what the LCD shows. previousSubLabel is the SETUP page the last packet
// showed, if any: a few pages are sent without a page number, and the page before them is the
// only way to tell which they are.
MeterReading decode(const MeterPacket &packet, const QDateTime &receivedAt, const QString &previousSubLabel = {});

// How many of the 5 main digits come before the decimal point in a mode's range; -1 if unknown.
int integerDigits(MeterMode mode, int range);

// The main digits: right-aligned in 5 places, the decimal point after integerDigits of them,
// zeros after the point (" 0.050", "12.106").
QString formatMain(int raw, int integerDigits);
// The sub display's digits: decimals after the point, "0.05" style.
QString formatSub(int raw, int decimals);

} // namespace decoder

namespace siformat {
// Formats a value in an SI base unit with a readable prefix: 0.0123 V -> "12.3 mV".
QString format(double value, const QString &unit, int significant = 4);
} // namespace siformat

} // namespace readout
