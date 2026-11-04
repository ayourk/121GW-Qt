// SPDX-License-Identifier: MIT
// The 121GW's rotary dial and input jacks, from the EEVblog 121GW manual (front panel, Measuring and
// Power (VA) sections) and firmware 2.05 (the dial switch inputs, each position's MODE cycle, and the
// current-jack sensing that picks A or mA, VA or mVA).
#pragma once

#include "metermode.h"

#include <QList>
#include <QString>
#include <optional>

namespace readout {

// The four input jacks: three along the bottom, VΩ on its own above COM.
enum class Jack : std::uint8_t {
    Amps,        // "A 500mA": 500 mA, 5 A and 10 A ranges, 11 A HRC fuse
    MilliMicro,  // "mA µA": 50 mA, 500 µA and 50 µA ranges, 400 mA HRC fuse
    Com,         // "COM": the common (black) lead for everything
    VoltOhm,     // "VΩ": voltage, resistance, capacitance, diode, continuity, temperature
};

// What a lead in a jack is for.
struct JackUse {
    Jack jack;
    QString role;   // "black", "red", "red (current)", "red (voltage)"
    QString note;   // when it applies ("over 50 mA"), or empty
};

// One function MODE steps to at a dial position.
struct DialFunction {
    MeterMode mode;
    bool acPlusDc = false;   // AC+DC on V: the AC mode with AC+DC coupling
    QString label;           // "DC", "AC", "Temperature"…
    // On the A·mA and mVA/VA positions the meter picks the function by which current jack has a
    // lead in it (only mA·µA: mA or mVA; otherwise A or VA), and MODE only steps between DC and AC.
    std::optional<Jack> currentJack;
};

struct DialPosition {
    QString id;              // "off_left", "lowz", "v", "mv", "hz", "ohm", "va", "ua", "a", "off_right"
    QString legend;          // what's printed by the dial ("Low Z", "mVA/VA")
    QString redLegend;       // the RED legend: functions reached with MODE ("Temp", "ms %")
    double angle = 0;        // degrees counterclockwise from 3 o'clock: OFF right is 0, OFF left 180
    QList<DialFunction> functions;   // in MODE order; the first is the firmware's default
    bool isOff() const { return functions.isEmpty(); }
};

namespace dial {

const QList<DialPosition> &positions();
int indexOf(const QString &id);   // -1 if unknown; plain "off" is the left OFF

// The positions a main mode can be on, most likely first.
QList<int> positionsFor(MeterMode mode);
// Where the knob is for a reading in mode; -1 when the mode isn't on the dial.
int positionFor(MeterMode mode);
// MODE presses from one function of a position to another: -1 when MODE can't get there because
// the red lead has to move to the other current jack instead.
int modePresses(int position, int from, int to);

// The leads for a mode: black in COM, red where it measures; VA modes need a second red lead.
QList<JackUse> jacksForMode(MeterMode mode);
// The leads for a dial position before there's a reading: one function's, or with function -1
// every function's, with notes where it depends on the current ("over 50 mA").
QList<JackUse> jacksForPosition(int index, int function = -1);

QString jackId(Jack jack);    // "a", "ma", "com", "v"
QString jackLabel(Jack jack); // "A 500mA", "mA µA", "COM", "VΩ"

} // namespace dial

} // namespace readout
