// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Simulation/SimulatedMeter.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "simulatedmeter.h"

#include "dial.h"
#include "packetbuilder.h"
#include "reading.h"

#include <QDate>
#include <QList>
#include <QTime>
#include <algorithm>
#include <cmath>

namespace readout {

namespace {

// What SETUP steps through after the mode's own sub reading (page 0), in a real meter's order.
// The buzzer, ID and interval pages come with sub mode 0 and no value, as the real meter sends them.
const QList<MeterMode> SetupPages = {
    MeterMode::SetupTemperatureC, MeterMode::Battery, MeterMode::AutoPowerOffOn, MeterMode::LowZ,
    MeterMode::LcdContrast, MeterMode::Year, MeterMode::Date, MeterMode::Time, MeterMode::LowZ, MeterMode::LowZ,
};

// A US bench: 120 V 60 Hz mains, a 12 V supply, a 4.7 kΩ resistor, a small load.
double mains(double t) { return 120.0 + 0.8 * std::sin(t / 6.0); }
double supply(double t) { return 12.0 + 0.35 * std::sin(t / 4.0) + 0.02 * std::sin(t * 7.3); }
double millivolts(double t) { return 12.106 + 0.4 * std::sin(t / 3.0); }
double acMillivolts(double t) { return 35.2 + 0.3 * std::sin(t / 5.0); }
double fahrenheit(double t) { return 74.1 + 0.5 * std::sin(t / 20.0); }
double hertz(double t) { return 60.0 + 0.01 * std::sin(t / 3.0); }
double pulseMs(double t) { return 8.333 + 0.002 * std::sin(t / 3.0); }
double duty(double t) { return 50.0 + 0.2 * std::sin(t / 4.0); }
double kilohms(double t) { return 4.7 + 0.004 * std::sin(t / 9.0); }
double shorted(double t) { return 0.3 + 0.05 * std::sin(t); }
double diode(double t) { return 0.6123 + 0.001 * std::sin(t / 5.0); }
double microfarads(double t) { return 22.04 + 0.01 * std::sin(t / 7.0); }
double watts(double t) { return 6.0 + 0.2 * std::sin(t / 2.5); }
double milliwatts(double t) { return 15.0 + 0.5 * std::sin(t / 2.5); }
double microwatts(double t) { return 250.0 + 5 * std::sin(t / 2.5); }
double microamps(double t) { return 123.45 + 2 * std::sin(t / 2.5); }
double milliamps(double t) { return 1.25 + 0.4 * std::sin(t / 2.5); }
double amps(double t) { return 1.2345 + 0.05 * std::sin(t / 2.5); }

BarScale barScaleFor(int integerDigits)
{
    switch (integerDigits) {
    case 1: return BarScale::Five;
    case 2: return BarScale::Fifty;
    case 3: return BarScale::FiveHundred;
    default: return BarScale::OneThousand;
    }
}

} // namespace

SimulatedMeter::SimulatedMeter(quint32 seed) : m_random(seed) {}

void SimulatedMeter::turnDial(int position)
{
    if (position < 0 || position >= dial::positions().size() || position == m_dial)
        return;
    m_dial = position;
    m_function = 0;
    m_setupPage = 0;
    m_relative = false;
    m_hold = HoldState::Off;
    m_held = false;
    m_minMax = MinMaxState::Off;
    m_autoRange = true;
    followLead();
}

void SimulatedMeter::setCurrentLead(std::optional<Jack> jack)
{
    m_lead = jack;
    followLead();
}

void SimulatedMeter::followLead()
{
    if (isOff())
        return;
    const auto &fns = dial::positions()[m_dial].functions;
    // Only the mA·µA jack occupied: the mA functions; otherwise the A ones.
    const Jack sensed = m_lead == Jack::MilliMicro ? Jack::MilliMicro : Jack::Amps;
    if (!fns[m_function].currentJack || fns[m_function].currentJack == sensed)
        return;
    const bool ac = fns[m_function].label.startsWith(QLatin1String("AC"));
    for (int i = 0; i < fns.size(); ++i)
        if (fns[i].currentJack == sensed && fns[i].label.startsWith(QLatin1String("AC")) == ac)
            m_function = i;
}

bool SimulatedMeter::isOff() const
{
    return dial::positions()[m_dial].isOff();
}

MeterMode SimulatedMeter::currentMode() const
{
    return isOff() ? MeterMode::LowZ : current().mode;
}

SimulatedMeter::Sim SimulatedMeter::current() const
{
    const DialFunction &f = dial::positions()[m_dial].functions[m_function];
    switch (f.mode) {
    case MeterMode::LowZ: return {f.mode, false, 0, Coupling::Ac, mains};
    case MeterMode::DcVolts: return {f.mode, false, 1, Coupling::Dc, supply};
    case MeterMode::AcVolts:
        return {f.mode, f.acPlusDc, 2, f.acPlusDc ? Coupling::AcPlusDc : Coupling::Ac, mains};
    case MeterMode::DcMillivolts: return {f.mode, false, 0, Coupling::Dc, millivolts};
    case MeterMode::AcMillivolts: return {f.mode, false, 0, Coupling::Ac, acMillivolts};
    case MeterMode::Temperature: return {MeterMode::TemperatureF, false, 0, Coupling::None, fahrenheit};
    case MeterMode::Frequency: return {f.mode, false, 0, Coupling::None, hertz};
    case MeterMode::PulseWidth: return {f.mode, false, 0, Coupling::None, pulseMs};
    case MeterMode::Duty: return {f.mode, false, 0, Coupling::None, duty};
    case MeterMode::Resistance: return {f.mode, false, 2, Coupling::None, kilohms};
    case MeterMode::Continuity: return {f.mode, false, 0, Coupling::None, shorted};
    case MeterMode::Diode: return {f.mode, false, 0, Coupling::None, diode};
    case MeterMode::Capacitance: return {f.mode, false, 2, Coupling::None, microfarads};
    case MeterMode::DcVoltAmps: return {f.mode, false, 2, Coupling::Dc, watts};
    case MeterMode::AcVoltAmps: return {f.mode, false, 2, Coupling::Ac, watts};
    case MeterMode::DcMilliVoltAmps: return {f.mode, false, 0, Coupling::Dc, milliwatts};
    case MeterMode::AcMilliVoltAmps: return {f.mode, false, 0, Coupling::Ac, milliwatts};
    case MeterMode::DcMicroVoltAmps: return {f.mode, false, 0, Coupling::Dc, microwatts};
    case MeterMode::AcMicroVoltAmps: return {f.mode, false, 0, Coupling::Ac, microwatts};
    case MeterMode::DcMicroamps: return {f.mode, false, 1, Coupling::Dc, microamps};
    case MeterMode::AcMicroamps: return {f.mode, false, 1, Coupling::Ac, microamps};
    case MeterMode::DcMilliamps: return {f.mode, false, 0, Coupling::Dc, milliamps};
    case MeterMode::AcMilliamps: return {f.mode, false, 0, Coupling::Ac, milliamps};
    case MeterMode::DcAmps: return {f.mode, false, 1, Coupling::Dc, amps};
    case MeterMode::AcAmps: return {f.mode, false, 1, Coupling::Ac, amps};
    default: return {MeterMode::DcVolts, false, 1, Coupling::Dc, supply};
    }
}

QByteArray SimulatedMeter::packetAt(double t)
{
    if (isOff())
        return {};
    const Sim sim = current();
    const int digits = decoder::integerDigits(sim.mode, sim.range);
    const double fullScale = 5 * std::pow(10.0, digits - 1);
    const double live = sim.signal(t) + (m_random.generateDouble() - 0.5) * fullScale * 1e-4;
    double value = m_held ? m_heldValue : live;
    if (m_relative)
        value -= m_relativeOffset;

    const double scale = std::pow(10.0, 5 - digits);
    PacketBuilder b;
    b.serial = serial;
    const bool temperature = sim.mode == MeterMode::TemperatureC || sim.mode == MeterMode::TemperatureF;
    b.mode = temperature ? MeterMode::Temperature : sim.mode;
    b.mainCelsius = sim.mode == MeterMode::TemperatureC;
    b.mainFahrenheit = sim.mode == MeterMode::TemperatureF;
    b.mainRangeIndex = sim.range;
    b.mainRawValue = static_cast<int>(std::lround(std::abs(value) * scale));
    b.mainNegative = value < 0;
    b.barOn = !temperature;
    b.barScale = barScaleFor(digits);
    b.barNegative = value < 0;
    b.barValue = static_cast<int>(std::clamp(std::lround(std::abs(value) / fullScale * 25), 0L, 25L));
    b.coupling = sim.coupling;
    b.acIcon = sim.coupling == Coupling::Ac || sim.coupling == Coupling::AcPlusDc;
    b.dcIcon = sim.coupling == Coupling::Dc || sim.coupling == Coupling::AcPlusDc;
    b.autoRange = m_autoRange;
    b.relative = m_relative;
    b.hold = m_hold;
    b.minMax = m_minMax;

    // Page 0 is the mode's own reading; on AC volts that's the mains frequency, elsewhere the
    // meter's own temperature, where a real meter's small display rests (so SETUP's temperature
    // page is skipped there, as on the meter).
    const bool acVolts = sim.mode == MeterMode::AcVolts || sim.mode == MeterMode::LowZ;
    const QList<MeterMode> pages = acVolts ? SetupPages : SetupPages.mid(1);
    if (m_setupPage > 0) {
        const MeterMode page = pages[(m_setupPage - 1) % pages.size()];
        b.subMode = page;
        switch (page) {
        case MeterMode::SetupTemperatureC: b.subDecimals = 1; b.subRawValue = 235; break;
        case MeterMode::Battery: b.subDecimals = 1; b.subRawValue = 62; break;
        case MeterMode::AutoPowerOffOn: b.subDecimals = 2; b.subRawValue = 0; break;
        case MeterMode::LcdContrast: b.subRawValue = 4; break;
        case MeterMode::Year: b.subRawValue = QDate::currentDate().year() % 100; break;
        case MeterMode::Date: {
            const QDate d = QDate::currentDate();
            b.subRawValue = d.month() << 8 | d.day();
            break;
        }
        case MeterMode::Time: {
            const QTime now = QTime::currentTime();
            b.subRawValue = now.hour() << 8 | now.minute();
            break;
        }
        default: break;
        }
    } else if (acVolts) {
        b.subMode = MeterMode::Frequency;
        b.subDecimals = 2;
        b.subRawValue = static_cast<int>(std::lround(hertz(t) * 100));
        b.subHertz = true;
    } else {
        b.subMode = MeterMode::SetupTemperatureC;
        b.subDecimals = 1;
        b.subRawValue = static_cast<int>(std::lround((23.5 + 0.1 * std::sin(t / 30.0)) * 10));
    }

    m_lastLive = live;
    return b.build();
}

void SimulatedMeter::press(MeterKey key, bool longPress)
{
    if (isOff())
        return;
    switch (key) {
    case MeterKey::Setup:
        if (!longPress) {
            const bool acVolts = current().mode == MeterMode::AcVolts || current().mode == MeterMode::LowZ;
            const int count = static_cast<int>(acVolts ? SetupPages.size() : SetupPages.size() - 1) + 1;
            m_setupPage = (m_setupPage + 1) % count;
        }
        break;
    case MeterKey::Mode: {
        if (longPress)
            break;   // the backlight
        // MODE steps through the functions the lead's jack allows.
        const auto &fns = dial::positions()[m_dial].functions;
        const int n = static_cast<int>(fns.size());
        for (int step = 1; step <= n; ++step) {
            const int next = (m_function + step) % n;
            if (fns[next].currentJack == fns[m_function].currentJack) {
                m_function = next;
                break;
            }
        }
        m_setupPage = 0;
        m_relative = false;
        m_hold = HoldState::Off;
        m_held = false;
        m_minMax = MinMaxState::Off;
        break;
    }
    case MeterKey::Range:
        m_autoRange = longPress || !m_autoRange;
        break;
    case MeterKey::Hold:
        // As the manual has it: HOLD, then A-HOLD, then live again.
        m_hold = m_hold == HoldState::Off ? HoldState::Hold
               : m_hold == HoldState::Hold ? HoldState::AutoHold : HoldState::Off;
        m_held = m_hold != HoldState::Off;
        m_heldValue = m_lastLive;
        break;
    case MeterKey::Rel:
        m_relative = !m_relative;
        m_relativeOffset = m_lastLive;
        break;
    case MeterKey::MinMax:
        // MAX, MIN, AVG, then all three; holding the button leaves.
        if (longPress)
            m_minMax = MinMaxState::Off;
        else
            m_minMax = m_minMax == MinMaxState::Off ? MinMaxState::Max
                     : m_minMax == MinMaxState::Max ? MinMaxState::Min
                     : m_minMax == MinMaxState::Min ? MinMaxState::Average
                     : m_minMax == MinMaxState::Average ? MinMaxState::All : MinMaxState::Max;
        break;
    default:
        break;
    }
}

} // namespace readout
