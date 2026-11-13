// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#pragma once

#include <QMap>
#include <QString>
#include <QStringView>

#include <optional>

namespace Pmmt
{

/// A VCP feature code (VESA MCCS 2.2a). Codes are one byte.
using VcpCode = quint8;

/// A VCP value. DDC/CI 1.1 "Set VCP Feature" carries the value in two bytes
/// (SH, SL), so a value is 0…65535.
using VcpValue = quint16;

/// VCP code → value, sorted by code.
using VcpValues = QMap<VcpCode, VcpValue>;

/// VCP codes pmmt treats specially. Names follow VESA MCCS 2.2a.
namespace Vcp
{
constexpr VcpCode Luminance = 0x10;
constexpr VcpCode Contrast = 0x12;
constexpr VcpCode SelectColorPreset = 0x14;
constexpr VcpCode VideoGainRed = 0x16;
constexpr VcpCode VideoGainGreen = 0x18;
constexpr VcpCode VideoGainBlue = 0x1A;
} // namespace Vcp

/// The VCP code for a named config key (`brightness`, `contrast`, …), see
/// docs/development/architecture.md, "Data model". Empty for any other key.
std::optional<VcpCode> vcpCodeForKey(QStringView key);

/// The named config key for a VCP code, or an empty string if it has none.
QString keyForVcpCode(VcpCode code);

/// Parses a raw VCP code written as `0xNN` (exactly two hex digits, either
/// case), as used for `vcp."0xNN"` keys and `after_brightness`.
std::optional<VcpCode> parseVcpCode(QStringView text);

/// Formats a VCP code as `0xNN` with upper-case hex digits, e.g. `0xE8`.
QString formatVcpCode(VcpCode code);

} // namespace Pmmt
