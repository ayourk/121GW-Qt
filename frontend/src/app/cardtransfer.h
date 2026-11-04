// SPDX-License-Identifier: MIT
// File transfer with the meter's SD card over Bluetooth (custom firmware): lists the card,
// downloads and uploads files with YMODEM-1K, and reports progress, rate and time left. The UI
// and the MCP tools both use it; one operation at a time.
#pragma once

#include "cardprotocol.h"
#include "ymodem.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <memory>

namespace readout {

class MeterConnection;
class MeterSession;

class CardTransfer : public QObject
{
    Q_OBJECT
public:
    enum class Operation { None, List, Download, Upload };
    enum class Phase {
        Idle,
        Requesting,     // the command is sent; waiting for the meter's status frame
        Starting,       // YMODEM: waiting for the other end to begin
        Confirming,     // upload: the meter is waiting for SETUP before it accepts the file
        Transferring,
        Finishing,      // the data is through; ending the batch
    };

    struct Result {
        Operation operation = Operation::None;
        bool ok = false;
        bool canceled = false;
        QString message;          // what happened, for the user
        QString name;             // the card's file
        QByteArray data;          // download: the file
        QList<card::CardFile> files;   // list: the card's files
        qint64 bytes = 0;
        double seconds = 0;
    };
    using Done = std::function<void(const Result &)>;

    explicit CardTransfer(MeterSession *session, QObject *parent = nullptr);
    ~CardTransfer() override;

    // The meter answers a command within 3 s; then it is sent again (same SEQ), up to replyTries times.
    int replyTimeoutMs = 3000;
    int replyTries = 4;
    // Downloads: 128-byte blocks (a damaged notification costs 128 bytes, not 1 KB) unless set;
    // pace = bytes per 100 ms the meter sends (0: its default, 16).
    bool download1K = false;
    int downloadPace = 0;
    // An upload's block 0 unacknowledged this long: the meter is asking its user.
    int confirmHintMs = 2500;
    ymodem::SenderOptions senderOptions;
    ymodem::ReceiverOptions receiverOptions;

    bool busy() const { return m_operation != Operation::None; }
    Operation operation() const { return m_operation; }
    Phase phase() const { return m_phase; }
    QString fileName() const { return m_name; }
    qint64 bytesDone() const;
    qint64 bytesTotal() const;
    // Bytes per second over the last few seconds; 0 until known.
    double bytesPerSecond() const;
    // Seconds left at the current rate; -1 until known.
    int secondsLeft() const;
    double secondsElapsed() const;
    // The meter will ask for SETUP before writing this upload (it exists, or it's EEVBLOG.BIN).
    bool confirmationExpected() const { return m_confirmExpected; }

    // The last listing, and when it was made (invalid: never).
    const QList<card::CardFile> &files() const { return m_files; }
    QDateTime listedAt() const { return m_listedAt; }
    std::optional<card::CardFile> find(const QString &name) const;

    // The last finished operation.
    const Result &lastResult() const { return m_last; }

    // Each returns an error message when it can't start (not connected, busy, a bad name), or
    // an empty string and calls done when it ends.
    QString list(Done done = {});
    QString download(const QString &name, Done done = {});
    // Uploads data as name. The caller has warned about overwriting; CAL.BIN is always refused.
    QString upload(const QString &name, const QByteArray &data, Done done = {});
    // Stops the operation (CAN CAN in a transfer); its result is "canceled".
    void cancel();

    static QString operationName(Operation op);
    static QString phaseName(Phase phase);

signals:
    // Phase, file, or busy changed.
    void changed();
    void progressChanged();
    void filesChanged();
    void finished(const readout::CardTransfer::Result &result);

private:
    QString begin(Operation op, const QString &name, card::Command command, const QByteArray &payload, Done done);
    void sendCommand(card::Command command, const QByteArray &payload);
    void onBytes(const QByteArray &bytes);
    void onReply(const card::Frame &frame, const QByteArray &after);
    void startEngine(const QByteArray &after);
    void flush();
    void updatePhase();
    void sample();
    void complete(bool ok, const QString &message, bool canceled = false);
    void setPhase(Phase phase);
    void extendQuietHold();
    static QDateTime meterClockNow();

    MeterSession *m_session;
    QPointer<MeterConnection> m_link;
    QMetaObject::Connection m_bytesConnection;
    Operation m_operation = Operation::None;
    Phase m_phase = Phase::Idle;
    QString m_name;
    QByteArray m_upload;
    QByteArray m_data;         // a finished download
    Done m_done;
    quint8 m_seq = 0;
    QByteArray m_command;        // the command frame waiting for its reply (sent again on timeout)
    int m_tries = 0;
    card::FrameParser m_parser;
    QList<card::CardFile> m_listing;
    int m_listIndex = 0;
    qint64 m_expectedSize = -1;  // a download's size, from the meter's reply
    std::unique_ptr<ymodem::Engine> m_engine;
    ymodem::Sender *m_sender = nullptr;
    ymodem::Receiver *m_receiver = nullptr;
    QTimer m_replyTimer;
    QTimer m_engineTimer;
    QTimer m_hintTimer;
    bool m_hintElapsed = false;
    bool m_confirmExpected = false;
    QElapsedTimer m_clock;
    QList<QPair<qint64, qint64>> m_samples;   // (ms, bytes done)
    QList<card::CardFile> m_files;
    QDateTime m_listedAt;
    Result m_last;
};

} // namespace readout
