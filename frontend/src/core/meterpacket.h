// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Protocol/MeterPacket.cs, PacketFramer.cs), Copyright (c) 2026 Alejandro Mora,
// MIT; which ports the official EEVblog 121GW app (121GW.Core/Controls/Multimeter/Packet121GW.cs on its
// PrivatePostRelease branch), Copyright (c) 2018 David Ledger, MIT.
#pragma once

#include "metermode.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QList>
#include <optional>

namespace readout {

// One 19-byte status packet from the 121GW. The meter sends one several times a second.
//
//  0     0xF2 start byte
//  1     year - 2000
//  2     month (high nibble), serial digit 1 (low nibble)
//  3-4   serial digits 2-5, one per nibble
//  5     main mode (bits 0-4), main value bits 16-17 (bits 6-7)
//  6     main flags (high nibble: overload, negative, °C, °F), main range (low nibble)
//  7-8   main value bits 0-15, big-endian
//  9     sub mode
//  10    sub flags (high nibble: overload, negative, k, Hz), sub decimal places (low nibble)
//  11-12 sub value, big-endian
//  13    bar status
//  14    bar value (bits 0-4)
//  15-17 status bytes 1-3 (symbols)
//  18    XOR of bytes 0-17
class MeterPacket
{
public:
    static constexpr int Length = 19;
    static constexpr std::uint8_t StartByte = 0xF2;

    // An all-zero packet (DC volts, nothing lit), so readings can be default-constructed and kept in lists.
    MeterPacket() : m_data(Length, '\0') {}

    // One whole packet; nullopt when the length, start byte or checksum is wrong.
    static std::optional<MeterPacket> tryParse(QByteArrayView frame);
    // True when byte 18 is the XOR of bytes 0-17.
    static bool checksumMatches(QByteArrayView frame);
    // XOR of every byte.
    static std::uint8_t checksum(QByteArrayView bytes);

    // The packet's bytes, start byte and checksum included.
    const QByteArray &bytes() const { return m_data; }
    QString hex() const;

    // Identity: the meter sends a fixed year and month (not today's date), and the serial
    // number the user set in SETUP.
    int year() const { return byte(1) + 2000; }
    int month() const { return high(2); }
    int serial() const { return low(2) * 10000 + high(3) * 1000 + low(3) * 100 + high(4) * 10 + low(4); }

    // Main display. rawMode() is the mode bits as sent; mode() resolves plain Temperature to °C or °F.
    MeterMode rawMode() const { return static_cast<MeterMode>(byte(5) & 0x1F); }
    MeterMode mode() const;
    bool mainOverload() const { return (high(6) & 0x8) != 0; }
    bool mainNegative() const { return (high(6) & 0x4) != 0; }
    bool mainCelsius() const { return (high(6) & 0x2) != 0; }
    bool mainFahrenheit() const { return (high(6) & 0x1) != 0; }
    int mainRangeIndex() const { return low(6); }
    // The main reading without its decimal point: 18 bits, so up to 262143.
    int mainRawValue() const { return ((byte(5) >> 6) & 0x3) << 16 | byte(7) << 8 | byte(8); }

    // Sub display.
    MeterMode subMode() const { return static_cast<MeterMode>(byte(9)); }
    bool subOverload() const { return (high(10) & 0x8) != 0; }
    bool subNegative() const { return (high(10) & 0x4) != 0; }
    bool subKilo() const { return (high(10) & 0x2) != 0; }   // the "k" annunciator
    bool subHertz() const { return (high(10) & 0x1) != 0; }
    int subDecimals() const { return byte(10) & 0x07; }      // 0-4; bit 3 is always 0
    int subRawValue() const { return byte(11) << 8 | byte(12); }

    // Bar graph. It shows when bit 4 is clear: EEVblog's packet document labels the bit "USE",
    // which reads as the opposite, but the shipped app treats set as off, and others have
    // confirmed that on real meters. barNegative() is the official app's net effect ("set =
    // negative"; it decodes the bit inverted and then swaps the two LCD layers).
    bool barOn() const { return (byte(13) & 0x10) == 0; }
    bool barTicks150() const { return (byte(13) & 0x08) != 0; }   // 0-150 tick labels instead of 0-5
    bool barNegative() const { return (byte(13) & 0x04) != 0; }
    BarScale barScale() const { return static_cast<BarScale>(byte(13) & 0x03); }
    int barValue() const { return byte(14) & 0x1F; }             // 0-31; the LCD lights this + 1 of 26

    // Status 1.
    std::uint8_t status1() const { return byte(15); }
    bool celsiusIcon() const { return (status1() & 0x80) != 0; }
    bool lowPassFilter() const { return (status1() & 0x40) != 0; }   // "1kHz": the AC low-pass filter
    bool oneMsPeak() const { return (status1() & 0x20) != 0; }       // "1ms": 1 ms PEAK capture
    Coupling coupling() const { return static_cast<Coupling>((status1() >> 3) & 0x3); }
    bool autoRange() const { return (status1() & 0x04) != 0; }
    bool autoPowerOff() const { return (status1() & 0x02) != 0; }
    bool lowBattery() const { return (status1() & 0x01) != 0; }

    // Status 2.
    std::uint8_t status2() const { return byte(16); }
    bool fahrenheitIcon() const { return (status2() & 0x80) != 0; }
    bool bluetooth() const { return (status2() & 0x40) != 0; }
    bool arrow() const { return (status2() & 0x20) != 0; }
    bool relative() const { return (status2() & 0x10) != 0; }
    bool decibels() const { return (status2() & 0x08) != 0; }
    MinMaxState minMax() const { return static_cast<MinMaxState>(status2() & 0x07); }

    // Status 3.
    std::uint8_t status3() const { return byte(17); }
    bool test() const { return (status3() & 0x40) != 0; }
    int memory() const { return (status3() & 0x30) >> 4; }   // 0 when memory is off, 1-3 otherwise
    HoldState hold() const { return static_cast<HoldState>((status3() >> 2) & 0x3); }
    bool acIcon() const { return (status3() & 0x02) != 0; }
    bool dcIcon() const { return (status3() & 0x01) != 0; }

private:
    explicit MeterPacket(QByteArray data) : m_data(std::move(data)) {}
    int byte(int index) const { return static_cast<std::uint8_t>(m_data[index]); }
    int high(int index) const { return byte(index) >> 4; }
    int low(int index) const { return byte(index) & 0x0F; }

    QByteArray m_data;
};

// Turns the meter's byte stream into packets. A packet can arrive split across Bluetooth
// notifications, or several in one, so bytes are buffered until a whole valid packet is there.
// Unlike the original app, a frame with a bad checksum doesn't throw the bytes after it away:
// the search restarts one byte later, so the next good packet is found at once.
class PacketFramer
{
public:
    // Adds bytes and returns every whole packet they complete, oldest first.
    QList<MeterPacket> push(QByteArrayView bytes);
    void reset() { m_buffer.clear(); }
    // Frames that started with 0xF2 but failed the checksum.
    int rejectedFrames() const { return m_rejected; }

private:
    QByteArray m_buffer;
    int m_rejected = 0;
};

} // namespace readout
