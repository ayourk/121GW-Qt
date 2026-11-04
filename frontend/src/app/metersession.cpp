// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Connection/MeterSession.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "metersession.h"

#include "dial.h"
#include "simulatedconnection.h"

#include <QTimer>
#include <algorithm>
#include <cmath>

namespace readout {

namespace {
// A display change this soon after sending a key is that key's, not a press on the meter.
constexpr qint64 SentKeyEchoMs = 1500;
// How long after a mode change a 0 can be the meter's blank display.
constexpr qint64 BlankAfterModeChangeMs = 2500;
} // namespace

MeterSession::MeterSession(MeterConnection *bluetooth, SimulatedConnection *demo, QObject *parent)
    : QObject(parent), m_bluetooth(bluetooth), m_demo(demo), m_active(bluetooth)
{
    for (MeterConnection *c : {bluetooth, static_cast<MeterConnection *>(demo)}) {
        connect(c, &MeterConnection::stateChanged, this, [this, c] {
            if (c != m_active)
                return;
            if (c->state() != ConnectionState::Connected) {
                m_latest.reset();
                setDial(isDemo() ? m_dial : -1, -1);
                emit readingReceived();
            }
            emit stateChanged();
        });
        connect(c, &MeterConnection::packetReceived, this, [this, c](const MeterPacket &p) {
            if (c == m_active)
                onPacket(p);
        });
        connect(c, &MeterConnection::errorOccurred, this, &MeterSession::errorOccurred);
    }
    connect(bluetooth, &MeterConnection::meterDiscovered, this, &MeterSession::onDiscovered);
    connect(bluetooth, &MeterConnection::connectionLost, this, &MeterSession::onConnectionLost);
}

bool MeterSession::isDemo() const
{
    return m_active == m_demo;
}

void MeterSession::setLogging(bool on)
{
    if (m_logging == on)
        return;
    m_logging = on;
    emit loggingChanged();
}

void MeterSession::clearLog()
{
    m_log.clear();
    emit logChanged();
}

QList<DiscoveredMeter> MeterSession::discovered() const
{
    QList<DiscoveredMeter> list = m_discovered.values();
    std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) { return a.rssi > b.rssi; });
    return list;
}

void MeterSession::startScan(bool autoConnect)
{
    leaveDemo();
    m_autoConnect = autoConnect;
    // A scan already running (a background reconnect) keeps its list: Bluetooth stacks report
    // each device once per scan, so what it found won't be reported again.
    if (m_bluetooth->state() != ConnectionState::Scanning) {
        m_discovered.clear();
        emit discoveredChanged();
    }
    m_userDisconnected = false;
    m_bluetooth->startScan();
}

void MeterSession::stopScan()
{
    m_bluetooth->stopScan();
}

void MeterSession::connectTo(const DiscoveredMeter &meter)
{
    leaveDemo();
    m_userDisconnected = false;
    m_bluetooth->stopScan();
    m_bluetooth->connectTo(meter);
    if (meter.id != preferredMeterId) {
        // Remembered once it actually connects.
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(m_bluetooth, &MeterConnection::stateChanged, this, [this, meter, conn] {
            if (m_bluetooth->state() == ConnectionState::Connected) {
                preferredMeterId = meter.id;
                emit preferredMeterChanged(meter.id);
            }
            if (m_bluetooth->state() != ConnectionState::Connecting)
                disconnect(*conn);
        });
    }
}

void MeterSession::startDemo()
{
    m_userDisconnected = true;
    m_bluetooth->stopScan();
    if (m_bluetooth->state() != ConnectionState::Disconnected)
        m_bluetooth->disconnectFrom();
    m_active = m_demo;
    emit stateChanged();
    m_demo->connectTo(SimulatedConnection::demoMeter());
    setDial(m_demo->simulator().dialPosition(), 0);
}

void MeterSession::disconnectFrom()
{
    m_userDisconnected = true;
    if (m_active->state() == ConnectionState::Scanning)
        m_active->stopScan();
    else
        m_active->disconnectFrom();
    if (isDemo())
        setDial(-1, -1);
}

void MeterSession::holdStockCommands(int ms)
{
    m_stockHold.start();
    m_stockHoldMs = ms;
}

int MeterSession::stockHoldLeftMs() const
{
    if (!m_stockHold.isValid())
        return 0;
    return static_cast<int>(std::max<qint64>(0, m_stockHoldMs - m_stockHold.elapsed()));
}

bool MeterSession::sendKey(MeterKey key, bool longPress)
{
    // During a file transfer a key frame would land in the middle of YMODEM.
    if (m_active->state() != ConnectionState::Connected || m_active->rawMode())
        return false;
    // Just after one, the meter drops key frames until its line has been quiet: send it then.
    if (const int wait = stockHoldLeftMs(); wait > 0) {
        QTimer::singleShot(wait, this, [this, key, longPress] { sendKey(key, longPress); });
        return true;
    }
    m_sentKeys[key] = QDateTime::currentDateTimeUtc();
    emit keyPressed({key, longPress}, true);
    m_active->sendKey(key, longPress);
    return true;
}

bool MeterSession::setClock()
{
    if (m_active->state() != ConnectionState::Connected || m_active->rawMode())
        return false;
    if (const int wait = stockHoldLeftMs(); wait > 0) {
        QTimer::singleShot(wait, this, [this] { setClock(); });   // the time is taken when it goes
        return true;
    }
    const QDateTime now = QDateTime::currentDateTime();
    return m_active->sendCommand(setClockCommand(now.date().year(), now.date().month(), now.date().day(),
                                                 now.time().hour(), now.time().minute(), now.time().second()));
}

void MeterSession::plugDemoLead(std::optional<Jack> jack)
{
    if (!isDemo())
        return;
    m_demo->simulator().setCurrentLead(jack);
    emit dialChanged();
}

std::optional<Jack> MeterSession::demoLead() const
{
    return isDemo() ? m_demo->simulator().currentLead() : std::nullopt;
}

void MeterSession::turnDemoDial(int position)
{
    if (!isDemo() || position < 0 || position >= dial::positions().size())
        return;
    m_demo->turnDial(position);
    const bool off = dial::positions()[position].isOff();
    if (off) {
        m_latest.reset();
        emit readingReceived();
    }
    m_modeChangedAt = QDateTime::currentDateTime();
    setDial(position, off ? -1 : 0);
}

void MeterSession::suggest(const DialSuggestion &suggestion)
{
    m_suggestion = suggestion;
    emit suggestionChanged();
}

void MeterSession::clearSuggestion()
{
    if (!m_suggestion)
        return;
    m_suggestion.reset();
    emit suggestionChanged();
}

bool MeterSession::suggestionMet() const
{
    if (!m_suggestion || m_dial != m_suggestion->position)
        return false;
    return m_suggestion->function < 0 || m_suggestion->function == m_function;
}

void MeterSession::leaveDemo()
{
    if (!isDemo())
        return;
    m_demo->disconnectFrom();
    m_active = m_bluetooth;
    setDial(-1, -1);
    emit stateChanged();
}

void MeterSession::setDial(int position, int function)
{
    if (m_dial == position && m_function == function)
        return;
    const bool wasMet = suggestionMet();
    m_dial = position;
    m_function = function;
    emit dialChanged();
    if (wasMet != suggestionMet())
        emit suggestionChanged();
}

void MeterSession::updateDialFromReading()
{
    if (!m_latest)
        return;
    const MeterPacket &p = m_latest->packet;
    MeterMode mode = p.mode();
    if (mode == MeterMode::TemperatureC || mode == MeterMode::TemperatureF)
        mode = MeterMode::Temperature;
    // The demo meter's knob is known; a real one's is worked out from the mode.
    const int position = isDemo() ? m_demo->simulator().dialPosition() : dial::positionFor(mode);
    int function = -1;
    if (position >= 0) {
        const auto &fns = dial::positions()[position].functions;
        const bool acPlusDc = p.coupling() == Coupling::AcPlusDc;
        for (int i = 0; i < fns.size(); ++i)
            if (fns[i].mode == mode && (fns[i].acPlusDc == acPlusDc || mode != MeterMode::AcVolts)) {
                function = i;
                break;
            }
    }
    setDial(position, function);
}

void MeterSession::onPacket(const MeterPacket &packet)
{
    const QDateTime now = QDateTime::currentDateTime();
    MeterReading reading = decoder::decode(packet, now, m_latest ? m_latest->sub.label : QString());
    std::optional<MeterKeyPress> pressed;
    if (m_latest)
        pressed = keyinference::infer(m_latest->packet, packet);

    // After a mode change the meter's display stays blank until it has measured, but its packets
    // say 0 (a real meter, on temperature, for about two seconds).
    if (m_latest && (m_latest->packet.mode() != packet.mode()
                     || (m_latest->packet.coupling() == Coupling::AcPlusDc) != (packet.coupling() == Coupling::AcPlusDc)))
        m_modeChangedAt = now;
    if (m_modeChangedAt) {
        if (!packet.mainOverload() && packet.mainRawValue() == 0 && m_modeChangedAt->msecsTo(now) < BlankAfterModeChangeMs) {
            reading.blank = true;
            reading.mainText.clear();
            reading.mainNegative = false;
            reading.mainUnit.clear();
            reading.value.reset();
            reading.baseValue.reset();
        } else {
            m_modeChangedAt.reset();
        }
    }
    m_latest = reading;
    if (pressed) {
        const auto sent = m_sentKeys.find(pressed->key);
        const bool echo = sent != m_sentKeys.end() && sent->msecsTo(QDateTime::currentDateTimeUtc()) < SentKeyEchoMs;
        if (!echo)
            emit keyPressed(*pressed, false);
    }
    if (packet.subMode() == MeterMode::Battery && !packet.subOverload())
        m_batteryVolts = packet.subRawValue() / std::pow(10.0, packet.subDecimals());
    if (m_logging && !reading.blank) {
        m_log.add(reading);
        emit logChanged();
    }
    updateDialFromReading();
    emit readingReceived();
}

void MeterSession::onDiscovered(const DiscoveredMeter &meter)
{
    m_discovered[meter.id] = meter;
    emit discoveredChanged();
    if (m_autoConnect && meter.id == preferredMeterId && !isDemo() && m_bluetooth->state() == ConnectionState::Scanning)
        QTimer::singleShot(0, this, [this, meter] { connectTo(meter); });
}

void MeterSession::onConnectionLost()
{
    // Out of range or switched off: keep looking, and the remembered meter reconnects by
    // itself when it's seen again.
    if (autoReconnect && !m_userDisconnected && !isDemo())
        QTimer::singleShot(0, this, [this] { startScan(true); });
}

} // namespace readout
