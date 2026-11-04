// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Simulation/SimulatedMeter.cs), Copyright (c) 2026 Alejandro Mora, MIT,
// and extended with a dial: the demo meter's knob is turned from the app, and MODE steps through the
// functions of the position it's on, as on the real meter.
#pragma once

#include "dial.h"
#include "metermode.h"

#include <QByteArray>
#include <QRandomGenerator>
#include <optional>

namespace readout {

// A pretend 121GW: plausible packets for every dial position, reacting to MODE, RANGE, HOLD,
// REL, MIN/MAX and SETUP. The app's demo mode and the tests use it.
class SimulatedMeter
{
public:
    explicit SimulatedMeter(quint32 seed = 121);

    int serial = 12100;

    // The knob: an index into dial::positions(). Starts on V.
    int dialPosition() const { return m_dial; }
    void turnDial(int position);
    bool isOff() const;

    MeterMode currentMode() const;

    // The red lead's current jack (A·500mA or mA·µA), or none: on the A·mA and mVA/VA positions
    // it picks the function, as the meter's jack sensing does.
    std::optional<Jack> currentLead() const { return m_lead; }
    void setCurrentLead(std::optional<Jack> jack);

    // The packet the meter would send seconds after it was switched on; empty when it's off.
    QByteArray packetAt(double seconds);

    // Reacts to a button the way the real meter roughly does.
    void press(MeterKey key, bool longPress = false);

private:
    struct Sim {
        MeterMode mode;
        bool acPlusDc;
        int range;
        Coupling coupling;
        double (*signal)(double t);   // in the range's display unit
    };
    Sim current() const;
    // Moves m_function into the cycle the lead's jack allows.
    void followLead();

    QRandomGenerator m_random;
    int m_dial = 2;
    int m_function = 0;
    std::optional<Jack> m_lead;
    bool m_autoRange = true;
    bool m_relative = false;
    double m_relativeOffset = 0;
    HoldState m_hold = HoldState::Off;
    bool m_held = false;
    double m_heldValue = 0;
    MinMaxState m_minMax = MinMaxState::Off;
    double m_lastLive = 0;
    int m_setupPage = 0;
};

} // namespace readout
