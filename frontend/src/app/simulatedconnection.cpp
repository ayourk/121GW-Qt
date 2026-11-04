// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Connection/SimulatedMeterConnection.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "simulatedconnection.h"

namespace readout {

QString connectionStateName(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Disconnected: return QStringLiteral("Disconnected");
    case ConnectionState::Scanning: return QStringLiteral("Scanning");
    case ConnectionState::Connecting: return QStringLiteral("Connecting");
    case ConnectionState::Connected: return QStringLiteral("Connected");
    }
    return {};
}

SimulatedConnection::SimulatedConnection(QObject *parent) : MeterConnection(parent)
{
    m_timer.setInterval(200);
    connect(&m_timer, &QTimer::timeout, this, &SimulatedConnection::tick);
    connect(&m_card, &SimulatedCard::transmit, this, [this](const QByteArray &bytes) {
        if (state() == ConnectionState::Connected)
            deliver(bytes);
    });
}

DiscoveredMeter SimulatedConnection::demoMeter()
{
    return {QStringLiteral("demo"), QStringLiteral("121GW (demo)"), -50};
}

std::optional<int> SimulatedConnection::signalStrength() const
{
    if (state() == ConnectionState::Connected)
        return demoMeter().rssi;
    return std::nullopt;
}

void SimulatedConnection::startScan()
{
    if (state() != ConnectionState::Disconnected)
        return;
    setState(ConnectionState::Scanning);
    emit meterDiscovered(demoMeter());
}

void SimulatedConnection::stopScan()
{
    if (state() == ConnectionState::Scanning)
        setState(ConnectionState::Disconnected);
}

void SimulatedConnection::connectTo(const DiscoveredMeter &)
{
    if (state() == ConnectionState::Connected)
        return;
    m_sim = SimulatedMeter();
    m_meter = demoMeter();
    resetFraming();
    m_poweredOn.start();
    setState(ConnectionState::Connecting);
    setState(ConnectionState::Connected);
    m_timer.start();
}

void SimulatedConnection::disconnectFrom()
{
    m_timer.stop();
    m_card.linkDown();
    m_meter.reset();
    setState(ConnectionState::Disconnected);
}

void SimulatedConnection::sendKey(MeterKey key, bool longPress)
{
    if (state() != ConnectionState::Connected)
        return;
    if (m_card.inQuietPeriod()) {
        ++m_droppedKeys;   // as the meter does in the second after a transfer
        return;
    }
    m_sim.press(key, longPress);
}

bool SimulatedConnection::sendCommand(const QByteArray &command)
{
    if (state() != ConnectionState::Connected)
        return false;
    // Card commands and YMODEM; the demo meter's clock is this computer's, so F8 is just ignored.
    m_card.receive(command);
    return true;
}

void SimulatedConnection::turnDial(int position)
{
    m_sim.turnDial(position);
}

void SimulatedConnection::tick()
{
    if (m_card.transferActive() || m_card.transmitting() || m_card.inQuietPeriod())
        return;   // the meter pauses its status packets during a file transfer, and never splits a frame
    const QByteArray bytes = m_sim.packetAt(m_poweredOn.elapsed() / 1000.0);
    if (bytes.isEmpty())
        return;   // switched off: a real meter goes quiet (its Bluetooth goes with it)
    for (const QByteArray &part : {bytes.left(7), bytes.mid(7)})
        deliver(part);
}

} // namespace readout
