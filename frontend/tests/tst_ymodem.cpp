// SPDX-License-Identifier: MIT
// YMODEM-1K and the card's command frames: a sender and a receiver wired back to back on a
// virtual clock, with a link that can damage, drop or delay bytes.
#include "cardprotocol.h"
#include "packetbuilder.h"
#include "ymodem.h"

#include <QRandomGenerator>
#include <QTest>
#include <functional>

using namespace readout;
using namespace readout::ymodem;

namespace {

QByteArray pattern(qsizetype size, quint32 seed = 7)
{
    QRandomGenerator rng(seed);
    QByteArray data(size, Qt::Uninitialized);
    for (char &c : data)
        c = static_cast<char>(rng.bounded(256));
    return data;
}

// Runs a sender and a receiver against each other. Bytes arrive at once unless a filter changes
// them; when neither side has anything to send, the earliest timer fires (virtual time jumps).
struct Link {
    Sender &sender;
    Receiver &receiver;
    // Called with every chunk on its way; returns what arrives. toReceiver: the sender's chunk.
    std::function<QByteArray(const QByteArray &chunk, bool toReceiver)> filter;
    // In AwaitingApproval the "user" answers after this long: true approves, false rejects.
    int approvalDelayMs = 0;
    bool approve = true;

    qint64 now = 0;
    qint64 senderDeadline = -1, receiverDeadline = -1, approvalAt = -1;
    QList<QByteArray> sent;       // the sender's chunks, as sent
    QList<QByteArray> answers;    // the receiver's chunks, as sent

    void takeTimers()
    {
        const TimerRequest s = sender.takeTimer();
        if (s.changed)
            senderDeadline = s.ms < 0 ? -1 : now + s.ms;
        const TimerRequest r = receiver.takeTimer();
        if (r.changed)
            receiverDeadline = r.ms < 0 ? -1 : now + r.ms;
    }

    QByteArray pass(const QByteArray &chunk, bool toReceiver)
    {
        return filter ? filter(chunk, toReceiver) : chunk;
    }

    void run(int maxSteps = 100000)
    {
        for (int step = 0; step < maxSteps; ++step) {
            takeTimers();
            const QByteArray fromSender = sender.takeOutput();
            const QByteArray fromReceiver = receiver.takeOutput();
            if (!fromSender.isEmpty()) {
                sent << fromSender;
                const QByteArray in = pass(fromSender, true);
                if (!in.isEmpty())
                    receiver.feed(in);
            }
            if (!fromReceiver.isEmpty()) {
                answers << fromReceiver;
                const QByteArray in = pass(fromReceiver, false);
                if (!in.isEmpty())
                    sender.feed(in);
            }
            if (!fromSender.isEmpty() || !fromReceiver.isEmpty())
                continue;
            if (sender.finished() && receiver.finished())
                return;
            if (receiver.phase() == Receiver::Phase::AwaitingApproval && approvalAt < 0 && !receiver.finished())
                approvalAt = now + approvalDelayMs;
            // The earliest of the timers and the user.
            qint64 next = -1;
            for (qint64 d : {senderDeadline, receiverDeadline, approvalAt})
                if (d >= 0 && (next < 0 || d < next))
                    next = d;
            if (next < 0)
                return;   // stuck: nothing will happen
            now = next;
            if (approvalAt == next) {
                approvalAt = -1;
                if (approve)
                    receiver.approve();
                else
                    receiver.reject(QStringLiteral("The user didn't confirm."));
            } else if (senderDeadline == next) {
                senderDeadline = -1;
                sender.timeout();
            } else {
                receiverDeadline = -1;
                receiver.timeout();
            }
        }
        QFAIL("the transfer never ended");
    }

    int blocksStartingWith(char first) const
    {
        int n = 0;
        for (const QByteArray &c : sent)
            n += !c.isEmpty() && c[0] == first;
        return n;
    }
    int headersSent() const
    {
        int n = 0;
        for (const QByteArray &c : sent)
            n += c.size() >= 3 && (c[0] == SOH || c[0] == STX) && c[1] == 0;
        return n;
    }
};

OutgoingFile file(const QString &name, const QByteArray &data)
{
    return {{name, data.size(), QDateTime::fromSecsSinceEpoch(1762000000)}, data};
}

} // namespace

class TstYmodem : public QObject
{
    Q_OBJECT

private slots:
    // ── pieces ──────────────────────────────────────────────────────────────
    void crcCheckValue()
    {
        QCOMPARE(crc16("123456789"), quint16(0x31C3));   // CRC-16/XMODEM's check value
        QCOMPARE(crc16(""), quint16(0));
    }

    void headerRoundTrip()
    {
        const FileHeader h{"EEVBLOG.BIN", 92160, QDateTime::fromSecsSinceEpoch(1762000000)};
        const QByteArray payload = encodeHeader(h);
        QVERIFY(payload.startsWith("EEVBLOG.BIN\0" "92160 "));
        const auto back = decodeHeader(payload);
        QVERIFY(back);
        QCOMPARE(back->name, h.name);
        QCOMPARE(back->size, h.size);
        QCOMPARE(back->modified.toSecsSinceEpoch(), h.modified.toSecsSinceEpoch());

        const auto end = decodeHeader(QByteArray(128, '\0'));
        QVERIFY(end && end->name.isEmpty());
        QVERIFY(!decodeHeader(QByteArray("NONUL")));
        const auto noSize = decodeHeader(QByteArray("A.TXT\0\0", 7));
        QVERIFY(noSize && noSize->size == -1);
        QVERIFY(!decodeHeader(QByteArray("A.TXT\0x12\0", 10)));
    }

    void blockLayout()
    {
        const QByteArray b = encodeBlock(1, "hi", 128);
        QCOMPARE(b.size(), 133);
        QCOMPARE(b[0], SOH);
        QCOMPARE(quint8(b[1]), quint8(1));
        QCOMPARE(quint8(b[2]), quint8(0xFE));
        QCOMPARE(b.mid(3, 2), QByteArray("hi"));
        QCOMPARE(b[5], SUB);
        const quint16 crc = crc16(b.mid(3, 128));
        QCOMPARE(quint8(b[131]), quint8(crc >> 8));
        QCOMPARE(quint8(b[132]), quint8(crc & 0xFF));
        QCOMPARE(encodeBlock(2, pattern(1024), 1024).size(), 1029);
        QCOMPARE(encodeBlock(2, pattern(1024), 1024)[0], STX);
    }

    // ── loopback ────────────────────────────────────────────────────────────
    void loopback_data()
    {
        QTest::addColumn<int>("size");
        QTest::addColumn<bool>("use1K");
        for (int size : {0, 1, 127, 128, 129, 896, 897, 1023, 1024, 1025, 3000, 92160 + 37, 146484, 257 * 1024 + 1}) {
            QTest::addRow("%d bytes, 1K", size) << size << true;
            QTest::addRow("%d bytes, 128", size) << size << false;
        }
    }

    void loopback()
    {
        QFETCH(int, size);
        QFETCH(bool, use1K);
        const QByteArray data = pattern(size);
        SenderOptions so;
        so.use1K = use1K;
        Sender sender({file("DATA.BIN", data)}, so);
        Receiver receiver;
        Link link{sender, receiver};
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(sender.outcome(), Outcome::Completed);
        QCOMPARE(receiver.outcome(), Outcome::Completed);
        QCOMPARE(receiver.files().size(), 1);
        QCOMPARE(receiver.files()[0].header.name, QStringLiteral("DATA.BIN"));
        QCOMPARE(receiver.files()[0].data.size(), data.size());   // cut to block 0's size
        QVERIFY(receiver.files()[0].data == data);
        QCOMPARE(receiver.files()[0].header.modified.toSecsSinceEpoch(), qint64(1762000000));
        QCOMPARE(sender.bytesDone(), qint64(size));
        QCOMPARE(receiver.bytesDone(), qint64(size));
        // Block sizes: 1K only when the option is on, and never for a tail of 896 bytes or less.
        const int stx = link.blocksStartingWith(STX);
        if (!use1K)
            QCOMPARE(stx, 0);
        else
            QCOMPARE(stx, size > 896 ? int((size - 1) / 1024 + (size % 1024 > 896 || size % 1024 == 0 ? 1 : 0)) : 0);
        QCOMPARE(link.blocksStartingWith(EOT), 2);   // the first EOT is NAKed
        if (size == 0)
            QCOMPARE(link.sent.size(), 4);   // block 0, EOT, EOT, null block 0
    }

    void batchOfTwo()
    {
        Sender sender({file("A.CSV", pattern(2000, 1)), file("B.CSV", pattern(10, 2))});
        Receiver receiver;
        Link link{sender, receiver};
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Completed);
        QCOMPARE(receiver.files().size(), 2);
        QVERIFY(receiver.files()[0].data == pattern(2000, 1));
        QVERIFY(receiver.files()[1].data == pattern(10, 2));
        QCOMPARE(sender.bytesDone(), qint64(2010));
        QCOMPARE(link.headersSent(), 3);   // two files and the end of the batch
    }

    // ── damage ──────────────────────────────────────────────────────────────
    void corruptedBlocksAreNakedAndResent()
    {
        const QByteArray data = pattern(5000);
        Sender sender({file("X.BIN", data)});
        Receiver receiver;
        Link link{sender, receiver};
        int chunk = 0;
        link.filter = [&chunk](const QByteArray &c, bool toReceiver) {
            if (!toReceiver)
                return c;
            QByteArray out = c;
            ++chunk;
            if (chunk == 3 || chunk == 4 || chunk == 1)   // block 0, then block 2 twice
                out[out.size() / 2] = static_cast<char>(out[out.size() / 2] ^ 0x40);
            return out;
        };
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Completed);
        QVERIFY(receiver.files().value(0).data == data);
        int naks = 0;
        for (const QByteArray &a : link.answers)
            naks += a.count(NAK);
        QCOMPARE(naks, 3 + 1);   // three damaged blocks and the first EOT
    }

    void corruptedHeaderBytes()
    {
        // Damage in the block number's complement, not just the data.
        const QByteArray data = pattern(300);
        Sender sender({file("X.BIN", data)});
        Receiver receiver;
        Link link{sender, receiver};
        bool done = false;
        link.filter = [&done](const QByteArray &c, bool toReceiver) {
            QByteArray out = c;
            if (toReceiver && !done && c.size() > 3 && c[1] == 1) {
                out[2] = 0;
                done = true;
            }
            return out;
        };
        sender.start();
        receiver.start();
        link.run();
        QVERIFY(receiver.files().value(0).data == data);
    }

    void droppedBytes()
    {
        const QByteArray data = pattern(4000);
        Sender sender({file("X.BIN", data)});
        Receiver receiver;
        Link link{sender, receiver};
        int chunk = 0;
        link.filter = [&chunk](const QByteArray &c, bool toReceiver) {
            if (!toReceiver)
                return c;
            ++chunk;
            QByteArray out = c;
            if (chunk == 2)
                out.remove(500, 1);        // a byte lost in the middle of block 1
            if (chunk == 5)
                out.remove(0, 1);          // a block's first byte lost
            if (chunk == 7)
                out.chop(1);               // its last
            return out;
        };
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Completed);
        QVERIFY(receiver.files().value(0).data == data);
        QCOMPARE(sender.outcome(), Outcome::Completed);
    }

    void lostAcksGiveDuplicates()
    {
        const QByteArray data = pattern(3000);
        Sender sender({file("X.BIN", data)});
        Receiver receiver;
        Link link{sender, receiver};
        int acks = 0;
        link.filter = [&acks](const QByteArray &c, bool toReceiver) {
            if (toReceiver || c != QByteArray(1, ACK))
                return c;
            ++acks;
            return acks == 2 ? QByteArray() : c;   // the ACK of block 1 is lost
        };
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Completed);
        QVERIFY(receiver.files().value(0).data == data);   // the duplicate isn't appended
    }

    void lostHeaderAck()
    {
        // Block 0's ACK (sent with the 'C') is lost: the sender times out and sends block 0 again.
        const QByteArray data = pattern(700);
        Sender sender({file("X.BIN", data)});
        Receiver receiver;
        Link link{sender, receiver};
        bool dropped = false;
        link.filter = [&dropped](const QByteArray &c, bool toReceiver) {
            if (!toReceiver && !dropped && c == QByteArray("\x06" "C", 2)) {
                dropped = true;
                return QByteArray();
            }
            return c;
        };
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Completed);
        QVERIFY(receiver.files().value(0).data == data);
        QVERIFY(link.headersSent() >= 2);
    }

    void lostFinalAckStillCompletes()
    {
        // The ACK of the null block 0 is lost: every file was acknowledged, so the batch ends.
        const QByteArray data = pattern(500);
        Sender sender({file("X.BIN", data)});
        Receiver receiver;
        Link link{sender, receiver};
        link.filter = [&receiver](const QByteArray &c, bool toReceiver) {
            if (!toReceiver && receiver.finished())
                return QByteArray();   // the receiver's last word (ACK of the null block 0)
            return c;
        };
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Completed);
        QCOMPARE(sender.outcome(), Outcome::Completed);
        QCOMPARE(link.headersSent(), 2);   // the null block 0 wasn't sent again
        QVERIFY(link.now < 20000);         // the ACK timeout, not block 0's 40 s ones
    }

    void noiseBeforeTheTransfer()
    {
        // Bytes left over from the meter's status packets (which pause during a transfer) are ignored.
        const QByteArray data = pattern(200);
        Sender sender({file("X.BIN", data)});
        Receiver receiver;
        Link link{sender, receiver};
        bool first = true;
        link.filter = [&first](const QByteArray &c, bool toReceiver) {
            if (toReceiver && first) {
                first = false;
                PacketBuilder b;
                return b.build() + c;
            }
            return c;
        };
        sender.start();
        receiver.start();
        link.run();
        QVERIFY(receiver.files().value(0).data == data);
    }

    // ── the meter's confirmation ────────────────────────────────────────────
    void delayedHeaderAck()
    {
        // The meter holds block 0's ACK while its user decides (up to 30 s): the sender waits
        // without sending block 0 again.
        const QByteArray data = pattern(2500);
        Sender sender({file("EEVBLOG.BIN", data)});
        ReceiverOptions ro;
        ro.approveHeaders = true;
        Receiver receiver(ro);
        Link link{sender, receiver};
        link.approvalDelayMs = 29000;
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(sender.outcome(), Outcome::Completed);
        QCOMPARE(receiver.files().value(0).data, data);
        QCOMPARE(receiver.files().value(0).header.name, QStringLiteral("EEVBLOG.BIN"));
        QCOMPARE(link.headersSent(), 2);   // the file's block 0 once, and the end of the batch
        QVERIFY(link.now >= 29000);
    }

    void rejectedWithCan()
    {
        Sender sender({file("CAL.BIN", pattern(1200))});
        ReceiverOptions ro;
        ro.approveHeaders = true;
        Receiver receiver(ro);
        Link link{sender, receiver};
        link.approvalDelayMs = 30000;
        link.approve = false;
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Canceled);
        QCOMPARE(sender.outcome(), Outcome::RemoteCanceled);
        QVERIFY(receiver.files().isEmpty());
        QCOMPARE(sender.bytesDone(), qint64(0));
    }

    void canFromTheReceiverMidway()
    {
        Sender sender({file("X.BIN", pattern(6000))});
        Receiver receiver;
        Link link{sender, receiver};
        int blocks = 0;
        link.filter = [&](const QByteArray &c, bool toReceiver) {
            if (toReceiver && c.size() > 1000 && ++blocks == 3)
                receiver.cancel(QStringLiteral("card full"));
            return c;
        };
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Canceled);
        QCOMPARE(receiver.errorString(), QStringLiteral("card full"));
        QCOMPARE(sender.outcome(), Outcome::RemoteCanceled);
        QVERIFY(sender.bytesDone() < 6000);
    }

    void cancelFromTheSender()
    {
        Sender sender({file("X.BIN", pattern(6000))});
        Receiver receiver;
        Link link{sender, receiver};
        int blocks = 0;
        link.filter = [&](const QByteArray &c, bool toReceiver) {
            if (toReceiver && c.size() > 1000 && ++blocks == 2)
                sender.cancel();
            return c;
        };
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(sender.outcome(), Outcome::Canceled);
        QCOMPARE(receiver.outcome(), Outcome::RemoteCanceled);
        QVERIFY(receiver.files().isEmpty());
    }

    void singleCanIsNoise()
    {
        Sender sender({file("X.BIN", pattern(2000))});
        Receiver receiver;
        Link link{sender, receiver};
        bool injected = false;
        link.filter = [&injected](const QByteArray &c, bool toReceiver) {
            if (!toReceiver && c == QByteArray(1, ACK) && !injected) {
                injected = true;
                return QByteArray(1, CAN) + c;
            }
            return c;
        };
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(sender.outcome(), Outcome::Completed);
    }

    void silentReceiverTimesOut()
    {
        Sender sender({file("X.BIN", pattern(100))});
        sender.start();
        const TimerRequest t = sender.takeTimer();
        QVERIFY(t.changed && t.ms > 0);
        sender.timeout();
        QCOMPARE(sender.outcome(), Outcome::Failed);
        QCOMPARE(sender.takeOutput(), QByteArray(2, CAN));
    }

    void silentSenderTimesOut()
    {
        ReceiverOptions ro;
        ro.startTries = 3;
        Receiver receiver(ro);
        receiver.start();
        for (int i = 0; i < 3; ++i)
            receiver.timeout();
        QCOMPARE(receiver.outcome(), Outcome::Failed);
        QCOMPARE(receiver.takeOutput(), QByteArray("CCC") + QByteArray(2, CAN));
    }

    void stubbornDamageGivesUp()
    {
        Sender sender({file("X.BIN", pattern(3000))});
        Receiver receiver;
        Link link{sender, receiver};
        link.filter = [](const QByteArray &c, bool toReceiver) {
            QByteArray out = c;
            if (toReceiver && c.size() > 1000)
                out[10] = static_cast<char>(out[10] ^ 1);
            return out;
        };
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Failed);
        QCOMPARE(sender.outcome(), Outcome::RemoteCanceled);
    }

    void tooLargeIsRefused()
    {
        ReceiverOptions ro;
        ro.maxFileSize = 1000;
        Sender sender({file("BIG.BIN", pattern(5000))});
        Receiver receiver(ro);
        Link link{sender, receiver};
        sender.start();
        receiver.start();
        link.run();
        QCOMPARE(receiver.outcome(), Outcome::Canceled);
        QCOMPARE(sender.outcome(), Outcome::RemoteCanceled);
    }

    // ── command frames ──────────────────────────────────────────────────────
    void commandFrameLayout()
    {
        // The firmware's layout: F6 LEN SEQ CMD payload CRC(hi, lo), CRC over F6..payload.
        const QByteArray f = card::commandFrame(card::Command::Send, 7, card::sendPayload("log.csv"));
        QCOMPARE(f.toHex(), QByteArray("f6090711") + QByteArray("0000") + QByteArray("LOG.CSV").toHex()
                                 + QByteArray::number(crc16(f.first(f.size() - 2)), 16).rightJustified(4, '0'));
        QCOMPARE(f.size(), 9 + 6);
        QCOMPARE(card::commandFrame(card::Command::List, 1, card::listPayload(258)).mid(4, 2), QByteArray("\x01\x02", 2));
        QCOMPARE(card::commandFrame(card::Command::Receive, 2).size(), 6);
        const auto send = card::parseSendPayload(card::sendPayload("eevblog.bin", true, 20));
        QVERIFY(send && send->blocks1K && send->pace == 20 && send->name == "EEVBLOG.BIN");
        QVERIFY(!card::parseSendPayload(QByteArray("\0\0", 2)));
        QCOMPARE(card::parseSendPayload(QByteArray("\0\x40" "A.B", 5))->pace, 20);   // taken as 20
        QVERIFY(!card::parseSendPayload(QByteArray("\x02\0" "A.B", 5)));          // unknown flag bit
        QCOMPARE(int(card::sendPayload("A.B", false, 99)[1]), 20);
    }

    void replyFramesAmongPackets()
    {
        const QByteArray reply = card::replyFrame(7, card::Status::Ok, card::sizePayload(92016));
        QCOMPARE(quint8(reply[0]), card::ReplyStart);
        QCOMPARE(int(reply[1]), 4);
        QCOMPARE(card::parseSizePayload(QByteArrayView(reply).sliced(4, 4)).value_or(-1), qint64(92016));
        card::FrameParser parser;
        // Among status packets and noise (a 0xD6 with an impossible length, one with a bad CRC),
        // split across notifications, with YMODEM's 'C' right behind it.
        const QByteArray stream = PacketBuilder().build() + QByteArray("\xD6\x50zz", 4) + QByteArray("\xD6\x01\x07\x00x\x12\x34", 7)
                                  + reply + "C";
        QList<card::Frame> frames;
        for (qsizetype i = 0; i < stream.size(); i += 5)
            frames += parser.push(QByteArrayView(stream).sliced(i, std::min<qsizetype>(5, stream.size() - i)));
        QCOMPARE(frames.size(), 1);
        QCOMPARE(frames[0].seq, quint8(7));
        QCOMPARE(frames[0].status(), card::Status::Ok);
        // The 'C' in the same notification as the reply is kept for YMODEM.
        parser.reset();
        QCOMPARE(parser.push(PacketBuilder().build() + reply + "C").size(), 1);
        QCOMPARE(parser.takeRest(), QByteArray("C"));
        QCOMPARE(parser.takeRest(), QByteArray());

        QByteArray bad = card::replyFrame(8, card::Status::NotFound);
        bad[bad.size() - 1] = static_cast<char>(bad[bad.size() - 1] ^ 1);
        QVERIFY(parser.push(bad).isEmpty());
        // The meter's side finds commands, not replies.
        card::FrameParser commands(card::CommandStart, card::MaxCommandPayload);
        QVERIFY(commands.push(reply).isEmpty());
        QCOMPARE(commands.push(card::commandFrame(card::Command::Abort, 9)).size(), 1);
    }

    void listEntries()
    {
        const card::CardFile f{"26110300.CSV", 123456, QDateTime(QDate(2026, 11, 3), QTime(14, 30, 2))};
        const QByteArray e = card::encodeEntry(f);
        QCOMPARE(e.size(), 9 + 12);   // LEN = 9 + the name's length
        const auto back = card::decodeEntry(e);
        QVERIFY(back);
        QCOMPARE(back->name, f.name);
        QCOMPARE(back->size, f.size);
        QCOMPARE(back->modified, f.modified);
        QVERIFY(!back->directory);
        QVERIFY(card::decodeEntry(card::encodeEntry({"LOGS", 0, {}, true}))->directory);
        QVERIFY(!card::decodeEntry(QByteArray(5, '\0')));
        QCOMPARE(card::parseListPayload(card::listPayload(300)).value_or(-1), 300);
    }

    void nameRules()
    {
        QVERIFY(card::isValidName("eevblog.bin"));
        QCOMPARE(card::normalizeName(" eevblog.bin "), QStringLiteral("EEVBLOG.BIN"));
        QVERIFY(card::isValidName("26110300.CSV"));
        QVERIFY(card::isValidName("README"));
        QString why;
        QVERIFY(!card::isValidName("toolongname.csv", &why));
        QVERIFY(!why.isEmpty());
        QVERIFY(!card::isValidName("a b.txt"));
        QVERIFY(!card::isValidName("a.text"));
        QVERIFY(!card::isValidName("dir/a.txt"));
        QVERIFY(!card::isValidName(""));
        QVERIFY(card::isProtected("cal.bin"));
        QVERIFY(card::isRefused("btrecv.tmp"));
        QVERIFY(card::isRefused("Cal.Bin"));
        QVERIFY(!card::isRefused("EEVBLOG.BIN"));
        QVERIFY(!card::isProtected("CAL.BAK"));
        QVERIFY(card::needsConfirmation("EEVBlog.bin", false));
        QVERIFY(card::needsConfirmation("LOG.CSV", true));
        QVERIFY(!card::needsConfirmation("LOG.CSV", false));
        QCOMPARE(card::suggestName("/home/x/My firmware v2.bin"), QStringLiteral("MYFIRMWA.BIN"));
        QCOMPARE(card::suggestName("EEVBlog2_05.bin"), QStringLiteral("EEVBLOG2.BIN"));
        QCOMPARE(card::suggestName("notes"), QStringLiteral("NOTES"));
        QVERIFY(card::isValidName(card::suggestName("ü.ü")));
    }

    void fatDateTime()
    {
        const QDateTime t(QDate(2026, 11, 3), QTime(15, 0, 4));
        const quint32 packed = card::toFatDateTime(t);
        QCOMPARE(card::fromFatDateTime(packed >> 16, packed & 0xFFFF), t);
        QVERIFY(!card::fromFatDateTime(0, 0).isValid());
    }
};

QTEST_GUILESS_MAIN(TstYmodem)
#include "tst_ymodem.moc"
