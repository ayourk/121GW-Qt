// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Logging/SessionStore.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "sessionstore.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <algorithm>
#include <stdexcept>

namespace readout {

namespace sessionsummary {

MeterMode mainMode(const QList<Sample> &samples)
{
    if (samples.isEmpty())
        return MeterMode::DcVolts;
    QHash<MeterMode, std::pair<int, int>> counts;   // with a value, all
    QList<MeterMode> order;
    for (const Sample &s : samples) {
        if (!counts.contains(s.mode))
            order << s.mode;
        auto &c = counts[s.mode];
        c.first += s.value ? 1 : 0;
        c.second += 1;
    }
    MeterMode best = order.first();
    for (MeterMode m : order)
        if (counts[m] > counts[best])
            best = m;
    return best;
}

QString suggestedName(const QList<Sample> &samples)
{
    if (samples.isEmpty())
        return QStringLiteral("Recording");
    return QStringLiteral("%1, %2").arg(decoder::modeName(mainMode(samples)),
                                        QLocale(QLocale::English).toString(samples.first().time.toLocalTime(), "MMM d HH:mm"));
}

SessionInfo describe(const QUuid &id, const QString &name, const QString &notes, const QList<Sample> &samples)
{
    if (samples.isEmpty())
        throw std::invalid_argument("A session needs at least one sample.");
    SessionInfo info;
    info.id = id;
    info.name = name;
    info.notes = notes;
    info.startedAt = samples.first().time;
    info.endedAt = samples.last().time;
    info.sampleCount = static_cast<int>(samples.size());
    info.mainMode = mainMode(samples);
    QList<Sample> inMode;
    for (const Sample &s : samples)
        if (s.mode == info.mainMode)
            inMode << s;
    info.unit = inMode.isEmpty() ? QString() : inMode.first().unit;
    info.statistics = MeasurementLog::statistics(inMode);
    return info;
}

} // namespace sessionsummary

namespace {

QString idText(const QUuid &id)
{
    return id.toString(QUuid::Id128);   // 32 hex digits, like C#'s "N"
}

// System.Text.Json's names: PascalCase, enums as strings, dates in ISO 8601 with an offset.
QJsonObject toJson(const SessionInfo &i)
{
    QJsonObject o;
    o["Id"] = i.id.toString(QUuid::WithoutBraces);
    o["Name"] = i.name;
    o["Notes"] = i.notes;
    o["StartedAt"] = i.startedAt.toOffsetFromUtc(i.startedAt.offsetFromUtc()).toString(Qt::ISODateWithMs);
    o["EndedAt"] = i.endedAt.toOffsetFromUtc(i.endedAt.offsetFromUtc()).toString(Qt::ISODateWithMs);
    o["SampleCount"] = i.sampleCount;
    o["MainMode"] = modeEnumName(i.mainMode);
    o["Unit"] = i.unit;
    if (i.statistics) {
        QJsonObject s;
        s["Count"] = i.statistics->count;
        s["Min"] = i.statistics->min;
        s["Max"] = i.statistics->max;
        s["Average"] = i.statistics->average;
        o["Statistics"] = s;
    } else {
        o["Statistics"] = QJsonValue::Null;
    }
    return o;
}

std::optional<SessionInfo> fromJson(const QJsonObject &o)
{
    SessionInfo i;
    i.id = QUuid::fromString(o["Id"].toString());
    if (i.id.isNull())
        return std::nullopt;
    i.name = o["Name"].toString();
    i.notes = o["Notes"].toString();
    i.startedAt = QDateTime::fromString(o["StartedAt"].toString(), Qt::ISODateWithMs);
    i.endedAt = QDateTime::fromString(o["EndedAt"].toString(), Qt::ISODateWithMs);
    i.sampleCount = o["SampleCount"].toInt();
    i.mainMode = modeFromEnumName(o["MainMode"].toVariant().toString()).value_or(MeterMode::DcVolts);
    i.unit = o["Unit"].toString();
    if (o["Statistics"].isObject()) {
        const QJsonObject s = o["Statistics"].toObject();
        i.statistics = LogStatistics{s["Count"].toInt(), s["Min"].toDouble(), s["Max"].toDouble(), s["Average"].toDouble()};
    }
    return i;
}

} // namespace

QString SessionStore::csvPath(const QUuid &id) const
{
    return QDir(m_dir).filePath(idText(id) + ".csv");
}

QString SessionStore::infoPath(const QUuid &id) const
{
    return QDir(m_dir).filePath(idText(id) + ".json");
}

SessionInfo SessionStore::save(const QString &name, const QList<Sample> &samples)
{
    QDir().mkpath(m_dir);
    const SessionInfo info = sessionsummary::describe(QUuid::createUuid(), name, QString(), samples);
    QSaveFile csv(csvPath(info.id));
    if (!csv.open(QIODevice::WriteOnly))
        throw std::runtime_error(csv.errorString().toStdString());
    {
        QTextStream out(&csv);
        MeasurementLog::writeCsv(samples, out);
    }
    if (!csv.commit())
        throw std::runtime_error(csv.errorString().toStdString());
    writeInfo(info);
    return info;
}

QList<SessionInfo> SessionStore::list() const
{
    QList<SessionInfo> sessions;
    const QDir dir(m_dir);
    for (const QString &file : dir.entryList({"*.json"}, QDir::Files)) {
        QFile f(dir.filePath(file));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject())
            continue;   // a damaged file: leave it, show the rest
        if (auto info = fromJson(doc.object()))
            sessions << *info;
    }
    std::sort(sessions.begin(), sessions.end(),
              [](const SessionInfo &a, const SessionInfo &b) { return a.startedAt > b.startedAt; });
    return sessions;
}

QList<Sample> SessionStore::loadSamples(const QUuid &id) const
{
    QFile f(csvPath(id));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QTextStream in(&f);
    return MeasurementLog::readCsv(in);
}

void SessionStore::update(const SessionInfo &info)
{
    writeInfo(info);
}

void SessionStore::remove(const QUuid &id)
{
    QFile::remove(csvPath(id));
    QFile::remove(infoPath(id));
}

void SessionStore::writeInfo(const SessionInfo &info)
{
    // QSaveFile writes then renames, so a crash mid-write can't leave a half-written file.
    QSaveFile f(infoPath(info.id));
    if (!f.open(QIODevice::WriteOnly))
        throw std::runtime_error(f.errorString().toStdString());
    f.write(QJsonDocument(toJson(info)).toJson(QJsonDocument::Indented));
    if (!f.commit())
        throw std::runtime_error(f.errorString().toStdString());
}

} // namespace readout
