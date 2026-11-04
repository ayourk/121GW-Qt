// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Simulation/PacketBuilder.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#pragma once

#include "meterpacket.h"

namespace readout {

// Builds valid 19-byte packets field by field: the inverse of MeterPacket. The demo meter and
// the tests use it.
struct PacketBuilder {
    int year = 2017;
    int month = 10;
    int serial = 12345;            // five decimal digits, 0-99999
    MeterMode mode = MeterMode::DcVolts;
    int mainRangeIndex = 0;
    int mainRawValue = 0;          // 0-262143 (18 bits)
    bool mainOverload = false, mainNegative = false, mainCelsius = false, mainFahrenheit = false;
    MeterMode subMode = MeterMode::LowZ;
    int subRawValue = 0;
    int subDecimals = 0;
    bool subOverload = false, subNegative = false, subKilo = false, subHertz = false;
    bool barOn = true, barTicks150 = false, barNegative = false;
    BarScale barScale = BarScale::Five;
    int barValue = 0;              // 0-31
    bool celsiusIcon = false, fahrenheitIcon = false, lowPassFilter = false, oneMsPeak = false;
    Coupling coupling = Coupling::None;
    bool autoRange = false, autoPowerOff = false, lowBattery = false;
    bool bluetooth = true, arrow = false, relative = false, decibels = false;
    MinMaxState minMax = MinMaxState::Off;
    bool test = false;
    int memory = 0;                // 0-3
    HoldState hold = HoldState::Off;
    bool acIcon = false, dcIcon = false;

    QByteArray build() const;
    MeterPacket buildPacket() const;
};

} // namespace readout
