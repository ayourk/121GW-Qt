// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Display/KeyInference.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "keyinference.h"

#include "dial.h"

namespace readout::keyinference {

namespace {

// The function MODE steps through: AC+DC on V is the AC mode with AC+DC coupling.
bool sameFunction(const MeterPacket &a, const MeterPacket &b)
{
    return a.mode() == b.mode() && (a.coupling() == Coupling::AcPlusDc) == (b.coupling() == Coupling::AcPlusDc);
}

// SETUP pages: numbered 100 and up, and the buzzer, ID and interval pages that come as 0. The VA
// position's small display switching between volts and amps by itself isn't one.
bool isSetupPage(MeterMode sub)
{
    return static_cast<int>(sub) >= 100 || sub == MeterMode::LowZ;
}

bool shareDialPosition(MeterMode a, MeterMode b)
{
    const QList<int> pa = dial::positionsFor(a);
    for (int p : dial::positionsFor(b))
        if (pa.contains(p))
            return true;
    return false;
}

} // namespace

std::optional<MeterKeyPress> infer(const MeterPacket &before, const MeterPacket &after)
{
    if (!sameFunction(before, after)) {
        if (shareDialPosition(before.mode(), after.mode()))
            return MeterKeyPress{MeterKey::Mode, false};
        return std::nullopt;
    }

    // These also switch AUTO off, so they come before RANGE.
    if (before.hold() != after.hold())
        return MeterKeyPress{MeterKey::Hold, false};
    if (before.lowPassFilter() != after.lowPassFilter())
        return MeterKeyPress{MeterKey::Rel, true};
    if (before.relative() != after.relative())
        return MeterKeyPress{MeterKey::Rel, false};
    // 1ms PEAK lights MAX too, so it comes before MIN/MAX.
    if (before.oneMsPeak() != after.oneMsPeak())
        return MeterKeyPress{MeterKey::Peak, false};
    if (before.minMax() != after.minMax())
        return MeterKeyPress{MeterKey::MinMax, after.minMax() == MinMaxState::Off};
    if (before.memory() != after.memory())
        return MeterKeyPress{MeterKey::Mem, false};
    if (before.subMode() != after.subMode() && (isSetupPage(before.subMode()) || isSetupPage(after.subMode())))
        return MeterKeyPress{MeterKey::Setup, false};

    if (before.autoRange() && !after.autoRange())
        return MeterKeyPress{MeterKey::Range, false};
    if (!before.autoRange() && after.autoRange())
        return MeterKeyPress{MeterKey::Range, true};
    if (!before.autoRange() && !after.autoRange() && before.mainRangeIndex() != after.mainRangeIndex())
        return MeterKeyPress{MeterKey::Range, false};
    return std::nullopt;
}

} // namespace readout::keyinference
