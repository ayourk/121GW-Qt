// SPDX-License-Identifier: MIT
#include "metercontroller.h"

#include "cardtransfer.h"
#include "dial.h"
#include "mcpserver.h"
#include "metersession.h"
#include "metertools.h"
#include "sessionstore.h"
#include "simulatedconnection.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QTime>

namespace readout {

MeterController::MeterController(MeterSession *session, SessionStore *store, MeterTools *tools, McpServer *mcp,
                                 CardTransfer *card, QSettings *settings, QObject *parent)
    : QObject(parent), m_session(session), m_store(store), m_tools(tools), m_mcp(mcp), m_card(card), m_settings(settings)
{
    connect(card, &CardTransfer::changed, this, &MeterController::cardChanged);
    connect(card, &CardTransfer::progressChanged, this, &MeterController::cardProgressChanged);
    connect(card, &CardTransfer::filesChanged, this, &MeterController::cardFilesChanged);
    // Results of operations started elsewhere (an assistant over MCP) show in the card view too.
    connect(card, &CardTransfer::finished, this, [this](const CardTransfer::Result &r) {
        setCardMessage(r.message, r.ok);
    });
    connect(session, &MeterSession::stateChanged, this, &MeterController::stateChanged);
    connect(session, &MeterSession::stateChanged, this, &MeterController::jacksChanged);
    connect(session, &MeterSession::discoveredChanged, this, &MeterController::discoveredChanged);
    connect(session, &MeterSession::readingReceived, this, &MeterController::readingChanged);
    connect(session, &MeterSession::readingReceived, this, &MeterController::jacksChanged);
    connect(session, &MeterSession::loggingChanged, this, &MeterController::loggingChanged);
    connect(session, &MeterSession::logChanged, this, &MeterController::logChanged);
    connect(session, &MeterSession::dialChanged, this, &MeterController::dialChanged);
    connect(session, &MeterSession::dialChanged, this, &MeterController::suggestionChanged);
    connect(session, &MeterSession::suggestionChanged, this, &MeterController::suggestionChanged);
    connect(session, &MeterSession::suggestionChanged, this, &MeterController::jacksChanged);
    connect(session, &MeterSession::errorOccurred, this, &MeterController::setError);
    connect(session, &MeterSession::preferredMeterChanged, this, [this](const QString &id) {
        m_settings->setValue("meter/preferred", id);
    });
    connect(session, &MeterSession::keyPressed, this, [this](MeterKeyPress press, bool fromApp) {
        // The meter doesn't report its backlight; holding MODE toggles it.
        if (press.key == MeterKey::Mode && press.longPress) {
            m_backlight = !m_backlight;
            emit backlightChanged();
        }
        emit keyFlash(QString::fromLatin1(keyId(press.key)), press.longPress, fromApp);
    });
    connect(mcp, &McpServer::toolCalled, this, [this](const QString &name) {
        m_mcpActivity = QStringLiteral("%1  %2").arg(QTime::currentTime().toString("HH:mm:ss"), name);
        emit mcpChanged();
    });
}

QString MeterController::connectionState() const { return connectionStateName(m_session->state()); }
bool MeterController::connected() const { return m_session->state() == ConnectionState::Connected; }
bool MeterController::scanning() const { return m_session->state() == ConnectionState::Scanning; }
bool MeterController::demo() const { return m_session->isDemo(); }
QString MeterController::meterName() const { return m_session->meter() ? m_session->meter()->name : QString(); }

QString MeterController::signalText() const
{
    const auto rssi = m_session->signalStrength();
    return rssi ? QStringLiteral("%1 dBm").arg(*rssi) : QString();
}

QVariantList MeterController::discovered() const
{
    QVariantList list;
    for (const DiscoveredMeter &m : m_session->discovered())
        list << QVariantMap{{"id", m.id}, {"name", m.name}, {"rssi", m.rssi}};
    return list;
}

bool MeterController::hasReading() const { return m_session->latest().has_value(); }
QString MeterController::modeName() const { return m_session->latest() ? m_session->latest()->modeName : QString(); }

QString MeterController::mainLine() const
{
    const auto &r = m_session->latest();
    return r ? (r->blank ? QString() : r->mainLine()) : QString();
}

QString MeterController::subLine() const
{
    const auto &r = m_session->latest();
    if (!r || r->sub.text.isEmpty())
        return {};
    return r->sub.label.isEmpty() ? r->sub.line() : QStringLiteral("%1: %2").arg(r->sub.label, r->sub.line());
}

QString MeterController::batteryText() const
{
    const auto v = m_session->batteryVolts();
    return v ? QStringLiteral("%1 V").arg(*v, 0, 'f', 1) : QString();
}

bool MeterController::logging() const { return m_session->isLogging(); }

void MeterController::setLogging(bool on)
{
    m_session->setLogging(on);
    m_settings->setValue("log/enabled", on);
}

int MeterController::logCount() const { return m_session->log().count(); }

QVariantList MeterController::dialPositions() const
{
    QVariantList list;
    for (const DialPosition &p : dial::positions()) {
        QStringList fns;
        for (const DialFunction &f : p.functions)
            fns << f.label;
        list << QVariantMap{{"id", p.id}, {"legend", p.legend}, {"redLegend", p.redLegend}, {"angle", p.angle},
                            {"functions", fns}, {"off", p.isOff()}};
    }
    return list;
}

int MeterController::dialPosition() const { return m_session->dialPosition(); }

QString MeterController::dialFunction() const
{
    const int pos = m_session->dialPosition(), f = m_session->dialFunction();
    if (pos < 0 || f < 0)
        return {};
    return dial::positions()[pos].functions.value(f).label;
}

int MeterController::suggestedPosition() const
{
    return m_session->suggestion() ? m_session->suggestion()->position : -1;
}

QString MeterController::suggestedFunction() const
{
    const auto &s = m_session->suggestion();
    if (!s || s->function < 0)
        return {};
    return dial::positions()[s->position].functions.value(s->function).label;
}

QString MeterController::suggestionReason() const
{
    return m_session->suggestion() ? m_session->suggestion()->reason : QString();
}

bool MeterController::suggestionMet() const { return m_session->suggestionMet(); }

int MeterController::modePressesNeeded() const
{
    const auto &s = m_session->suggestion();
    if (!s || s->function < 0 || m_session->dialPosition() != s->position || m_session->dialFunction() < 0)
        return 0;
    return std::max(0, dial::modePresses(s->position, m_session->dialFunction(), s->function));
}

QString MeterController::moveLeadTo() const
{
    const auto &s = m_session->suggestion();
    if (!s || s->function < 0 || m_session->dialPosition() != s->position || m_session->dialFunction() < 0)
        return {};
    if (dial::modePresses(s->position, m_session->dialFunction(), s->function) >= 0)
        return {};
    return dial::jackLabel(*dial::positions()[s->position].functions[s->function].currentJack);
}

QString MeterController::demoLead() const
{
    const auto lead = m_session->demoLead();
    return lead ? dial::jackId(*lead) : QString();
}

void MeterController::plugLead(const QString &jackId)
{
    if (jackId == QLatin1String("a"))
        m_session->plugDemoLead(Jack::Amps);
    else if (jackId == QLatin1String("ma"))
        m_session->plugDemoLead(Jack::MilliMicro);
    else
        m_session->plugDemoLead(std::nullopt);
}

bool MeterController::setClock()
{
    return m_session->setClock();
}

QVariantMap MeterController::jacks() const
{
    QList<JackUse> uses;
    const auto &s = m_session->suggestion();
    if (s && !m_session->suggestionMet())
        uses = dial::jacksForPosition(s->position, s->function);
    else if (m_session->latest())
        uses = dial::jacksForMode(m_session->latest()->packet.mode());
    else if (m_session->dialPosition() >= 0)
        uses = dial::jacksForPosition(m_session->dialPosition());
    QVariantMap map;
    for (const JackUse &u : uses) {
        // Where a jack is listed twice (VA: current), keep the first.
        if (!map.contains(dial::jackId(u.jack)))
            map.insert(dial::jackId(u.jack), QVariantMap{{"role", u.role}, {"note", u.note}});
    }
    return map;
}

QString MeterController::jacksSource() const
{
    const auto &s = m_session->suggestion();
    if (s && !m_session->suggestionMet())
        return QStringLiteral("suggestion");
    if (m_session->latest())
        return QStringLiteral("reading");
    return m_session->dialPosition() >= 0 ? QStringLiteral("dial") : QString();
}

bool MeterController::mcpRunning() const { return m_mcp->isRunning(); }
QString MeterController::mcpEndpoint() const { return m_mcp->isRunning() ? m_mcp->endpoint() : QString(); }

QString MeterController::mcpCommand() const
{
    if (!m_mcp->isRunning())
        return {};
    return QStringLiteral("claude mcp add --transport http 121gw %1 --header \"Authorization: Bearer %2\"")
        .arg(m_mcp->endpoint(), m_settings->value("mcp/token").toString());
}

QString MeterController::version() const { return QCoreApplication::applicationVersion(); }

void MeterController::press(const QString &key, bool longPress)
{
    MeterKey k;
    if (!keyFromName(key, &k))
        return;
    if (k == MeterKey::Peak && longPress) {
        setError(QStringLiteral("Holding 1ms PEAK turns the meter's Bluetooth off; do that on the meter itself."));
        return;
    }
    if (m_session->transferActive()) {
        setError(QStringLiteral("A file transfer is running; the meter's buttons work again when it ends."));
        return;
    }
    m_session->sendKey(k, longPress);
}

void MeterController::turnDial(int position)
{
    if (m_session->isDemo())
        m_session->turnDemoDial(position);
    else
        emit dialIsManual();
}

void MeterController::startScan()
{
    m_session->startScan(false);
}

void MeterController::stopScan() { m_session->stopScan(); }

void MeterController::connectMeter(const QString &id)
{
    for (const DiscoveredMeter &m : m_session->discovered())
        if (m.id == id)
            m_session->connectTo(m);
}

void MeterController::startDemo() { m_session->startDemo(); }
void MeterController::disconnectMeter() { m_session->disconnectFrom(); }
void MeterController::clearLog() { m_session->clearLog(); }

QString MeterController::saveSession(const QString &name)
{
    const auto &samples = m_session->log().samples();
    if (samples.isEmpty())
        return {};
    try {
        const SessionInfo info = m_store->save(name.trimmed().isEmpty() ? sessionsummary::suggestedName(samples) : name.trimmed(),
                                               samples);
        m_session->clearLog();
        return info.name;
    } catch (const std::exception &e) {
        setError(QStringLiteral("Couldn't save the session: %1").arg(QString::fromUtf8(e.what())));
        return {};
    }
}

void MeterController::suggest(int position, const QString &reason)
{
    if (position < 0 || position >= dial::positions().size())
        return;
    DialSuggestion s;
    s.position = position;
    s.reason = reason;
    s.at = QDateTime::currentDateTime();
    m_session->suggest(s);
}

void MeterController::clearSuggestion() { m_session->clearSuggestion(); }

void MeterController::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void MeterController::setError(const QString &message)
{
    m_error = message;
    emit errorChanged();
}

void MeterController::dismissError() { setError({}); }

// ── SD card ─────────────────────────────────────────────────────────────────

namespace {

QString sizeText(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

QString durationText(int seconds)
{
    if (seconds < 60)
        return QStringLiteral("%1 s").arg(seconds);
    if (seconds < 3600)
        return QStringLiteral("%1 min %2 s").arg(seconds / 60).arg(seconds % 60);
    return QStringLiteral("%1 h %2 min").arg(seconds / 3600).arg(seconds / 60 % 60);
}

// The 121GW's link, for estimates before a transfer has a rate of its own.
constexpr double NominalBytesPerSecond = 190;

} // namespace

QVariantList MeterController::cardFiles() const
{
    QVariantList list;
    for (const card::CardFile &f : m_card->files())
        list << QVariantMap{{"name", f.name}, {"size", static_cast<double>(f.size)}, {"sizeText", sizeText(f.size)},
                            {"date", f.modified.isValid() ? f.modified.toString("yyyy-MM-dd HH:mm") : QString()},
                            {"protected", card::isProtected(f.name)}, {"firmware", f.name == card::FirmwareFile},
                            {"directory", f.directory}};
    return list;
}

QString MeterController::cardListedAt() const
{
    return m_card->listedAt().isValid() ? m_card->listedAt().toString("HH:mm:ss") : QString();
}

bool MeterController::cardBusy() const { return m_card->busy(); }
QString MeterController::cardOperation() const { return CardTransfer::operationName(m_card->operation()); }
QString MeterController::cardPhase() const { return m_card->busy() ? CardTransfer::phaseName(m_card->phase()) : QString(); }
QString MeterController::cardFile() const { return m_card->fileName(); }

double MeterController::cardProgress() const
{
    const qint64 total = m_card->bytesTotal();
    if (!m_card->busy() || total < 0)
        return -1;
    return total == 0 ? 1.0 : std::clamp(static_cast<double>(m_card->bytesDone()) / total, 0.0, 1.0);
}

QString MeterController::cardProgressText() const
{
    if (!m_card->busy() || m_card->operation() == CardTransfer::Operation::List)
        return {};
    const qint64 total = m_card->bytesTotal();
    return total >= 0 ? QStringLiteral("%1 of %2").arg(sizeText(m_card->bytesDone()), sizeText(total))
                      : sizeText(m_card->bytesDone());
}

QString MeterController::cardRateText() const
{
    const double rate = m_card->bytesPerSecond();
    return m_card->busy() && rate > 0 ? QStringLiteral("%1 B/s").arg(qRound(rate)) : QString();
}

QString MeterController::cardEtaText() const
{
    const int left = m_card->secondsLeft();
    return m_card->busy() && left >= 0 ? QStringLiteral("about %1 left").arg(durationText(left)) : QString();
}

bool MeterController::cardDemoReject() const
{
    return m_session->demoConnection()->card().rejectConfirmations;
}

void MeterController::setCardDemoReject(bool on)
{
    m_session->demoConnection()->card().rejectConfirmations = on;
    m_settings->setValue("demo/rejectConfirmations", on);
    emit cardChanged();
}

void MeterController::setCardMessage(const QString &message, bool ok)
{
    m_cardMessage = message;
    m_cardMessageOk = ok;
    emit cardChanged();
}

void MeterController::cardList()
{
    const QString e = m_card->list();
    if (!e.isEmpty())
        setCardMessage(e, false);
}

void MeterController::cardDownload(const QString &name)
{
    const QString dir = m_settings->value("card/saveDir", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString();
    const QString target = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Save %1 from the meter").arg(name),
                                                        QDir(dir).filePath(name));
    if (target.isEmpty())
        return;
    m_settings->setValue("card/saveDir", QFileInfo(target).absolutePath());
    const QString e = m_card->download(name, [this, target](const CardTransfer::Result &r) {
        if (!r.ok)
            return;   // the finished signal already showed why
        QSaveFile out(target);
        if (!out.open(QIODevice::WriteOnly) || out.write(r.data) != r.data.size() || !out.commit())
            setCardMessage(QStringLiteral("Downloaded %1, but couldn't save it: %2").arg(r.name, out.errorString()), false);
        else
            setCardMessage(QStringLiteral("Saved %1 to %2 (%3, %4 s).").arg(r.name, target, sizeText(r.data.size()))
                               .arg(qRound(r.seconds)), true);
    });
    if (!e.isEmpty())
        setCardMessage(e, false);
}

QString MeterController::chooseUploadFile()
{
    const QString dir = m_settings->value("card/openDir", QDir::homePath()).toString();
    const QString path = QFileDialog::getOpenFileName(nullptr, QStringLiteral("Upload a file to the meter's SD card"), dir);
    if (!path.isEmpty())
        m_settings->setValue("card/openDir", QFileInfo(path).absolutePath());
    return path;
}

QString MeterController::chooseFirmwareFile()
{
    const QString dir = m_settings->value("card/firmwareDir", QDir::homePath()).toString();
    const QString path = QFileDialog::getOpenFileName(nullptr, QStringLiteral("Choose a 121GW firmware image"), dir,
                                                      QStringLiteral("Firmware images (*.bin *.BIN);;All files (*)"));
    if (!path.isEmpty())
        m_settings->setValue("card/firmwareDir", QFileInfo(path).absolutePath());
    return path;
}

QString MeterController::cardUpload(const QString &path, const QString &name)
{
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly))
        return QStringLiteral("Can't read %1: %2").arg(path, in.errorString());
    if (in.size() > 4 * 1024 * 1024)
        return QStringLiteral("That file is over 4 MB: at about 190 bytes per second it would take hours.");
    const QString e = m_card->upload(name, in.readAll());
    if (!e.isEmpty())
        setCardMessage(e, false);
    return e;
}

void MeterController::cardCancel() { m_card->cancel(); }

QString MeterController::suggestCardName(const QString &path) const
{
    return card::suggestName(QFileInfo(path).fileName());
}

QString MeterController::cardNameProblem(const QString &name) const
{
    QString why;
    if (!card::isValidName(name, &why))
        return why;
    card::isRefused(name, &why);
    return why;
}

bool MeterController::cardFileExists(const QString &name) const { return m_card->find(name).has_value(); }

bool MeterController::cardNeedsConfirmation(const QString &name) const
{
    return card::needsConfirmation(name, cardFileExists(name));
}

double MeterController::localFileSize(const QString &path) const
{
    return static_cast<double>(QFileInfo(path).size());
}

QString MeterController::localFileText(const QString &path) const
{
    const QFileInfo info(path);
    if (!info.isFile())
        return {};
    return QStringLiteral("%1, about %2 over Bluetooth").arg(sizeText(info.size()),
                                                             durationText(qRound((info.size() * 1.06 + 300) / NominalBytesPerSecond)));
}

} // namespace readout
