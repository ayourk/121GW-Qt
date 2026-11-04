// SPDX-License-Identifier: MIT
// YMODEM-1K, written for 121gw-qt from the public protocol description (Chuck Forsberg,
// "XMODEM/YMODEM Protocol Reference", 1988).
#include "ymodem.h"


namespace readout::ymodem {

quint16 crc16(QByteArrayView data)
{
    quint16 crc = 0;
    for (const char c : data) {
        crc ^= static_cast<quint16>(static_cast<quint8>(c)) << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? static_cast<quint16>((crc << 1) ^ 0x1021) : static_cast<quint16>(crc << 1);
    }
    return crc;
}

QByteArray encodeHeader(const FileHeader &header)
{
    QByteArray payload = header.name.toLatin1();
    payload.append('\0');
    if (header.name.isEmpty())
        return payload;
    payload += QByteArray::number(std::max<qint64>(0, header.size));
    if (header.modified.isValid())
        payload += ' ' + QByteArray::number(header.modified.toSecsSinceEpoch(), 8);
    payload.append('\0');
    return payload;
}

std::optional<FileHeader> decodeHeader(QByteArrayView payload)
{
    FileHeader h;
    const qsizetype nul = payload.indexOf('\0');
    if (nul < 0)
        return std::nullopt;
    h.name = QString::fromLatin1(payload.first(nul));
    if (h.name.isEmpty())
        return h;   // end of batch
    // Fields after the name, space separated, up to the next NUL: size (decimal), then
    // modification time, mode, serial (octal). Only size and time are used.
    QByteArrayView rest = payload.sliced(nul + 1);
    const qsizetype end = rest.indexOf('\0');
    if (end >= 0)
        rest = rest.first(end);
    const QList<QByteArray> fields = rest.toByteArray().simplified().split(' ');
    if (!fields.isEmpty() && !fields[0].isEmpty()) {
        bool ok = false;
        const qint64 size = fields[0].toLongLong(&ok, 10);
        if (!ok || size < 0)
            return std::nullopt;
        h.size = size;
    }
    if (fields.size() > 1) {
        bool ok = false;
        const qint64 secs = fields[1].toLongLong(&ok, 8);
        if (ok && secs > 0)
            h.modified = QDateTime::fromSecsSinceEpoch(secs, Qt::UTC);
    }
    return h;
}

QByteArray encodeBlock(quint8 number, QByteArrayView payload, int blockSize, char pad)
{
    QByteArray block;
    block.reserve(blockSize + 5);
    block.append(blockSize == 1024 ? STX : SOH);
    block.append(static_cast<char>(number));
    block.append(static_cast<char>(~number));
    QByteArray data = payload.first(std::min<qsizetype>(payload.size(), blockSize)).toByteArray();
    data.append(QByteArray(blockSize - data.size(), pad));
    block += data;
    const quint16 crc = crc16(data);
    block.append(static_cast<char>(crc >> 8));
    block.append(static_cast<char>(crc & 0xFF));
    return block;
}

// ── Engine ──────────────────────────────────────────────────────────────────

QByteArray Engine::takeOutput()
{
    QByteArray out;
    out.swap(m_output);
    return out;
}

TimerRequest Engine::takeTimer()
{
    const TimerRequest t = m_timer;
    m_timer.changed = false;
    return t;
}

void Engine::cancel(const QString &why)
{
    if (finished())
        return;
    send(QByteArray(2, CAN));
    finish(Outcome::Canceled, why);
}

void Engine::finish(Outcome outcome, const QString &error)
{
    m_outcome = outcome;
    m_error = error;
    disarm();
}

bool Engine::retryOrFail(int maxRetries, const QString &error)
{
    if (++m_retries <= maxRetries)
        return false;
    send(QByteArray(2, CAN));
    finish(Outcome::Failed, error);
    return true;
}

bool Engine::canceledBy(char byte)
{
    m_cans = byte == CAN ? m_cans + 1 : 0;
    return m_cans >= 2;
}

// ── Sender ──────────────────────────────────────────────────────────────────

Sender::Sender(QList<OutgoingFile> files, SenderOptions options) : m_files(std::move(files)), m_options(options)
{
    for (OutgoingFile &f : m_files)
        f.header.size = f.data.size();
    m_bytesTotal = batchBytes();
}

qint64 Sender::batchBytes() const
{
    qint64 total = 0;
    for (const OutgoingFile &f : m_files)
        total += f.data.size();
    return total;
}

void Sender::start()
{
    m_phase = Phase::WaitingForReceiver;
    arm(m_options.startTimeoutMs);
}

void Sender::feed(QByteArrayView bytes)
{
    for (const char b : bytes) {
        if (finished())
            return;
        handle(b);
    }
}

void Sender::handle(char byte)
{
    if (canceledBy(byte)) {
        finish(Outcome::RemoteCanceled, QStringLiteral("The receiver canceled the transfer."));
        return;
    }
    switch (m_phase) {
    case Phase::WaitingForReceiver:
        if (byte == CRC_REQUEST)
            sendHeader();
        break;   // anything else (a NAK asking for checksum mode, noise) waits for 'C'
    case Phase::WaitingForHeaderAck:
        // A 'C' here was sent before block 0 arrived; answering it would send block 0 twice.
        if (byte == ACK) {
            m_retries = 0;
            if (m_file >= m_files.size()) {
                m_phase = Phase::Done;
                finish(Outcome::Completed);
            } else {
                m_phase = Phase::WaitingForDataStart;
                arm(m_options.ackTimeoutMs);
            }
        } else if (byte == NAK) {
            if (!retryOrFail(m_options.maxRetries, QStringLiteral("The receiver kept rejecting the file's name block."))) {
                send(m_lastBlock);
                arm(m_options.headerAckTimeoutMs + m_options.msPerByte * m_lastBlock.size());
            }
        }
        break;
    case Phase::WaitingForDataStart:
        if (byte == CRC_REQUEST || byte == NAK) {
            if (m_files[m_file].data.isEmpty())
                sendEot();
            else
                sendBlock();
        }
        break;
    case Phase::SendingData:
        if (byte == ACK) {
            m_retries = 0;
            m_offset += blockLength();
            m_bytesDone = m_doneBefore + std::min<qint64>(m_offset, m_files[m_file].data.size());
            if (m_offset >= m_files[m_file].data.size())
                sendEot();
            else
                sendBlock();
        } else if (byte == NAK) {
            if (!retryOrFail(m_options.maxRetries, QStringLiteral("Too many errors: block %1 was rejected %2 times.")
                                                         .arg(m_block).arg(m_options.maxRetries + 1))) {
                send(m_lastBlock);
                arm(m_options.ackTimeoutMs + m_options.msPerByte * m_lastBlock.size());
            }
        }
        break;   // a stray 'C' is ignored: it was sent before the block arrived
    case Phase::Ending:
        if (byte == ACK) {
            m_retries = 0;
            m_doneBefore += m_files[m_file].data.size();
            m_bytesDone = m_doneBefore;
            ++m_file;
            m_offset = 0;
            m_phase = Phase::WaitingForReceiver;
            arm(m_options.ackTimeoutMs);
        } else if (byte == NAK) {
            // The protocol has the receiver NAK the first EOT; send it again.
            if (!retryOrFail(m_options.maxRetries, QStringLiteral("The receiver didn't acknowledge the end of the file.")))
                sendEot();
        }
        break;
    case Phase::Done:
        break;
    }
}

void Sender::sendHeader()
{
    FileHeader header;   // empty: the end of the batch
    if (m_file < m_files.size()) {
        header = m_files[m_file].header;
        m_currentFile = header.name;
    }
    const QByteArray payload = encodeHeader(header);
    m_lastBlock = encodeBlock(0, payload, payload.size() > 128 ? 1024 : 128, '\0');
    m_block = 0;
    send(m_lastBlock);
    m_phase = Phase::WaitingForHeaderAck;
    // The null block 0 needs no user: the normal ACK timeout (after which the batch is done anyway).
    arm((atBatchEnd() ? m_options.ackTimeoutMs : m_options.headerAckTimeoutMs) + m_options.msPerByte * m_lastBlock.size());
}

int Sender::blockLength() const
{
    const qint64 remaining = m_files[m_file].data.size() - m_offset;
    return m_options.use1K && remaining > 896 ? 1024 : 128;
}

void Sender::sendBlock()
{
    ++m_block;
    const QByteArray &data = m_files[m_file].data;
    const int length = blockLength();
    m_lastBlock = encodeBlock(m_block, QByteArrayView(data).sliced(m_offset, std::min<qint64>(length, data.size() - m_offset)),
                              length);
    send(m_lastBlock);
    m_phase = Phase::SendingData;
    arm(m_options.ackTimeoutMs + m_options.msPerByte * m_lastBlock.size());
}

void Sender::sendEot()
{
    m_lastBlock = QByteArray(1, EOT);
    send(EOT);
    m_phase = Phase::Ending;
    arm(m_options.ackTimeoutMs);
}

void Sender::timeout()
{
    if (finished())
        return;
    switch (m_phase) {
    case Phase::WaitingForReceiver:
        if (m_file == 0) {
            send(QByteArray(2, CAN));
            finish(Outcome::Failed, QStringLiteral("The receiver never started the transfer."));
        }
        else if (!retryOrFail(m_options.maxRetries, QStringLiteral("The receiver stopped answering.")))
            arm(m_options.ackTimeoutMs);
        break;
    case Phase::WaitingForHeaderAck:
        if (atBatchEnd()) {
            // Every file was acknowledged; a receiver may end without ACKing the null block 0
            // (lrzsz's rz does), and the 121GW ends its batch without that ACK too.
            m_phase = Phase::Done;
            finish(Outcome::Completed);
            break;
        }
        if (!retryOrFail(m_options.maxRetries, QStringLiteral("The receiver didn't accept the file."))) {
            send(m_lastBlock);
            arm(m_options.headerAckTimeoutMs + m_options.msPerByte * m_lastBlock.size());
        }
        break;
    case Phase::WaitingForDataStart:
        if (!retryOrFail(m_options.maxRetries, QStringLiteral("The receiver stopped answering.")))
            arm(m_options.ackTimeoutMs);
        break;
    case Phase::SendingData:
    case Phase::Ending:
        if (!retryOrFail(m_options.maxRetries, QStringLiteral("The receiver stopped answering."))) {
            send(m_lastBlock);
            arm(m_options.ackTimeoutMs + m_options.msPerByte * m_lastBlock.size());
        }
        break;
    case Phase::Done:
        break;
    }
}

// ── Receiver ────────────────────────────────────────────────────────────────

Receiver::Receiver(ReceiverOptions options) : m_options(options) {}

void Receiver::start()
{
    m_phase = Phase::WaitingForHeader;
    m_startTries = 0;
    requestHeader();
}

void Receiver::requestHeader()
{
    ++m_startTries;
    send(CRC_REQUEST);
    arm(m_options.startIntervalMs);
}

void Receiver::feed(QByteArrayView bytes)
{
    if (finished())
        return;
    if (m_purging) {
        arm(m_options.purgeQuietMs);   // still arriving: wait for quiet
        return;
    }
    m_buffer.append(bytes.data(), bytes.size());
    process();
}

void Receiver::process()
{
    while (!m_buffer.isEmpty() && !finished() && !m_purging) {
        const char first = m_buffer[0];
        if (first == SOH || first == STX) {
            const int length = (first == STX ? 1024 : 128) + 5;
            if (m_buffer.size() < length) {
                arm(m_options.interByteTimeoutMs);
                return;
            }
            const QByteArrayView block = QByteArrayView(m_buffer).first(length);
            const quint8 number = static_cast<quint8>(block[1]);
            const QByteArrayView payload = block.sliced(3, length - 5);
            const quint16 crc = static_cast<quint16>(static_cast<quint8>(block[length - 2]) << 8
                                                     | static_cast<quint8>(block[length - 1]));
            if (static_cast<quint8>(block[2]) != static_cast<quint8>(~number) || crc16(payload) != crc) {
                startPurge();
                return;
            }
            const QByteArray copy = payload.toByteArray();
            m_buffer.remove(0, length);
            handleBlock(number, copy);
        } else if (first == EOT) {
            m_buffer.remove(0, 1);
            canceledBy(first);
            handleEot();
        } else if (first == CAN) {
            m_buffer.remove(0, 1);
            if (canceledBy(first))
                finish(Outcome::RemoteCanceled, QStringLiteral("The sender canceled the transfer."));
        } else {
            canceledBy(first);
            m_buffer.remove(0, 1);   // noise between blocks
        }
    }
}

void Receiver::startPurge()
{
    // Throw away the rest of the bad block, wait for the line to go quiet, then NAK.
    m_buffer.clear();
    m_purging = true;
    arm(m_options.purgeQuietMs);
}

void Receiver::handleBlock(quint8 number, QByteArrayView payload)
{
    canceledBy(0);
    switch (m_phase) {
    case Phase::WaitingForHeader:
        if (number == 0)
            handleHeader(payload);
        else
            cancel(QStringLiteral("The sender started without a file name block (not YMODEM)."));
        break;
    case Phase::AwaitingApproval:
        break;   // block 0 again while the user decides: ignore
    case Phase::ReceivingData:
        if (number == m_expected) {
            m_retries = 0;
            m_eotSeen = false;
            m_data.append(payload.data(), payload.size());
            if (m_data.size() > m_options.maxFileSize + 1024) {
                cancel(QStringLiteral("The file is larger than %1 bytes.").arg(m_options.maxFileSize));
                return;
            }
            m_bytesDone = m_header.size >= 0 ? std::min<qint64>(m_data.size(), m_header.size) : m_data.size();
            ++m_expected;
            send(ACK);
            arm(m_options.blockTimeoutMs);
        } else if (number == static_cast<quint8>(m_expected - 1)) {
            // Our ACK was lost and the sender sent it again.
            send(ACK);
            if (number == 0 && m_data.isEmpty())
                send(CRC_REQUEST);   // block 0 again: its ACK was lost
            arm(m_options.blockTimeoutMs);
        } else {
            cancel(QStringLiteral("Block %1 arrived when %2 was expected.").arg(number).arg(m_expected));
        }
        break;
    case Phase::Done:
        break;
    }
}

void Receiver::handleHeader(QByteArrayView payload)
{
    const auto header = decodeHeader(payload);
    if (!header) {
        cancel(QStringLiteral("The file name block is malformed."));
        return;
    }
    m_retries = 0;
    if (header->name.isEmpty()) {
        send(ACK);
        m_phase = Phase::Done;
        finish(Outcome::Completed);
        return;
    }
    if (header->size > m_options.maxFileSize) {
        cancel(QStringLiteral("%1 is larger than %2 bytes.").arg(header->name).arg(m_options.maxFileSize));
        return;
    }
    m_header = *header;
    m_anyHeader = true;
    m_currentFile = m_header.name;
    m_bytesTotal = m_header.size;
    m_bytesDone = 0;
    if (m_options.approveHeaders) {
        m_phase = Phase::AwaitingApproval;
        disarm();
        return;
    }
    acceptHeader();
}

void Receiver::approve()
{
    if (m_phase == Phase::AwaitingApproval && !finished())
        acceptHeader();
}

void Receiver::reject(const QString &why)
{
    cancel(why);
}

void Receiver::acceptHeader()
{
    m_data.clear();
    m_expected = 1;
    m_eotSeen = false;
    m_phase = Phase::ReceivingData;
    send(ACK);
    send(CRC_REQUEST);
    arm(m_options.blockTimeoutMs);
}

void Receiver::handleEot()
{
    if (m_phase == Phase::WaitingForHeader && m_anyHeader) {
        send(ACK);   // our ACK of the EOT was lost
        return;
    }
    if (m_phase != Phase::ReceivingData)
        return;
    if (!m_eotSeen) {
        // NAK the first EOT: a noise byte that looks like EOT must not end the file.
        m_eotSeen = true;
        send(NAK);
        arm(m_options.blockTimeoutMs);
        return;
    }
    send(ACK);
    QByteArray data = m_data;
    if (m_header.size >= 0) {
        if (data.size() < m_header.size) {
            cancel(QStringLiteral("%1 ended after %2 of its %3 bytes.").arg(m_header.name).arg(data.size()).arg(m_header.size));
            return;
        }
        data.truncate(m_header.size);
    } else {
        while (data.endsWith(SUB))
            data.chop(1);
    }
    m_files << ReceivedFile{m_header, data};
    m_bytesDone = data.size();
    m_data.clear();
    m_retries = 0;
    m_phase = Phase::WaitingForHeader;
    m_startTries = 0;
    requestHeader();
}

void Receiver::timeout()
{
    if (finished())
        return;
    if (m_purging) {
        m_purging = false;
        if (!retryOrFail(m_options.maxRetries, QStringLiteral("Too many damaged blocks."))) {
            send(NAK);
            arm(m_options.blockTimeoutMs);
        }
        return;
    }
    switch (m_phase) {
    case Phase::WaitingForHeader: {
        // Part of block 0 arrived and then stopped: NAK it (a sender waiting for block 0's ACK
        // ignores 'C'). Nothing arrived: ask again.
        const bool partial = !m_buffer.isEmpty();
        m_buffer.clear();
        if (m_startTries >= m_options.startTries) {
            send(QByteArray(2, CAN));
            finish(Outcome::Failed, m_anyHeader ? QStringLiteral("The sender stopped answering.")
                                                : QStringLiteral("The sender never started the transfer."));
        } else if (partial) {
            ++m_startTries;
            send(NAK);
            arm(m_options.startIntervalMs);
        } else {
            requestHeader();
        }
        break;
    }
    case Phase::AwaitingApproval:
        break;
    case Phase::ReceivingData:
        m_buffer.clear();
        if (!retryOrFail(m_options.maxRetries, QStringLiteral("The sender stopped answering."))) {
            send(NAK);
            arm(m_options.blockTimeoutMs);
        }
        break;
    case Phase::Done:
        break;
    }
}

} // namespace readout::ymodem
