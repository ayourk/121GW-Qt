// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Connection/MeterSession.cs), Copyright (c) 2026 Alejandro Mora, MIT,
// with the dial added: where the knob is (worked out from the mode) and where an assistant asked
// for it to go.
#pragma once

#include "dial.h"
#include "keyinference.h"
#include "measurementlog.h"
#include "meterconnection.h"
#include "reading.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <optional>

namespace readout {

class BleConnection;
class SimulatedConnection;

// A dial position an assistant (or the user) asked for, and why.
struct DialSuggestion {
    int position = -1;       // index into dial::positions()
    int function = -1;       // index into that position's functions, or -1 for any
    QString reason;          // shown beside the dial ("Measure the 5 V rail")
    QDateTime at;
};

// The app's one meter: picks Bluetooth or the demo meter, decodes packets into readings, logs
// them, and reconnects to the remembered meter when it's seen again or the link drops.
// Everything happens on the main thread.
class MeterSession : public QObject
{
    Q_OBJECT
public:
    MeterSession(MeterConnection *bluetooth, SimulatedConnection *demo, QObject *parent = nullptr);

    MeterConnection *active() const { return m_active; }
    SimulatedConnection *demoConnection() const { return m_demo; }
    bool isDemo() const;
    ConnectionState state() const { return m_active->state(); }
    std::optional<DiscoveredMeter> meter() const { return m_active->meter(); }
    std::optional<int> signalStrength() const { return m_active->signalStrength(); }

    // The meter to connect to by itself when a scan sees it (the last one used).
    QString preferredMeterId;
    // Reconnect by itself when the link drops.
    bool autoReconnect = true;

    MeasurementLog &log() { return m_log; }
    bool isLogging() const { return m_logging; }
    void setLogging(bool on);
    void clearLog();

    const std::optional<MeterReading> &latest() const { return m_latest; }
    // The battery in volts, as last shown on the SETUP battery page; nullopt until then.
    std::optional<double> batteryVolts() const { return m_batteryVolts; }

    // Meters seen in the current scan, strongest signal first.
    QList<DiscoveredMeter> discovered() const;

    // Where the knob is: worked out from the mode (MODE doesn't move it), or the demo meter's
    // own knob; -1 when unknown (no meter, or a mode that isn't on the dial).
    int dialPosition() const { return m_dial; }
    // Which of the position's functions the meter is on; -1 when unknown.
    int dialFunction() const { return m_function; }
    // Turns the demo meter's knob (the real one is turned by hand).
    void turnDemoDial(int position);

    const std::optional<DialSuggestion> &suggestion() const { return m_suggestion; }
    void suggest(const DialSuggestion &suggestion);
    void clearSuggestion();
    // True once the meter is where the suggestion asked (position, and function if one was named).
    bool suggestionMet() const;

    void startScan(bool autoConnect = false);
    void stopScan();
    void connectTo(const DiscoveredMeter &meter);
    void startDemo();
    void disconnectFrom();
    // A file transfer has the link (keys and the clock command are refused).
    bool transferActive() const { return m_active->rawMode(); }
    // After a transfer the meter drops key (F4) and clock (F8) frames until its line has been
    // quiet for a second: for ms from now, sendKey() and setClock() wait instead of sending.
    // The card transfer sets it, and extends it with every byte it sends meanwhile.
    void holdStockCommands(int ms);
    int stockHoldLeftMs() const;
    // Presses a button; false when no meter is connected or a file transfer is running.
    bool sendKey(MeterKey key, bool longPress = false);
    // Sets the meter's clock to this computer's (F8); false when no meter is connected.
    bool setClock();
    // Plugs the demo meter's red current lead into a jack (or unplugs it).
    void plugDemoLead(std::optional<Jack> jack);
    std::optional<Jack> demoLead() const;

signals:
    void stateChanged();
    void discoveredChanged();
    void readingReceived();
    void keyPressed(readout::MeterKeyPress press, bool fromApp);
    void loggingChanged();
    void logChanged();
    void dialChanged();
    void suggestionChanged();
    void preferredMeterChanged(const QString &id);
    void errorOccurred(const QString &message);

private:
    void onPacket(const MeterPacket &packet);
    void onDiscovered(const DiscoveredMeter &meter);
    void onConnectionLost();
    void leaveDemo();
    void setDial(int position, int function);
    void updateDialFromReading();

    MeterConnection *m_bluetooth;
    SimulatedConnection *m_demo;
    MeterConnection *m_active;
    QHash<QString, DiscoveredMeter> m_discovered;
    bool m_userDisconnected = false;
    bool m_autoConnect = false;
    bool m_logging = true;
    MeasurementLog m_log;
    std::optional<MeterReading> m_latest;
    std::optional<double> m_batteryVolts;
    QHash<MeterKey, QDateTime> m_sentKeys;
    std::optional<QDateTime> m_modeChangedAt;
    int m_dial = -1;
    int m_function = -1;
    std::optional<DialSuggestion> m_suggestion;
    QElapsedTimer m_stockHold;
    int m_stockHoldMs = 0;
};

} // namespace readout
