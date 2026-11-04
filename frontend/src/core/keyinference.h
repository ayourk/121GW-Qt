// SPDX-License-Identifier: MIT
// Ported from Readout (Readout.Core/Display/KeyInference.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#pragma once

#include "meterpacket.h"

#include <optional>

namespace readout {

// A key press on the meter: which key, and whether it was held.
struct MeterKeyPress {
    MeterKey key;
    bool longPress = false;
    bool operator==(const MeterKeyPress &) const = default;
};

// Works out which key was pressed on the meter itself from how the display changed. The meter
// never reports its keys, but nearly every press leaves a mark: HOLD and REL light their
// symbols, MIN/MAX steps MAX, MIN, AVG, SETUP moves the small display to another page, and MODE
// moves to another function on the same dial position. Turning the dial changes the mode too,
// so a mode change only counts as MODE within one dial position.
namespace keyinference {
std::optional<MeterKeyPress> infer(const MeterPacket &before, const MeterPacket &after);
}

} // namespace readout
