// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Logging/SessionStore.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#pragma once

#include "measurementlog.h"

#include <QUuid>

namespace readout {

// A saved recording. mainMode is the mode with the most readings; unit and statistics are for it
// (statistics nullopt when none of them had a value).
struct SessionInfo {
    QUuid id;
    QString name;
    QString notes;
    QDateTime startedAt;
    QDateTime endedAt;
    int sampleCount = 0;
    MeterMode mainMode = MeterMode::DcVolts;
    QString unit;
    std::optional<LogStatistics> statistics;
};

namespace sessionsummary {
// The mode with the most readings that have a value (else the most readings at all).
MeterMode mainMode(const QList<Sample> &samples);
// A name for a new session: its main mode and when it started ("DC voltage, Oct 27 14:05").
QString suggestedName(const QList<Sample> &samples);
SessionInfo describe(const QUuid &id, const QString &name, const QString &notes, const QList<Sample> &samples);
} // namespace sessionsummary

// Saved sessions on disk: each is {id}.csv (the samples, in the export format) plus {id}.json
// (name, notes and a summary, so the list loads without reading samples). The same files
// Readout writes.
class SessionStore
{
public:
    explicit SessionStore(QString directory) : m_dir(std::move(directory)) {}
    const QString &directory() const { return m_dir; }
    QString csvPath(const QUuid &id) const;

    // Throws std::runtime_error when the files can't be written.
    SessionInfo save(const QString &name, const QList<Sample> &samples);
    // Every saved session, newest first. Files that can't be read are skipped.
    QList<SessionInfo> list() const;
    QList<Sample> loadSamples(const QUuid &id) const;
    void update(const SessionInfo &info);
    void remove(const QUuid &id);

private:
    QString infoPath(const QUuid &id) const;
    void writeInfo(const SessionInfo &info);
    QString m_dir;
};

} // namespace readout
