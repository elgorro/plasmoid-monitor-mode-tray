// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#pragma once

#include "vcp.h"

#include <QList>
#include <QString>

namespace Pmmt
{

/// A `[displays.<key>]` entry of modes.toml.
struct DisplayConfig {
    /// The table key in modes.toml, e.g. `acer`. Modes refer to it.
    QString key;
    /// Stable ID built from the EDID, `MFG:MODEL:SERIAL`,
    /// e.g. `ACR:SA243Y:1234ABCD`.
    QString id;
    /// Connector name, e.g. `DP-8`. Only a hint; may be empty.
    QString connector;
    /// Codes the monitor clears when brightness is written. Written after
    /// brightness, ascending. See architecture.md, "Write order".
    QList<VcpCode> afterBrightness;

    bool operator==(const DisplayConfig &) const = default;
};

/// A `[modes.<name>]` entry of modes.toml: a named set of VCP values for one
/// display.
struct Mode {
    QString name;
    /// Key of the DisplayConfig this mode applies to.
    QString display;
    VcpValues values;

    bool operator==(const Mode &) const = default;
};

} // namespace Pmmt
