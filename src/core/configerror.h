// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

namespace Pmmt
{

/// A problem found while loading modes.toml.
struct ConfigError {
    QString file;
    /// 1-based; 0 if the error has no position (e.g. file not readable).
    int line = 0;
    /// 1-based; 0 if unknown.
    int column = 0;
    QString message;

    /// `file:line:column: message`, the GNU error message format, so editors
    /// and terminals can jump to the position. Parts that are unknown are
    /// left out.
    QString toString() const
    {
        QString out = file;
        if (line > 0) {
            out += QLatin1Char(':') + QString::number(line);
            if (column > 0) {
                out += QLatin1Char(':') + QString::number(column);
            }
        }
        return out + QLatin1String(": ") + message;
    }
};

} // namespace Pmmt
