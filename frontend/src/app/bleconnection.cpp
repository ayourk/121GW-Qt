// SPDX-License-Identifier: MIT
// Ported from Readout (Readout/Services/BleMeterConnection.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "bleconnection.h"

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include <QBluetoothUuid>
#include <QLoggingCategory>
#include <QLowEnergyController>

Q_LOGGING_CATEGORY(lcBle, "121gw.ble")

namespace readout {

namespace {

// The 121GW's data characteristic: indications carry the packets, writes carry key presses.
const QBluetoothUuid DataCharacteristic(QStringLiteral("{E7ADD780-B042-4876-AAE1-112855353CC1}"));
constexpr int ConnectTimeoutMs = 15000;
constexpr int WriteWithoutResponseGapMs = 15;

bool isStandardService(const QBluetoothUuid &uuid)
{
    bool ok = false;
    const quint16 shortId = uuid.toUInt16(&ok);
    // 0x1800 Generic Access, 0x1801 Generic Attribute, 0x180A Device Information.
    return ok && (shortId == 0x1800 || shortId == 0x1801 || shortId == 0x180A);
}

} // namespace

BleConnection::BleConnection(QObject *parent) : MeterConnection(parent)
{
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(10000);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &BleConnection::onDeviceFound);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceUpdated, this,
            [this](const QBluetoothDeviceInfo &info, QBluetoothDeviceInfo::Fields) { onDeviceFound(info); });
    // Each scan ends after the timeout; keep restarting it until stopped, as Readout does.
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished, this, [this] {
        if (state() == ConnectionState::Scanning)
            m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    });
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
            [this](QBluetoothDeviceDiscoveryAgent::Error) {
                qCWarning(lcBle) << "scan failed:" << m_agent->errorString();
                if (state() == ConnectionState::Scanning) {
                    setState(ConnectionState::Disconnected);
                    emit errorOccurred(QStringLiteral("Bluetooth scan failed: %1").arg(m_agent->errorString()));
                }
            });
    m_connectTimeout.setSingleShot(true);
    connect(&m_connectTimeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("The meter didn't answer in time.")); });
}

BleConnection::~BleConnection()
{
    m_disconnecting = true;
    closeLink();
}

bool BleConnection::isMeterName(const QString &name)
{
    // The meter advertises as "121GW"; early firmware as the Bluegiga module's own name.
    return name.contains(QLatin1String("121GW"), Qt::CaseInsensitive)
        || name.startsWith(QLatin1String("Bluegiga"), Qt::CaseInsensitive);
}

void BleConnection::startScan()
{
    if (state() != ConnectionState::Disconnected)
        return;
    QBluetoothLocalDevice local;
    if (!local.isValid() || local.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        emit errorOccurred(QStringLiteral("Bluetooth is off or this computer has no Bluetooth adapter."));
        return;
    }
    qCInfo(lcBle) << "scan start";
    m_seen.clear();
    setState(ConnectionState::Scanning);
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void BleConnection::stopScan()
{
    if (m_agent->isActive())
        m_agent->stop();
    if (state() == ConnectionState::Scanning)
        setState(ConnectionState::Disconnected);
}

void BleConnection::onDeviceFound(const QBluetoothDeviceInfo &info)
{
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration) || !isMeterName(info.name()))
        return;
    // macOS (CoreBluetooth) never reveals a device's address, only a per-computer UUID: every
    // address there is 00:00:00:00:00:00, so the UUID is the meter's id.
    const QString id = info.address().isNull() ? info.deviceUuid().toString() : info.address().toString();
    m_seen[id] = info;
    emit meterDiscovered({id, info.name(), info.rssi()});
}

void BleConnection::connectTo(const DiscoveredMeter &meter)
{
    if (state() == ConnectionState::Connected || state() == ConnectionState::Connecting)
        return;
    if (!m_seen.contains(meter.id)) {
        emit errorOccurred(QStringLiteral("That meter hasn't been seen in this scan."));
        return;
    }
    const QBluetoothDeviceInfo info = m_seen.value(meter.id);
    stopScan();
    m_meter = meter;
    m_rssi = meter.rssi;
    setState(ConnectionState::Connecting);
    qCInfo(lcBle) << "connect" << meter.name << meter.id;

    m_controller = QLowEnergyController::createCentral(info, this);
    connect(m_controller, &QLowEnergyController::connected, m_controller, &QLowEnergyController::discoverServices);
    connect(m_controller, &QLowEnergyController::discoveryFinished, this, &BleConnection::onServiceDiscoveryFinished);
    connect(m_controller, &QLowEnergyController::errorOccurred, this, [this](QLowEnergyController::Error) {
        if (state() == ConnectionState::Connecting)
            fail(QStringLiteral("Couldn't connect: %1").arg(m_controller ? m_controller->errorString() : QString()));
    });
    connect(m_controller, &QLowEnergyController::disconnected, this, &BleConnection::linkDropped);
    resetFraming();
    m_connectTimeout.start(ConnectTimeoutMs);
    m_controller->connectToDevice();
}

void BleConnection::onServiceDiscoveryFinished()
{
    // Every service is opened to find the data characteristic; the meter has only a few.
    const QList<QBluetoothUuid> uuids = m_controller->services();
    m_pendingServices = 0;
    for (const QBluetoothUuid &uuid : uuids) {
        QLowEnergyService *service = m_controller->createServiceObject(uuid, this);
        if (!service)
            continue;
        m_services << service;
        ++m_pendingServices;
        connect(service, &QLowEnergyService::stateChanged, this, &BleConnection::onServiceState);
        connect(service, &QLowEnergyService::characteristicChanged, this, &BleConnection::onValue);
        connect(service, &QLowEnergyService::characteristicWritten, this, [this](const QLowEnergyCharacteristic &c) {
            if (c.uuid() == m_data.uuid())
                writeDone();
        });
        connect(service, &QLowEnergyService::errorOccurred, this, [this](QLowEnergyService::ServiceError e) {
            if (e == QLowEnergyService::CharacteristicWriteError) {
                qCWarning(lcBle) << "write failed";
                writeDone();   // a file transfer's own retries cover a lost chunk
            }
        });
        service->discoverDetails();
    }
    if (m_pendingServices == 0)
        fail(QStringLiteral("This device has no 121GW data characteristic."));
}

void BleConnection::onServiceState(QLowEnergyService::ServiceState s)
{
    if (s != QLowEnergyService::RemoteServiceDiscovered || state() != ConnectionState::Connecting)
        return;
    if (--m_pendingServices > 0)
        return;

    // The data characteristic by UUID, else the first one that can notify outside the standard services.
    QLowEnergyService *found = nullptr;
    QLowEnergyCharacteristic data, fallback;
    QLowEnergyService *fallbackService = nullptr;
    for (QLowEnergyService *service : std::as_const(m_services)) {
        for (const QLowEnergyCharacteristic &c : service->characteristics()) {
            if (c.uuid() == DataCharacteristic) {
                found = service;
                data = c;
            } else if (!fallback.isValid() && !isStandardService(service->serviceUuid())
                       && (c.properties() & (QLowEnergyCharacteristic::Notify | QLowEnergyCharacteristic::Indicate))) {
                fallback = c;
                fallbackService = service;
            }
        }
    }
    if (!found && fallbackService) {
        qCWarning(lcBle) << "data characteristic not found by UUID, using" << fallback.uuid();
        found = fallbackService;
        data = fallback;
    }
    if (!found) {
        fail(QStringLiteral("This device has no 121GW data characteristic."));
        return;
    }

    // Subscribe: indications if the characteristic has them (the 121GW does), else notifications.
    const QLowEnergyDescriptor cccd = data.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    if (cccd.isValid()) {
        const bool indicate = data.properties() & QLowEnergyCharacteristic::Indicate;
        found->writeDescriptor(cccd, indicate ? QByteArray::fromHex("0200") : QByteArray::fromHex("0100"));
    }
    m_dataService = found;
    m_data = data;
    m_connectTimeout.stop();
    setState(ConnectionState::Connected);
    qCInfo(lcBle) << "connected, data on" << data.uuid();
}

void BleConnection::onValue(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    if (c.uuid() != m_data.uuid() || value.isEmpty())
        return;
    deliver(value);
}

void BleConnection::sendKey(MeterKey key, bool longPress)
{
    qCInfo(lcBle) << "key" << keyName(key) << (longPress ? "long" : "short");
    sendCommand(keyPressCommand(key, longPress));
}

bool BleConnection::sendCommand(const QByteArray &command)
{
    if (!m_dataService || !m_data.isValid())
        return false;
    // 20-byte pieces (the default ATT MTU less 3, as the firmware's own PC tool writes them): a
    // YMODEM block goes one write at a time, in order; the meter reassembles the stream.
    constexpr int chunk = 20;
    for (qsizetype at = 0; at < command.size(); at += chunk)
        m_writeQueue << command.mid(at, chunk);
    pumpWrites();
    return true;
}

void BleConnection::pumpWrites()
{
    if (m_writing || m_writeQueue.isEmpty() || !m_dataService || !m_data.isValid())
        return;
    const bool withoutResponse = (m_data.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                                 && !(m_data.properties() & QLowEnergyCharacteristic::Write);
    m_writing = true;
    m_dataService->writeCharacteristic(m_data, m_writeQueue.takeFirst(),
                                       withoutResponse ? QLowEnergyService::WriteWithoutResponse
                                                       : QLowEnergyService::WriteWithResponse);
    // Without a response there is no characteristicWritten: pace the writes instead.
    if (withoutResponse)
        QTimer::singleShot(WriteWithoutResponseGapMs, this, &BleConnection::writeDone);
}

void BleConnection::writeDone()
{
    m_writing = false;
    pumpWrites();
}

void BleConnection::disconnectFrom()
{
    if (state() == ConnectionState::Scanning) {
        stopScan();
        return;
    }
    m_disconnecting = true;
    closeLink();
    m_disconnecting = false;
    m_meter.reset();
    setState(ConnectionState::Disconnected);
    qCInfo(lcBle) << "disconnected";
}

void BleConnection::fail(const QString &message)
{
    qCWarning(lcBle) << "connect failed:" << message;
    m_disconnecting = true;
    closeLink();
    m_disconnecting = false;
    m_meter.reset();
    setState(ConnectionState::Disconnected);
    emit errorOccurred(message);
}

void BleConnection::closeLink()
{
    m_connectTimeout.stop();
    m_rssi.reset();
    m_writeQueue.clear();
    m_writing = false;
    m_dataService = nullptr;
    m_data = {};
    qDeleteAll(m_services);
    m_services.clear();
    if (m_controller) {
        QLowEnergyController *controller = m_controller;
        m_controller = nullptr;
        controller->disconnect(this);
        controller->disconnectFromDevice();
        controller->deleteLater();
    }
}

void BleConnection::linkDropped()
{
    if (m_disconnecting || state() == ConnectionState::Disconnected)
        return;
    const bool wasConnected = state() == ConnectionState::Connected;
    qCWarning(lcBle) << "link lost";
    closeLink();
    m_meter.reset();
    setState(ConnectionState::Disconnected);
    if (wasConnected)
        emit connectionLost();
    else
        emit errorOccurred(QStringLiteral("The meter dropped the connection."));
}

} // namespace readout
