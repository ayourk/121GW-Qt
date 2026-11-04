// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Connection/IMeterConnection.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#pragma once

#include "meterpacket.h"

#include <QObject>
#include <optional>

namespace readout {

enum class ConnectionState { Disconnected, Scanning, Connecting, Connected };
QString connectionStateName(ConnectionState state);

// A meter seen while scanning. id is the Bluetooth address (stable across launches).
struct DiscoveredMeter {
    QString id;
    QString name;
    int rssi = 0;
};

// One way of talking to a meter: Bluetooth LE, or the demo meter. Lives on the main thread.
class MeterConnection : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    ConnectionState state() const { return m_state; }
    // The meter being connected to or connected; nullopt otherwise.
    const std::optional<DiscoveredMeter> &meter() const { return m_meter; }
    // dBm (−40 strong, −90 weak); nullopt when unknown.
    virtual std::optional<int> signalStrength() const { return std::nullopt; }

    virtual void startScan() = 0;
    virtual void stopScan() = 0;
    virtual void connectTo(const DiscoveredMeter &meter) = 0;
    virtual void disconnectFrom() = 0;
    // Presses one of the meter's buttons remotely.
    virtual void sendKey(MeterKey key, bool longPress) = 0;
    // Sends a raw command (the F8 clock command, card commands, YMODEM bytes); false when it can't.
    virtual bool sendCommand(const QByteArray &command) = 0;

    // Raw mode, for file transfers: received bytes only go out as bytesReceived, not framed into
    // packets (the meter pauses its status packets during a transfer).
    bool rawMode() const { return m_rawMode; }
    void setRawMode(bool on)
    {
        m_rawMode = on;
        m_framer.reset();
    }

signals:
    void stateChanged();
    void meterDiscovered(const readout::DiscoveredMeter &meter);
    void packetReceived(const readout::MeterPacket &packet);
    // Every byte the meter sends, as it arrives (card command replies, YMODEM).
    void bytesReceived(const QByteArray &bytes);
    // The link dropped without disconnectFrom() being called.
    void connectionLost();
    // Something went wrong that the user should see ("Bluetooth is off").
    void errorOccurred(const QString &message);

protected:
    void setState(ConnectionState state)
    {
        if (m_state == state)
            return;
        m_state = state;
        emit stateChanged();
    }
    // Hands bytes from the meter on: always as bytesReceived, and framed into packets unless in raw mode.
    void deliver(const QByteArray &bytes)
    {
        emit bytesReceived(bytes);
        if (m_rawMode)
            return;
        for (const MeterPacket &packet : m_framer.push(bytes))
            emit packetReceived(packet);
    }
    void resetFraming()
    {
        m_framer.reset();
        m_rawMode = false;
    }
    std::optional<DiscoveredMeter> m_meter;

private:
    ConnectionState m_state = ConnectionState::Disconnected;
    PacketFramer m_framer;
    bool m_rawMode = false;
};

} // namespace readout

Q_DECLARE_METATYPE(readout::DiscoveredMeter)
Q_DECLARE_METATYPE(readout::MeterPacket)
