// SPDX-License-Identifier: MIT
// The demo meter's SD card: an in-memory card that answers the card commands and runs YMODEM
// the way the custom firmware does (firmware-re/docs/ble-ymodem.md), confirmation prompt included.
#pragma once

#include "cardprotocol.h"
#include "ymodem.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <memory>

namespace readout {

class SimulatedCard : public QObject
{
    Q_OBJECT
public:
    struct File {
        QByteArray data;
        QDateTime modified;
    };

    explicit SimulatedCard(QObject *parent = nullptr);

    // The card's files by 8.3 name; starts with two logs and CAL.BIN.
    QMap<QString, File> files;
    void reset();

    // Writing an existing file or EEVBLOG.BIN waits for "SETUP" on the meter: the demo presses it
    // after confirmDelayMs, or (rejectConfirmations) lets it time out, which refuses with CAN.
    int confirmDelayMs = 3000;
    bool rejectConfirmations = false;
    // The link's speed in both directions; 0 delivers at once (tests).
    int bytesPerSecond = 190;
    // Loses the next this many replies, as a damaged notification would (tests).
    int dropReplies = 0;

    bool transferActive() const { return m_engine != nullptr; }
    // Bytes still on their way to the app: the meter sends one thing at a time, so its status
    // packets wait.
    bool transmitting() const { return !m_out.isEmpty(); }
    // The meter's prompt is showing ("press SETUP to write NAME").
    bool awaitingConfirmation() const { return m_confirm.isActive(); }
    QString promptFile() const { return m_promptFile; }
    // The second after a transfer in which the meter drops key and clock frames (and its status
    // packets wait); it starts again with every byte received.
    bool inQuietPeriod();

    // Bytes from the app.
    void receive(const QByteArray &bytes);
    // The link dropped: ends any transfer.
    void linkDown();

signals:
    // Bytes to the app.
    void transmit(const QByteArray &bytes);
    void changed();

private:
    void pace();
    void process(const QByteArray &bytes);
    void handleCommand(const card::Frame &frame);
    void reply(quint8 seq, card::Status status, const QByteArray &data = {});
    void sendCans();
    void transmitRaw(const QByteArray &bytes);
    void startedBy(const card::Frame &frame);
    void flush();
    void checkApproval();
    void endTransfer();

    card::FrameParser m_parser{card::CommandStart, card::MaxCommandPayload};
    QByteArray m_lastReply;
    // The frame that started the running transfer, and its reply, for a repeat of it.
    int m_startSeq = -1;
    int m_startCode = -1;
    QByteArray m_startReply;
    QElapsedTimer m_quiet;
    bool m_quietActive = false;
    std::unique_ptr<ymodem::Engine> m_engine;
    ymodem::Receiver *m_receiver = nullptr;   // m_engine when receiving
    QTimer m_engineTimer;
    QTimer m_confirm;
    QTimer m_paceTimer;
    QByteArray m_in, m_out;
    QString m_promptFile;
    bool m_approvalHandled = false;
};

} // namespace readout
