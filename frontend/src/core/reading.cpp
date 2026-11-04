// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Display/MeterReading.cs, ReadingDecoder.cs, ModeInfo.cs, SiFormat.cs),
// Copyright (c) 2026 Alejandro Mora, MIT.
#include "reading.h"

#include <QLocale>
#include <QStringList>
#include <cmath>
#include <vector>

namespace readout {

namespace {

constexpr int LcdDigits = 5;

// What each main mode measures and how its ranges are scaled. integerDigits: per range, how many
// of the 5 LCD digits come before the decimal point. notation: per range, the SI prefix ('n', 'u',
// 'm', 'k', 'M' or ' '). impliedPrefix: a prefix the mode always has, which its range notation
// leaves out (µA, ms). The ranges are the official app's mRangeLookup (PrivatePostRelease branch).
struct ModeInfo {
    const char *name;
    const char *baseUnit;
    char impliedPrefix;
    std::vector<int> integerDigits;
    const char *notation;

    bool hasRange(int index) const { return index >= 0 && index < static_cast<int>(integerDigits.size()); }
    char prefixFor(int range) const { return notation[range] != ' ' ? notation[range] : impliedPrefix; }
};

const std::vector<ModeInfo> &table()
{
    static const std::vector<ModeInfo> t = {
        {"Low-Z voltage", "V", ' ', {4}, " "},                        // 0
        {"DC voltage", "V", ' ', {1, 2, 3, 4}, "    "},               // 1
        {"AC voltage", "V", ' ', {1, 2, 3, 4}, "    "},               // 2
        {"DC millivolts", "V", ' ', {2, 3}, "mm"},                    // 3
        {"AC millivolts", "V", ' ', {2, 3}, "mm"},                    // 4
        {"Temperature", "°C", ' ', {4}, " "},                         // 5
        {"Frequency", "Hz", ' ', {2, 3, 1, 2, 3}, "  kkk"},           // 6
        {"Pulse width", "s", 'm', {1, 2, 3}, "   "},                  // 7
        {"Duty cycle", "%", ' ', {4}, " "},                           // 8
        {"Resistance", "Ω", ' ', {2, 3, 1, 2, 3, 1, 2}, "  kkkMM"},   // 9
        {"Continuity", "Ω", ' ', {3}, " "},                           // 10
        {"Diode", "V", ' ', {1, 2}, "  "},                            // 11
        {"Capacitance", "F", ' ', {3, 4, 2, 3, 4, 5}, "nnuuuu"},      // 12
        {"AC power", "VA", 'u', {3, 4, 4, 5}, "    "},                // 13
        {"AC power", "VA", ' ', {2, 3, 3, 4}, "mm  "},                // 14
        {"AC power", "VA", ' ', {4, 5, 2, 3}, "mm  "},                // 15
        {"AC current", "A", 'u', {2, 3}, "  "},                       // 16
        {"DC current", "A", 'u', {2, 3}, "  "},                       // 17
        {"AC current", "A", ' ', {1, 2}, "mm"},                       // 18
        {"DC current", "A", ' ', {1, 2}, "mm"},                       // 19
        {"AC current", "A", ' ', {3, 1, 2}, "m  "},                   // 20
        {"DC current", "A", ' ', {3, 1, 2}, "m  "},                   // 21
        {"DC power", "VA", 'u', {3, 4, 4, 5}, "    "},                // 22
        {"DC power", "VA", ' ', {2, 3, 3, 4}, "mm  "},                // 23
        {"DC power", "VA", ' ', {4, 5, 2, 3}, "mm  "},                // 24
        {"Temperature", "°C", ' ', {4}, " "},                         // 25
        {"Temperature", "°F", ' ', {4}, " "},                         // 26
    };
    return t;
}

const ModeInfo *infoFor(MeterMode mode)
{
    const auto index = static_cast<std::size_t>(mode);
    return index < table().size() ? &table()[index] : nullptr;
}

QString prefixSymbol(char prefix)
{
    switch (prefix) {
    case 'n': return QStringLiteral("n");
    case 'u': return QStringLiteral("µ");
    case 'm': return QStringLiteral("m");
    case 'k': case 'K': return QStringLiteral("k");
    case 'M': return QStringLiteral("M");
    default: return {};
    }
}

double prefixMultiplier(char prefix)
{
    switch (prefix) {
    case 'n': return 1e-9;
    case 'u': return 1e-6;
    case 'm': return 1e-3;
    case 'k': case 'K': return 1e3;
    case 'M': return 1e6;
    default: return 1;
    }
}

// The mode name, with AC+DC where the meter says so: on V it's a MODE step of its own, sent as
// AC voltage with the AC+DC coupling.
QString displayModeName(MeterMode mode, Coupling coupling)
{
    const QString name = decoder::modeName(mode);
    return coupling == Coupling::AcPlusDc && name.startsWith(QLatin1String("AC "))
        ? QStringLiteral("AC+DC ") + name.mid(3)
        : name;
}

const QString BuzzerPage = QStringLiteral("Buzzer");
const QString IdOrIntervalPage = QStringLiteral("Meter ID or interval");

// SETUP pages whose digits aren't a plain number, drawn the way the meter shows them; nullopt
// for everything else.
std::optional<SubDisplay> setupPageText(const MeterPacket &packet, const QString &previousLabel)
{
    const int raw = packet.subRawValue();
    const int high = raw >> 8, low = raw & 0xFF;
    const auto two = [](int v) { return QStringLiteral("%1").arg(v, 2, 10, QLatin1Char('0')); };
    switch (packet.subMode()) {
    case MeterMode::AutoPowerOffOn:
        return SubDisplay{QStringLiteral("APO.on"), false, {}, QStringLiteral("Auto power-off: on")};
    case MeterMode::AutoPowerOffOff:
        return SubDisplay{QStringLiteral("APO.oF"), false, {}, QStringLiteral("Auto power-off: off")};
    case MeterMode::Year:   // two digits on the wire; the meter shows the full year
        return SubDisplay{QString::number(raw < 100 ? 2000 + raw : raw), false, {}, QStringLiteral("Year")};
    case MeterMode::Date:   // month and day, one byte each
        return SubDisplay{two(high) + QLatin1Char('-') + two(low), false, {}, QStringLiteral("Date")};
    case MeterMode::Time:   // hour and minute, one byte each
        return SubDisplay{two(high) + QLatin1Char('-') + two(low), false, {}, QStringLiteral("Time")};
    case MeterMode::LowZ:
        // The buzzer, meter ID and logging interval pages come with no page number and no value.
        // The buzzer follows auto power-off; ID and interval follow the time.
        return SubDisplay{{}, false, {},
            previousLabel == BuzzerPage || previousLabel.startsWith(QLatin1String("Auto power-off"))
                ? BuzzerPage : IdOrIntervalPage};
    default:
        return std::nullopt;
    }
}

bool isPower(MeterMode m)
{
    return m == MeterMode::AcMicroVoltAmps || m == MeterMode::AcMilliVoltAmps || m == MeterMode::AcVoltAmps
        || m == MeterMode::DcMicroVoltAmps || m == MeterMode::DcMilliVoltAmps || m == MeterMode::DcVoltAmps;
}

bool isMilliOrMicroCurrent(MeterMode m)
{
    return m == MeterMode::AcMilliamps || m == MeterMode::AcMicroamps
        || m == MeterMode::DcMilliamps || m == MeterMode::DcMicroamps;
}

bool isCurrent(MeterMode m)
{
    return isMilliOrMicroCurrent(m) || m == MeterMode::AcAmps || m == MeterMode::DcAmps;
}

// The sub display's "m", which the packet has no bit for. The official app lights it when the
// main display shows power and the sub display shows current in mA or µA, or the main range
// itself is milli or smaller.
bool subMilli(const MeterPacket &packet)
{
    const MeterMode main = packet.mode();
    const MeterMode sub = packet.subMode();
    if (!isPower(main) || !isCurrent(sub))
        return false;
    const ModeInfo *info = infoFor(main);
    if (info && info->hasRange(packet.mainRangeIndex())) {
        const char n = info->notation[packet.mainRangeIndex()];
        if (n == 'm' || n == 'n' || n == 'u')
            return true;
    }
    return isMilliOrMicroCurrent(sub);
}

SubDisplay decodeSub(const MeterPacket &packet, const QString &previousLabel)
{
    if (auto page = setupPageText(packet, previousLabel))
        return *page;
    MeterMode mode = packet.subMode();
    // Burden voltage comes as 150 and, on the µA, mA and A positions, 156.
    if (static_cast<int>(mode) > 150 && static_cast<int>(mode) < 160)
        mode = MeterMode::BurdenVoltage;
    const QString milli = subMilli(packet) ? QStringLiteral("m") : QString();
    QString unit;
    switch (mode) {
    case MeterMode::LowZ: unit = QStringLiteral("V"); break;
    case MeterMode::DcVolts: case MeterMode::AcVolts: unit = milli + QStringLiteral("V"); break;
    case MeterMode::DcMillivolts: case MeterMode::AcMillivolts: unit = QStringLiteral("mV"); break;
    // Plain temperature says neither °C nor °F; the LCD's °F symbol does.
    case MeterMode::Temperature: unit = packet.fahrenheitIcon() ? QStringLiteral("°F") : QStringLiteral("°C"); break;
    case MeterMode::TemperatureC: case MeterMode::SetupTemperatureC: unit = QStringLiteral("°C"); break;
    case MeterMode::TemperatureF: case MeterMode::SetupTemperatureF: unit = QStringLiteral("°F"); break;
    case MeterMode::Frequency: unit = QStringLiteral("Hz"); break;
    case MeterMode::PulseWidth: unit = QStringLiteral("ms"); break;
    case MeterMode::Duty: unit = QStringLiteral("%"); break;
    case MeterMode::Resistance: unit = QStringLiteral("Ω"); break;
    case MeterMode::Diode: unit = QStringLiteral("V"); break;
    case MeterMode::AcVoltAmps: unit = QStringLiteral("VA"); break;
    case MeterMode::AcMicroamps: case MeterMode::DcMicroamps: case MeterMode::AcMilliamps:
    case MeterMode::DcMilliamps: case MeterMode::AcAmps: case MeterMode::DcAmps:
        unit = milli + QStringLiteral("A"); break;
    case MeterMode::DcMicroVoltAmps: case MeterMode::DcMilliVoltAmps: case MeterMode::DcVoltAmps:
        unit = milli + QStringLiteral("VA"); break;
    case MeterMode::Battery: case MeterMode::BurdenVoltage: unit = QStringLiteral("V"); break;
    case MeterMode::Dbm: unit = QStringLiteral("dBm"); break;
    case MeterMode::Interval: unit = QStringLiteral("ms"); break;
    default: break;
    }
    if (packet.subKilo())
        unit = QStringLiteral("k") + unit;
    QString label;
    switch (mode) {
    case MeterMode::SetupTemperatureC: case MeterMode::SetupTemperatureF: label = decoder::MeterTemperaturePage; break;
    case MeterMode::Battery: label = QStringLiteral("Battery"); break;
    case MeterMode::AutoPowerOffOn: label = QStringLiteral("Auto power-off: on"); break;
    case MeterMode::AutoPowerOffOff: label = QStringLiteral("Auto power-off: off"); break;
    case MeterMode::Year: label = QStringLiteral("Year"); break;
    case MeterMode::Date: label = QStringLiteral("Date"); break;
    case MeterMode::Time: label = QStringLiteral("Time"); break;
    case MeterMode::BurdenVoltage: label = QStringLiteral("Burden voltage"); break;
    case MeterMode::LcdContrast: label = QStringLiteral("LCD contrast"); break;
    case MeterMode::Dbm: label = QStringLiteral("dBm"); break;
    case MeterMode::Interval: label = QStringLiteral("Logging interval"); break;
    default: break;
    }
    const QString text = packet.subOverload() ? QStringLiteral("OFL") : decoder::formatSub(packet.subRawValue(), packet.subDecimals());
    return SubDisplay{text, packet.subNegative() && !packet.subOverload(), unit, label};
}

Annunciators decodeIcons(const MeterPacket &packet)
{
    const Coupling coupling = packet.coupling();
    const MinMaxState minMax = packet.minMax();
    Annunciators a;
    a.lowZ = packet.mode() == MeterMode::LowZ;
    // The LCD has one stacked AC/DC pair; the coupling bits and the separate AC and DC icon bits
    // both light it.
    a.ac = coupling == Coupling::Ac || coupling == Coupling::AcPlusDc
        || packet.mode() == MeterMode::AcMillivolts || packet.acIcon();
    a.dc = coupling == Coupling::Dc || coupling == Coupling::AcPlusDc || packet.dcIcon();
    a.acPlusDc = coupling == Coupling::AcPlusDc;
    a.autoRange = packet.autoRange();
    a.autoPowerOff = packet.autoPowerOff();
    a.lowBattery = packet.lowBattery();
    a.bluetooth = packet.bluetooth();
    a.relative = packet.relative();
    a.decibels = packet.decibels();
    a.max = minMax == MinMaxState::Max || minMax == MinMaxState::All;
    a.min = minMax == MinMaxState::Min || minMax == MinMaxState::All;
    a.average = minMax == MinMaxState::Average || minMax == MinMaxState::All;
    a.test = packet.test();
    a.memory = packet.memory() > 0;
    a.hold = packet.hold() == HoldState::Hold || packet.hold() == HoldState::AutoHold;
    a.autoHold = packet.hold() == HoldState::AutoHold;
    a.beep = packet.mode() == MeterMode::Continuity;
    a.diode = packet.mode() == MeterMode::Diode;
    a.lowPassFilter = packet.lowPassFilter();
    a.oneMsPeak = packet.oneMsPeak();
    a.arrow = packet.arrow();
    return a;
}

double roundSignificant(double value, int significant)
{
    if (value == 0)
        return 0;
    const int digits = significant - 1 - static_cast<int>(std::floor(std::log10(std::fabs(value))));
    if (digits < 0 || digits > 15)
        return value;
    const double scale = std::pow(10.0, digits);
    return std::round(value * scale) / scale;   // std::round rounds half away from zero, as .NET's AwayFromZero
}

} // namespace

QString SubDisplay::line() const
{
    return ((negative ? QStringLiteral("-") : QString()) + text + QLatin1Char(' ') + unit).trimmed();
}

int BarGraph::fullScale() const
{
    switch (scale) {
    case BarScale::Five: return 5;
    case BarScale::Fifty: return 50;
    case BarScale::FiveHundred: return 500;
    case BarScale::OneThousand: return 1000;
    }
    return 1000;
}

QStringList Annunciators::names() const
{
    QStringList n;
    const auto add = [&n](bool on, const char *name) { if (on) n << QLatin1String(name); };
    add(lowZ, "LoZ");
    add(acPlusDc, "AC+DC");
    add(ac && !acPlusDc, "AC");
    add(dc && !acPlusDc, "DC");
    add(autoRange, "AUTO");
    add(autoHold, "A-HOLD");
    add(hold && !autoHold, "HOLD");
    add(relative, "REL");
    add(max, "MAX");
    add(min, "MIN");
    add(average, "AVG");
    add(decibels, "dBm");
    add(lowPassFilter, "1kHz LPF");
    add(oneMsPeak, "1ms PEAK");
    add(memory, "MEM");
    add(test, "TEST");
    add(beep, "continuity beep");
    add(diode, "diode");
    add(bluetooth, "Bluetooth");
    add(autoPowerOff, "APO");
    add(lowBattery, "LOW BATTERY");
    add(arrow, "arrow");
    return n;
}

QString MeterReading::mainLine() const
{
    return ((mainNegative && !mainOverload ? QStringLiteral("-") : QString())
            + mainText.trimmed() + QLatin1Char(' ') + mainUnit).trimmed();
}

namespace decoder {

QString modeName(MeterMode mode)
{
    if (const ModeInfo *info = infoFor(mode))
        return QString::fromUtf8(info->name);
    return QStringLiteral("Mode %1").arg(static_cast<int>(mode));
}

MeterReading decode(const MeterPacket &packet, const QDateTime &receivedAt, const QString &previousSubLabel)
{
    const MeterMode mode = packet.mode();
    const ModeInfo *info = infoFor(mode);
    const int range = packet.mainRangeIndex();
    const bool known = info && info->hasRange(range);

    MeterReading r;
    r.packet = packet;
    r.receivedAt = receivedAt;
    r.mode = mode;
    r.modeName = displayModeName(mode, packet.coupling());
    r.mainNegative = packet.mainNegative();
    r.mainOverload = packet.mainOverload();
    r.baseUnit = info ? QString::fromUtf8(info->baseUnit) : QString();

    if (known) {
        const int integerDigits = info->integerDigits[range];
        const char prefix = info->prefixFor(range);
        r.mainUnit = prefixSymbol(prefix) + QString::fromUtf8(info->baseUnit);
        if (packet.mainOverload()) {
            r.mainText = QStringLiteral("OFL");
        } else {
            r.mainText = formatMain(packet.mainRawValue(), integerDigits);
            const double magnitude = packet.mainRawValue() / std::pow(10.0, LcdDigits - integerDigits);
            r.value = packet.mainNegative() ? -magnitude : magnitude;
            r.baseValue = *r.value * prefixMultiplier(prefix);
        }
    } else {
        // A mode or range this table doesn't know: show the digits, but don't guess the decimal
        // point, and don't log a number that may be off by a power of ten.
        r.mainText = packet.mainOverload() ? QStringLiteral("OFL") : QString::number(packet.mainRawValue());
    }
    r.sub = decodeSub(packet, previousSubLabel);
    r.bar = BarGraph{packet.barOn(),
                     packet.barOn() ? std::min(BarGraph::TotalSegments, packet.barValue() + 1) : 0,
                     packet.barNegative(), packet.barScale(), packet.barTicks150()};
    r.icons = decodeIcons(packet);
    return r;
}

int integerDigits(MeterMode mode, int range)
{
    const ModeInfo *info = infoFor(mode);
    return info && info->hasRange(range) ? info->integerDigits[range] : -1;
}

QString formatMain(int raw, int integerDigits)
{
    QString digits = QString::number(raw).rightJustified(LcdDigits, QLatin1Char(' '));
    if (integerDigits >= LcdDigits)
        return digits;
    // Counted from the right, so a value past 99999 keeps the same number of decimals.
    const int point = digits.size() - (LcdDigits - integerDigits);
    QString whole = digits.left(point);
    const QString fraction = digits.mid(point).replace(QLatin1Char(' '), QLatin1Char('0'));
    if (whole.isEmpty() || whole.back() == QLatin1Char(' '))
        whole = whole.left(whole.size() - std::min<qsizetype>(1, whole.size())) + QLatin1Char('0');
    return whole + QLatin1Char('.') + fraction;
}

QString formatSub(int raw, int decimals)
{
    QString digits = QString::number(raw);
    if (decimals <= 0)
        return digits;
    digits = digits.rightJustified(decimals + 1, QLatin1Char('0'));
    return digits.insert(digits.size() - decimals, QLatin1Char('.'));
}

} // namespace decoder

namespace siformat {

QString format(double value, const QString &unit, int significant)
{
    const QLocale c = QLocale::c();
    if (std::isnan(value) || std::isinf(value))
        return (QStringLiteral("– ") + unit).trimmed();
    static const QStringList plain = {QStringLiteral("%"), QStringLiteral("°C"), QStringLiteral("°F"),
                                      QStringLiteral("dBm"), QString()};
    if (plain.contains(unit) || value == 0)
        return (c.toString(roundSignificant(value, significant), 'g', significant) + QLatin1Char(' ') + unit).trimmed();
    static const struct { double factor; const char *symbol; } prefixes[] = {
        {1e6, "M"}, {1e3, "k"}, {1, ""}, {1e-3, "m"}, {1e-6, "µ"}, {1e-9, "n"}, {1e-12, "p"},
    };
    const double magnitude = std::fabs(value);
    double factor = 1e-12;
    const char *symbol = "p";
    for (const auto &p : prefixes) {
        if (magnitude >= p.factor * 0.9995) {
            factor = p.factor;
            symbol = p.symbol;
            break;
        }
    }
    const double scaled = roundSignificant(value / factor, significant);
    // "0.####": at most four decimals, no trailing zeros.
    QString number = c.toString(scaled, 'f', 4);
    if (number.contains(QLatin1Char('.'))) {
        while (number.endsWith(QLatin1Char('0')))
            number.chop(1);
        if (number.endsWith(QLatin1Char('.')))
            number.chop(1);
    }
    if (number == QLatin1String("-0"))
        number = QStringLiteral("0");
    return number + QLatin1Char(' ') + QString::fromUtf8(symbol) + unit;
}

} // namespace siformat

} // namespace readout
