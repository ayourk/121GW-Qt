// SPDX-License-Identifier: MIT
// The meter's SD card over Bluetooth (custom firmware): the command and reply frames that list
// the card and start or abort a YMODEM transfer, and the rules about which files may be written.
//
// Every detail of the firmware's command frames is in this file and its .cpp, following the
// firmware's wire protocol (firmware-re/docs/ble-ymodem.md, "Wire protocol"). The YMODEM
// transfer itself (ymodem.h) is standard.
#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QDateTime>
#include <QList>
#include <QString>
#include <optional>

namespace readout::card {

// Command (app -> meter):  0xF6 | LEN | SEQ | CMD    | payload (LEN, 0-64) | CRC-16
// Reply   (meter -> app):  0xD6 | LEN | SEQ | STATUS | payload (LEN, 0-32) | CRC-16
//
// CRC-16/XMODEM (as YMODEM's) over every byte from the start byte through the payload, high
// byte first. The reply echoes the command's SEQ. A frame with a bad CRC gets no reply: the app
// repeats it with the same SEQ. While a transfer runs, a repeat of the frame that started it
// gets the start reply again (nothing starts twice); any other repeat is run again (INFO, LIST
// and ABORT don't mind). Outside a transfer, replies and status packets never interleave.
constexpr quint8 CommandStart = 0xF6;
constexpr quint8 ReplyStart = 0xD6;
constexpr int MaxCommandPayload = 64;
constexpr int MaxReplyPayload = 32;
// Send's PACE: bytes per 100 ms, one BLE notification at most.
constexpr int MaxPace = 20;
// After a transfer the meter drops F4 (key) and F8 (clock) frames until no byte has reached it
// for this long; command frames still work.
constexpr int QuietPeriodMs = 1000;

enum class Command : quint8 {
    Info = 0x01,      // reply: protocol version, max command payload, feature bits (bit 0: YMODEM)
    Receive = 0x10,   // the meter receives a batch (upload); the names are in each block 0
    Send = 0x11,      // the meter sends a file (download); payload FLAGS PACE NAME; reply: size (4, BE)
    List = 0x12,      // payload INDEX (2, BE); reply: one entry, or End past the last
    Abort = 0x1F,     // ends a transfer or the meter's prompt; outside a transfer a no-op
};

enum class Status : quint8 {
    Ok = 0x00,
    BadCommand = 0x01,
    BadArgument = 0x02,   // a payload or name the meter can't use
    Busy = 0x03,          // a transfer, the overwrite prompt, or logging to the card
    NotFound = 0x04,
    SdError = 0x05,       // no card, or it can't be mounted or read
    End = 0x06,           // List: no entry at that index
};
QString statusText(Status status);

// One frame, either way: code is the command (in a command) or the status (in a reply).
struct Frame {
    quint8 seq = 0;
    quint8 code = 0;
    QByteArray payload;

    Command command() const { return static_cast<Command>(code); }
    Status status() const { return static_cast<Status>(code); }
};

QByteArray commandFrame(Command command, quint8 seq, QByteArrayView payload = {});
QByteArray replyFrame(quint8 seq, Status status, QByteArrayView payload = {});

// Finds frames of one kind (commands or replies) in a byte stream that also carries the
// meter's 0xF2 status packets: a candidate whose length or CRC is wrong is skipped one byte at
// a time, as PacketFramer does.
class FrameParser
{
public:
    // Replies (the app's side) by default.
    explicit FrameParser(quint8 startByte = ReplyStart, int maxPayload = MaxReplyPayload)
        : m_start(startByte), m_maxPayload(maxPayload) {}
    QList<Frame> push(QByteArrayView bytes);
    void reset() { m_buffer.clear(); }
    // The bytes after the last frame found, still buffered (YMODEM's first bytes can follow the
    // reply in the same notification).
    QByteArray takeRest();

private:
    quint8 m_start;
    int m_maxPayload;
    QByteArray m_buffer;
    QByteArray m_rest;
};

// A file (or folder) on the card.
struct CardFile {
    QString name;          // 8.3, upper case
    qint64 size = 0;
    QDateTime modified;    // invalid when unknown
    bool directory = false;
};

// Send's payload: FLAGS (bit 0: 1024-byte blocks; else 128; other bits 0), PACE (bytes per
// 100 ms the meter sends, 1-20; 0: its default, 16), the name.
QByteArray sendPayload(const QString &name, bool blocks1K = false, int pace = 0);
struct SendRequest {
    bool blocks1K = false;
    int pace = 0;
    QString name;
};
std::optional<SendRequest> parseSendPayload(QByteArrayView payload);
// Send's reply payload: the file's size.
QByteArray sizePayload(qint64 size);
std::optional<qint64> parseSizePayload(QByteArrayView payload);

// List's payload (the index) and its reply's entry: ATTR (FAT attributes; 0x10 a folder),
// SIZE (4, BE), FAT date (2, BE), FAT time (2, BE), the name.
QByteArray listPayload(int index);
std::optional<int> parseListPayload(QByteArrayView payload);
QByteArray encodeEntry(const CardFile &file);
std::optional<CardFile> decodeEntry(QByteArrayView payload);

// FAT's packed date and time (the meter's local time, two-second resolution).
quint32 toFatDateTime(const QDateTime &time);
QDateTime fromFatDateTime(quint16 date, quint16 time);

// ── The meter's rules for files ─────────────────────────────────────────────

// The firmware image the meter's bootloader flashes from the card.
inline const QString FirmwareFile = QStringLiteral("EEVBLOG.BIN");
// The calibration backup: never written over Bluetooth.
inline const QString CalibrationFile = QStringLiteral("CAL.BIN");
// Where the meter keeps an upload until it is complete; it refuses this name.
inline const QString TemporaryFile = QStringLiteral("BTRECV.TMP");

// The name as the card stores it: trimmed, upper case. The meter's FAT driver has no long names.
QString normalizeName(const QString &name);
// Whether name is a valid 8.3 name for the card; why not in *why.
bool isValidName(const QString &name, QString *why = nullptr);
// CAL.BIN: the meter refuses to write it over Bluetooth.
bool isProtected(const QString &name);
// Names the meter refuses to receive: CAL.BIN and its own temporary file. Why in *why.
bool isRefused(const QString &name, QString *why = nullptr);
// The meter asks its user (SETUP) before writing this file: it exists, or it is the firmware.
bool needsConfirmation(const QString &name, bool exists);
// A suggested 8.3 name for a file from this computer ("My log.csv" -> "MYLOG.CSV").
QString suggestName(const QString &localFileName);

} // namespace readout::card
