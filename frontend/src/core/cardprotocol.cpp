// SPDX-License-Identifier: MIT
#include "cardprotocol.h"

#include "ymodem.h"

#include <QRegularExpression>

namespace readout::card {

QString statusText(Status status)
{
    switch (status) {
    case Status::Ok: return QStringLiteral("OK");
    case Status::BadCommand: return QStringLiteral("The meter's firmware doesn't know that command.");
    case Status::BadArgument: return QStringLiteral("The meter can't use that name or request.");
    case Status::Busy: return QStringLiteral("The meter is busy: another transfer, its overwrite prompt, or logging to the "
                                             "card. Try again when it's done.");
    case Status::NotFound: return QStringLiteral("The meter has no such file.");
    case Status::SdError: return QStringLiteral("The meter couldn't read its SD card (is there one?).");
    case Status::End: return QStringLiteral("No more files.");
    }
    return QStringLiteral("The meter answered with an unknown status (0x%1).").arg(static_cast<int>(status), 2, 16, QLatin1Char('0'));
}

namespace {

QByteArray frame(quint8 start, quint8 seq, quint8 code, QByteArrayView payload)
{
    QByteArray out;
    out.append(static_cast<char>(start));
    out.append(static_cast<char>(payload.size()));
    out.append(static_cast<char>(seq));
    out.append(static_cast<char>(code));
    out.append(payload.data(), payload.size());
    const quint16 crc = ymodem::crc16(out);
    out.append(static_cast<char>(crc >> 8));
    out.append(static_cast<char>(crc & 0xFF));
    return out;
}

void appendBig(QByteArray &out, quint64 value, int bytes)
{
    for (int i = bytes - 1; i >= 0; --i)
        out.append(static_cast<char>((value >> (8 * i)) & 0xFF));
}

quint64 readBig(QByteArrayView in, qsizetype at, int bytes)
{
    quint64 v = 0;
    for (int i = 0; i < bytes; ++i)
        v = v << 8 | static_cast<quint8>(in[at + i]);
    return v;
}

} // namespace

QByteArray commandFrame(Command command, quint8 seq, QByteArrayView payload)
{
    return frame(CommandStart, seq, static_cast<quint8>(command), payload.first(std::min<qsizetype>(payload.size(), MaxCommandPayload)));
}

QByteArray replyFrame(quint8 seq, Status status, QByteArrayView payload)
{
    return frame(ReplyStart, seq, static_cast<quint8>(status), payload.first(std::min<qsizetype>(payload.size(), MaxReplyPayload)));
}

QList<Frame> FrameParser::push(QByteArrayView bytes)
{
    constexpr int MaxBuffered = 4096;
    m_buffer.append(bytes.data(), bytes.size());
    if (m_buffer.size() > MaxBuffered)
        m_buffer.remove(0, m_buffer.size() - (m_maxPayload + 6));

    QList<Frame> frames;
    qsizetype start = 0;
    qsizetype lastEnd = -1;
    QByteArray rest;
    while (true) {
        start = m_buffer.indexOf(static_cast<char>(m_start), start);
        if (start < 0 || m_buffer.size() - start < 2) {
            if (lastEnd >= 0)
                rest = m_buffer.mid(lastEnd);
            if (start < 0)
                m_buffer.clear();
            else
                m_buffer.remove(0, start);
            break;
        }
        const int len = static_cast<quint8>(m_buffer[start + 1]);
        if (len > m_maxPayload) {
            ++start;   // not a frame: the meter never sends (or takes) one this long
            continue;
        }
        const qsizetype total = len + 6;
        if (m_buffer.size() - start < total) {
            if (lastEnd >= 0)
                rest = m_buffer.mid(lastEnd);
            m_buffer.remove(0, start);
            break;
        }
        const QByteArrayView f = QByteArrayView(m_buffer).sliced(start, total);
        const quint16 crc = static_cast<quint16>(static_cast<quint8>(f[total - 2]) << 8 | static_cast<quint8>(f[total - 1]));
        if (ymodem::crc16(f.first(total - 2)) != crc) {
            ++start;
            continue;
        }
        frames << Frame{static_cast<quint8>(f[2]), static_cast<quint8>(f[3]), f.sliced(4, len).toByteArray()};
        start += total;
        lastEnd = start;
    }
    if (!frames.isEmpty())
        m_rest = rest;
    return frames;
}

QByteArray FrameParser::takeRest()
{
    QByteArray r;
    r.swap(m_rest);
    return r;
}

QByteArray sendPayload(const QString &name, bool blocks1K, int pace)
{
    QByteArray p;
    p.append(static_cast<char>(blocks1K ? 0x01 : 0x00));
    p.append(static_cast<char>(std::clamp(pace, 0, MaxPace)));
    p += normalizeName(name).toLatin1().left(12);
    return p;
}

std::optional<SendRequest> parseSendPayload(QByteArrayView payload)
{
    if (payload.size() < 3 || payload.size() > 14)
        return std::nullopt;
    if (static_cast<quint8>(payload[0]) & ~0x01)
        return std::nullopt;   // other flag bits must be 0
    SendRequest r;
    r.blocks1K = payload[0] & 0x01;
    // One notification at most: larger values are taken as 20; 0 is the default, 16.
    r.pace = std::min<int>(static_cast<quint8>(payload[1]), MaxPace);
    r.name = normalizeName(QString::fromLatin1(payload.sliced(2)));
    if (!isValidName(r.name))
        return std::nullopt;
    return r;
}

QByteArray sizePayload(qint64 size)
{
    QByteArray p;
    appendBig(p, static_cast<quint64>(std::clamp<qint64>(size, 0, 0xFFFFFFFF)), 4);
    return p;
}

std::optional<qint64> parseSizePayload(QByteArrayView payload)
{
    if (payload.size() < 4)
        return std::nullopt;
    return static_cast<qint64>(readBig(payload, 0, 4));
}

QByteArray listPayload(int index)
{
    QByteArray p;
    appendBig(p, static_cast<quint64>(std::clamp(index, 0, 0xFFFF)), 2);
    return p;
}

std::optional<int> parseListPayload(QByteArrayView payload)
{
    if (payload.size() != 2)
        return std::nullopt;
    return static_cast<int>(readBig(payload, 0, 2));
}

QByteArray encodeEntry(const CardFile &f)
{
    QByteArray e;
    e.append(static_cast<char>(f.directory ? 0x10 : 0x20));   // FAT: directory, or archive
    appendBig(e, static_cast<quint64>(std::clamp<qint64>(f.size, 0, 0xFFFFFFFF)), 4);
    appendBig(e, toFatDateTime(f.modified), 4);
    e += f.name.toLatin1().left(12);
    return e;
}

std::optional<CardFile> decodeEntry(QByteArrayView payload)
{
    if (payload.size() < 10)
        return std::nullopt;
    CardFile f;
    f.directory = payload[0] & 0x10;
    f.size = static_cast<qint64>(readBig(payload, 1, 4));
    f.modified = fromFatDateTime(static_cast<quint16>(readBig(payload, 5, 2)), static_cast<quint16>(readBig(payload, 7, 2)));
    f.name = QString::fromLatin1(payload.sliced(9));
    return f;
}

quint32 toFatDateTime(const QDateTime &when)
{
    if (!when.isValid())
        return 0;
    const QDateTime t = when.toLocalTime();
    const QDate d = t.date();
    const QTime tm = t.time();
    const quint16 date = static_cast<quint16>(std::clamp(d.year() - 1980, 0, 127) << 9 | d.month() << 5 | d.day());
    const quint16 time = static_cast<quint16>(tm.hour() << 11 | tm.minute() << 5 | tm.second() / 2);
    return static_cast<quint32>(date) << 16 | time;
}

QDateTime fromFatDateTime(quint16 date, quint16 time)
{
    if (date == 0)
        return {};
    const QDate d(1980 + (date >> 9), (date >> 5) & 0x0F, date & 0x1F);
    const QTime t((time >> 11) & 0x1F, (time >> 5) & 0x3F, (time & 0x1F) * 2);
    if (!d.isValid() || !t.isValid())
        return {};
    return QDateTime(d, t);
}

QString normalizeName(const QString &name)
{
    return name.trimmed().toUpper();
}

bool isValidName(const QString &name, QString *why)
{
    const auto fail = [why](const QString &message) {
        if (why)
            *why = message;
        return false;
    };
    const QString n = normalizeName(name);
    if (n.isEmpty())
        return fail(QStringLiteral("Give the file a name."));
    // The meter's 8.3 names: letters, digits and $ % ' - _ @ ~ ` ! ( ) { } ^ # &.
    static const QRegularExpression valid(QStringLiteral("^[A-Z0-9!#$%&'()\\-@^_`{}~]{1,8}(\\.[A-Z0-9!#$%&'()\\-@^_`{}~]{1,3})?$"));
    if (!valid.match(n).hasMatch())
        return fail(QStringLiteral("\"%1\" isn't a name the meter can store: up to 8 letters or digits, then optionally a dot "
                                   "and up to 3 more (8.3, no spaces).").arg(name.trimmed()));
    if (why)
        why->clear();
    return true;
}

bool isProtected(const QString &name)
{
    return normalizeName(name) == CalibrationFile;
}

bool isRefused(const QString &name, QString *why)
{
    const QString n = normalizeName(name);
    QString message;
    if (n == CalibrationFile)
        message = QStringLiteral("CAL.BIN can't be written over Bluetooth: the meter saves its calibration only itself "
                                 "(power on with MEM held, then REL).");
    else if (n == TemporaryFile)
        message = QStringLiteral("BTRECV.TMP is the meter's own temporary file for uploads; use another name.");
    if (why)
        *why = message;
    return !message.isEmpty();
}

bool needsConfirmation(const QString &name, bool exists)
{
    return exists || normalizeName(name) == FirmwareFile;
}

QString suggestName(const QString &localFileName)
{
    QString base = localFileName.section(QLatin1Char('/'), -1).toUpper();
    QString ext;
    const qsizetype dot = base.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) {
        ext = base.mid(dot + 1);
        base = base.left(dot);
    }
    static const QRegularExpression invalid(QStringLiteral("[^A-Z0-9!#$%&'()\\-@^_`{}~]"));
    base.remove(invalid);
    ext.remove(invalid);
    if (base.isEmpty())
        base = QStringLiteral("FILE");
    base.truncate(8);
    ext.truncate(3);
    return ext.isEmpty() ? base : base + QLatin1Char('.') + ext;
}

} // namespace readout::card
