// SPDX-License-Identifier: MIT
// 121gw-qt: a Qt 6 companion for the EEVblog 121GW multimeter: the whole front panel live (LCD,
// dial, buttons, jacks), remote buttons, logging, and an MCP server so an AI assistant can read
// the meter and guide its user. Ported from Readout (https://github.com/amd989/readout, MIT).
#include "bleconnection.h"
#include "cardtransfer.h"
#include "facelayout.h"
#include "lcditem.h"
#include "mcpserver.h"
#include "metercontroller.h"
#include "metersession.h"
#include "metertools.h"
#include "sessionstore.h"
#include "simulatedconnection.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QtQml>

using namespace readout;

namespace {

QString configDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath("121gw-qt");
}

QString dataDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath("121gw-qt");
}

QByteArray newToken()
{
    QByteArray bytes(32, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(bytes.data()), bytes.size() / 4);
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

// Copies the built-in QML to dir, to start a customized front panel from.
int exportQml(const QString &dir)
{
    QDir().mkpath(dir);
    QDirIterator it(QStringLiteral(":/qml"), QDirIterator::Subdirectories);
    int count = 0;
    while (it.hasNext()) {
        const QString src = it.next();
        if (QFileInfo(src).isDir())
            continue;
        const QString dst = QDir(dir).filePath(src.mid(QStringLiteral(":/qml/").size()));
        QDir().mkpath(QFileInfo(dst).path());
        if (QFile::exists(dst)) {
            QTextStream(stderr) << "exists, left alone: " << dst << '\n';
            continue;
        }
        QFile::copy(src, dst);
        QFile::setPermissions(dst, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
        ++count;
    }
    QTextStream(stdout) << "copied " << count << " files to " << dir << '\n';
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);   // Widgets only for the platform file dialogs
    QGuiApplication::setApplicationName("121gw-qt");
    QGuiApplication::setApplicationVersion(QStringLiteral(GW121_VERSION));
    QGuiApplication::setDesktopFileName("121gw-qt");
    QGuiApplication::setWindowIcon(QIcon(":/icons/121gw-qt.svg"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Live front panel, remote buttons, logging and an MCP server for the EEVblog 121GW multimeter.");
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption demoOpt("demo", "Start with the demo meter (no Bluetooth).");
    const QCommandLineOption noMcpOpt("no-mcp", "Don't start the MCP server.");
    const QCommandLineOption portOpt("mcp-port", "MCP server port on 127.0.0.1 (default 47121).", "port");
    const QCommandLineOption qmlOpt("qml", "Load the front panel from this directory's main.qml instead of "
                                           "~/.config/121gw-qt/qml or the built-in one.", "dir");
    const QCommandLineOption exportOpt("export-qml", "Copy the built-in QML to a directory (default "
                                                     "~/.config/121gw-qt/qml) to customize, then exit.", "dir");
    const QCommandLineOption commandOpt("print-mcp-command", "Print the 'claude mcp add' command for this meter and exit.");
    const QCommandLineOption svgOpt("face-svg", "Draw the face from this SVG (default ~/.config/121gw-qt/face.svg if it exists).", "file");
    const QCommandLineOption exportSvgOpt("export-svg", "Write a starter face SVG (the artwork plus one named rect per element) and exit.", "file");
    const QCommandLineOption shotOpt("screenshot", "Save the window to a PNG after --screenshot-delay ms and exit (for tests).", "file");
    const QCommandLineOption shotDelayOpt("screenshot-delay", "Milliseconds before --screenshot (default 2500).", "ms", "2500");
    parser.addOptions({demoOpt, noMcpOpt, portOpt, qmlOpt, exportOpt, commandOpt, svgOpt, exportSvgOpt, shotOpt, shotDelayOpt});
    parser.process(app);

    if (parser.isSet(exportOpt))
        return exportQml(parser.value(exportOpt).isEmpty() ? QDir(configDir()).filePath("qml") : parser.value(exportOpt));

    QDir().mkpath(configDir());
    QSettings settings(QDir(configDir()).filePath("settings.conf"), QSettings::IniFormat);
    if (settings.value("mcp/token").toString().isEmpty())
        settings.setValue("mcp/token", QString::fromLatin1(newToken()));
    settings.sync();
    QFile::setPermissions(settings.fileName(), QFile::ReadOwner | QFile::WriteOwner);   // it holds the token
    const quint16 port = static_cast<quint16>(
        parser.isSet(portOpt) ? parser.value(portOpt).toUInt() : settings.value("mcp/port", McpServer::DefaultPort).toUInt());

    if (parser.isSet(commandOpt)) {
        QTextStream(stdout) << "claude mcp add --transport http 121gw http://127.0.0.1:" << port
                            << "/mcp --header \"Authorization: Bearer " << settings.value("mcp/token").toString() << "\"\n";
        return 0;
    }

    qRegisterMetaType<DiscoveredMeter>();
    qRegisterMetaType<MeterPacket>();

    auto *bluetooth = new BleConnection(&app);
    auto *demo = new SimulatedConnection(&app);
    MeterSession session(bluetooth, demo);
    session.preferredMeterId = settings.value("meter/preferred").toString();
    session.setLogging(settings.value("log/enabled", true).toBool());
    SessionStore store(QDir(dataDir()).filePath("sessions"));
    demo->card().bytesPerSecond = settings.value("demo/cardBytesPerSecond", 190).toInt();
    demo->card().rejectConfirmations = settings.value("demo/rejectConfirmations", false).toBool();
    CardTransfer card(&session);
    MeterTools tools(&session, &store);
    tools.setCard(&card);
    McpServer mcp(&tools, QGuiApplication::applicationVersion());
    if (!parser.isSet(noMcpOpt) && settings.value("mcp/enabled", true).toBool())
        mcp.start(port, settings.value("mcp/token").toByteArray());
    MeterController controller(&session, &store, &tools, &mcp, &card, &settings);

    qmlRegisterType<LcdItem>("GW121", 1, 0, "Lcd");
    qmlRegisterUncreatableType<MeterController>("GW121", 1, 0, "MeterController", "Use the 'meter' context property.");

    // An exported template is drawn from the QML face, not from an SVG.
    const QString svgFile = parser.isSet(exportSvgOpt) ? QString()
                          : parser.isSet(svgOpt) ? parser.value(svgOpt) : QDir(configDir()).filePath("face.svg");
    FaceLayout layout(QDir(configDir()).filePath("layout.json"), svgFile);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("meter", &controller);
    engine.rootContext()->setContextProperty("layout", &layout);
    engine.addImageProvider("face", new FaceImageProvider(&layout));

    // The front panel: --qml, else a customized copy in ~/.config/121gw-qt/qml, else the built-in
    // one. A customized one that fails to load falls back to the built-in one.
    QString custom = parser.value(qmlOpt);
    if (custom.isEmpty() && QFile::exists(QDir(configDir()).filePath("qml/main.qml")))
        custom = QDir(configDir()).filePath("qml");
    if (!custom.isEmpty()) {
        engine.load(QUrl::fromLocalFile(QDir(custom).filePath("main.qml")));
        if (engine.rootObjects().isEmpty())
            qWarning() << "couldn't load" << custom << "- using the built-in front panel";
    }
    if (engine.rootObjects().isEmpty())
        engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (parser.isSet(demoOpt))
        session.startDemo();
    else if (!session.preferredMeterId.isEmpty())
        QTimer::singleShot(0, &session, [&session] { session.startScan(true); });   // reconnect to the last meter

    if (parser.isSet(exportSvgOpt)) {
        const QString file = parser.value(exportSvgOpt);
        QTimer::singleShot(1000, &app, [&engine, &layout, file] {
            auto *face = engine.rootObjects().value(0)->findChild<QQuickItem *>("meterFace");
            if (!face) {
                QTextStream(stderr) << "no meterFace in this QML\n";
                QCoreApplication::exit(1);
                return;
            }
            layout.exportSvg(face, file, [file](bool ok, const QString &message) {
                QTextStream(ok ? stdout : stderr) << (ok ? "wrote " : "couldn't write ") << file << ": " << message << '\n';
                QCoreApplication::exit(ok ? 0 : 1);
            });
        });
    }

    if (parser.isSet(shotOpt)) {
        const QString file = parser.value(shotOpt);
        QTimer::singleShot(parser.value(shotDelayOpt).toInt(), &app, [&engine, file] {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0)))
                window->grabWindow().save(file);
            QCoreApplication::quit();
        });
    }

    const int rc = app.exec();
    settings.setValue("log/enabled", session.isLogging());
    return rc;
}
