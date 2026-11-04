// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Simulation/PacketBuilder.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "packetbuilder.h"

#include <QString>
#include <stdexcept>

namespace readout {

namespace {
int flags(bool b3, bool b2, bool b1, bool b0)
{
    return (b3 ? 8 : 0) | (b2 ? 4 : 0) | (b1 ? 2 : 0) | (b0 ? 1 : 0);
}
} // namespace

QByteArray PacketBuilder::build() const
{
    QByteArray d(MeterPacket::Length, '\0');
    const auto set = [&d](int i, int v) { d[i] = static_cast<char>(v & 0xFF); };
    const QString s = QStringLiteral("%1").arg(serial, 5, 10, QLatin1Char('0'));
    const auto digit = [&s](int i) { return s[i].unicode() - '0'; };
    set(0, MeterPacket::StartByte);
    set(1, year - 2000);
    set(2, month << 4 | digit(0));
    set(3, digit(1) << 4 | digit(2));
    set(4, digit(3) << 4 | digit(4));
    set(5, (static_cast<int>(mode) & 0x1F) | ((mainRawValue >> 16) & 0x3) << 6);
    set(6, flags(mainOverload, mainNegative, mainCelsius, mainFahrenheit) << 4 | (mainRangeIndex & 0x0F));
    set(7, mainRawValue >> 8);
    set(8, mainRawValue);
    set(9, static_cast<int>(subMode));
    set(10, flags(subOverload, subNegative, subKilo, subHertz) << 4 | (subDecimals & 0x07));
    set(11, subRawValue >> 8);
    set(12, subRawValue);
    set(13, (barOn ? 0 : 0x10) | (barTicks150 ? 0x08 : 0) | (barNegative ? 0x04 : 0) | (static_cast<int>(barScale) & 0x3));
    set(14, barValue & 0x1F);
    set(15, (celsiusIcon ? 0x80 : 0) | (lowPassFilter ? 0x40 : 0) | (oneMsPeak ? 0x20 : 0)
            | (static_cast<int>(coupling) & 0x3) << 3 | (autoRange ? 0x04 : 0) | (autoPowerOff ? 0x02 : 0)
            | (lowBattery ? 0x01 : 0));
    set(16, (fahrenheitIcon ? 0x80 : 0) | (bluetooth ? 0x40 : 0) | (arrow ? 0x20 : 0) | (relative ? 0x10 : 0)
            | (decibels ? 0x08 : 0) | (static_cast<int>(minMax) & 0x07));
    set(17, (test ? 0x40 : 0) | (memory & 0x3) << 4 | (static_cast<int>(hold) & 0x3) << 2
            | (acIcon ? 0x02 : 0) | (dcIcon ? 0x01 : 0));
    set(18, MeterPacket::checksum(QByteArrayView(d).first(MeterPacket::Length - 1)));
    return d;
}

MeterPacket PacketBuilder::buildPacket() const
{
    if (auto packet = MeterPacket::tryParse(build()))
        return *packet;
    throw std::logic_error("PacketBuilder produced an invalid packet");
}

} // namespace readout
