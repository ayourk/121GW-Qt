// SPDX-License-Identifier: MIT
#include "cardtransfer.h"

#include "meterconnection.h"
#include "metersession.h"

#include <QLoggingCategory>
#include <QRandomGenerator>

Q_LOGGING_CATEGORY(lcCard, "121gw.card")

namespace readout {

namespace {
// Rate over the last this many milliseconds.
constexpr qint64 RateWindowMs = 10000;
// The meter's quiet second after a transfer, and a margin for the Bluetooth write queue.
constexpr int QuietHoldMs = card::QuietPeriodMs + 500;
} // namespace

CardTransfer::CardTransfer(MeterSession *session, QObject *parent) : QObject(parent), m_session(session)
{
    m_replyTimer.setSingleShot(true);
    connect(&m_replyTimer, &QTimer::timeout, this, [this] {
        // No reply (the command or the reply was damaged): the same frame again; the meter
        // answers a repeated SEQ without running the command twice.
        if (m_tries < replyTries && m_link) {
            ++m_tries;
            extendQuietHold();
            m_link->sendCommand(m_command);
            m_replyTimer.start(replyTimeoutMs);
            return;
        }
        complete(false, m_operation == Operation::List && !m_listing.isEmpty()
                            ? QStringLiteral("The meter stopped answering part way through the listing.")
                            : QStringLiteral("The meter didn't answer the file command. File transfer needs the custom "
                                             "firmware with Bluetooth file transfer; the stock firmware ignores it."));
    });
    // Per the firmware: at least 15 s for a 1 KB block at the meter's pace (128-byte ones need 5),
    // and 'C' no faster than every 5 s (a 'C' while block 0 is going out makes it start over).
    receiverOptions.blockTimeoutMs = 15000;
    receiverOptions.startIntervalMs = 5000;
    // The meter answers a repeated SEQ from its cache: start somewhere else every run.
    m_seq = static_cast<quint8>(QRandomGenerator::global()->bounded(256));
    m_engineTimer.setSingleShot(true);
    connect(&m_engineTimer, &QTimer::timeout, this, [this] {
        if (m_engine) {
            m_engine->timeout();
            flush();
        }
    });
    m_hintTimer.setSingleShot(true);
    connect(&m_hintTimer, &QTimer::timeout, this, [this] {
        m_hintElapsed = true;
        updatePhase();
    });
    connect(session, &MeterSession::stateChanged, this, [this] {
        if (busy() && (m_session->state() != ConnectionState::Connected || m_session->active() != m_link))
            complete(false, QStringLiteral("The meter disconnected."));
        // The next meter (or this one, later) may have another card.
        if (m_session->state() != ConnectionState::Connected && m_listedAt.isValid()) {
            m_files.clear();
            m_listedAt = {};
            emit filesChanged();
        }
    });
}

CardTransfer::~CardTransfer()
{
    if (m_link)
        m_link->setRawMode(false);
}

QString CardTransfer::operationName(Operation op)
{
    switch (op) {
    case Operation::None: return {};
    case Operation::List: return QStringLiteral("list");
    case Operation::Download: return QStringLiteral("download");
    case Operation::Upload: return QStringLiteral("upload");
    }
    return {};
}

QString CardTransfer::phaseName(Phase phase)
{
    switch (phase) {
    case Phase::Idle: return QStringLiteral("idle");
    case Phase::Requesting: return QStringLiteral("requesting");
    case Phase::Starting: return QStringLiteral("starting");
    case Phase::Confirming: return QStringLiteral("confirm_on_meter");
    case Phase::Transferring: return QStringLiteral("transferring");
    case Phase::Finishing: return QStringLiteral("finishing");
    }
    return {};
}

qint64 CardTransfer::bytesDone() const
{
    return m_engine ? m_engine->bytesDone() : 0;
}

qint64 CardTransfer::bytesTotal() const
{
    if (m_operation == Operation::Upload)
        return m_upload.size();
    if (m_engine && m_engine->bytesTotal() >= 0)
        return m_engine->bytesTotal();
    return m_expectedSize;
}

double CardTransfer::bytesPerSecond() const
{
    // From the oldest sample in the window to now, so a stall shows as a falling rate.
    if (m_samples.isEmpty() || !m_clock.isValid())
        return 0;
    const auto &first = m_samples.first();
    const qint64 ms = m_clock.elapsed() - first.first;
    if (ms < 500)
        return 0;
    return (m_samples.last().second - first.second) * 1000.0 / ms;
}

int CardTransfer::secondsLeft() const
{
    const double rate = bytesPerSecond();
    const qint64 total = bytesTotal();
    if (rate <= 0 || total < 0)
        return -1;
    return static_cast<int>(std::ceil(std::max<qint64>(0, total - bytesDone()) / rate));
}

double CardTransfer::secondsElapsed() const
{
    return busy() && m_clock.isValid() ? m_clock.elapsed() / 1000.0 : 0;
}

std::optional<card::CardFile> CardTransfer::find(const QString &name) const
{
    const QString n = card::normalizeName(name);
    for (const card::CardFile &f : m_files)
        if (f.name == n)
            return f;
    return std::nullopt;
}

QString CardTransfer::list(Done done)
{
    return begin(Operation::List, {}, card::Command::List, card::listPayload(0), std::move(done));
}

QString CardTransfer::download(const QString &name, Done done)
{
    const QString n = card::normalizeName(name);
    QString why;
    if (!card::isValidName(n, &why))
        return why;
    return begin(Operation::Download, n, card::Command::Send, card::sendPayload(n, download1K, downloadPace), std::move(done));
}

QString CardTransfer::upload(const QString &name, const QByteArray &data, Done done)
{
    const QString n = card::normalizeName(name);
    QString why;
    if (!card::isValidName(n, &why))
        return why;
    if (card::isRefused(n, &why))
        return why;
    if (data.size() > receiverOptions.maxFileSize)
        return QStringLiteral("That file is too large (%1 bytes).").arg(data.size());
    m_upload = data;
    m_confirmExpected = card::needsConfirmation(n, find(n).has_value());
    const QString error = begin(Operation::Upload, n, card::Command::Receive, {}, std::move(done));
    if (!error.isEmpty()) {
        m_upload.clear();
        m_confirmExpected = false;
    }
    return error;
}

QString CardTransfer::begin(Operation op, const QString &name, card::Command command, const QByteArray &payload, Done done)
{
    if (busy())
        return QStringLiteral("A file %1 is already running.").arg(operationName(m_operation));
    if (m_session->state() != ConnectionState::Connected)
        return QStringLiteral("No meter is connected.");
    m_link = m_session->active();
    m_operation = op;
    m_name = name;
    m_done = std::move(done);
    m_listing.clear();
    m_listIndex = 0;
    m_expectedSize = -1;
    m_parser.reset();
    m_parser.takeRest();
    m_samples.clear();
    m_hintElapsed = false;
    m_clock.start();
    // A transfer's bytes must not reach the packet framer; the meter pauses its status packets.
    if (op != Operation::List)
        m_link->setRawMode(true);
    m_bytesConnection = connect(m_link, &MeterConnection::bytesReceived, this, &CardTransfer::onBytes);
    setPhase(Phase::Requesting);
    qCInfo(lcCard) << operationName(op) << name;
    sendCommand(command, payload);
    emit changed();
    return {};
}

void CardTransfer::sendCommand(card::Command command, const QByteArray &payload)
{
    m_seq = static_cast<quint8>(m_seq + 1);
    m_command = card::commandFrame(command, m_seq, payload);
    m_tries = 1;
    extendQuietHold();
    if (!m_link || !m_link->sendCommand(m_command))
        return complete(false, QStringLiteral("Couldn't send to the meter."));
    m_replyTimer.start(replyTimeoutMs);
}

void CardTransfer::onBytes(const QByteArray &bytes)
{
    if (m_engine) {
        m_engine->feed(bytes);
        flush();
        return;
    }
    if (m_phase != Phase::Requesting)
        return;
    for (const card::Frame &frame : m_parser.push(bytes)) {
        if (frame.seq != m_seq)
            continue;   // a late reply to an earlier try
        onReply(frame, m_parser.takeRest());
        if (m_phase != Phase::Requesting || !busy())
            return;
    }
}

void CardTransfer::onReply(const card::Frame &frame, const QByteArray &after)
{
    m_replyTimer.stop();
    const card::Status status = frame.status();
    if (m_operation == Operation::List) {
        // One entry per command, index 0 up, until End.
        if (status == card::Status::Ok) {
            if (const auto entry = card::decodeEntry(frame.payload))
                m_listing << *entry;
            sendCommand(card::Command::List, card::listPayload(++m_listIndex));
            return;
        }
        if (status != card::Status::End)
            return complete(false, card::statusText(status));
        m_files = m_listing;
        std::sort(m_files.begin(), m_files.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
        m_listedAt = QDateTime::currentDateTime();
        emit filesChanged();
        return complete(true, QStringLiteral("%1 files on the card.").arg(m_files.size()));
    }
    if (status != card::Status::Ok) {
        if (status == card::Status::NotFound)
            return complete(false, QStringLiteral("The meter has no file named %1.").arg(m_name));
        if (status == card::Status::BadArgument)
            return complete(false, QStringLiteral("The meter can't use the name %1.").arg(m_name));
        return complete(false, card::statusText(status));
    }
    if (m_operation == Operation::Download)
        m_expectedSize = card::parseSizePayload(frame.payload).value_or(-1);
    startEngine(after);
}

void CardTransfer::startEngine(const QByteArray &after)
{
    m_replyTimer.stop();
    if (m_operation == Operation::Upload) {
        auto sender = std::make_unique<ymodem::Sender>(
            QList<ymodem::OutgoingFile>{{{m_name, m_upload.size(), meterClockNow()}, m_upload}}, senderOptions);
        m_sender = sender.get();
        m_engine = std::move(sender);
        m_sender->start();
    } else {
        auto receiver = std::make_unique<ymodem::Receiver>(receiverOptions);
        m_receiver = receiver.get();
        m_engine = std::move(receiver);
        m_receiver->start();
    }
    setPhase(Phase::Starting);
    sample();
    if (!after.isEmpty())
        m_engine->feed(after);
    flush();
}

void CardTransfer::flush()
{
    if (!m_engine)
        return;
    const QByteArray out = m_engine->takeOutput();
    if (!out.isEmpty() && m_link) {
        extendQuietHold();
        m_link->sendCommand(out);
    }
    const ymodem::TimerRequest t = m_engine->takeTimer();
    if (t.changed) {
        if (t.ms < 0)
            m_engineTimer.stop();
        else
            m_engineTimer.start(t.ms);
    }
    sample();
    if (m_engine->finished()) {
        const ymodem::Outcome outcome = m_engine->outcome();
        const QString error = m_engine->errorString();
        const bool waitingForAccept = m_sender && !m_sender->atBatchEnd()
                                      && m_sender->phase() == ymodem::Sender::Phase::WaitingForHeaderAck;
        switch (outcome) {
        case ymodem::Outcome::Completed:
            if (m_operation == Operation::Download) {
                const auto &files = m_receiver->files();
                if (files.isEmpty())
                    return complete(false, QStringLiteral("The meter sent no file."));
                m_data = files.first().data;
                if (card::normalizeName(files.first().header.name) != m_name)
                    qCWarning(lcCard) << "asked for" << m_name << "got" << files.first().header.name;
                return complete(true, QStringLiteral("Downloaded %1 (%2 bytes).").arg(m_name).arg(files.first().data.size()));
            }
            // The card now has it: keep the listing current without asking again.
            {
                card::CardFile f{m_name, m_upload.size(), QDateTime::currentDateTime()};
                bool replaced = false;
                for (card::CardFile &existing : m_files)
                    if (existing.name == m_name) {
                        existing = f;
                        replaced = true;
                    }
                if (!replaced) {
                    m_files << f;
                    std::sort(m_files.begin(), m_files.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
                }
                emit filesChanged();
            }
            return complete(true, m_name == card::FirmwareFile
                                      ? QStringLiteral("Uploaded EEVBLOG.BIN (%1 bytes). The meter flashes it the next time "
                                                       "you run its firmware update procedure.").arg(m_upload.size())
                                      : QStringLiteral("Uploaded %1 (%2 bytes).").arg(m_name).arg(m_upload.size()));
        case ymodem::Outcome::Canceled:
            return complete(false, error, true);
        case ymodem::Outcome::RemoteCanceled:
            if (m_operation == Operation::Upload && waitingForAccept)
                return complete(false, m_confirmExpected || m_hintElapsed
                                           ? QStringLiteral("The meter refused %1: it wasn't confirmed with SETUP on the meter "
                                                            "within 30 seconds (or another key was pressed).").arg(m_name)
                                           : QStringLiteral("The meter refused %1.").arg(m_name));
            return complete(false, QStringLiteral("The meter canceled the transfer."));
        case ymodem::Outcome::Failed:
            return complete(false, error);
        case ymodem::Outcome::Running:
            break;
        }
    }
    updatePhase();
}

void CardTransfer::updatePhase()
{
    if (!m_engine)
        return;
    Phase phase = Phase::Transferring;
    if (m_sender) {
        switch (m_sender->phase()) {
        case ymodem::Sender::Phase::WaitingForReceiver:
            phase = m_sender->atBatchEnd() ? Phase::Finishing : Phase::Starting;
            break;
        case ymodem::Sender::Phase::WaitingForHeaderAck:
            if (m_sender->atBatchEnd()) {
                phase = Phase::Finishing;
            } else {
                // Block 0 is out and not yet accepted: the meter may be asking for SETUP.
                if (!m_hintTimer.isActive() && !m_hintElapsed)
                    m_hintTimer.start(confirmHintMs);
                phase = m_confirmExpected || m_hintElapsed ? Phase::Confirming : Phase::Starting;
            }
            break;
        case ymodem::Sender::Phase::WaitingForDataStart:
        case ymodem::Sender::Phase::SendingData:
            phase = Phase::Transferring;
            break;
        case ymodem::Sender::Phase::Ending:
        case ymodem::Sender::Phase::Done:
            phase = Phase::Finishing;
            break;
        }
        if (phase != Phase::Confirming && phase != Phase::Starting)
            m_hintTimer.stop();
    } else if (m_receiver) {
        phase = m_receiver->phase() == ymodem::Receiver::Phase::WaitingForHeader
                    ? (m_receiver->files().isEmpty() ? Phase::Starting : Phase::Finishing)
                    : Phase::Transferring;
    }
    setPhase(phase);
}

void CardTransfer::sample()
{
    if (!m_engine)
        return;
    const qint64 now = m_clock.elapsed();
    const qint64 done = m_engine->bytesDone();
    if (!m_samples.isEmpty() && m_samples.last().second == done && now - m_samples.last().first < 1000)
        return;
    m_samples.append({now, done});
    while (m_samples.size() > 2 && now - m_samples.first().first > RateWindowMs)
        m_samples.removeFirst();
    emit progressChanged();
}

void CardTransfer::extendQuietHold()
{
    // Any byte the meter receives during its quiet second starts the second again.
    if (m_session->stockHoldLeftMs() > 0)
        m_session->holdStockCommands(QuietHoldMs);
}

void CardTransfer::setPhase(Phase phase)
{
    if (m_phase == phase)
        return;
    // The rate counts from when data starts to flow, not from the wait for the meter.
    if (phase == Phase::Transferring && m_engine) {
        m_samples.clear();
        m_samples.append({m_clock.elapsed(), m_engine->bytesDone()});
    }
    m_phase = phase;
    emit changed();
}

void CardTransfer::cancel()
{
    if (!busy())
        return;
    const QPointer<MeterConnection> link = m_link;
    const bool list = m_operation == Operation::List;
    const quint8 abortSeq = static_cast<quint8>(m_seq + 1);
    m_seq = abortSeq;
    if (m_engine) {
        m_engine->cancel(QStringLiteral("Canceled."));
        flush();   // sends CAN CAN and completes
    } else {
        complete(false, QStringLiteral("Canceled."), true);
    }
    // While its overwrite prompt is up the meter hears only ABORT, and CAN CAN only where a
    // block header is due; ABORT also drops a request still being answered. Outside a
    // transfer it does nothing.
    if (link && !list) {
        link->sendCommand(card::commandFrame(card::Command::Abort, abortSeq));
        m_session->holdStockCommands(QuietHoldMs);   // the ABORT restarts the meter's quiet second
    }
}

QDateTime CardTransfer::meterClockNow()
{
    // The meter takes block 0's time as UTC and stamps the file with it: send this computer's
    // wall-clock time as if it were UTC, so the card shows local time like its own logs.
    const QDateTime now = QDateTime::currentDateTime();
    return QDateTime(now.date(), now.time(), Qt::UTC);
}

void CardTransfer::complete(bool ok, const QString &message, bool canceled)
{
    if (!busy())
        return;
    Result r;
    r.operation = m_operation;
    r.ok = ok;
    r.canceled = canceled;
    r.message = message;
    r.name = m_name;
    r.data = ok ? m_data : QByteArray();
    r.files = m_operation == Operation::List ? m_files : QList<card::CardFile>{};
    r.bytes = m_engine ? m_engine->bytesDone() : 0;
    r.seconds = m_clock.isValid() ? m_clock.elapsed() / 1000.0 : 0;
    qCInfo(lcCard) << operationName(m_operation) << m_name << (ok ? "ok" : "failed") << message;

    m_replyTimer.stop();
    m_engineTimer.stop();
    m_hintTimer.stop();
    disconnect(m_bytesConnection);
    if (m_link)
        m_link->setRawMode(false);
    // After a transfer (or one that was being asked for) the meter drops key and clock frames
    // until nothing has reached it for a second.
    if (m_operation != Operation::List)
        m_session->holdStockCommands(QuietHoldMs);
    m_engine.reset();
    m_sender = nullptr;
    m_receiver = nullptr;
    m_operation = Operation::None;
    m_phase = Phase::Idle;
    m_upload.clear();
    m_data.clear();
    m_confirmExpected = false;
    m_samples.clear();
    m_last = r;
    Done done = std::move(m_done);
    m_done = nullptr;
    emit changed();
    emit progressChanged();
    emit finished(r);
    if (done)
        done(r);
}

} // namespace readout
