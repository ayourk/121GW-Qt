// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Protocol/MeterPacket.cs, PacketFramer.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "meterpacket.h"

#include <QString>

namespace readout {

std::optional<MeterPacket> MeterPacket::tryParse(QByteArrayView frame)
{
    if (frame.size() != Length || static_cast<std::uint8_t>(frame[0]) != StartByte || !checksumMatches(frame))
        return std::nullopt;
    return MeterPacket(frame.toByteArray());
}

bool MeterPacket::checksumMatches(QByteArrayView frame)
{
    return frame.size() == Length
        && checksum(frame.first(Length - 1)) == static_cast<std::uint8_t>(frame[Length - 1]);
}

std::uint8_t MeterPacket::checksum(QByteArrayView bytes)
{
    std::uint8_t sum = 0;
    for (const char b : bytes)
        sum ^= static_cast<std::uint8_t>(b);
    return sum;
}

QString MeterPacket::hex() const
{
    return QString::fromLatin1(m_data.toHex()).toUpper();
}

MeterMode MeterPacket::mode() const
{
    const MeterMode m = rawMode();
    if (m == MeterMode::Temperature) {
        if (mainCelsius())
            return MeterMode::TemperatureC;
        if (mainFahrenheit())
            return MeterMode::TemperatureF;
    }
    return m;
}

QList<MeterPacket> PacketFramer::push(QByteArrayView bytes)
{
    // Far more than one packet; only reached when the stream is garbage.
    constexpr int MaxBuffered = 1024;
    m_buffer.append(bytes.data(), bytes.size());
    if (m_buffer.size() > MaxBuffered)
        m_buffer.remove(0, m_buffer.size() - MeterPacket::Length);

    QList<MeterPacket> packets;
    qsizetype start = 0;
    while (true) {
        start = m_buffer.indexOf(static_cast<char>(MeterPacket::StartByte), start);
        if (start < 0) {
            m_buffer.clear();
            break;
        }
        if (m_buffer.size() - start < MeterPacket::Length) {
            m_buffer.remove(0, start);
            break;
        }
        if (auto packet = MeterPacket::tryParse(QByteArrayView(m_buffer).sliced(start, MeterPacket::Length))) {
            packets.append(*packet);
            start += MeterPacket::Length;
        } else {
            ++m_rejected;
            ++start;
        }
    }
    return packets;
}

} // namespace readout
