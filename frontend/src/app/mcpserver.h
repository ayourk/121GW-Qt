// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Mcp/McpHttpServer.cs, LoopbackHttp.cs), Copyright (c) 2026 Alejandro Mora, MIT:
// a stateless MCP server over HTTP on this computer only, answering each POST with JSON.
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <functional>

class QTcpServer;
class QTcpSocket;

namespace readout {

class MeterTools;

class McpServer : public QObject
{
    Q_OBJECT
public:
    static constexpr quint16 DefaultPort = 47121;
    McpServer(MeterTools *tools, QString version, QObject *parent = nullptr);

    bool start(quint16 port, const QByteArray &token);
    void stop();
    bool isRunning() const;
    quint16 port() const;
    QString endpoint() const;
    QString errorString() const { return m_error; }

signals:
    // A tool was called (for the app's activity line).
    void toolCalled(const QString &name);

private:
    struct Request {
        QByteArray method, path;
        QHash<QByteArray, QByteArray> headers;   // lower-case names
        QByteArray body;
    };
    void onReadyRead(QTcpSocket *socket);
    void respond(QTcpSocket *socket, const Request &request);
    int check(const Request &request) const;
    void handleMessage(const QJsonObject &message, std::function<void(const QJsonObject &)> reply);
    static void write(QTcpSocket *socket, int status, const QByteArray &body = {},
                      const QList<QPair<QByteArray, QByteArray>> &headers = {});

    MeterTools *m_tools;
    QString m_version;
    QTcpServer *m_server;
    QByteArray m_token;
    QString m_error;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

} // namespace readout
