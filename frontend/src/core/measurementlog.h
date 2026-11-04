// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Logging/MeasurementLog.cs, SampleCsv.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#pragma once

#include "reading.h"

#include <QList>
#include <QTextStream>
#include <optional>

namespace readout {

// One logged reading. value is in unit, the SI base unit; nullopt on overload. display is the
// main reading as the LCD showed it ("12.106 mV"); sub is the sub display ("50.00 Hz") or empty.
struct Sample {
    QDateTime time;
    MeterMode mode = MeterMode::DcVolts;
    std::optional<double> value;
    QString unit;
    QString display;
    QString sub;
};

// Summary of the logged values in one unit.
struct LogStatistics {
    int count = 0;
    double min = 0, max = 0, average = 0;
    double peakToPeak() const { return max - min; }
};

// Every reading since the log was last cleared. Used from the main thread only.
class MeasurementLog
{
public:
    const QList<Sample> &samples() const { return m_samples; }
    int count() const { return static_cast<int>(m_samples.size()); }
    void add(const MeterReading &reading);
    void append(const Sample &sample);
    void clear();
    // Min, max and average of the samples that have a value; nullopt when none do.
    static std::optional<LogStatistics> statistics(const QList<Sample> &samples);
    // The CSV format Readout reads and writes: time_utc,elapsed_s,mode,value,unit,display,sub_display.
    static void writeCsv(const QList<Sample> &samples, QTextStream &out);
    static QList<Sample> readCsv(QTextStream &in);
    static const QString CsvHeader;

private:
    QList<Sample> m_samples;
};

// The enum name Readout writes in its CSV "mode" column ("DcVolts", "Resistance"…).
QString modeEnumName(MeterMode mode);
std::optional<MeterMode> modeFromEnumName(const QString &name);

} // namespace readout
