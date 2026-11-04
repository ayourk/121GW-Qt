// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Connection/SimulatedMeterConnection.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#pragma once

#include "meterconnection.h"
#include "simulatedcard.h"
#include "simulatedmeter.h"

#include <QElapsedTimer>
#include <QTimer>

namespace readout {

// A connection to SimulatedMeter: a scan finds it at once, and once connected it sends a packet
// every 200 ms, split across two "notifications" the way Bluetooth delivers them, so the framing
// path is the real one. Its dial is turned from the app. Commands go to its in-memory SD card;
// status packets pause while a file transfer runs, as on the meter.
class SimulatedConnection : public MeterConnection
{
    Q_OBJECT
public:
    explicit SimulatedConnection(QObject *parent = nullptr);
    static DiscoveredMeter demoMeter();

    SimulatedMeter &simulator() { return m_sim; }
    // Its SD card (kept across reconnects, so uploaded files stay).
    SimulatedCard &card() { return m_card; }
    // Key frames that arrived in the card's quiet second and were dropped.
    int droppedKeys() const { return m_droppedKeys; }
    std::optional<int> signalStrength() const override;

    void startScan() override;
    void stopScan() override;
    void connectTo(const DiscoveredMeter &meter) override;
    void disconnectFrom() override;
    void sendKey(MeterKey key, bool longPress) override;
    bool sendCommand(const QByteArray &command) override;
    void turnDial(int position);
    // Makes the next packet arrive now (tests drive the meter with this).
    void tick();

private:
    SimulatedMeter m_sim;
    SimulatedCard m_card;
    int m_droppedKeys = 0;
    QTimer m_timer;
    QElapsedTimer m_poweredOn;
};

} // namespace readout
