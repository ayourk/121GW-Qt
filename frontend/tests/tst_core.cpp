// SPDX-License-Identifier: MIT
// Ported from Readout's tests (Readout.Tests: MeterPacketTests, PacketFramerTests, ReadingDecoderTests,
// SiFormatTests), Copyright (c) 2026 Alejandro Mora, MIT. The hex packets are captures from a real meter.
#include "meterpacket.h"
#include "packetbuilder.h"
#include "reading.h"
#include "dial.h"
#include "keyinference.h"
#include "measurementlog.h"
#include "sessionstore.h"
#include "simulatedmeter.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace readout;

namespace {

// Built by hand from the byte layout, not with PacketBuilder, so a shared mistake in the
// builder and the parser can't hide here.
QByteArray handBuilt()
{
    const unsigned char b[] = {
        0xF2,        // start
        0x11,        // 2017
        0xA1,        // month 10, serial digit 1
        0x23, 0x45,  // serial digits 2345
        0x01,        // DC volts, value bits 16-17 = 0
        0x41,        // negative, range 1
        0x30, 0x39,  // 12345
        0x05,        // sub: temperature
        0x01,        // sub: one decimal
        0x00, 0xEB,  // sub: 235
        0x01,        // bar on, scale 50
        0x06,        // bar value 6
        0x0C,        // status 1: DC, auto
        0x40,        // status 2: Bluetooth
        0x08,        // status 3: HOLD
        0xC1,        // XOR of the above
    };
    return QByteArray(reinterpret_cast<const char *>(b), sizeof b);
}

MeterReading decodeBuilt(const PacketBuilder &b, const QString &previous = {})
{
    return decoder::decode(b.buildPacket(), QDateTime::currentDateTimeUtc(), previous);
}

} // namespace

class TstCore : public QObject
{
    Q_OBJECT

private slots:
    // ── packet ──────────────────────────────────────────────────────────────
    void checksumIsXorOfFirst18()
    {
        QCOMPARE(MeterPacket::checksum(QByteArrayView(handBuilt()).first(18)), 0xC1);
    }

    void handBuiltPacketParses()
    {
        const auto p = MeterPacket::tryParse(handBuilt());
        QVERIFY(p);
        QCOMPARE(p->year(), 2017);
        QCOMPARE(p->month(), 10);
        QCOMPARE(p->serial(), 12345);
        QCOMPARE(p->mode(), MeterMode::DcVolts);
        QVERIFY(p->mainNegative());
        QVERIFY(!p->mainOverload());
        QCOMPARE(p->mainRangeIndex(), 1);
        QCOMPARE(p->mainRawValue(), 12345);
        QCOMPARE(p->subMode(), MeterMode::Temperature);
        QCOMPARE(p->subDecimals(), 1);
        QCOMPARE(p->subRawValue(), 235);
        QVERIFY(p->barOn());
        QCOMPARE(p->barScale(), BarScale::Fifty);
        QCOMPARE(p->barValue(), 6);
        QCOMPARE(p->coupling(), Coupling::Dc);
        QVERIFY(p->autoRange());
        QVERIFY(p->bluetooth());
        QCOMPARE(p->hold(), HoldState::Hold);
        QVERIFY(!p->relative());
        QCOMPARE(p->minMax(), MinMaxState::Off);
    }

    void badChecksumOrStartIsRejected()
    {
        QByteArray bad = handBuilt();
        bad[18] = static_cast<char>(0xC2);
        QVERIFY(!MeterPacket::tryParse(bad));
        QByteArray wrongStart = handBuilt();
        wrongStart[0] = 0x00;
        wrongStart[18] = static_cast<char>(MeterPacket::checksum(QByteArrayView(wrongStart).first(18)));
        QVERIFY(!MeterPacket::tryParse(wrongStart));
        QVERIFY(!MeterPacket::tryParse(QByteArray(18, '\0')));
        QVERIFY(!MeterPacket::tryParse(QByteArray(20, '\0')));
    }

    void eighteenBitValueAndTemperatureUnits()
    {
        PacketBuilder b;
        b.mode = MeterMode::Resistance;
        b.mainRawValue = 0x3FFFF;
        QCOMPARE(b.buildPacket().mainRawValue(), 0x3FFFF);
        PacketBuilder t;
        t.mode = MeterMode::Temperature;
        t.mainCelsius = true;
        QCOMPARE(t.buildPacket().mode(), MeterMode::TemperatureC);
        QCOMPARE(t.buildPacket().rawMode(), MeterMode::Temperature);
        t.mainCelsius = false;
        t.mainFahrenheit = true;
        QCOMPARE(t.buildPacket().mode(), MeterMode::TemperatureF);
    }

    void builderRoundTrips()
    {
        PacketBuilder b;
        b.serial = 98765; b.year = 2024; b.month = 3;
        b.mode = MeterMode::AcMilliamps; b.mainRangeIndex = 1; b.mainRawValue = 4321; b.mainOverload = true;
        b.subMode = MeterMode::Battery; b.subRawValue = 891; b.subDecimals = 4;
        b.subOverload = b.subNegative = b.subKilo = b.subHertz = true;
        b.barOn = false;
        const MeterPacket p = b.buildPacket();
        QCOMPARE(p.serial(), 98765);
        QCOMPARE(p.year(), 2024);
        QCOMPARE(p.month(), 3);
        QCOMPARE(p.mode(), MeterMode::AcMilliamps);
        QCOMPARE(p.mainRangeIndex(), 1);
        QCOMPARE(p.mainRawValue(), 4321);
        QVERIFY(p.mainOverload());
        QCOMPARE(p.subMode(), MeterMode::Battery);
        QCOMPARE(p.subRawValue(), 891);
        QCOMPARE(p.subDecimals(), 4);
        QVERIFY(p.subOverload() && p.subNegative() && p.subKilo() && p.subHertz());
        QVERIFY(!p.barOn());
    }

    void keyPressCommands()
    {
        QCOMPARE(keyPressCommand(MeterKey::Hold).toHex(), QByteArray("f430323032"));
        QCOMPARE(keyPressCommand(MeterKey::Setup, true).toHex(), QByteArray("f438383838"));
        MeterKey k;
        QVERIFY(keyFromName(QStringLiteral("Min/Max"), &k) && k == MeterKey::MinMax);
        QVERIFY(keyFromName(QStringLiteral("1ms peak"), &k) && k == MeterKey::Peak);
        QVERIFY(!keyFromName(QStringLiteral("power"), &k));
    }

    // ── framer ──────────────────────────────────────────────────────────────
    void framerJoinsSplitPackets()
    {
        PacketFramer f;
        const QByteArray p = handBuilt();
        QVERIFY(f.push(p.first(7)).isEmpty());
        const auto out = f.push(p.mid(7));
        QCOMPARE(out.size(), 1);
        QCOMPARE(out[0].bytes(), p);
    }

    void framerSplitsJoinedPacketsAndSkipsGarbage()
    {
        PacketFramer f;
        QByteArray stream = QByteArray("\x01\x02", 2) + handBuilt();
        QByteArray bad = handBuilt();
        bad[18] = static_cast<char>(0x00);
        stream += bad.first(10) + handBuilt();   // a broken frame, then a good one right after
        const auto out = f.push(stream);
        QCOMPARE(out.size(), 2);
        QCOMPARE(f.rejectedFrames() > 0, true);
    }

    // ── decoder ─────────────────────────────────────────────────────────────
    void decodesTheHandBuiltPacket()
    {
        const auto r = decoder::decode(*MeterPacket::tryParse(handBuilt()), QDateTime::currentDateTimeUtc());
        QCOMPARE(r.modeName, QStringLiteral("DC voltage"));
        QCOMPARE(r.mainText, QStringLiteral("12.345"));
        QCOMPARE(r.mainUnit, QStringLiteral("V"));
        QCOMPARE(r.mainLine(), QStringLiteral("-12.345 V"));
        QCOMPARE(r.sub.line(), QStringLiteral("23.5 °C"));
        QVERIFY(r.value && qFuzzyCompare(*r.value, -12.345));
    }

    void formatMain_data()
    {
        QTest::addColumn<int>("raw");
        QTest::addColumn<int>("integerDigits");
        QTest::addColumn<QString>("expected");
        QTest::newRow("12106/2") << 12106 << 2 << "12.106";
        QTest::newRow("5/1") << 5 << 1 << "0.0005";
        QTest::newRow("123/4") << 123 << 4 << "  12.3";
        QTest::newRow("0/1") << 0 << 1 << "0.0000";
        QTest::newRow("50/3") << 50 << 3 << "  0.50";
        QTest::newRow("99999/5") << 99999 << 5 << "99999";
        QTest::newRow("123456/2") << 123456 << 2 << "123.456";
    }
    void formatMain()
    {
        QFETCH(int, raw);
        QFETCH(int, integerDigits);
        QFETCH(QString, expected);
        QCOMPARE(decoder::formatMain(raw, integerDigits), expected);
    }

    void formatSub_data()
    {
        QTest::addColumn<int>("raw");
        QTest::addColumn<int>("decimals");
        QTest::addColumn<QString>("expected");
        QTest::newRow("5000/3") << 5000 << 3 << "5.000";
        QTest::newRow("5/2") << 5 << 2 << "0.05";
        QTest::newRow("235/1") << 235 << 1 << "23.5";
        QTest::newRow("42/0") << 42 << 0 << "42";
    }
    void formatSub()
    {
        QFETCH(int, raw);
        QFETCH(int, decimals);
        QFETCH(QString, expected);
        QCOMPARE(decoder::formatSub(raw, decimals), expected);
    }

    void ranges_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<int>("range");
        QTest::addColumn<int>("raw");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("unit");
        QTest::addColumn<double>("base");
        QTest::newRow("4.7 kΩ") << int(MeterMode::Resistance) << 2 << 47000 << "4.7000" << "kΩ" << 4700.0;
        QTest::newRow("12.345 MΩ") << int(MeterMode::Resistance) << 6 << 12345 << "12.345" << "MΩ" << 12345000.0;
        QTest::newRow("100 nF") << int(MeterMode::Capacitance) << 0 << 10000 << "100.00" << "nF" << 1e-7;
        QTest::newRow("µA") << int(MeterMode::DcMicroamps) << 1 << 12345 << "123.45" << "µA" << 123.45e-6;
        QTest::newRow("mA") << int(MeterMode::DcMilliamps) << 1 << 12345 << "12.345" << "mA" << 0.012345;
        QTest::newRow("A range 0") << int(MeterMode::DcAmps) << 0 << 12345 << "123.45" << "mA" << 0.12345;
        QTest::newRow("A range 1") << int(MeterMode::DcAmps) << 1 << 12345 << "1.2345" << "A" << 1.2345;
        QTest::newRow("pulse width") << int(MeterMode::PulseWidth) << 0 << 12345 << "1.2345" << "ms" << 0.0012345;
        QTest::newRow("kHz") << int(MeterMode::Frequency) << 3 << 50000 << "50.000" << "kHz" << 50000.0;
        QTest::newRow("mV") << int(MeterMode::DcMillivolts) << 0 << 12345 << "12.345" << "mV" << 0.012345;
        QTest::newRow("µVA") << int(MeterMode::AcMicroVoltAmps) << 0 << 12345 << "123.45" << "µVA" << 123.45e-6;
        QTest::newRow("VA") << int(MeterMode::DcVoltAmps) << 2 << 12345 << "12.345" << "VA" << 12.345;
    }
    void ranges()
    {
        QFETCH(int, mode);
        QFETCH(int, range);
        QFETCH(int, raw);
        QFETCH(QString, text);
        QFETCH(QString, unit);
        QFETCH(double, base);
        PacketBuilder b;
        b.mode = static_cast<MeterMode>(mode);
        b.mainRangeIndex = range;
        b.mainRawValue = raw;
        const auto r = decodeBuilt(b);
        QCOMPARE(r.mainText, text);
        QCOMPARE(r.mainUnit, unit);
        QVERIFY(r.baseValue);
        QVERIFY(qAbs(*r.baseValue - base) <= qAbs(base) * 1e-9);
    }

    void overloadAndUnknownModes()
    {
        PacketBuilder o;
        o.mode = MeterMode::Resistance; o.mainRangeIndex = 6; o.mainOverload = true;
        const auto r = decodeBuilt(o);
        QCOMPARE(r.mainText, QStringLiteral("OFL"));
        QVERIFY(!r.value);
        QCOMPARE(r.mainLine(), QStringLiteral("OFL MΩ"));
        PacketBuilder u;
        u.mode = static_cast<MeterMode>(30); u.mainRawValue = 123;
        const auto ur = decodeBuilt(u);
        QCOMPARE(ur.mainText, QStringLiteral("123"));
        QCOMPARE(ur.mainUnit, QString());
        QCOMPARE(ur.modeName, QStringLiteral("Mode 30"));
    }

    void fahrenheitMain()
    {
        PacketBuilder b;
        b.mode = MeterMode::Temperature; b.mainFahrenheit = true; b.mainRawValue = 721;
        const auto r = decodeBuilt(b);
        QCOMPARE(r.mainText, QStringLiteral("  72.1"));
        QCOMPARE(r.mainUnit, QStringLiteral("°F"));
        QCOMPARE(r.baseUnit, QStringLiteral("°F"));
    }

    void subPages_data()
    {
        QTest::addColumn<int>("subMode");
        QTest::addColumn<QString>("label");
        QTest::addColumn<QString>("unit");
        QTest::newRow("battery") << int(MeterMode::Battery) << "Battery" << "V";
        QTest::newRow("dBm") << int(MeterMode::Dbm) << "dBm" << "dBm";
        QTest::newRow("burden 156") << 156 << "Burden voltage" << "V";
    }
    void subPages()
    {
        QFETCH(int, subMode);
        QFETCH(QString, label);
        QFETCH(QString, unit);
        PacketBuilder b;
        b.subMode = static_cast<MeterMode>(subMode);
        const auto r = decodeBuilt(b);
        QCOMPARE(r.sub.label, label);
        QCOMPARE(r.sub.unit, unit);
    }

    void realMeterSetupPages_data()
    {
        QTest::addColumn<QByteArray>("hex");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("label");
        QTest::newRow("APO on") << QByteArray("F21780000000000000780200000B0002400056") << "APO.on" << "Auto power-off: on";
        QTest::newRow("year") << QByteArray("F21780000000000000820000130B00024000BD") << "2019" << "Year";
        QTest::newRow("date") << QByteArray("F2178000000000000087000A0E0B00024000AF") << "10-14" << "Date";
        QTest::newRow("time") << QByteArray("F217800000000000008C00102A0B000240009A") << "16-42" << "Time";
        QTest::newRow("contrast") << QByteArray("F21780000000000000A00000040B0002400088") << "4" << "LCD contrast";
    }
    void realMeterSetupPages()
    {
        QFETCH(QByteArray, hex);
        QFETCH(QString, text);
        QFETCH(QString, label);
        const auto p = MeterPacket::tryParse(QByteArray::fromHex(hex));
        QVERIFY(p);
        const auto r = decoder::decode(*p, QDateTime::currentDateTimeUtc());
        QCOMPARE(r.sub.text, text);
        QCOMPARE(r.sub.label, label);
    }

    void unnumberedPagesFollowThePageBefore()
    {
        PacketBuilder b;   // sub mode 0, value 0: buzzer, ID or interval
        QCOMPARE(decodeBuilt(b, QStringLiteral("Auto power-off: on")).sub.label, QStringLiteral("Buzzer"));
        QCOMPARE(decodeBuilt(b, QStringLiteral("Buzzer")).sub.label, QStringLiteral("Buzzer"));
        QCOMPARE(decodeBuilt(b, QStringLiteral("Time")).sub.label, QStringLiteral("Meter ID or interval"));
        QCOMPARE(decodeBuilt(b).sub.label, QStringLiteral("Meter ID or interval"));
    }

    // ── SI format ───────────────────────────────────────────────────────────
    void siFormat_data()
    {
        QTest::addColumn<double>("value");
        QTest::addColumn<QString>("unit");
        QTest::addColumn<QString>("expected");
        QTest::newRow("mV") << 0.0123 << "V" << "12.3 mV";
        QTest::newRow("V") << 12.345 << "V" << "12.35 V";
        QTest::newRow("kΩ") << 4700.0 << "Ω" << "4.7 kΩ";
        QTest::newRow("MΩ") << 12345000.0 << "Ω" << "12.35 MΩ";
        QTest::newRow("nF") << 1e-7 << "F" << "100 nF";
        QTest::newRow("-mA") << -0.5 << "A" << "-500 mA";
        QTest::newRow("zero") << 0.0 << "V" << "0 V";
        QTest::newRow("°C") << 23.45 << "°C" << "23.45 °C";
        QTest::newRow("%") << 50.0 << "%" << "50 %";
        QTest::newRow("0.9999 V") << 0.9999 << "V" << "0.9999 V";
    }
    void siFormat()
    {
        QFETCH(double, value);
        QFETCH(QString, unit);
        QFETCH(QString, expected);
        QCOMPARE(siformat::format(value, unit), expected);
    }

    void siFormatNaN()
    {
        QCOMPARE(siformat::format(std::nan(""), QStringLiteral("V")), QStringLiteral("– V"));
    }

    // The dial as firmware 2.05 has it: one VA position, the current jack picking A or mA.
    void dialPositionsFollowTheFirmware()
    {
        using namespace readout::dial;
        QCOMPARE(positions()[positionFor(MeterMode::DcMilliamps)].id, QStringLiteral("a"));
        QCOMPARE(positions()[positionFor(MeterMode::AcAmps)].id, QStringLiteral("a"));
        QCOMPARE(positions()[positionFor(MeterMode::DcMilliVoltAmps)].id, QStringLiteral("va"));
        QCOMPARE(positions()[positionFor(MeterMode::AcVoltAmps)].id, QStringLiteral("va"));
        QCOMPARE(positions()[positionFor(MeterMode::AcMicroVoltAmps)].id, QStringLiteral("ua"));
        QCOMPARE(positions()[positionFor(MeterMode::TemperatureF)].id, QStringLiteral("mv"));
        QCOMPARE(positionsFor(MeterMode::DcMilliVoltAmps).size(), 1);
        QCOMPARE(positionFor(MeterMode::SetupTemperatureC), -1);
        QCOMPARE(indexOf("off"), 0);
        QCOMPARE(indexOf("OHM"), indexOf("ohm"));
        QCOMPARE(indexOf("nope"), -1);
        // The firmware's default for each position comes first.
        QCOMPARE(positions()[indexOf("a")].functions.first().mode, MeterMode::DcMilliamps);
        QCOMPARE(positions()[indexOf("va")].functions.first().mode, MeterMode::DcVoltAmps);
    }

    void modePressesStayOnTheLeadsJack()
    {
        using namespace readout::dial;
        QCOMPARE(modePresses(indexOf("v"), 0, 2), 2);        // DC -> AC -> AC+DC
        QCOMPARE(modePresses(indexOf("v"), 2, 0), 1);        // AC+DC wraps to DC
        QCOMPARE(modePresses(indexOf("a"), 0, 1), 1);        // DC mA -> AC mA
        QCOMPARE(modePresses(indexOf("a"), 1, 0), 1);        // and back
        QCOMPARE(modePresses(indexOf("a"), 0, 2), -1);       // DC mA -> DC A: move the lead
        QCOMPARE(modePresses(indexOf("va"), 2, 3), 1);       // DC mVA -> AC mVA
        QCOMPARE(modePresses(indexOf("ua"), 1, 3), 2);       // AC µA -> DC µVA -> AC µVA
    }

    void jacksForEachKindOfMode()
    {
        using namespace readout::dial;
        auto jacks = [](const QList<JackUse> &uses) {
            QStringList ids;
            for (const JackUse &u : uses)
                ids << jackId(u.jack) + ":" + u.role;
            return ids.join(' ');
        };
        QCOMPARE(jacks(jacksForMode(MeterMode::DcVolts)), QStringLiteral("v:red com:black"));
        QCOMPARE(jacks(jacksForMode(MeterMode::AcAmps)), QStringLiteral("a:red com:black"));
        QCOMPARE(jacks(jacksForMode(MeterMode::DcMicroamps)), QStringLiteral("ma:red com:black"));
        QCOMPARE(jacks(jacksForMode(MeterMode::DcVoltAmps)),
                 QStringLiteral("a:red (current) v:red (voltage) com:black"));
        QCOMPARE(jacks(jacksForMode(MeterMode::AcMilliVoltAmps)),
                 QStringLiteral("ma:red (current) v:red (voltage) com:black"));
        QCOMPARE(jacksForPosition(indexOf("a")).size(), 3);   // either current jack, and COM
        QCOMPARE(jacks(jacksForPosition(indexOf("a"), 2)), QStringLiteral("a:red com:black"));
        QVERIFY(jacksForPosition(indexOf("off_right")).isEmpty());
    }

    // F8 + YY MM DD hh mm ss + XOR, each as two ASCII hex digits (firmware 2.05).
    void clockCommand()
    {
        const QByteArray c = readout::setClockCommand(2026, 10, 31, 12, 34, 56);
        QCOMPARE(c.size(), 15);
        QCOMPARE(static_cast<unsigned char>(c[0]), 0xF8);
        QCOMPARE(c.mid(1), QByteArray("1A0A1F0C223819"));
    }

    void csvRoundTrip()
    {
        QList<readout::Sample> samples;
        readout::Sample a;
        a.time = QDateTime(QDate(2026, 10, 31), QTime(18, 5, 1, 250), Qt::UTC);
        a.mode = MeterMode::DcVolts;
        a.value = 12.106;
        a.unit = "V";
        a.display = "12.106 V";
        readout::Sample b = a;
        b.time = a.time.addMSecs(1500);
        b.mode = MeterMode::Resistance;
        b.value.reset();
        b.unit = "Ω";
        b.display = "OFL, \"open\"";
        b.sub = "23.5 °C";
        samples << a << b;

        QString text;
        {
            QTextStream out(&text);
            readout::MeasurementLog::writeCsv(samples, out);
        }
        QVERIFY(text.startsWith(readout::MeasurementLog::CsvHeader + '\n'));
        QVERIFY(text.contains("2026-10-31T18:05:01.250Z,0.000,DcVolts,12.106,V,12.106 V,"));
        QVERIFY(text.contains("1.500,Resistance,,Ω,\"OFL, \"\"open\"\"\",23.5 °C"));

        QTextStream in(&text);
        const QList<readout::Sample> back = readout::MeasurementLog::readCsv(in);
        QCOMPARE(back.size(), 2);
        QCOMPARE(back[0].time, a.time);
        QCOMPARE(*back[0].value, 12.106);
        QCOMPARE(back[1].mode, MeterMode::Resistance);
        QVERIFY(!back[1].value);
        QCOMPARE(back[1].display, b.display);
        QCOMPARE(back[1].sub, b.sub);

        const auto stats = readout::MeasurementLog::statistics(back);
        QVERIFY(stats);
        QCOMPARE(stats->count, 1);
    }

    void sessionStoreSavesAndLists()
    {
        QTemporaryDir dir;
        readout::SessionStore store(dir.path());
        QList<readout::Sample> samples;
        for (int i = 0; i < 5; ++i) {
            readout::Sample s;
            s.time = QDateTime(QDate(2026, 10, 31), QTime(18, 0, i), Qt::UTC);
            s.mode = MeterMode::DcVolts;
            s.value = 5.0 + i * 0.01;
            s.unit = "V";
            s.display = QString::number(*s.value, 'f', 3) + " V";
            samples << s;
        }
        const readout::SessionInfo info = store.save("5 V rail", samples);
        QVERIFY(QFile::exists(store.csvPath(info.id)));
        const auto list = store.list();
        QCOMPARE(list.size(), 1);
        QCOMPARE(list[0].name, QStringLiteral("5 V rail"));
        QCOMPARE(list[0].sampleCount, 5);
        QCOMPARE(list[0].mainMode, MeterMode::DcVolts);
        QVERIFY(list[0].statistics);
        QCOMPARE(list[0].statistics->max, 5.04);
        QCOMPARE(store.loadSamples(info.id).size(), 5);
        store.remove(info.id);
        QVERIFY(store.list().isEmpty());
    }

    // The demo meter: the knob picks the group, the lead's jack picks A or mA, MODE toggles DC/AC.
    void demoMeterFollowsDialAndLead()
    {
        using namespace readout;
        SimulatedMeter m;
        auto modeNow = [&m] { return MeterPacket::tryParse(m.packetAt(1.0))->mode(); };
        QCOMPARE(modeNow(), MeterMode::DcVolts);
        m.turnDial(dial::indexOf("a"));
        QCOMPARE(modeNow(), MeterMode::DcAmps);          // no lead in mA·µA: the A functions
        m.setCurrentLead(Jack::MilliMicro);
        QCOMPARE(modeNow(), MeterMode::DcMilliamps);
        m.press(MeterKey::Mode);
        QCOMPARE(modeNow(), MeterMode::AcMilliamps);
        m.press(MeterKey::Mode);
        QCOMPARE(modeNow(), MeterMode::DcMilliamps);     // MODE stays on the mA functions
        m.setCurrentLead(Jack::Amps);
        QCOMPARE(modeNow(), MeterMode::DcAmps);
        m.turnDial(dial::indexOf("mv"));
        m.press(MeterKey::Mode);
        m.press(MeterKey::Mode);
        QCOMPARE(modeNow(), MeterMode::TemperatureF);
        m.turnDial(dial::indexOf("off_right"));
        QVERIFY(m.packetAt(1.0).isEmpty());
    }

    void modeChangeOnOnePositionIsMode()
    {
        using namespace readout;
        PacketBuilder b;
        b.mode = MeterMode::DcVolts;
        const MeterPacket dc = b.buildPacket();
        b.mode = MeterMode::AcVolts;
        const MeterPacket ac = b.buildPacket();
        b.mode = MeterMode::Resistance;
        const MeterPacket ohms = b.buildPacket();
        QCOMPARE(keyinference::infer(dc, ac), (MeterKeyPress{MeterKey::Mode, false}));
        QVERIFY(!keyinference::infer(dc, ohms));   // the dial turned
        b.mode = MeterMode::DcVolts;
        b.hold = HoldState::Hold;
        QCOMPARE(keyinference::infer(dc, b.buildPacket()), (MeterKeyPress{MeterKey::Hold, false}));
    }
};

QTEST_GUILESS_MAIN(TstCore)
#include "tst_core.moc"
