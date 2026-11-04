// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Protocol/MeterKey.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "metermode.h"

#include <QString>
#include <stdexcept>

namespace readout {

const char *keyName(MeterKey key)
{
    switch (key) {
    case MeterKey::Range: return "RANGE";
    case MeterKey::Hold: return "HOLD";
    case MeterKey::Rel: return "REL";
    case MeterKey::Peak: return "1ms PEAK";
    case MeterKey::Mode: return "MODE";
    case MeterKey::MinMax: return "MIN/MAX";
    case MeterKey::Mem: return "MEM";
    case MeterKey::Setup: return "SETUP";
    }
    return "";
}

const char *keyId(MeterKey key)
{
    switch (key) {
    case MeterKey::Peak: return "PEAK";
    case MeterKey::MinMax: return "MINMAX";
    default: return keyName(key);
    }
}

bool keyFromName(const QString &name, MeterKey *key)
{
    // Letters and digits only, so "MIN/MAX", "minmax" and "Min Max" all match.
    QString n;
    for (const QChar c : name)
        if (c.isLetterOrNumber())
            n += c.toLower();
    static const struct { const char *name; MeterKey key; } names[] = {
        {"range", MeterKey::Range}, {"hold", MeterKey::Hold}, {"rel", MeterKey::Rel},
        {"peak", MeterKey::Peak}, {"1mspeak", MeterKey::Peak}, {"mode", MeterKey::Mode},
        {"minmax", MeterKey::MinMax}, {"mem", MeterKey::Mem}, {"setup", MeterKey::Setup},
    };
    for (const auto &entry : names) {
        if (n == QLatin1String(entry.name)) {
            *key = entry.key;
            return true;
        }
    }
    return false;
}

QByteArray keyPressCommand(MeterKey key, bool longPress)
{
    const auto number = static_cast<std::uint8_t>(key);
    if (number < 1 || number > 8)
        throw std::out_of_range("not a 121GW key");
    const char kind = longPress ? '8' : '0';
    const char code = static_cast<char>('0' + number);
    QByteArray command;
    command.append(static_cast<char>(0xF4));
    command.append(kind);
    command.append(code);
    command.append(kind);
    command.append(code);
    return command;
}

QByteArray setClockCommand(int year, int month, int day, int hour, int minute, int second)
{
    const int fields[] = {year % 100, month, day, hour, minute, second};
    QByteArray command(1, char(0xF8));
    int x = 0;
    for (int f : fields) {
        command += QByteArray::number(f, 16).rightJustified(2, '0').toUpper();
        x ^= f;
    }
    command += QByteArray::number(x, 16).rightJustified(2, '0').toUpper();
    return command;
}

} // namespace readout
