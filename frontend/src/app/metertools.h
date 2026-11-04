// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Mcp/MeterTools.cs, McpResults.cs), Copyright (c) 2026 Alejandro Mora, MIT,
// with the dial tools added (get_dial, suggest_dial_position, clear_dial_suggestion).
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <functional>

namespace readout {

class CardTransfer;
class MeterSession;
class SessionStore;

struct ToolResult {
    QString text;      // JSON, or the error message
    bool isError = false;
};

// The MCP tools: what an AI assistant can do with the meter. Results are snake_case JSON, as
// Readout's are.
class MeterTools : public QObject
{
    Q_OBJECT
public:
    MeterTools(MeterSession *session, SessionStore *store, QObject *parent = nullptr);
    // The SD card tools (list_files, download_file, upload_file, ...) need it.
    void setCard(CardTransfer *card);
    // Where download_file saves when not told (default: the Downloads folder).
    QString downloadDir;

    static const QString Instructions;
    // How long press_button waits for the meter to react before reporting the reading.
    int settleMs = 800;

    QJsonArray definitions() const;
    bool has(const QString &name) const;
    void call(const QString &name, const QJsonObject &args, std::function<void(const ToolResult &)> done);

    // The reading as the tools report it (also used by the app's "copy reading").
    QJsonObject readingJson() const;

signals:
    // A session was saved through the tools (the app refreshes its list).
    void sessionSaved();

private:
    QJsonObject status() const;
    QJsonObject dialJson() const;
    QJsonObject suggestionJson() const;
    QJsonObject transferJson() const;
    void callCard(const QString &name, const QJsonObject &args, std::function<void(const ToolResult &)> done);
    void startUpload(const QString &name, const QByteArray &data, bool overwrite, int waitMs,
                     std::function<void(const ToolResult &)> done);

    MeterSession *m_session;
    SessionStore *m_store;
    CardTransfer *m_card = nullptr;
    // The last finished card operation, as the tools reported it.
    QJsonObject m_lastTransfer;
};

} // namespace readout
