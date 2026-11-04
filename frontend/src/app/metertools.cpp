// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Mcp/MeterTools.cs, McpResults.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "metertools.h"

#include "cardtransfer.h"
#include "dial.h"
#include "metersession.h"
#include "sessionstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace readout {

const QString MeterTools::Instructions = QStringLiteral(
    "Tools for an EEVblog 121GW multimeter connected to the 121gw-qt app over Bluetooth. "
    "get_reading returns what the meter shows right now; values are also given in SI base units. "
    "The dial on the meter picks the measurement group (V, mV, Hz, Ω, A…) and can only be turned by hand: "
    "get_dial lists its positions, where the knob is, and which jacks the probe leads go in; "
    "suggest_dial_position lights a position on the app's dial for the user (with your reason) and can wait "
    "until they've turned it there. press_button MODE cycles within the dial position (for example DC, AC, "
    "AC+DC on V). The app logs every reading while connected; get_recent_readings and save_session work on that log. "
    "Safety first: say which jacks to use before asking for a current measurement, and never suggest measuring "
    "current across a voltage source. "
    "With the custom firmware, list_files, download_file and upload_file reach the meter's SD card over Bluetooth "
    "(about 190 bytes per second: a 92 KB firmware image takes about 8 minutes). Overwriting a file, and every "
    "write of EEVBLOG.BIN (the firmware image the meter flashes on its next update), waits for the user to press "
    "SETUP on the meter within 30 seconds: tell them before you start. CAL.BIN can never be written.");

namespace {

using Done = std::function<void(const ToolResult &)>;

ToolResult ok(const QJsonObject &o)
{
    return {QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented)), false};
}

ToolResult error(const QString &message)
{
    return {message, true};
}

QJsonObject tool(const char *name, const char *title, const char *description, const QJsonObject &properties,
                 const QJsonArray &required, bool readOnly, bool destructive = false, bool idempotent = false)
{
    QJsonObject schema{{"type", "object"}, {"properties", properties}};
    if (!required.isEmpty())
        schema["required"] = required;
    return {
        {"name", name},
        {"title", title},
        {"description", QString::fromUtf8(description)},
        {"inputSchema", schema},
        {"annotations", QJsonObject{{"title", title}, {"readOnlyHint", readOnly}, {"destructiveHint", destructive},
                                    {"idempotentHint", idempotent}, {"openWorldHint", false}}},
    };
}

QJsonObject prop(const char *type, const char *description)
{
    return {{"type", type}, {"description", QString::fromUtf8(description)}};
}

QString signalWord(int rssi)
{
    if (rssi >= -60) return QStringLiteral("excellent");
    if (rssi >= -70) return QStringLiteral("good");
    if (rssi >= -80) return QStringLiteral("fair");
    if (rssi >= -90) return QStringLiteral("weak");
    return QStringLiteral("very weak");
}

QJsonValue optionalNumber(const std::optional<double> &v)
{
    return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null);
}

// Readout's ParseButton: forgiving about case, spaces, "_" and "/".
std::optional<MeterKey> parseButton(QString button)
{
    QString name = button.trimmed().toUpper();
    name.remove(u' ');
    name.remove(u'_');
    name.remove(u'/');
    if (name == "RANGE") return MeterKey::Range;
    if (name == "HOLD") return MeterKey::Hold;
    if (name == "REL" || name == "RELATIVE" || name == "UP") return MeterKey::Rel;
    if (name == "PEAK" || name == "1MSPEAK" || name == "1MS" || name == "DOWN") return MeterKey::Peak;
    if (name == "MODE") return MeterKey::Mode;
    if (name == "MINMAX") return MeterKey::MinMax;
    if (name == "MEM" || name == "MEMORY") return MeterKey::Mem;
    if (name == "SETUP") return MeterKey::Setup;
    return std::nullopt;
}

// At most max evenly spaced items, always including the last.
template <typename T>
QList<T> thin(const QList<T> &items, int max)
{
    if (items.size() <= max)
        return items;
    QList<T> result;
    result.reserve(max);
    for (qint64 k = 0; k < max; ++k)
        result << items[static_cast<qsizetype>(k * (items.size() - 1) / (max - 1))];
    return result;
}

QJsonObject sampleJson(const Sample &s)
{
    return {{"time", s.time.toString(Qt::ISODateWithMs)}, {"mode", decoder::modeName(s.mode)},
            {"value", optionalNumber(s.value)}, {"unit", s.unit}, {"display", s.display}};
}

QJsonValue statsJson(const std::optional<LogStatistics> &s, const QString &unit)
{
    if (!s)
        return QJsonValue::Null;
    return QJsonObject{{"count", s->count}, {"min", s->min}, {"max", s->max}, {"average", s->average},
                       {"peak_to_peak", s->peakToPeak()}, {"unit", unit}};
}

QJsonObject sessionJson(const SessionInfo &i)
{
    return {{"id", i.id.toString(QUuid::Id128)}, {"name", i.name}, {"notes", i.notes},
            {"started", i.startedAt.toString(Qt::ISODateWithMs)}, {"ended", i.endedAt.toString(Qt::ISODateWithMs)},
            {"readings", i.sampleCount}, {"mode", decoder::modeName(i.mainMode)}, {"unit", i.unit},
            {"statistics", statsJson(i.statistics, i.unit)}};
}

QJsonArray jacksJson(const QList<JackUse> &jacks)
{
    QJsonArray a;
    for (const JackUse &j : jacks) {
        QJsonObject o{{"jack", dial::jackLabel(j.jack)}, {"lead", j.role}};
        if (!j.note.isEmpty())
            o["when"] = j.note;
        a << o;
    }
    return a;
}

QString positionName(int index)
{
    const auto &p = dial::positions();
    return index >= 0 && index < p.size() ? p[index].id : QString();
}

} // namespace

MeterTools::MeterTools(MeterSession *session, SessionStore *store, QObject *parent)
    : QObject(parent), m_session(session), m_store(store)
{
}

QJsonArray MeterTools::definitions() const
{
    return {
        tool("get_status", "Meter status",
             "Whether a meter is connected, its signal strength and battery, whether logging is on, where the dial is, "
             "any dial suggestion pending, and the latest reading.", {}, {}, true),
        tool("set_clock", "Set the meter's clock",
             "Sets the meter's date and time (used for its own SD card logging) to this computer's, with the meter's "
             "F8 clock command (found in firmware 2.05, not in the manual). The meter accepts it once per Bluetooth "
             "connection; check the result on its SETUP Year, Date and Time pages.", {}, {}, false, false, true),
        tool("get_reading", "Read the meter",
             "The meter's current reading: the main display (value and unit), the small display, the mode, the lit LCD "
             "symbols (AUTO, HOLD, REL, MIN/MAX…) and the raw packet in hex. On a SETUP page, setup_page names it and "
             "sub_display has its value. Fails when no meter is connected.", {}, {}, true),
        tool("get_recent_readings", "Recent readings",
             "Readings from the app's log over the last few seconds or minutes, with min, max, average and peak-to-peak "
             "for the current mode. Long stretches are thinned to maxPoints evenly spaced readings. Only logged readings "
             "are included: nothing is logged while logging is paused.",
             {{"seconds", prop("number", "How far back to look, in seconds (1 to 86400). Default 30.")},
              {"maxPoints", prop("integer", "At most this many readings are returned (10 to 5000). Default 300.")}},
             {}, true),
        tool("press_button", "Press a meter button",
             "Presses one of the meter's buttons remotely, as if pressed on the meter: RANGE, HOLD, REL, PEAK (1ms PEAK), "
             "MODE, MINMAX, MEM or SETUP. longPress holds it (RANGE back to auto range, REL turns the 1 kHz low-pass "
             "filter on in AC, MIN/MAX leaves MIN/MAX mode, MODE toggles the backlight). Holding PEAK is refused: it "
             "would turn the meter's Bluetooth off and end the connection. Returns the reading shortly after the press.",
             {{"button", prop("string", "RANGE, HOLD, REL, PEAK, MODE, MINMAX, MEM or SETUP.")},
              {"longPress", prop("boolean", "Hold the button instead of tapping it.")}},
             {"button"}, false),
        tool("get_dial", "The rotary dial",
             "The meter's dial positions (id, legend, the functions there in MODE order, and the jacks the leads go in), "
             "where the knob is now (worked out from the meter's mode), and any pending suggestion. On the A·mA and "
             "mVA/VA positions the meter picks A or mA (VA or mVA) by which current jack the red lead is in, and MODE "
             "only toggles DC/AC; those functions carry the jack they need. From firmware 2.05.",
             {}, {}, true),
        tool("suggest_dial_position", "Suggest a dial position",
             "Lights a dial position on the app's picture of the meter, with your reason, and highlights the jacks the "
             "leads go in, so the user knows where to turn the knob and where to plug the probes. Optionally names the "
             "function within the position (reached with MODE; you may press MODE yourself once the knob is there). "
             "With waitSeconds, waits until the meter is there (or the time runs out) and returns the reading.",
             {{"position", prop("string", "Dial position id from get_dial: lowz, v, mv, hz, ohm, va, ua, a or off.")},
              {"function", prop("string", "Optional: the function at that position, as get_dial lists it (\"AC\", \"Diode\").")},
              {"reason", prop("string", "Shown to the user beside the dial: what this measurement is for.")},
              {"waitSeconds", prop("number", "Wait up to this long (0 to 120) for the meter to get there. Default 0.")}},
             {"position"}, false, false, true),
        tool("clear_dial_suggestion", "Clear the dial suggestion",
             "Removes the dial suggestion from the app.", {}, {}, false, false, true),
        tool("set_logging", "Pause or resume logging",
             "Turns the app's logging of readings on or off. The display keeps updating either way.",
             {{"enabled", prop("boolean", "true to log readings, false to pause.")}}, {"enabled"}, false, false, true),
        tool("clear_log", "Clear the log",
             "Deletes the readings logged so far and starts a new recording. Save them first with save_session to keep them.",
             {}, {}, false, true),
        tool("save_session", "Save the recording",
             "Saves the logged readings as a named session and starts a new recording.",
             {{"name", prop("string", "A name for the session; leave out for one based on the mode and time.")}}, {}, false),
        tool("list_sessions", "Saved sessions", "The saved sessions, newest first, with their mode, length and statistics.",
             {{"limit", prop("integer", "At most this many sessions, newest first (1 to 500). Default 50.")}}, {}, true),
        tool("list_files", "Files on the meter's SD card",
             "Lists the files on the meter's SD card (name, size, date) over Bluetooth. Needs the custom firmware with "
             "Bluetooth file transfer; the meter's readings pause while a transfer runs.", {}, {}, true),
        tool("download_file", "Download a file from the meter",
             "Copies a file from the meter's SD card to this computer with YMODEM (about 190 bytes per second). Saves it "
             "to saveTo, or to the Downloads folder under its own name; a text file (the meter's CSV logs) up to 32 KB is "
             "also returned as text. Waits up to waitSeconds; a longer transfer continues, and get_transfer_status "
             "reports it.",
             {{"name", prop("string", "The file's name on the card (8.3, as list_files gives it).")},
              {"saveTo", prop("string", "Optional: an absolute path to save to.")},
              {"overwriteLocal", prop("boolean", "Replace saveTo if it exists. Default false.")},
              {"waitSeconds", prop("number", "Wait up to this long (1 to 900) for the transfer. Default 120.")}},
             {"name"}, false),
        tool("upload_file", "Upload a file to the meter",
             "Copies a file from this computer to the meter's SD card with YMODEM (about 190 bytes per second). "
             "Overwriting an existing file needs overwrite true, and the meter then waits for the user to press SETUP "
             "on it (30 seconds; any other key or the time running out refuses). EEVBLOG.BIN, the firmware image the "
             "meter flashes on its next firmware update procedure, always needs that key press and firmwareUpdate "
             "true: only upload firmware the user chose. CAL.BIN can never be written over Bluetooth. Waits up to "
             "waitSeconds; a longer transfer continues, and get_transfer_status reports it.",
             {{"path", prop("string", "The absolute path of the file on this computer.")},
              {"name", prop("string", "Optional: its 8.3 name on the card; default made from the file's own name.")},
              {"overwrite", prop("boolean", "Allow replacing a file of that name (the user confirms on the meter).")},
              {"firmwareUpdate", prop("boolean", "Required true to write EEVBLOG.BIN.")},
              {"waitSeconds", prop("number", "Wait up to this long (1 to 900) for the transfer. Default 120.")}},
             {"path"}, false, true),
        tool("get_transfer_status", "File transfer status",
             "The SD card transfer in progress (file, phase, bytes, rate, seconds left, whether the meter is waiting for "
             "SETUP) and the last one's result.", {}, {}, true),
        tool("cancel_transfer", "Cancel the file transfer",
             "Stops the SD card transfer in progress (YMODEM CAN). A partly written file on the card is the firmware's "
             "to clean up.", {}, {}, false, false, true),
        tool("get_session", "A saved session",
             "One saved session's details and readings, thinned to maxPoints evenly spaced readings when it's longer.",
             {{"id", prop("string", "The session's id, from list_sessions or save_session.")},
              {"maxPoints", prop("integer", "At most this many readings are returned (10 to 5000). Default 500.")}},
             {"id"}, true),
    };
}

bool MeterTools::has(const QString &name) const
{
    for (const QJsonValue &t : definitions())
        if (t.toObject().value("name").toString() == name)
            return true;
    return false;
}

QJsonObject MeterTools::readingJson() const
{
    const auto &latest = m_session->latest();
    if (!latest)
        return {};
    const MeterReading &r = *latest;
    QJsonArray symbols;
    for (const QString &s : r.icons.names())
        symbols << s;
    QJsonObject o{
        {"time", r.receivedAt.toString(Qt::ISODateWithMs)},
        {"age_seconds", std::round(std::max<qint64>(0, r.receivedAt.msecsTo(QDateTime::currentDateTime())) / 100.0) / 10.0},
        {"mode", r.modeName},
        {"display", r.blank ? QStringLiteral("(blank: measuring)") : r.mainLine()},
        {"value", optionalNumber(r.baseValue)},
        {"unit", r.baseUnit},
        {"overload", r.mainOverload},
        {"symbols", symbols},
        {"packet", r.packet.hex()},
    };
    if (!r.sub.text.isEmpty())
        o["sub_display"] = r.sub.line();
    if (!r.sub.label.isEmpty())
        o["setup_page"] = r.sub.label;
    return o;
}

QJsonObject MeterTools::dialJson() const
{
    const int pos = m_session->dialPosition();
    QJsonObject o{{"position", pos >= 0 ? QJsonValue(positionName(pos)) : QJsonValue(QJsonValue::Null)}};
    if (pos >= 0) {
        const DialPosition &p = dial::positions()[pos];
        o["legend"] = p.legend;
        const int f = m_session->dialFunction();
        if (f >= 0 && f < p.functions.size())
            o["function"] = p.functions[f].label;
        const auto &latest = m_session->latest();
        o["jacks"] = jacksJson(latest ? dial::jacksForMode(latest->packet.mode()) : dial::jacksForPosition(pos));
    }
    return o;
}

QJsonObject MeterTools::suggestionJson() const
{
    const auto &s = m_session->suggestion();
    if (!s)
        return {};
    const DialPosition &p = dial::positions()[s->position];
    QJsonObject o{{"position", p.id}, {"legend", p.legend}, {"reason", s->reason}, {"met", m_session->suggestionMet()},
                  {"jacks", jacksJson(dial::jacksForPosition(s->position, s->function))}};
    if (s->function >= 0) {
        o["function"] = p.functions[s->function].label;
        if (m_session->dialPosition() == s->position && m_session->dialFunction() >= 0) {
            const int presses = dial::modePresses(s->position, m_session->dialFunction(), s->function);
            if (presses >= 0)
                o["mode_presses_needed"] = presses;
            else
                o["move_red_lead_to"] = dial::jackLabel(*p.functions[s->function].currentJack);
        }
    }
    return o;
}

QJsonObject MeterTools::status() const
{
    const bool connected = m_session->state() == ConnectionState::Connected;
    const auto rssi = m_session->signalStrength();
    QJsonObject o{
        {"connection", connectionStateName(m_session->state())},
        {"meter", m_session->meter() ? QJsonValue(m_session->meter()->name) : QJsonValue(QJsonValue::Null)},
        {"demo", m_session->isDemo()},
        {"signal_dbm", rssi ? QJsonValue(*rssi) : QJsonValue(QJsonValue::Null)},
        {"signal", rssi ? QJsonValue(signalWord(*rssi)) : QJsonValue(QJsonValue::Null)},
        {"battery_volts", optionalNumber(m_session->batteryVolts())},
        {"low_battery", m_session->latest() ? m_session->latest()->packet.lowBattery() : false},
        {"logging", m_session->isLogging()},
        {"logged_readings", m_session->log().count()},
        {"dial", dialJson()},
    };
    if (m_session->suggestion())
        o["dial_suggestion"] = suggestionJson();
    if (connected && m_session->latest())
        o["reading"] = readingJson();
    return o;
}

void MeterTools::call(const QString &name, const QJsonObject &args, Done done)
{
    if (name == "list_files" || name == "download_file" || name == "upload_file" || name == "get_transfer_status"
        || name == "cancel_transfer")
        return callCard(name, args, std::move(done));

    const bool connected = m_session->state() == ConnectionState::Connected;
    const QString notConnected = QStringLiteral("No meter is connected. Connect one in the 121gw-qt app first.");

    if (name == "get_status") {
        done(ok(status()));
    } else if (name == "get_reading") {
        if (!connected)
            return done(error(notConnected));
        if (!m_session->latest())
            return done(error("The meter is connected but hasn't sent a reading yet. Try again in a moment."));
        done(ok(readingJson()));
    } else if (name == "get_recent_readings") {
        const double seconds = std::clamp(args.value("seconds").toDouble(30), 1.0, 86400.0);
        const int maxPoints = std::clamp(args.value("maxPoints").toInt(300), 10, 5000);
        const auto &samples = m_session->log().samples();
        const QDateTime since = QDateTime::currentDateTime().addMSecs(static_cast<qint64>(-seconds * 1000));
        qsizetype start = samples.size();
        while (start > 0 && samples[start - 1].time >= since)
            --start;
        const QList<Sample> window = samples.mid(start);
        QJsonValue stats = QJsonValue::Null;
        if (!window.isEmpty()) {
            const MeterMode mode = window.last().mode;
            QList<Sample> inMode;
            for (const Sample &s : window)
                if (s.mode == mode)
                    inMode << s;
            stats = statsJson(MeasurementLog::statistics(inMode), inMode.last().unit);
        }
        const QList<Sample> thinned = thin(window, maxPoints);
        QJsonArray readings;
        for (const Sample &s : thinned)
            readings << sampleJson(s);
        done(ok({{"seconds", seconds}, {"logging", m_session->isLogging()}, {"count", window.size()},
                 {"thinned", thinned.size() < window.size()}, {"statistics", stats}, {"readings", readings}}));
    } else if (name == "press_button") {
        const auto key = parseButton(args.value("button").toString());
        if (!key)
            return done(error(QStringLiteral("\"%1\" isn't a meter button. Use RANGE, HOLD, REL, PEAK, MODE, MINMAX, MEM or SETUP.")
                                  .arg(args.value("button").toString())));
        const bool longPress = args.value("longPress").toBool(false);
        if (*key == MeterKey::Peak && longPress)
            return done(error("Holding 1ms PEAK turns the meter's Bluetooth off, which would end this connection. "
                              "Press it briefly instead, or turn Bluetooth off on the meter itself."));
        // Right after a file transfer the key waits for the meter's quiet second.
        const int held = m_session->stockHoldLeftMs();
        if (!m_session->sendKey(*key, longPress))
            return done(error(m_session->transferActive()
                                  ? QStringLiteral("A file transfer is running; buttons wait until it ends.")
                                  : notConnected));
        QPointer<MeterTools> self(this);
        QTimer::singleShot(settleMs + held, this, [self, done, key, longPress] {
            if (!self)
                return;
            QJsonObject o{{"button", QString::fromLatin1(keyId(*key))}, {"long_press", longPress}};
            if (self->m_session->latest())
                o["reading_after"] = self->readingJson();
            done(ok(o));
        });
    } else if (name == "get_dial") {
        QJsonArray positions;
        const auto &all = dial::positions();
        for (int i = 0; i < all.size(); ++i) {
            const DialPosition &p = all[i];
            QJsonArray fns;
            for (const DialFunction &f : p.functions) {
                if (f.currentJack)
                    fns << QJsonObject{{"function", f.label}, {"needs_jack", dial::jackLabel(*f.currentJack)}};
                else
                    fns << f.label;
            }
            QJsonObject o{{"id", p.id}, {"legend", p.legend}, {"functions", fns}};
            if (!p.redLegend.isEmpty())
                o["red_legend"] = p.redLegend;
            if (!p.isOff())
                o["jacks"] = jacksJson(dial::jacksForPosition(i));
            positions << o;
        }
        QJsonObject o{{"positions", positions}, {"current", dialJson()}};
        if (m_session->suggestion())
            o["suggestion"] = suggestionJson();
        done(ok(o));
    } else if (name == "suggest_dial_position") {
        const int pos = dial::indexOf(args.value("position").toString());
        if (pos < 0)
            return done(error(QStringLiteral("\"%1\" isn't a dial position. Use one of: lowz, v, mv, hz, ohm, va, ua, a, off.")
                                  .arg(args.value("position").toString())));
        DialSuggestion s;
        s.position = pos;
        s.reason = args.value("reason").toString().left(200);
        s.at = QDateTime::currentDateTime();
        const QString fn = args.value("function").toString().trimmed();
        if (!fn.isEmpty()) {
            const auto &fns = dial::positions()[pos].functions;
            for (int i = 0; i < fns.size(); ++i)
                if (fns[i].label.compare(fn, Qt::CaseInsensitive) == 0)
                    s.function = i;
            if (s.function < 0)
                return done(error(QStringLiteral("\"%1\" isn't a function at %2; get_dial lists them.")
                                      .arg(fn, dial::positions()[pos].id)));
        }
        m_session->suggest(s);
        const int waitMs = static_cast<int>(std::clamp(args.value("waitSeconds").toDouble(0), 0.0, 120.0) * 1000);
        auto finish = [this, done] {
            QJsonObject o{{"suggestion", suggestionJson()}, {"dial", dialJson()}};
            if (m_session->latest())
                o["reading"] = readingJson();
            done(ok(o));
        };
        if (waitMs == 0 || m_session->suggestionMet())
            return finish();
        // Wait for the knob (and function) to get there, or the time to run out.
        auto timer = new QTimer(this);
        timer->setSingleShot(true);
        auto conn = std::make_shared<QMetaObject::Connection>();
        auto end = [timer, conn, finish] {
            QObject::disconnect(*conn);
            timer->stop();
            timer->deleteLater();
            // Let a fresh reading arrive after the knob settles.
            QTimer::singleShot(600, finish);
        };
        *conn = connect(m_session, &MeterSession::suggestionChanged, this, [this, end] {
            if (m_session->suggestionMet() || !m_session->suggestion())
                end();
        });
        connect(timer, &QTimer::timeout, this, end);
        timer->start(waitMs);
    } else if (name == "set_clock") {
        if (!m_session->setClock())
            return done(error(notConnected));
        done(ok({{"set_to", QDateTime::currentDateTime().toString(Qt::ISODate)},
                 {"note", "Sent once; the meter ignores further clock commands until Bluetooth is toggled."}}));
    } else if (name == "clear_dial_suggestion") {
        m_session->clearSuggestion();
        done(ok({{"cleared", true}}));
    } else if (name == "set_logging") {
        m_session->setLogging(args.value("enabled").toBool(true));
        done(ok({{"logging", m_session->isLogging()}, {"logged_readings", m_session->log().count()}}));
    } else if (name == "clear_log") {
        m_session->clearLog();
        done(ok({{"logging", m_session->isLogging()}, {"logged_readings", m_session->log().count()}}));
    } else if (name == "save_session") {
        const auto &samples = m_session->log().samples();
        if (samples.isEmpty())
            return done(error("Nothing has been logged yet, so there's nothing to save."));
        QString title = args.value("name").toString().trimmed();
        if (title.isEmpty())
            title = sessionsummary::suggestedName(samples);
        title = title.left(80);
        try {
            const SessionInfo info = m_store->save(title, samples);
            m_session->clearLog();
            emit sessionSaved();
            done(ok(sessionJson(info)));
        } catch (const std::exception &e) {
            done(error(QStringLiteral("Couldn't save the session: %1").arg(QString::fromUtf8(e.what()))));
        }
    } else if (name == "list_sessions") {
        const int limit = std::clamp(args.value("limit").toInt(50), 1, 500);
        QJsonArray list;
        for (const SessionInfo &i : m_store->list().mid(0, limit))
            list << sessionJson(i);
        done(ok({{"sessions", list}}));
    } else if (name == "get_session") {
        const QString id = args.value("id").toString();
        const QUuid uuid = QUuid::fromString(id.size() == 32
            ? QStringLiteral("%1-%2-%3-%4-%5").arg(id.mid(0, 8), id.mid(8, 4), id.mid(12, 4), id.mid(16, 4), id.mid(20))
            : id);
        if (uuid.isNull())
            return done(error(QStringLiteral("\"%1\" isn't a session id. Use list_sessions to find one.").arg(id)));
        const QList<SessionInfo> all = m_store->list();
        const auto it = std::find_if(all.begin(), all.end(), [&](const SessionInfo &i) { return i.id == uuid; });
        if (it == all.end())
            return done(error(QStringLiteral("No saved session has the id %1.").arg(id)));
        const QList<Sample> samples = m_store->loadSamples(uuid);
        const QList<Sample> thinned = thin(samples, std::clamp(args.value("maxPoints").toInt(500), 10, 5000));
        QJsonArray readings;
        for (const Sample &s : thinned)
            readings << sampleJson(s);
        done(ok({{"session", sessionJson(*it)}, {"thinned", thinned.size() < samples.size()}, {"readings", readings}}));
    } else {
        done(error(QStringLiteral("Unknown tool: %1").arg(name)));
    }
}

// ── SD card ─────────────────────────────────────────────────────────────────

namespace {

QJsonObject fileJson(const card::CardFile &f)
{
    QJsonObject o{{"name", f.name}, {"size", f.size}};
    o["modified"] = f.modified.isValid() ? QJsonValue(f.modified.toString(Qt::ISODate)) : QJsonValue(QJsonValue::Null);
    if (f.directory)
        o["directory"] = true;
    if (card::isProtected(f.name))
        o["note"] = "calibration backup: read-only over Bluetooth";
    else if (f.name == card::FirmwareFile)
        o["note"] = "firmware image: the meter flashes it on its next firmware update procedure";
    return o;
}

QJsonObject resultJson(const CardTransfer::Result &r)
{
    QJsonObject o{{"operation", CardTransfer::operationName(r.operation)}, {"ok", r.ok}, {"message", r.message},
                  {"seconds", std::round(r.seconds * 10) / 10}};
    if (!r.name.isEmpty())
        o["name"] = r.name;
    if (r.canceled)
        o["canceled"] = true;
    if (r.operation != CardTransfer::Operation::List)
        o["bytes"] = r.bytes;
    return o;
}

bool looksLikeText(const QByteArray &data)
{
    for (const char c : data) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 && c != '\r' && c != '\n' && c != '\t')
            return false;
    }
    return true;
}

// One answer per tool call: when the operation ends, or after the wait runs out (it goes on).
struct Waiter {
    std::function<void(const ToolResult &)> done;
    bool answered = false;
    void answer(const ToolResult &r)
    {
        if (answered)
            return;
        answered = true;
        done(r);
    }
};

} // namespace

void MeterTools::setCard(CardTransfer *card)
{
    m_card = card;
    // Operations started in the app show up in get_transfer_status too (a tool's own callback,
    // which runs after this, adds what it knows, such as where a download was saved).
    connect(card, &CardTransfer::finished, this, [this](const CardTransfer::Result &r) { m_lastTransfer = resultJson(r); });
}

QJsonObject MeterTools::transferJson() const
{
    QJsonObject o{{"busy", m_card->busy()}};
    if (m_card->busy()) {
        const qint64 total = m_card->bytesTotal();
        o["operation"] = CardTransfer::operationName(m_card->operation());
        o["phase"] = CardTransfer::phaseName(m_card->phase());
        o["name"] = m_card->fileName();
        o["bytes_done"] = m_card->bytesDone();
        o["bytes_total"] = total >= 0 ? QJsonValue(total) : QJsonValue(QJsonValue::Null);
        o["percent"] = total > 0 ? QJsonValue(std::round(m_card->bytesDone() * 1000.0 / total) / 10) : QJsonValue(QJsonValue::Null);
        o["bytes_per_second"] = std::round(m_card->bytesPerSecond());
        o["seconds_left"] = m_card->secondsLeft() >= 0 ? QJsonValue(m_card->secondsLeft()) : QJsonValue(QJsonValue::Null);
        o["seconds_elapsed"] = std::round(m_card->secondsElapsed());
        if (m_card->phase() == CardTransfer::Phase::Confirming) {
            o["confirm_on_meter"] = true;
            o["prompt"] = QStringLiteral("The meter is waiting for the user to press SETUP to accept %1 (30 seconds; any "
                                         "other key refuses).").arg(m_card->fileName());
        }
    }
    if (!m_lastTransfer.isEmpty())
        o["last"] = m_lastTransfer;
    return o;
}

void MeterTools::callCard(const QString &name, const QJsonObject &args, Done done)
{
    if (!m_card)
        return done(error("File transfer isn't available in this build."));
    const int waitMs = static_cast<int>(std::clamp(args.value("waitSeconds").toDouble(120), 1.0, 900.0) * 1000);

    if (name == "get_transfer_status")
        return done(ok(transferJson()));
    if (name == "cancel_transfer") {
        if (!m_card->busy())
            return done(error("No file transfer is running."));
        m_card->cancel();
        return done(ok(transferJson()));
    }
    if (m_session->state() != ConnectionState::Connected)
        return done(error("No meter is connected. Connect one in the 121gw-qt app first."));
    if (m_card->busy())
        return done(error(QStringLiteral("A file %1 is already running%2. Wait for it, or cancel_transfer.")
                              .arg(CardTransfer::operationName(m_card->operation()),
                                   m_card->fileName().isEmpty() ? QString() : QStringLiteral(" (%1)").arg(m_card->fileName()))));

    QPointer<MeterTools> self(this);
    auto waiter = std::make_shared<Waiter>(Waiter{done});

    if (name == "list_files") {
        const QString e = m_card->list([self, waiter](const CardTransfer::Result &r) {
            if (!self)
                return;
            if (!r.ok)
                return waiter->answer(error(r.message));
            QJsonArray files;
            for (const card::CardFile &f : r.files)
                files << fileJson(f);
            self->m_lastTransfer = resultJson(r);
            waiter->answer(ok({{"files", files}, {"count", files.size()}}));
        });
        if (!e.isEmpty())
            done(error(e));
        return;
    }

    if (name == "download_file") {
        const QString file = card::normalizeName(args.value("name").toString());
        QString why;
        if (!card::isValidName(file, &why))
            return done(error(why));
        QString target = args.value("saveTo").toString().trimmed();
        if (target.isEmpty()) {
            const QString dir = downloadDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                                                      : downloadDir;
            QDir().mkpath(dir);
            target = QDir(dir).filePath(file);
        }
        if (!QFileInfo(target).isAbsolute())
            return done(error("saveTo must be an absolute path."));
        if (!QFileInfo(QFileInfo(target).absolutePath()).isDir())
            return done(error(QStringLiteral("The folder %1 doesn't exist.").arg(QFileInfo(target).absolutePath())));
        if (QFileInfo::exists(target) && !args.value("overwriteLocal").toBool(false))
            return done(error(QStringLiteral("%1 already exists. Pass overwriteLocal true to replace it, or another saveTo.")
                                  .arg(target)));
        const QString e = m_card->download(file, [self, waiter, target](const CardTransfer::Result &r) {
            if (!self)
                return;
            QJsonObject o = resultJson(r);
            if (r.ok) {
                QSaveFile out(target);
                if (!out.open(QIODevice::WriteOnly) || out.write(r.data) != r.data.size() || !out.commit()) {
                    o["ok"] = false;
                    o["message"] = QStringLiteral("Downloaded, but couldn't save %1: %2").arg(target, out.errorString());
                } else {
                    o["saved_to"] = target;
                    o["size"] = r.data.size();
                }
            }
            self->m_lastTransfer = o;
            if (!o["ok"].toBool())
                return waiter->answer(error(o["message"].toString()));
            QJsonObject answer = o;
            if (r.data.size() <= 32 * 1024 && looksLikeText(r.data))
                answer["text"] = QString::fromLatin1(r.data);
            waiter->answer(ok(answer));
        });
        if (!e.isEmpty())
            return done(error(e));
        QTimer::singleShot(waitMs, this, [self, waiter] {
            if (!self || waiter->answered)
                return;
            QJsonObject o = self->transferJson();
            o["in_progress"] = true;
            o["note"] = "Still downloading; it is saved when done. get_transfer_status reports it.";
            waiter->answer(ok(o));
        });
        return;
    }

    if (name == "upload_file") {
        const QString path = args.value("path").toString().trimmed();
        if (!QFileInfo(path).isAbsolute())
            return done(error("path must be an absolute path to a file on this computer."));
        QFile in(path);
        if (!QFileInfo(path).isFile() || !in.open(QIODevice::ReadOnly))
            return done(error(QStringLiteral("Can't read %1.").arg(path)));
        if (in.size() > 4 * 1024 * 1024)
            return done(error("That file is over 4 MB: at about 190 bytes per second it would take hours."));
        const QByteArray data = in.readAll();
        const QString file = card::normalizeName(args.value("name").toString().trimmed().isEmpty()
                                                     ? card::suggestName(path) : args.value("name").toString());
        QString why;
        if (!card::isValidName(file, &why))
            return done(error(why));
        if (card::isRefused(file, &why))
            return done(error(why));
        if (file == card::FirmwareFile && !args.value("firmwareUpdate").toBool(false))
            return done(error("EEVBLOG.BIN is the firmware image the meter flashes on its next firmware update procedure. "
                              "Only upload one the user chose, and pass firmwareUpdate true; the user must also press "
                              "SETUP on the meter to accept it."));
        const bool overwrite = args.value("overwrite").toBool(false);
        // Ask the card first, so an existing file is never replaced without overwrite.
        const QString e = m_card->list([self, waiter, file, data, overwrite, waitMs](const CardTransfer::Result &r) {
            if (!self)
                return;
            if (!r.ok)
                return waiter->answer(error(r.message));
            self->startUpload(file, data, overwrite, waitMs, [waiter](const ToolResult &t) { waiter->answer(t); });
        });
        if (!e.isEmpty())
            done(error(e));
        return;
    }
    done(error(QStringLiteral("Unknown tool: %1").arg(name)));
}

void MeterTools::startUpload(const QString &file, const QByteArray &data, bool overwrite, int waitMs, Done done)
{
    const bool exists = m_card->find(file).has_value();
    if (exists && !overwrite)
        return done(error(QStringLiteral("%1 already exists on the card. Pass overwrite true to replace it; the user then "
                                         "has to press SETUP on the meter to accept.").arg(file)));
    QPointer<MeterTools> self(this);
    auto waiter = std::make_shared<Waiter>(Waiter{std::move(done)});
    const QString e = m_card->upload(file, data, [self, waiter](const CardTransfer::Result &r) {
        if (!self)
            return;
        self->m_lastTransfer = resultJson(r);
        if (!r.ok)
            return waiter->answer(error(r.message));
        waiter->answer(ok(self->m_lastTransfer));
    });
    if (!e.isEmpty())
        return waiter->answer(error(e));
    QTimer::singleShot(waitMs, this, [self, waiter] {
        if (!self || waiter->answered)
            return;
        QJsonObject o = self->transferJson();
        o["in_progress"] = true;
        o["note"] = "Still uploading; get_transfer_status reports it, cancel_transfer stops it.";
        waiter->answer(ok(o));
    });
}

} // namespace readout
