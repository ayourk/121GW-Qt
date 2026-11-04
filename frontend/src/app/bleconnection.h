// SPDX-License-Identifier: MIT
// Ported from Readout (Readout/Services/BleMeterConnection.cs), Copyright (c) 2026 Alejandro Mora, MIT,
// from Plugin.BLE to Qt Bluetooth (BlueZ over D-Bus on Linux).
#pragma once

#include "meterconnection.h"

#include <QBluetoothDeviceInfo>
#include <QHash>
#include <QLowEnergyService>
#include <QPointer>
#include <QTimer>

class QBluetoothDeviceDiscoveryAgent;
class QLowEnergyController;

namespace readout {

// The 121GW over Bluetooth LE: scans for it by name, subscribes to its data characteristic and
// frames what arrives into packets. Keys, commands and file transfers are written to the same
// characteristic, in pieces of one ATT write each.
class BleConnection : public MeterConnection
{
    Q_OBJECT
public:
    explicit BleConnection(QObject *parent = nullptr);
    ~BleConnection() override;

    std::optional<int> signalStrength() const override { return m_rssi; }

    void startScan() override;
    void stopScan() override;
    void connectTo(const DiscoveredMeter &meter) override;
    void disconnectFrom() override;
    void sendKey(MeterKey key, bool longPress) override;
    bool sendCommand(const QByteArray &command) override;

    static bool isMeterName(const QString &name);

private:
    void onDeviceFound(const QBluetoothDeviceInfo &info);
    void onServiceDiscoveryFinished();
    void onServiceState(QLowEnergyService::ServiceState state);
    void onValue(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void fail(const QString &message);
    void closeLink();
    void linkDropped();
    void pumpWrites();
    void writeDone();

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QPointer<QLowEnergyController> m_controller;
    QList<QLowEnergyService *> m_services;
    QLowEnergyService *m_dataService = nullptr;
    QLowEnergyCharacteristic m_data;
    QHash<QString, QBluetoothDeviceInfo> m_seen;
    QList<QByteArray> m_writeQueue;
    bool m_writing = false;
    QTimer m_connectTimeout;
    std::optional<int> m_rssi;
    bool m_disconnecting = false;
    int m_pendingServices = 0;
};

} // namespace readout
