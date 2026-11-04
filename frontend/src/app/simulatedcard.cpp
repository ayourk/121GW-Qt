// SPDX-License-Identifier: MIT
#include "simulatedcard.h"

#include <QRandomGenerator>
#include <cmath>

namespace readout {

namespace {

constexpr int PaceIntervalMs = 50;

// A log the way the meter writes one (docs/sd-card.md): CRLF lines, MAX/MIN at the end.
QByteArray sampleLog(const QDateTime &start, int rows, double volts)
{
    QByteArray csv;
    const auto line = [&csv](const QString &s) { csv += s.toLatin1() + "\r\n"; };
    line(QStringLiteral("START,%1,%2,").arg(start.toString("yyyy/MM/dd"), start.toString("HH:mm:ss")));
    line(QStringLiteral("ID,170812100,"));
    line(QStringLiteral("INTERVAL,001,sec,"));
    line(QStringLiteral(",,,MAIN,,,SUB-1,,,SUB-2,,,Remark,"));
    line(QStringLiteral("No. ,Date      ,Time    ,Func. ,Value,Unit,Func. ,Value,Unit,Func. ,Value,Unit,"));
    double lo = 1e9, hi = -1e9;
    int loAt = 1, hiAt = 1;
    for (int i = 1; i <= rows; ++i) {
        const QDateTime t = start.addSecs(i);
        const double v = volts + 0.0004 * std::sin(i / 7.0);
        if (v < lo) { lo = v; loAt = i; }
        if (v > hi) { hi = v; hiAt = i; }
        line(QStringLiteral("%1,%2,%3,DCV,%4,V,,,,,,,,").arg(i).arg(t.toString("yyyy/MM/dd"), t.toString("HH:mm:ss"))
                 .arg(v, 7, 'f', 4, QLatin1Char('0')));
    }
    line(QStringLiteral("MAX,%1,DCV,%2,V,").arg(hiAt).arg(hi, 7, 'f', 4, QLatin1Char('0')));
    line(QStringLiteral("MIN,%1,DCV,%2,V,").arg(loAt).arg(lo, 7, 'f', 4, QLatin1Char('0')));
    return csv;
}

} // namespace

SimulatedCard::SimulatedCard(QObject *parent) : QObject(parent)
{
    m_engineTimer.setSingleShot(true);
    connect(&m_engineTimer, &QTimer::timeout, this, [this] {
        if (m_engine) {
            m_engine->timeout();
            flush();
        }
    });
    m_confirm.setSingleShot(true);
    connect(&m_confirm, &QTimer::timeout, this, [this] {
        // The meter's 30 s prompt: SETUP accepts; anything else (or nothing) refuses with CAN.
        if (!m_receiver)
            return;
        if (rejectConfirmations) {
            m_receiver->reject(QStringLiteral("Not confirmed on the meter."));
            sendCans();
        } else {
            m_receiver->approve();
        }
        m_promptFile.clear();
        flush();
        emit changed();
    });
    connect(&m_paceTimer, &QTimer::timeout, this, &SimulatedCard::pace);
    reset();
}

void SimulatedCard::reset()
{
    files.clear();
    const QDateTime day(QDate(2026, 11, 2), QTime(15, 0, 3));
    files.insert(QStringLiteral("26110200.CSV"), {sampleLog(day, 40, 12.0), day.addSecs(42)});
    files.insert(QStringLiteral("26110201.CSV"), {sampleLog(day.addSecs(3600), 12, 3.3), day.addSecs(3615)});
    QByteArray cal(1200, '\0');
    QRandomGenerator rng(121);
    for (char &c : cal)
        c = static_cast<char>(rng.bounded(256));
    files.insert(card::CalibrationFile, {cal, QDateTime(QDate(2026, 10, 30), QTime(9, 12, 40))});
}

void SimulatedCard::receive(const QByteArray &bytes)
{
    m_in += bytes;
    if (!m_paceTimer.isActive())
        m_paceTimer.start(bytesPerSecond > 0 ? PaceIntervalMs : 0);
}

void SimulatedCard::pace()
{
    // Each tick moves this many bytes each way; at 0 B/s, everything (the next event loop pass).
    const qsizetype budget = bytesPerSecond > 0 ? std::max(1, bytesPerSecond * PaceIntervalMs / 1000)
                                                : std::numeric_limits<qsizetype>::max();
    if (!m_out.isEmpty()) {
        const QByteArray chunk = m_out.left(budget);
        m_out.remove(0, chunk.size());
        emit transmit(chunk);
    }
    if (!m_in.isEmpty()) {
        const QByteArray chunk = m_in.left(budget);
        m_in.remove(0, chunk.size());
        process(chunk);
    }
    if (m_in.isEmpty() && m_out.isEmpty())
        m_paceTimer.stop();
}

void SimulatedCard::process(const QByteArray &bytes)
{
    if (m_quietActive)
        m_quiet.start();   // any byte starts the quiet second again
    // Commands are looked for even during a transfer: ABORT ends it (or the prompt), a repeat of
    // the frame that started it gets the start reply again, anything else is BUSY.
    const QList<card::Frame> frames = m_parser.push(bytes);
    if (m_engine) {
        for (const card::Frame &frame : frames) {
            if (frame.command() == card::Command::Abort) {
                reply(frame.seq, card::Status::Ok);
                m_engine->cancel(QStringLiteral("Aborted."));
                flush();
                sendCans();
                return;
            }
            if (frame.seq == m_startSeq && frame.code == m_startCode)
                transmitRaw(m_startReply);   // its OK was lost: nothing starts twice
            else
                reply(frame.seq, card::Status::Busy);
        }
        // While the prompt is up, block 0 again is ignored and CAN CAN cancels: the receiver
        // does both in that phase.
        m_engine->feed(bytes);
        flush();
        return;
    }
    for (const card::Frame &frame : frames)
        handleCommand(frame);
}

void SimulatedCard::transmitRaw(const QByteArray &bytes)
{
    m_out += bytes;
    if (!m_paceTimer.isActive())
        m_paceTimer.start(bytesPerSecond > 0 ? PaceIntervalMs : 0);
}

bool SimulatedCard::inQuietPeriod()
{
    if (m_quietActive && m_quiet.elapsed() >= card::QuietPeriodMs)
        m_quietActive = false;
    return m_quietActive;
}

void SimulatedCard::reply(quint8 seq, card::Status status, const QByteArray &data)
{
    m_lastReply = card::replyFrame(seq, status, data);
    if (dropReplies > 0) {
        --dropReplies;
        return;
    }
    transmitRaw(m_lastReply);
}

void SimulatedCard::startedBy(const card::Frame &frame)
{
    m_startSeq = frame.seq;
    m_startCode = frame.code;
    m_startReply = m_lastReply;
}

void SimulatedCard::sendCans()
{
    // The firmware cancels with three.
    m_out += ymodem::CAN;
    if (!m_paceTimer.isActive())
        m_paceTimer.start(bytesPerSecond > 0 ? PaceIntervalMs : 0);
}

void SimulatedCard::handleCommand(const card::Frame &frame)
{
    // Outside a transfer every command runs, repeats too (INFO, LIST and ABORT don't mind).
    switch (frame.command()) {
    case card::Command::Info:
        reply(frame.seq, card::Status::Ok, QByteArray("\x01\x40\x01", 3));
        break;
    case card::Command::List: {
        const auto index = card::parseListPayload(frame.payload);
        if (!index) {
            reply(frame.seq, card::Status::BadArgument);
            break;
        }
        if (*index >= files.size()) {
            reply(frame.seq, card::Status::End);
            break;
        }
        const auto it = std::next(files.cbegin(), *index);
        reply(frame.seq, card::Status::Ok, card::encodeEntry({it.key(), it->data.size(), it->modified}));
        break;
    }
    case card::Command::Receive: {
        reply(frame.seq, card::Status::Ok);
        startedBy(frame);
        ymodem::ReceiverOptions options;
        options.approveHeaders = true;
        options.maxFileSize = 4 * 1024 * 1024;
        auto receiver = std::make_unique<ymodem::Receiver>(options);
        m_receiver = receiver.get();
        m_engine = std::move(receiver);
        m_approvalHandled = false;
        m_receiver->start();
        flush();
        emit changed();
        break;
    }
    case card::Command::Send: {
        const auto request = card::parseSendPayload(frame.payload);
        if (!request) {
            reply(frame.seq, card::Status::BadArgument);
            break;
        }
        if (!files.contains(request->name)) {
            reply(frame.seq, card::Status::NotFound);
            break;
        }
        const File &f = files[request->name];
        reply(frame.seq, card::Status::Ok, card::sizePayload(f.data.size()));
        startedBy(frame);
        ymodem::SenderOptions options;
        options.use1K = request->blocks1K;
        ymodem::OutgoingFile out{{request->name, f.data.size(), f.modified}, f.data};
        auto sender = std::make_unique<ymodem::Sender>(QList<ymodem::OutgoingFile>{out}, options);
        m_receiver = nullptr;
        m_engine = std::move(sender);
        static_cast<ymodem::Sender *>(m_engine.get())->start();
        flush();
        emit changed();
        break;
    }
    case card::Command::Abort:
        reply(frame.seq, card::Status::Ok);   // nothing running: a no-op
        break;
    default:
        reply(frame.seq, card::Status::BadCommand);
        break;
    }
}

void SimulatedCard::flush()
{
    if (!m_engine)
        return;
    m_out += m_engine->takeOutput();
    if (!m_out.isEmpty() && !m_paceTimer.isActive())
        m_paceTimer.start(bytesPerSecond > 0 ? PaceIntervalMs : 0);
    const ymodem::TimerRequest t = m_engine->takeTimer();
    if (t.changed) {
        if (t.ms < 0)
            m_engineTimer.stop();
        else
            m_engineTimer.start(t.ms);
    }
    checkApproval();
    if (m_engine && m_engine->finished())
        endTransfer();
}

void SimulatedCard::checkApproval()
{
    if (!m_receiver || m_approvalHandled || m_receiver->phase() != ymodem::Receiver::Phase::AwaitingApproval)
        return;
    m_approvalHandled = true;
    const QString name = card::normalizeName(m_receiver->pendingHeader().name);
    if (card::isRefused(name) || !card::isValidName(name)) {
        m_receiver->reject(QStringLiteral("Refused: %1.").arg(name));   // CAL.BIN, BTRECV.TMP, not 8.3
        sendCans();
    } else if (card::needsConfirmation(name, files.contains(name))) {
        m_promptFile = name;
        m_confirm.start(rejectConfirmations ? std::min(confirmDelayMs, 30000) : confirmDelayMs);
        emit changed();
        return;
    } else {
        m_receiver->approve();
    }
    flush();
}

void SimulatedCard::endTransfer()
{
    if (m_receiver && m_receiver->outcome() == ymodem::Outcome::Completed) {
        for (const ymodem::ReceivedFile &f : m_receiver->files())
            files.insert(card::normalizeName(f.header.name), {f.data, QDateTime::currentDateTime()});
    }
    m_engineTimer.stop();
    m_confirm.stop();
    m_promptFile.clear();
    m_receiver = nullptr;
    m_engine.reset();
    m_approvalHandled = false;
    m_parser.reset();
    m_startSeq = m_startCode = -1;
    // Key and clock frames are dropped until the line has been quiet for a second.
    m_quietActive = true;
    m_quiet.start();
    emit changed();
}

void SimulatedCard::linkDown()
{
    m_in.clear();
    m_out.clear();
    m_paceTimer.stop();
    if (m_engine) {
        m_receiver = nullptr;
        m_engine.reset();
    }
    m_engineTimer.stop();
    m_confirm.stop();
    m_promptFile.clear();
    m_parser.reset();
    m_startSeq = m_startCode = -1;
    m_quietActive = false;
    emit changed();
}

} // namespace readout
