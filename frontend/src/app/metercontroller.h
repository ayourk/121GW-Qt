// SPDX-License-Identifier: MIT
// What the QML front panel binds to: the session's state as properties, and the panel's actions.
#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace readout {

class CardTransfer;
class MeterSession;
class McpServer;
class SessionStore;
class MeterTools;

class MeterController : public QObject
{
    Q_OBJECT
    // Connection
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
    Q_PROPERTY(bool demo READ demo NOTIFY stateChanged)
    Q_PROPERTY(QString meterName READ meterName NOTIFY stateChanged)
    Q_PROPERTY(QString signalText READ signalText NOTIFY readingChanged)
    Q_PROPERTY(QVariantList discovered READ discovered NOTIFY discoveredChanged)
    // Reading
    Q_PROPERTY(bool hasReading READ hasReading NOTIFY readingChanged)
    Q_PROPERTY(QString modeName READ modeName NOTIFY readingChanged)
    Q_PROPERTY(QString mainLine READ mainLine NOTIFY readingChanged)
    Q_PROPERTY(QString subLine READ subLine NOTIFY readingChanged)
    Q_PROPERTY(QString batteryText READ batteryText NOTIFY readingChanged)
    Q_PROPERTY(bool backlight READ backlight NOTIFY backlightChanged)
    // Log
    Q_PROPERTY(bool logging READ logging WRITE setLogging NOTIFY loggingChanged)
    Q_PROPERTY(int logCount READ logCount NOTIFY logChanged)
    // Dial and jacks
    Q_PROPERTY(QVariantList dialPositions READ dialPositions CONSTANT)
    Q_PROPERTY(int dialPosition READ dialPosition NOTIFY dialChanged)
    Q_PROPERTY(QString dialFunction READ dialFunction NOTIFY dialChanged)
    Q_PROPERTY(int suggestedPosition READ suggestedPosition NOTIFY suggestionChanged)
    Q_PROPERTY(QString suggestedFunction READ suggestedFunction NOTIFY suggestionChanged)
    Q_PROPERTY(QString suggestionReason READ suggestionReason NOTIFY suggestionChanged)
    Q_PROPERTY(bool suggestionMet READ suggestionMet NOTIFY suggestionChanged)
    Q_PROPERTY(int modePressesNeeded READ modePressesNeeded NOTIFY suggestionChanged)
    // When the suggested function needs the red lead in the other current jack: that jack's label.
    Q_PROPERTY(QString moveLeadTo READ moveLeadTo NOTIFY suggestionChanged)
    // The demo meter's red current lead: "a", "ma" or "".
    Q_PROPERTY(QString demoLead READ demoLead NOTIFY dialChanged)
    // jack id ("a", "ma", "com", "v") -> {role, note}; empty when nothing to show
    Q_PROPERTY(QVariantMap jacks READ jacks NOTIFY jacksChanged)
    Q_PROPERTY(QString jacksSource READ jacksSource NOTIFY jacksChanged)
    // MCP
    Q_PROPERTY(bool mcpRunning READ mcpRunning NOTIFY mcpChanged)
    Q_PROPERTY(QString mcpEndpoint READ mcpEndpoint NOTIFY mcpChanged)
    Q_PROPERTY(QString mcpCommand READ mcpCommand NOTIFY mcpChanged)
    Q_PROPERTY(QString mcpActivity READ mcpActivity NOTIFY mcpChanged)
    // SD card (custom firmware): the last listing, the transfer in progress, the last result
    Q_PROPERTY(QVariantList cardFiles READ cardFiles NOTIFY cardFilesChanged)
    Q_PROPERTY(QString cardListedAt READ cardListedAt NOTIFY cardFilesChanged)
    Q_PROPERTY(bool cardBusy READ cardBusy NOTIFY cardChanged)
    Q_PROPERTY(QString cardOperation READ cardOperation NOTIFY cardChanged)
    // "requesting", "starting", "confirm_on_meter", "transferring", "finishing", or "" when idle
    Q_PROPERTY(QString cardPhase READ cardPhase NOTIFY cardChanged)
    Q_PROPERTY(QString cardFile READ cardFile NOTIFY cardChanged)
    // 0-1; -1 when the size isn't known yet
    Q_PROPERTY(double cardProgress READ cardProgress NOTIFY cardProgressChanged)
    Q_PROPERTY(QString cardProgressText READ cardProgressText NOTIFY cardProgressChanged)
    Q_PROPERTY(QString cardRateText READ cardRateText NOTIFY cardProgressChanged)
    Q_PROPERTY(QString cardEtaText READ cardEtaText NOTIFY cardProgressChanged)
    Q_PROPERTY(QString cardMessage READ cardMessage NOTIFY cardChanged)
    Q_PROPERTY(bool cardMessageOk READ cardMessageOk NOTIFY cardChanged)
    // Demo meter: let its confirmation prompt time out (refuse) instead of pressing SETUP.
    Q_PROPERTY(bool cardDemoReject READ cardDemoReject WRITE setCardDemoReject NOTIFY cardChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    MeterController(MeterSession *session, SessionStore *store, MeterTools *tools, McpServer *mcp,
                    CardTransfer *card, QSettings *settings, QObject *parent = nullptr);

    MeterSession *session() const { return m_session; }

    QString connectionState() const;
    bool connected() const;
    bool scanning() const;
    bool demo() const;
    QString meterName() const;
    QString signalText() const;
    QVariantList discovered() const;
    bool hasReading() const;
    QString modeName() const;
    QString mainLine() const;
    QString subLine() const;
    QString batteryText() const;
    bool backlight() const { return m_backlight; }
    bool logging() const;
    void setLogging(bool on);
    int logCount() const;
    QVariantList dialPositions() const;
    int dialPosition() const;
    QString dialFunction() const;
    int suggestedPosition() const;
    QString suggestedFunction() const;
    QString suggestionReason() const;
    bool suggestionMet() const;
    int modePressesNeeded() const;
    QString moveLeadTo() const;
    QString demoLead() const;
    QVariantMap jacks() const;
    QString jacksSource() const;
    bool mcpRunning() const;
    QString mcpEndpoint() const;
    QString mcpCommand() const;
    QString mcpActivity() const { return m_mcpActivity; }
    QString errorText() const { return m_error; }
    QString version() const;
    QVariantList cardFiles() const;
    QString cardListedAt() const;
    bool cardBusy() const;
    QString cardOperation() const;
    QString cardPhase() const;
    QString cardFile() const;
    double cardProgress() const;
    QString cardProgressText() const;
    QString cardRateText() const;
    QString cardEtaText() const;
    QString cardMessage() const { return m_cardMessage; }
    bool cardMessageOk() const { return m_cardMessageOk; }
    bool cardDemoReject() const;
    void setCardDemoReject(bool on);

    Q_INVOKABLE void press(const QString &key, bool longPress);
    Q_INVOKABLE void turnDial(int position);
    // Demo meter: plugs the red current lead into "a" or "ma" ("" unplugs it).
    Q_INVOKABLE void plugLead(const QString &jackId);
    Q_INVOKABLE bool setClock();
    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void connectMeter(const QString &id);
    Q_INVOKABLE void startDemo();
    Q_INVOKABLE void disconnectMeter();
    Q_INVOKABLE void clearLog();
    Q_INVOKABLE QString saveSession(const QString &name);
    Q_INVOKABLE void suggest(int position, const QString &reason);
    Q_INVOKABLE void clearSuggestion();
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void dismissError();

    // SD card. The file dialogs are the platform's (Qt Widgets); each returns "" when canceled.
    Q_INVOKABLE void cardList();
    // Asks where to save, then downloads.
    Q_INVOKABLE void cardDownload(const QString &name);
    Q_INVOKABLE QString chooseUploadFile();
    Q_INVOKABLE QString chooseFirmwareFile();
    // Starts an upload of a local file as name; returns why it can't start, or "".
    Q_INVOKABLE QString cardUpload(const QString &path, const QString &name);
    Q_INVOKABLE void cardCancel();
    Q_INVOKABLE QString suggestCardName(const QString &path) const;
    // Why name can't be written ("" when it can).
    Q_INVOKABLE QString cardNameProblem(const QString &name) const;
    Q_INVOKABLE bool cardFileExists(const QString &name) const;
    Q_INVOKABLE bool cardNeedsConfirmation(const QString &name) const;
    // "92.2 KB, about 8 min over Bluetooth"
    Q_INVOKABLE QString localFileText(const QString &path) const;
    Q_INVOKABLE double localFileSize(const QString &path) const;

signals:
    void stateChanged();
    void discoveredChanged();
    void readingChanged();
    void backlightChanged();
    void loggingChanged();
    void logChanged();
    void dialChanged();
    void suggestionChanged();
    void jacksChanged();
    void mcpChanged();
    void errorChanged();
    void cardChanged();
    void cardProgressChanged();
    void cardFilesChanged();
    // A key was pressed (here, by an assistant, or on the meter itself), for the button flash.
    void keyFlash(const QString &key, bool longPress, bool fromApp);
    // The user turned a real meter's dial in the app: only the meter's own knob can do that.
    void dialIsManual();

private:
    void setError(const QString &message);
    void setCardMessage(const QString &message, bool ok);

    MeterSession *m_session;
    SessionStore *m_store;
    MeterTools *m_tools;
    McpServer *m_mcp;
    CardTransfer *m_card;
    QSettings *m_settings;
    QString m_cardMessage;
    bool m_cardMessageOk = true;
    bool m_backlight = false;
    QString m_mcpActivity;
    QString m_error;
};

} // namespace readout
