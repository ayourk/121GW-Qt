// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Logging/MeasurementLog.cs, SampleCsv.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "measurementlog.h"

#include <QHash>
#include <QTimeZone>
#include <algorithm>
#include <limits>

namespace readout {

const QString MeasurementLog::CsvHeader = QStringLiteral("time_utc,elapsed_s,mode,value,unit,display,sub_display");

namespace {

const QString TimeFormat = QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzzZ");

QString quote(const QString &field)
{
    for (QChar c : field) {
        if (c == u',' || c == u'"' || c == u'\r' || c == u'\n') {
            QString q = field;
            q.replace(u'"', QStringLiteral("\"\""));
            return u'"' + q + u'"';
        }
    }
    return field;
}

QStringList split(const QString &line)
{
    QStringList fields;
    QString field;
    bool quoted = false;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (quoted) {
            if (c == u'"' && i + 1 < line.size() && line[i + 1] == u'"') {
                field += u'"';
                ++i;
            } else if (c == u'"') {
                quoted = false;
            } else {
                field += c;
            }
        } else if (c == u'"') {
            quoted = true;
        } else if (c == u',') {
            fields << field;
            field.clear();
        } else {
            field += c;
        }
    }
    fields << field;
    return fields;
}

// The names C#'s MeterMode.ToString() gives, which Readout's CSV files use.
const QHash<MeterMode, QString> &enumNames()
{
    static const QHash<MeterMode, QString> names = {
        {MeterMode::LowZ, "LowZ"}, {MeterMode::DcVolts, "DcVolts"}, {MeterMode::AcVolts, "AcVolts"},
        {MeterMode::DcMillivolts, "DcMillivolts"}, {MeterMode::AcMillivolts, "AcMillivolts"},
        {MeterMode::Temperature, "Temperature"}, {MeterMode::Frequency, "Frequency"},
        {MeterMode::PulseWidth, "PulseWidth"}, {MeterMode::Duty, "Duty"}, {MeterMode::Resistance, "Resistance"},
        {MeterMode::Continuity, "Continuity"}, {MeterMode::Diode, "Diode"}, {MeterMode::Capacitance, "Capacitance"},
        {MeterMode::AcMicroVoltAmps, "AcMicroVoltAmps"}, {MeterMode::AcMilliVoltAmps, "AcMilliVoltAmps"},
        {MeterMode::AcVoltAmps, "AcVoltAmps"}, {MeterMode::AcMicroamps, "AcMicroamps"},
        {MeterMode::DcMicroamps, "DcMicroamps"}, {MeterMode::AcMilliamps, "AcMilliamps"},
        {MeterMode::DcMilliamps, "DcMilliamps"}, {MeterMode::AcAmps, "AcAmps"}, {MeterMode::DcAmps, "DcAmps"},
        {MeterMode::DcMicroVoltAmps, "DcMicroVoltAmps"}, {MeterMode::DcMilliVoltAmps, "DcMilliVoltAmps"},
        {MeterMode::DcVoltAmps, "DcVoltAmps"}, {MeterMode::TemperatureC, "TemperatureC"},
        {MeterMode::TemperatureF, "TemperatureF"},
    };
    return names;
}

} // namespace

QString modeEnumName(MeterMode mode)
{
    const auto &names = enumNames();
    const auto it = names.find(mode);
    return it != names.end() ? *it : QString::number(static_cast<int>(mode));
}

std::optional<MeterMode> modeFromEnumName(const QString &name)
{
    const auto &names = enumNames();
    for (auto it = names.begin(); it != names.end(); ++it)
        if (it.value() == name)
            return it.key();
    // C#'s Enum.TryParse also takes the number.
    bool ok = false;
    const int n = name.toInt(&ok);
    if (ok && n >= 0 && n <= 255)
        return static_cast<MeterMode>(n);
    return std::nullopt;
}

void MeasurementLog::add(const MeterReading &reading)
{
    Sample s;
    s.time = reading.receivedAt;
    s.mode = reading.mode;
    s.value = reading.baseValue;
    s.unit = reading.baseUnit;
    s.display = reading.mainLine();
    s.sub = reading.sub.label.isEmpty() ? reading.sub.line() : QString();
    append(s);
}

void MeasurementLog::append(const Sample &sample)
{
    m_samples.append(sample);
}

void MeasurementLog::clear()
{
    m_samples.clear();
}

std::optional<LogStatistics> MeasurementLog::statistics(const QList<Sample> &samples)
{
    LogStatistics st;
    st.min = std::numeric_limits<double>::max();
    st.max = std::numeric_limits<double>::lowest();
    double sum = 0;
    for (const Sample &s : samples) {
        if (!s.value)
            continue;
        ++st.count;
        st.min = std::min(st.min, *s.value);
        st.max = std::max(st.max, *s.value);
        sum += *s.value;
    }
    if (st.count == 0)
        return std::nullopt;
    st.average = sum / st.count;
    return st;
}

void MeasurementLog::writeCsv(const QList<Sample> &samples, QTextStream &out)
{
    out << CsvHeader << '\n';
    if (samples.isEmpty())
        return;
    const QDateTime start = samples.first().time;
    for (const Sample &s : samples) {
        out << s.time.toUTC().toString(TimeFormat) << ','
            << QString::number(start.msecsTo(s.time) / 1000.0, 'f', 3) << ','
            << quote(modeEnumName(s.mode)) << ','
            << (s.value ? QString::number(*s.value, 'g', 10) : QString()) << ','
            << quote(s.unit) << ',' << quote(s.display) << ',' << quote(s.sub) << '\n';
    }
}

QList<Sample> MeasurementLog::readCsv(QTextStream &in)
{
    QList<Sample> samples;
    QString line;
    while (in.readLineInto(&line)) {
        if (line.endsWith(u'\r'))
            line.chop(1);
        if (line.isEmpty() || line.startsWith(QStringLiteral("time_utc,")))
            continue;
        const QStringList f = split(line);
        if (f.size() < 7)
            continue;
        QDateTime time = QDateTime::fromString(f[0], TimeFormat);
        const auto mode = modeFromEnumName(f[2]);
        if (!time.isValid() || !mode)
            continue;
        time.setTimeSpec(Qt::UTC);
        Sample s;
        s.time = time;
        s.mode = *mode;
        bool ok = false;
        const double v = f[3].toDouble(&ok);
        if (ok)
            s.value = v;
        s.unit = f[4];
        s.display = f[5];
        s.sub = f[6];
        samples << s;
    }
    return samples;
}

} // namespace readout
