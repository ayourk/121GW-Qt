// SPDX-License-Identifier: MIT
// The MCP server end to end: real HTTP on 127.0.0.1, the demo meter behind it (its SD card too).
#include "cardtransfer.h"
#include "dial.h"
#include "mcpserver.h"
#include "metersession.h"
#include "metertools.h"
#include "sessionstore.h"
#include "simulatedconnection.h"

#include <QEventLoop>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace readout;

// A Bluetooth stand-in that never finds anything.
class NoBluetooth : public MeterConnection
{
public:
    using MeterConnection::MeterConnection;
    void startScan() override {}
    void stopScan() override {}
    void connectTo(const DiscoveredMeter &) override {}
    void disconnectFrom() override {}
    void sendKey(MeterKey, bool) override {}
    bool sendCommand(const QByteArray &) override { return false; }
};

// A connected meter with the stock firmware: takes commands, never answers a card command.
class StockMeter : public MeterConnection
{
public:
    using MeterConnection::MeterConnection;
    void startScan() override {}
    void stopScan() override {}
    void connectTo(const DiscoveredMeter &) override { setState(ConnectionState::Connected); }
    void disconnectFrom() override { setState(ConnectionState::Disconnected); }
    void sendKey(MeterKey, bool) override {}
    bool sendCommand(const QByteArray &) override { return true; }
};

class TstMcp : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    NoBluetooth *m_ble = nullptr;
    SimulatedConnection *m_demo = nullptr;
    MeterSession *m_session = nullptr;
    SessionStore *m_store = nullptr;
    MeterTools *m_tools = nullptr;
    CardTransfer *m_card = nullptr;
    McpServer *m_server = nullptr;
    QNetworkAccessManager m_net;
    const QByteArray m_token = "test-token";
    int m_id = 0;

    struct Reply {
        int status = 0;
        QJsonObject json;
    };

    Reply post(const QJsonObject &body, const QByteArray &token, const QList<QPair<QByteArray, QByteArray>> &headers = {},
               const QByteArray &verb = "POST")
    {
        QNetworkRequest req(QUrl(m_server->endpoint()));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Accept", "application/json, text/event-stream");
        if (!token.isEmpty())
            req.setRawHeader("Authorization", "Bearer " + token);
        for (const auto &h : headers)
            req.setRawHeader(h.first, h.second);
        QNetworkReply *r = m_net.sendCustomRequest(req, verb, QJsonDocument(body).toJson());
        QEventLoop loop;
        connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        loop.exec();
        Reply out;
        out.status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        out.json = QJsonDocument::fromJson(r->readAll()).object();
        r->deleteLater();
        return out;
    }

    QJsonObject rpc(const QString &method, const QJsonObject &params = {})
    {
        const Reply r = post({{"jsonrpc", "2.0"}, {"id", ++m_id}, {"method", method}, {"params", params}}, m_token);
        if (r.status != 200)
            qWarning() << "status" << r.status;
        return r.json;
    }

    // A tool's result: its JSON text parsed, and whether it was an error.
    std::pair<QJsonObject, bool> call(const QString &tool, const QJsonObject &args = {})
    {
        const QJsonObject result = rpc("tools/call", {{"name", tool}, {"arguments", args}}).value("result").toObject();
        const QString text = result.value("content").toArray().at(0).toObject().value("text").toString();
        const bool isError = result.value("isError").toBool();
        return {isError ? QJsonObject{{"error", text}} : QJsonDocument::fromJson(text.toUtf8()).object(), isError};
    }

    static void wait(int ms) { QTest::qWait(ms); }

private slots:
    void initTestCase()
    {
        m_ble = new NoBluetooth(this);
        m_demo = new SimulatedConnection(this);
        m_session = new MeterSession(m_ble, m_demo, this);
        m_store = new SessionStore(m_dir.filePath("sessions"));
        m_tools = new MeterTools(m_session, m_store, this);
        m_tools->settleMs = 300;
        m_card = new CardTransfer(m_session, this);
        m_card->replyTimeoutMs = 400;
        m_tools->setCard(m_card);
        m_tools->downloadDir = m_dir.filePath("downloads");
        m_demo->card().bytesPerSecond = 0;     // at once; one test sets a speed
        m_demo->card().confirmDelayMs = 300;   // the user presses SETUP after 0.3 s
        m_server = new McpServer(m_tools, "test", this);
        QVERIFY(m_server->start(0, m_token));
        m_session->startDemo();
        wait(600);   // a few packets
    }

    void cleanupTestCase() { delete m_store; }

    void refusesWithoutTheToken()
    {
        const QJsonObject ping{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}};
        QCOMPARE(post(ping, {}).status, 401);
        QCOMPARE(post(ping, "wrong").status, 401);
    }

    void refusesOtherHostsAndOrigins()
    {
        const QJsonObject ping{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}};
        QCOMPARE(post(ping, m_token, {{"Host", "evil.example:47121"}}).status, 403);
        QCOMPARE(post(ping, m_token, {{"Origin", "https://evil.example"}}).status, 403);
        QCOMPARE(post(ping, m_token, {{"Origin", "http://localhost:3000"}}).status, 200);
        QCOMPARE(post(ping, m_token, {}, "GET").status, 405);
    }

    void initializeAndListTools()
    {
        const QJsonObject init = rpc("initialize", {{"protocolVersion", "2025-03-26"}, {"capabilities", QJsonObject{}},
                                                     {"clientInfo", QJsonObject{{"name", "test"}, {"version", "1"}}}});
        QCOMPARE(init["result"].toObject()["protocolVersion"].toString(), QStringLiteral("2025-03-26"));
        QCOMPARE(init["result"].toObject()["serverInfo"].toObject()["name"].toString(), QStringLiteral("121gw-qt"));
        const QJsonObject notified = post({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}, m_token).json;
        QVERIFY(notified.isEmpty());

        QStringList names;
        for (const QJsonValue &t : rpc("tools/list")["result"].toObject()["tools"].toArray())
            names << t.toObject()["name"].toString();
        for (const char *expected : {"get_status", "get_reading", "get_recent_readings", "press_button", "get_dial",
                                     "suggest_dial_position", "clear_dial_suggestion", "set_clock", "set_logging",
                                     "clear_log", "save_session", "list_sessions", "get_session", "list_files",
                                     "download_file", "upload_file", "get_transfer_status", "cancel_transfer"})
            QVERIFY2(names.contains(expected), expected);
        QCOMPARE(rpc("nope")["error"].toObject()["code"].toInt(), -32601);
    }

    void readsTheDemoMeter()
    {
        const auto [reading, isError] = call("get_reading");
        QVERIFY(!isError);
        QCOMPARE(reading["mode"].toString(), QStringLiteral("DC voltage"));
        QCOMPARE(reading["unit"].toString(), QStringLiteral("V"));
        QVERIFY(reading["value"].toDouble() > 11 && reading["value"].toDouble() < 13);
        QCOMPARE(reading["packet"].toString().size(), 38);

        const auto [status, e2] = call("get_status");
        QCOMPARE(status["connection"].toString(), QStringLiteral("Connected"));
        QVERIFY(status["demo"].toBool());
        QCOMPARE(status["dial"].toObject()["position"].toString(), QStringLiteral("v"));
        QCOMPARE(status["dial"].toObject()["function"].toString(), QStringLiteral("DC"));
    }

    void refusesHoldingPeak()
    {
        const auto [r, isError] = call("press_button", {{"button", "1ms PEAK"}, {"longPress", true}});
        QVERIFY(isError);
        QVERIFY(r["error"].toString().contains("Bluetooth"));
        QVERIFY(call("press_button", {{"button", "TURBO"}}).second);
    }

    void modeStepsWithinThePosition()
    {
        const auto [r, isError] = call("press_button", {{"button", "mode"}});
        QVERIFY(!isError);
        QCOMPARE(r["button"].toString(), QStringLiteral("MODE"));
        QCOMPARE(r["reading_after"].toObject()["mode"].toString(), QStringLiteral("AC voltage"));
        QCOMPARE(m_session->dialFunction(), 1);
    }

    void suggestionIsMetWhileWaiting()
    {
        // The user (here: the test) turns to A·mA and plugs into mA µA while the tool waits.
        QTimer::singleShot(400, this, [this] {
            m_session->turnDemoDial(dial::indexOf("a"));
            m_session->plugDemoLead(Jack::MilliMicro);
        });
        const auto [r, isError] = call("suggest_dial_position",
                                       {{"position", "a"}, {"function", "DC mA"}, {"reason", "Arduino sleep current"},
                                        {"waitSeconds", 5}});
        QVERIFY(!isError);
        QVERIFY(r["suggestion"].toObject()["met"].toBool());
        QCOMPARE(r["dial"].toObject()["position"].toString(), QStringLiteral("a"));
        QCOMPARE(r["dial"].toObject()["function"].toString(), QStringLiteral("DC mA"));
        const QJsonArray jacks = r["suggestion"].toObject()["jacks"].toArray();
        QCOMPARE(jacks.at(0).toObject()["jack"].toString(), QStringLiteral("mA µA"));

        // Asking for AC A from DC mA: MODE can't get there; the lead has to move.
        call("suggest_dial_position", {{"position", "a"}, {"function", "AC A"}});
        const auto [dialInfo, e2] = call("get_dial");
        QCOMPARE(dialInfo["suggestion"].toObject()["move_red_lead_to"].toString(), QStringLiteral("A 500mA"));
        QVERIFY(call("suggest_dial_position", {{"position", "q"}}).second);
        QVERIFY(call("suggest_dial_position", {{"position", "a"}, {"function", "Resistance"}}).second);
        call("clear_dial_suggestion");
        QVERIFY(!m_session->suggestion());
    }

    void savesAndReadsBackASession()
    {
        wait(500);
        QVERIFY(m_session->log().count() > 0);
        const auto [saved, isError] = call("save_session", {{"name", "test run"}});
        QVERIFY(!isError);
        QVERIFY(saved["readings"].toInt() >= 3);
        // The log restarted; the demo meter's next packet may already be in it.
        QVERIFY(m_session->log().count() <= 2);
        const auto [list, e2] = call("list_sessions");
        QCOMPARE(list["sessions"].toArray().size(), 1);
        const QString id = list["sessions"].toArray().at(0).toObject()["id"].toString();
        QCOMPARE(id.size(), 32);
        const auto [detail, e3] = call("get_session", {{"id", id}, {"maxPoints", 10}});
        QVERIFY(!e3);
        QVERIFY(detail["readings"].toArray().size() <= 10);
        QVERIFY(call("get_session", {{"id", "x"}}).second);
    }

    // ── the SD card ─────────────────────────────────────────────────────────
    QString writeLocal(const QString &name, const QByteArray &data)
    {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(data);
        return path;
    }

    static QStringList names(const QJsonObject &listing)
    {
        QStringList n;
        for (const QJsonValue &f : listing["files"].toArray())
            n << f.toObject()["name"].toString();
        return n;
    }

    void listsTheCard()
    {
        const auto [r, isError] = call("list_files");
        QVERIFY2(!isError, qPrintable(r["error"].toString()));
        QCOMPARE(names(r), (QStringList{"26110200.CSV", "26110201.CSV", "CAL.BIN"}));
        QCOMPARE(r["files"].toArray().at(2).toObject()["size"].toInt(), 1200);
        QVERIFY(r["files"].toArray().at(2).toObject()["note"].toString().contains("read-only"));
        QVERIFY(!r["files"].toArray().at(0).toObject()["modified"].isNull());
    }

    void lostRepliesAreAskedAgain()
    {
        // The reply to the second LIST is lost: the app repeats it with the same SEQ, and the
        // meter answers the repeat without moving on.
        m_demo->card().dropReplies = 1;
        const auto [r, isError] = call("list_files");
        QVERIFY2(!isError, qPrintable(r["error"].toString()));
        QCOMPARE(r["files"].toArray().size(), 3);
        QCOMPARE(m_demo->card().dropReplies, 0);
    }

    void downloadsWith1KBlocks()
    {
        m_card->download1K = true;
        const auto [r, isError] = call("download_file", {{"name", "26110200.CSV"}, {"saveTo", m_dir.filePath("1k.csv")}});
        m_card->download1K = false;
        QVERIFY2(!isError, qPrintable(r["error"].toString()));
        QCOMPARE(r["text"].toString().toLatin1(), m_demo->card().files["26110200.CSV"].data);
    }

    void downloadsALog()
    {
        const auto [r, isError] = call("download_file", {{"name", "26110200.csv"}});
        QVERIFY2(!isError, qPrintable(r["error"].toString()));
        QVERIFY(r["ok"].toBool());
        const QString saved = r["saved_to"].toString();
        QCOMPARE(saved, QDir(m_dir.filePath("downloads")).filePath("26110200.CSV"));
        QFile f(saved);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray data = f.readAll();
        QVERIFY(data == m_demo->card().files["26110200.CSV"].data);   // exact size, no padding
        QVERIFY(r["text"].toString().startsWith("START,2026/11/02,15:00:03,"));
        // The readings came back after the transfer (status packets pause during one).
        QVERIFY(!m_session->transferActive());

        const auto [again, e2] = call("download_file", {{"name", "26110200.CSV"}});
        QVERIFY(e2);
        QVERIFY(again["error"].toString().contains("already exists"));
        QVERIFY(call("download_file", {{"name", "NOPE.CSV"}, {"saveTo", m_dir.filePath("nope.csv")}}).second);
        QVERIFY(call("download_file", {{"name", "not a name.csv"}}).second);
        QVERIFY(call("download_file", {{"name", "26110201.CSV"}, {"saveTo", "relative.csv"}}).second);
    }

    void uploadsAndReadsBack()
    {
        QByteArray data;
        for (int i = 0; i < 300; ++i)
            data += QByteArray::number(i * 7) + ",";
        const QString path = writeLocal("my notes.txt", data);
        const auto [r, isError] = call("upload_file", {{"path", path}});
        QVERIFY2(!isError, qPrintable(r["error"].toString()));
        QCOMPARE(r["name"].toString(), QStringLiteral("MYNOTES.TXT"));
        QVERIFY(m_demo->card().files.contains("MYNOTES.TXT"));
        QVERIFY(m_demo->card().files["MYNOTES.TXT"].data == data);
        QVERIFY(m_card->find("MYNOTES.TXT"));   // the listing knows without asking again

        const auto [back, e2] = call("download_file", {{"name", "MYNOTES.TXT"}, {"saveTo", m_dir.filePath("back.txt")}});
        QVERIFY(!e2);
        QCOMPARE(back["text"].toString().toLatin1(), data);

        // An empty file.
        const auto [empty, e3] = call("upload_file", {{"path", writeLocal("empty.dat", {})}});
        QVERIFY2(!e3, qPrintable(empty["error"].toString()));
        QVERIFY(m_demo->card().files["EMPTY.DAT"].data.isEmpty());
    }

    void overwriteNeedsTheMeter()
    {
        const QString path = writeLocal("v2.txt", "second version");
        // Not without overwrite.
        const auto [refused, e1] = call("upload_file", {{"path", path}, {"name", "MYNOTES.TXT"}});
        QVERIFY(e1);
        QVERIFY(refused["error"].toString().contains("overwrite"));
        QVERIFY(m_demo->card().files["MYNOTES.TXT"].data != "second version");

        // With it: the meter asks; the status shows it while waiting.
        m_demo->card().confirmDelayMs = 2500;
        const auto [waiting, e2] = call("upload_file", {{"path", path}, {"name", "mynotes.txt"}, {"overwrite", true},
                                                        {"waitSeconds", 1}});
        QVERIFY2(!e2, qPrintable(waiting["error"].toString()));
        QVERIFY(waiting["in_progress"].toBool());
        QCOMPARE(waiting["phase"].toString(), QStringLiteral("confirm_on_meter"));
        QVERIFY(waiting["confirm_on_meter"].toBool());
        QVERIFY(waiting["prompt"].toString().contains("SETUP"));
        QVERIFY(m_demo->card().awaitingConfirmation());
        // Keys would land in the middle of the transfer.
        QVERIFY(call("press_button", {{"button", "HOLD"}}).second);
        QTRY_VERIFY_WITH_TIMEOUT(!m_card->busy(), 5000);
        const auto [status, e3] = call("get_transfer_status");
        QVERIFY(status["last"].toObject()["ok"].toBool());
        QCOMPARE(m_demo->card().files["MYNOTES.TXT"].data, QByteArray("second version"));
        m_demo->card().confirmDelayMs = 300;

        // The user doesn't press SETUP: the meter refuses with CAN.
        m_demo->card().rejectConfirmations = true;
        const auto [rejected, e4] = call("upload_file", {{"path", writeLocal("v3.txt", "third")}, {"name", "MYNOTES.TXT"},
                                                         {"overwrite", true}});
        QVERIFY(e4);
        QVERIFY(rejected["error"].toString().contains("SETUP"));
        QCOMPARE(m_demo->card().files["MYNOTES.TXT"].data, QByteArray("second version"));
        m_demo->card().rejectConfirmations = false;
    }

    void abortsThePrompt()
    {
        // While its prompt is up the meter hears only ABORT; cancel sends it.
        m_demo->card().confirmDelayMs = 20000;
        const auto [waiting, e1] = call("upload_file", {{"path", writeLocal("v4.txt", "fourth")}, {"name", "MYNOTES.TXT"},
                                                        {"overwrite", true}, {"waitSeconds", 1}});
        QVERIFY2(!e1, qPrintable(waiting["error"].toString()));
        QVERIFY(m_demo->card().awaitingConfirmation());
        QVERIFY(!call("cancel_transfer").second);
        QTRY_VERIFY_WITH_TIMEOUT(!m_demo->card().transferActive(), 3000);
        QVERIFY(!m_demo->card().awaitingConfirmation());
        QCOMPARE(m_demo->card().files["MYNOTES.TXT"].data, QByteArray("second version"));
        m_demo->card().confirmDelayMs = 300;
        // The next command works (the meter isn't left in a transfer).
        QVERIFY(!call("list_files").second);
    }

    void protectsCalibrationAndFirmware()
    {
        const QString bin = writeLocal("fw.bin", QByteArray(5000, '\x5A'));
        const auto [cal, e1] = call("upload_file", {{"path", bin}, {"name", "cal.bin"}, {"overwrite", true}});
        QVERIFY(e1);
        QVERIFY(cal["error"].toString().contains("CAL.BIN"));
        QCOMPARE(m_demo->card().files["CAL.BIN"].data.size(), 1200);
        // Even past the tool's check, the card itself refuses it.
        QVERIFY(!m_card->upload("CAL.BIN", "x").isEmpty());

        const auto [fw, e2] = call("upload_file", {{"path", bin}, {"name", "EEVBlog.bin"}});
        QVERIFY(e2);
        QVERIFY(fw["error"].toString().contains("firmwareUpdate"));
        const auto [ok, e3] = call("upload_file", {{"path", bin}, {"name", "EEVBlog.bin"}, {"firmwareUpdate", true}});
        QVERIFY2(!e3, qPrintable(ok["error"].toString()));
        QVERIFY(ok["message"].toString().contains("next time"));
        QCOMPARE(m_demo->card().files["EEVBLOG.BIN"].data.size(), 5000);
        QVERIFY(call("upload_file", {{"path", "relative.bin"}}).second);
        QVERIFY(call("upload_file", {{"path", bin}, {"name", "BTRECV.TMP"}}).second);
        QVERIFY(!m_demo->card().files.contains("BTRECV.TMP"));
        QVERIFY(call("upload_file", {{"path", bin}, {"name", "far too long.bin"}}).second);
    }

    void cancelsAndReportsProgress()
    {
        m_demo->card().bytesPerSecond = 4000;   // paced: about three seconds for this file
        const QString path = writeLocal("big.bin", QByteArray(12000, '\x11'));
        const auto [started, e1] = call("upload_file", {{"path", path}, {"waitSeconds", 1}});
        QVERIFY2(!e1, qPrintable(started["error"].toString()));
        QVERIFY(started["in_progress"].toBool());
        QVERIFY(started["bytes_done"].toInt() > 0);
        QCOMPARE(started["bytes_total"].toInt(), 12000);
        QVERIFY(started["bytes_per_second"].toDouble() > 0);
        QVERIFY(!started["seconds_left"].isNull());
        const auto [busy, e2] = call("list_files");
        QVERIFY(e2);
        QVERIFY(busy["error"].toString().contains("already running"));
        const auto [canceled, e3] = call("cancel_transfer");
        QVERIFY(!e3);
        QVERIFY(canceled["last"].toObject()["canceled"].toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!m_demo->card().transferActive(), 5000);   // the meter got CAN CAN
        QVERIFY(!m_demo->card().files.contains("BIG.BIN"));
        QVERIFY(call("cancel_transfer").second);
        m_demo->card().bytesPerSecond = 0;
        wait(300);
        QVERIFY(m_session->latest());   // readings again
    }

    void repeatsAreCachedOnlyForTheTransferStart()
    {
        // The meter's rule, on a card of its own: a repeated LIST runs again; a repeat of the
        // frame that started the running transfer gets its reply again and starts nothing.
        SimulatedCard card;
        card.bytesPerSecond = 0;
        QByteArray out;
        connect(&card, &SimulatedCard::transmit, this, [&out](const QByteArray &b) { out += b; });
        card::FrameParser replies;
        const auto ask = [&](const QByteArray &frame) {
            out.clear();
            card.receive(frame);
            wait(50);
            return replies.push(out);
        };
        const QByteArray list0 = card::commandFrame(card::Command::List, 5, card::listPayload(0));
        const auto first = ask(list0);
        QCOMPARE(first.size(), 1);
        QCOMPARE(card::decodeEntry(first[0].payload)->name, QStringLiteral("26110200.CSV"));
        card.files.remove("26110200.CSV");
        const auto again = ask(list0);
        QCOMPARE(card::decodeEntry(again[0].payload)->name, QStringLiteral("26110201.CSV"));   // run again

        const QByteArray receive = card::commandFrame(card::Command::Receive, 9);
        auto started = ask(receive);
        QCOMPARE(started.size(), 1);
        QCOMPARE(started[0].status(), card::Status::Ok);
        QVERIFY(card.transferActive());
        QVERIFY(out.endsWith('C'));   // the receiver asks for block 0
        const auto repeat = ask(receive);
        QCOMPARE(repeat.size(), 1);
        QCOMPARE(repeat[0].seq, quint8(9));
        QCOMPARE(repeat[0].status(), card::Status::Ok);
        QVERIFY(!out.contains('C'));   // the cached reply only: the receiver didn't start over
        const auto busy = ask(card::commandFrame(card::Command::List, 10, card::listPayload(0)));
        QCOMPARE(busy[0].status(), card::Status::Busy);
        const auto aborted = ask(card::commandFrame(card::Command::Abort, 11));
        QCOMPARE(aborted[0].status(), card::Status::Ok);
        QVERIFY(out.endsWith(QByteArray(3, ymodem::CAN)));
        QVERIFY(!card.transferActive());
        // After the transfer the same RECEIVE frame is a new command again.
        QCOMPARE(ask(receive)[0].status(), card::Status::Ok);
        QVERIFY(out.endsWith('C'));
        ask(card::commandFrame(card::Command::Abort, 12));
    }

    void lostStartReplyDoesNotRestart()
    {
        // RECEIVE's OK is lost: the app repeats the frame, the meter answers from its cache, and
        // the upload goes through.
        // (Straight to the upload: upload_file would list first, and that reply would be the lost one.)
        m_demo->card().dropReplies = 1;
        std::optional<CardTransfer::Result> result;
        QVERIFY(m_card->upload("LOSTOK.TXT", "lost reply", [&result](const CardTransfer::Result &r) { result = r; }).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), 15000);
        QVERIFY2(result->ok, qPrintable(result->message));
        QCOMPARE(m_demo->card().dropReplies, 0);
        QCOMPARE(m_demo->card().files["LOSTOK.TXT"].data, QByteArray("lost reply"));
    }

    void keysWaitForTheQuietSecond()
    {
        // Right after a transfer the meter drops key frames until its line has been quiet for a
        // second: the app holds the key until then instead of losing it.
        wait(1600);
        const auto [r, isError] = call("upload_file", {{"path", writeLocal("q.txt", "quiet")}, {"name", "QUIET.TXT"}});
        QVERIFY2(!isError, qPrintable(r["error"].toString()));
        QVERIFY(m_demo->card().inQuietPeriod());
        QVERIFY(m_session->stockHoldLeftMs() > 0);
        const MeterMode before = m_demo->simulator().currentMode();
        const int dropped = m_demo->droppedKeys();
        QVERIFY(m_session->sendKey(MeterKey::Mode));
        wait(200);
        QCOMPARE(m_demo->simulator().currentMode(), before);   // not yet
        QTRY_VERIFY_WITH_TIMEOUT(m_demo->simulator().currentMode() != before, 3000);
        QCOMPARE(m_demo->droppedKeys(), dropped);   // and not lost
        QVERIFY(!m_demo->card().inQuietPeriod());
    }

    void stockFirmwareDoesNotAnswer()
    {
        StockMeter stock;
        SimulatedConnection demo;
        MeterSession session(&stock, &demo);
        stock.connectTo({});
        CardTransfer card(&session);
        card.replyTimeoutMs = 200;   // four tries
        std::optional<CardTransfer::Result> result;
        QVERIFY(card.list([&result](const CardTransfer::Result &r) { result = r; }).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), 3000);
        QVERIFY(!result->ok);
        QVERIFY(result->message.contains("custom"));
        QVERIFY(result->seconds >= 0.75);   // it asked four times
        QVERIFY(!stock.rawMode());
    }

    void reportsNoMeterWhenDisconnected()
    {
        m_session->disconnectFrom();
        QVERIFY(call("get_reading").second);
        QVERIFY(call("press_button", {{"button", "HOLD"}}).second);
        QVERIFY(call("list_files").second);
    }
};

QTEST_GUILESS_MAIN(TstMcp)
#include "tst_mcp.moc"
