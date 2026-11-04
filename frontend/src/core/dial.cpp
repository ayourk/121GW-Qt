// SPDX-License-Identifier: MIT
#include "dial.h"

namespace readout::dial {

namespace {

DialFunction fn(MeterMode mode, const char *label, bool acPlusDc = false)
{
    return DialFunction{mode, acPlusDc, QString::fromUtf8(label), std::nullopt};
}

DialFunction viaJack(Jack jack, MeterMode mode, const char *label)
{
    return DialFunction{mode, false, QString::fromUtf8(label), jack};
}

DialPosition pos(const char *id, const char *legend, const char *red, double angle, QList<DialFunction> functions)
{
    return DialPosition{QString::fromUtf8(id), QString::fromUtf8(legend), QString::fromUtf8(red), angle,
                        std::move(functions)};
}

} // namespace

const QList<DialPosition> &positions()
{
    // Ten detents over the top half, 20° apart: OFF at 9 o'clock round to OFF at 3 o'clock.
    static const QList<DialPosition> p = {
        pos("off_left", "OFF", "", 180, {}),
        pos("lowz", "Low Z", "", 160, {fn(MeterMode::LowZ, "Low Z AC")}),
        pos("v", "V", "", 140,
            {fn(MeterMode::DcVolts, "DC"), fn(MeterMode::AcVolts, "AC"), fn(MeterMode::AcVolts, "AC+DC", true)}),
        pos("mv", "mV", "Temp", 120,
            {fn(MeterMode::DcMillivolts, "DC"), fn(MeterMode::AcMillivolts, "AC"),
             fn(MeterMode::Temperature, "Temperature")}),
        pos("hz", "Hz", "ms %", 100,
            {fn(MeterMode::Frequency, "Frequency"), fn(MeterMode::PulseWidth, "Pulse width"),
             fn(MeterMode::Duty, "Duty cycle")}),
        pos("ohm", "Ω", "▶| ⊣⊢ ·)))", 80,
            {fn(MeterMode::Resistance, "Resistance"), fn(MeterMode::Continuity, "Continuity"),
             fn(MeterMode::Diode, "Diode"), fn(MeterMode::Capacitance, "Capacitance")}),
        pos("va", "mVA/VA", "", 60,
            {viaJack(Jack::Amps, MeterMode::DcVoltAmps, "DC VA"), viaJack(Jack::Amps, MeterMode::AcVoltAmps, "AC VA"),
             viaJack(Jack::MilliMicro, MeterMode::DcMilliVoltAmps, "DC mVA"),
             viaJack(Jack::MilliMicro, MeterMode::AcMilliVoltAmps, "AC mVA")}),
        pos("ua", "µA", "µVA", 40,
            {fn(MeterMode::DcMicroamps, "DC"), fn(MeterMode::AcMicroamps, "AC"),
             fn(MeterMode::DcMicroVoltAmps, "DC µVA"), fn(MeterMode::AcMicroVoltAmps, "AC µVA")}),
        pos("a", "A mA", "", 20,
            {viaJack(Jack::MilliMicro, MeterMode::DcMilliamps, "DC mA"),
             viaJack(Jack::MilliMicro, MeterMode::AcMilliamps, "AC mA"),
             viaJack(Jack::Amps, MeterMode::DcAmps, "DC A"), viaJack(Jack::Amps, MeterMode::AcAmps, "AC A")}),
        pos("off_right", "OFF", "", 0, {}),
    };
    return p;
}

int indexOf(const QString &id)
{
    const QString key = id.trimmed().toLower();
    if (key == QLatin1String("off"))
        return 0;
    const auto &p = positions();
    for (int i = 0; i < p.size(); ++i)
        if (p[i].id == key)
            return i;
    return -1;
}

QList<int> positionsFor(MeterMode mode)
{
    if (mode == MeterMode::TemperatureC || mode == MeterMode::TemperatureF)
        mode = MeterMode::Temperature;
    QList<int> result;
    const auto &p = positions();
    for (int i = 0; i < p.size(); ++i)
        for (const DialFunction &f : p[i].functions)
            if (f.mode == mode && !result.contains(i))
                result << i;
    return result;
}

int positionFor(MeterMode mode)
{
    const QList<int> candidates = positionsFor(mode);
    return candidates.isEmpty() ? -1 : candidates.first();
}

int modePresses(int position, int from, int to)
{
    const auto &p = positions();
    if (position < 0 || position >= p.size())
        return -1;
    const auto &fns = p[position].functions;
    if (from < 0 || to < 0 || from >= fns.size() || to >= fns.size())
        return -1;
    if (fns[from].currentJack != fns[to].currentJack)
        return -1;
    // MODE steps through the functions that share the lead's jack, in order.
    QList<int> cycle;
    for (int i = 0; i < fns.size(); ++i)
        if (fns[i].currentJack == fns[from].currentJack)
            cycle << i;
    const auto n = cycle.size();
    return static_cast<int>((cycle.indexOf(to) - cycle.indexOf(from) + n) % n);
}

QList<JackUse> jacksForMode(MeterMode mode)
{
    const JackUse black{Jack::Com, QStringLiteral("black"), {}};
    const QString red = QStringLiteral("red"), current = QStringLiteral("red (current)"),
                  voltage = QStringLiteral("red (voltage)");
    switch (mode) {
    case MeterMode::AcAmps: case MeterMode::DcAmps:
        return {{Jack::Amps, red, {}}, black};
    case MeterMode::AcMilliamps: case MeterMode::DcMilliamps:
    case MeterMode::AcMicroamps: case MeterMode::DcMicroamps:
        return {{Jack::MilliMicro, red, {}}, black};
    case MeterMode::AcVoltAmps: case MeterMode::DcVoltAmps:
        return {{Jack::Amps, current, {}}, {Jack::VoltOhm, voltage, {}}, black};
    case MeterMode::AcMilliVoltAmps: case MeterMode::DcMilliVoltAmps:
    case MeterMode::AcMicroVoltAmps: case MeterMode::DcMicroVoltAmps:
        return {{Jack::MilliMicro, current, {}}, {Jack::VoltOhm, voltage, {}}, black};
    default:
        return {{Jack::VoltOhm, red, {}}, black};
    }
}

QList<JackUse> jacksForPosition(int index, int function)
{
    const auto &p = positions();
    if (index < 0 || index >= p.size() || p[index].isOff())
        return {};
    if (function >= 0 && function < p[index].functions.size())
        return jacksForMode(p[index].functions[function].mode);
    const QString id = p[index].id;
    const JackUse black{Jack::Com, QStringLiteral("black"), {}};
    if (id == QLatin1String("a"))
        return {{Jack::Amps, QStringLiteral("red"), QStringLiteral("A: 500 mA, 5 A, 10 A ranges")},
                {Jack::MilliMicro, QStringLiteral("red"), QStringLiteral("mA: up to 50 mA")}, black};
    if (id == QLatin1String("va"))
        return {{Jack::Amps, QStringLiteral("red (current)"), QStringLiteral("VA: current over 50 mA")},
                {Jack::MilliMicro, QStringLiteral("red (current)"), QStringLiteral("mVA: current up to 50 mA")},
                {Jack::VoltOhm, QStringLiteral("red (voltage)"), {}}, black};
    if (id == QLatin1String("ua"))
        return {{Jack::MilliMicro, QStringLiteral("red"), {}},
                {Jack::VoltOhm, QStringLiteral("red (voltage)"), QStringLiteral("µVA only")}, black};
    return jacksForMode(p[index].functions.first().mode);
}

QString jackId(Jack jack)
{
    switch (jack) {
    case Jack::Amps: return QStringLiteral("a");
    case Jack::MilliMicro: return QStringLiteral("ma");
    case Jack::Com: return QStringLiteral("com");
    case Jack::VoltOhm: return QStringLiteral("v");
    }
    return {};
}

QString jackLabel(Jack jack)
{
    switch (jack) {
    case Jack::Amps: return QStringLiteral("A 500mA");
    case Jack::MilliMicro: return QStringLiteral("mA µA");
    case Jack::Com: return QStringLiteral("COM");
    case Jack::VoltOhm: return QStringLiteral("VΩ");
    }
    return {};
}

} // namespace readout::dial
