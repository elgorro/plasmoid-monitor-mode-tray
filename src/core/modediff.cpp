// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "modediff.h"

#include <algorithm>

namespace Pmmt
{

namespace
{

// MCCS 2.2a: 0xDC "Display Application" selects the picture mode on many
// monitors (standard, movie, game, …), like a preset.
constexpr VcpCode DisplayApplication = 0xDC;

int writeGroup(VcpCode code)
{
    switch (code) {
    case Vcp::SelectColorPreset:
    case DisplayApplication:
        return 0;
    case Vcp::VideoGainRed:
    case Vcp::VideoGainGreen:
    case Vcp::VideoGainBlue:
        return 1;
    default:
        return 2;
    }
}

} // namespace

VcpValues diffValues(const VcpValues &target, const VcpValues &current)
{
    VcpValues changed;
    for (auto it = target.cbegin(); it != target.cend(); ++it) {
        const auto known = current.constFind(it.key());
        if (known == current.cend() || known.value() != it.value()) {
            changed.insert(it.key(), it.value());
        }
    }
    return changed;
}

QList<VcpCode> ddcWriteOrder(const VcpValues &values, const QList<VcpCode> &afterBrightness)
{
    QList<VcpCode> order;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.key() != Vcp::Luminance && !afterBrightness.contains(it.key())) {
            order.append(it.key());
        }
    }
    // QMap iterates by ascending code, and stable_sort keeps that order
    // inside each group.
    std::stable_sort(order.begin(), order.end(), [](VcpCode a, VcpCode b) {
        return writeGroup(a) < writeGroup(b);
    });
    return order;
}

} // namespace Pmmt
