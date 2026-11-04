// SPDX-License-Identifier: MIT
// YMODEM-1K (batch, CRC-16), written for 121gw-qt from the public protocol description (Chuck
// Forsberg, "XMODEM/YMODEM Protocol Reference", 1988); no code from other implementations.
#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QDateTime>
#include <QList>
#include <QString>
#include <optional>

namespace readout::ymodem {

// Control bytes.
constexpr char SOH = 0x01;   // a 128-byte block follows
constexpr char STX = 0x02;   // a 1024-byte block follows
constexpr char EOT = 0x04;   // end of file
constexpr char ACK = 0x06;
constexpr char NAK = 0x15;
constexpr char CAN = 0x18;   // two in a row cancel the transfer
constexpr char CRC_REQUEST = 'C';   // the receiver asks for a block in CRC-16 mode
constexpr char SUB = 0x1A;   // pads the last data block

// CRC-16/XMODEM: polynomial 0x1021, initial value 0, no reflection. Sent high byte first.
quint16 crc16(QByteArrayView data);

// Block 0: the file's name and size (and its modification time when known). An empty name
// marks the end of the batch.
struct FileHeader {
    QString name;
    qint64 size = -1;          // -1 when the sender didn't say
    QDateTime modified;        // invalid when the sender didn't say
};

// Block 0's payload: "name\0size mtime" (size in decimal, mtime in octal seconds since 1970,
// UTC), unpadded. An empty name gives the end-of-batch block.
QByteArray encodeHeader(const FileHeader &header);
// Reads block 0's payload; nullopt when it's malformed.
std::optional<FileHeader> decodeHeader(QByteArrayView payload);
// One whole block on the wire: SOH or STX, number, its complement, the payload padded to
// blockSize (128 or 1024) with pad, and the CRC.
QByteArray encodeBlock(quint8 number, QByteArrayView payload, int blockSize, char pad = SUB);

// How a transfer ended (or that it hasn't).
enum class Outcome { Running, Completed, Canceled, RemoteCanceled, Failed };

// A request to the host's timer: after every call into an engine, the host takes it, and when
// changed restarts its timer with ms (or stops it when ms < 0). When it fires, it calls timeout().
struct TimerRequest {
    bool changed = false;
    int ms = -1;
};

// What a sender and a receiver share: the bytes to send, the timer, how it ended, progress.
// Neither touches a transport or a clock: the host feeds received bytes in, writes takeOutput()
// to the link, and runs the timer.
class Engine
{
public:
    virtual ~Engine() = default;

    // Bytes for the link, oldest first; empties the queue.
    QByteArray takeOutput();
    TimerRequest takeTimer();

    virtual void feed(QByteArrayView bytes) = 0;
    virtual void timeout() = 0;
    // Stops the transfer from this end: sends CAN CAN; the outcome is Canceled.
    void cancel(const QString &why = QStringLiteral("Canceled."));

    Outcome outcome() const { return m_outcome; }
    bool finished() const { return m_outcome != Outcome::Running; }
    // Why it failed or was canceled; empty otherwise.
    QString errorString() const { return m_error; }

    // The file being transferred and how much of it has been acknowledged.
    QString currentFile() const { return m_currentFile; }
    qint64 bytesDone() const { return m_bytesDone; }
    qint64 bytesTotal() const { return m_bytesTotal; }   // -1 when unknown
    // Retries since the last good block (NAKs and timeouts).
    int retries() const { return m_retries; }

protected:
    void send(QByteArrayView bytes) { m_output.append(bytes.data(), bytes.size()); }
    void send(char byte) { m_output.append(byte); }
    void arm(int ms) { m_timer = {true, ms}; }
    void disarm() { m_timer = {true, -1}; }
    void finish(Outcome outcome, const QString &error = {});
    // Fails with CAN CAN once retries passes maxRetries; true when it did.
    bool retryOrFail(int maxRetries, const QString &error);
    // Counts CAN bytes in a row; true on the second.
    bool canceledBy(char byte);

    QString m_currentFile;
    qint64 m_bytesDone = 0;
    qint64 m_bytesTotal = -1;
    int m_retries = 0;

private:
    QByteArray m_output;
    TimerRequest m_timer;
    Outcome m_outcome = Outcome::Running;
    QString m_error;
    int m_cans = 0;
};

struct OutgoingFile {
    FileHeader header;   // the size is set from data
    QByteArray data;
};

struct SenderOptions {
    bool use1K = true;                 // 1024-byte blocks while more than 896 bytes remain
    int startTimeoutMs = 60000;        // for the receiver's first 'C'
    int ackTimeoutMs = 10000;          // for an ACK, plus msPerByte for every byte of the block
    // Block 0's ACK may wait for the receiver's user (the meter asks for SETUP for up to 30 s).
    int headerAckTimeoutMs = 40000;
    int msPerByte = 6;                 // the link's speed: ~190 B/s on the 121GW
    int maxRetries = 10;
};

// Sends a batch of files: waits for 'C', sends block 0, waits for its ACK (however long the
// receiver takes to accept), then 'C', the data blocks, EOT, and after the last file a null
// block 0. With every file acknowledged, a missing ACK for the null block 0 still completes.
class Sender : public Engine
{
public:
    enum class Phase { WaitingForReceiver, WaitingForHeaderAck, WaitingForDataStart, SendingData, Ending, Done };

    explicit Sender(QList<OutgoingFile> files, SenderOptions options = {});

    // Arms the timer for the receiver's 'C'.
    void start();
    void feed(QByteArrayView bytes) override;
    void timeout() override;
    Phase phase() const { return m_phase; }
    // Every file is sent: what remains is the end-of-batch block 0.
    bool atBatchEnd() const { return m_file >= m_files.size(); }
    // The batch's total bytes.
    qint64 batchBytes() const;

private:
    void handle(char byte);
    void sendHeader();
    void sendBlock();
    void sendEot();
    int blockLength() const;

    QList<OutgoingFile> m_files;
    SenderOptions m_options;
    Phase m_phase = Phase::WaitingForReceiver;
    int m_file = 0;            // index into m_files; == size() for the end-of-batch header
    qint64 m_offset = 0;       // into the current file, of the block being sent
    quint8 m_block = 0;
    QByteArray m_lastBlock;    // to resend
    qint64 m_doneBefore = 0;   // bytes of the files already sent
};

struct ReceiverOptions {
    int startIntervalMs = 3000;        // 'C' again until the sender starts (the 121GW: no faster than 5 s)
    int startTries = 10;
    int blockTimeoutMs = 10000;        // for the next block after an ACK
    int interByteTimeoutMs = 2000;     // a block that stops arriving part way
    int purgeQuietMs = 1000;           // after a bad block: the line goes quiet, then NAK
    int maxRetries = 10;
    qint64 maxFileSize = 16 * 1024 * 1024;
    // Holds block 0's ACK: phase AwaitingApproval until approve() or reject() (the meter asks
    // its user first). Off: every file is accepted at once.
    bool approveHeaders = false;
};

struct ReceivedFile {
    FileHeader header;
    QByteArray data;
};

// Receives a batch: sends 'C', takes block 0, the data blocks (128 or 1024 bytes, CRC-16) and
// EOT (NAKs the first EOT, as the protocol suggests), cuts each file to the size block 0 gave,
// and ends on a null block 0.
class Receiver : public Engine
{
public:
    enum class Phase { WaitingForHeader, AwaitingApproval, ReceivingData, Done };

    explicit Receiver(ReceiverOptions options = {});

    // Sends the first 'C'.
    void start();
    void feed(QByteArrayView bytes) override;
    void timeout() override;
    Phase phase() const { return m_phase; }

    // In AwaitingApproval: the file the sender offers.
    const FileHeader &pendingHeader() const { return m_header; }
    // Accepts the pending file: ACK, then 'C' for its data.
    void approve();
    // Refuses it: CAN CAN; the outcome is Canceled.
    void reject(const QString &why);

    // The files received so far, complete.
    const QList<ReceivedFile> &files() const { return m_files; }

private:
    void process();
    void handleBlock(quint8 number, QByteArrayView payload);
    void handleHeader(QByteArrayView payload);
    void handleEot();
    void acceptHeader();
    void startPurge();
    void requestHeader();

    ReceiverOptions m_options;
    Phase m_phase = Phase::WaitingForHeader;
    QByteArray m_buffer;
    bool m_purging = false;
    int m_startTries = 0;
    FileHeader m_header;
    QByteArray m_data;
    quint8 m_expected = 1;
    bool m_eotSeen = false;
    bool m_anyHeader = false;
    QList<ReceivedFile> m_files;
};

} // namespace readout::ymodem
