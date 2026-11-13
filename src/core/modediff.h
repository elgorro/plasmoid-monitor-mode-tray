// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#pragma once

#include "vcp.h"

#include <QList>

namespace Pmmt
{

/// The values of @p target that need writing: those that differ from
/// @p current or are missing from it. Unknown current values count as
/// changed.
VcpValues diffValues(const VcpValues &target, const VcpValues &current);

/// The order in which ModeApplier writes the codes of @p values over DDC/CI,
/// see docs/development/architecture.md, "Applying a mode":
///
/// 1. colour preset (0x14) and vendor picture mode (0xDC, MCCS "Display
///    Application");
/// 2. RGB gains (0x16, 0x18, 0x1A);
/// 3. contrast (0x12) and every other code, ascending.
///
/// Brightness (0x10) and the codes in @p afterBrightness are left out; the
/// applier writes them last, in that order.
QList<VcpCode> ddcWriteOrder(const VcpValues &values, const QList<VcpCode> &afterBrightness);

} // namespace Pmmt
