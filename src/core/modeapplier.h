// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#pragma once

#include "displaybackend.h"
#include "mode.h"

#include <QHash>
#include <QStringList>

#include <optional>

namespace Pmmt
{

/// Applies a Mode to its display: writes only the values that changed, in the
/// order from docs/development/architecture.md, "Applying a mode", and sets
/// brightness through KDE.
///
/// The applier remembers the values it wrote per display, so applying the
/// same mode again writes nothing (except `after_brightness` codes, which are
/// always written). It only trusts values from the last apply on a display:
/// codes the new mode doesn't set are forgotten, because the monitor may
/// change them by itself (e.g. writing an RGB gain switches the Acer SA243Y's
/// colour preset to User). Call forget() when the monitor may have reset,
/// e.g. after resume or hotplug.
class ModeApplier
{
public:
    enum class Error {
        None,
        /// The mode's display is not connected or not reachable.
        DisplayNotFound,
        /// A DDC/CI read or write, or the brightness sync, failed.
        DdcFailed,
    };

    struct Result {
        Error error = Error::None;
        /// Human-readable description of the error. Empty on success.
        QString message;
        /// The VCP code whose read or write failed.
        std::optional<VcpCode> failedCode;
        /// Codes written, in order. 0x10 is included when brightness was set,
        /// through KDE or over DDC/CI.
        QList<VcpCode> written;
        /// Problems that didn't stop the apply, e.g. brightness fallback or a
        /// read-back mismatch.
        QStringList warnings;

        bool ok() const
        {
            return error == Error::None;
        }
    };

    /// @p brightness may be null; brightness is then written over DDC/CI with
    /// a warning.
    ModeApplier(DisplayBackend &backend, BrightnessSync *brightness);

    /// Read back every value of the mode after writing and warn about
    /// differences. Off by default, since it doubles the bus traffic.
    void setVerify(bool verify);
    bool verify() const;

    /// Applies @p mode to @p display. @p mode.display must be @p display.key.
    /// Stops at the first failed write; values written before stay written.
    Result apply(const Mode &mode, const DisplayConfig &display);

    /// The values the applier believes the display has, by display ID.
    VcpValues knownValues(const QString &displayId) const;

    /// Drops what the applier knows about a display, so the next apply writes
    /// every value.
    void forget(const QString &displayId);

private:
    std::optional<VcpValue> brightnessMaximum(const QString &displayId, Result &result);
    bool write(const QString &displayId, VcpCode code, VcpValue value, Result &result);
    bool setBrightness(const DisplayInfo &display, VcpValue value, Result &result);

    DisplayBackend &m_backend;
    BrightnessSync *m_brightness;
    bool m_verify = false;
    QHash<QString, VcpValues> m_known;
    QHash<QString, VcpValue> m_brightnessMax;
};

} // namespace Pmmt
