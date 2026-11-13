// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#pragma once

#include "vcp.h"

#include <QList>
#include <QString>

#include <optional>

namespace Pmmt
{

/// A monitor as a backend sees it.
struct DisplayInfo {
    /// Stable ID built from the EDID, `MFG:MODEL:SERIAL`. Matches
    /// DisplayConfig::id.
    QString id;
    /// Human-readable name, e.g. the EDID model name.
    QString label;
    /// Connector name, e.g. `DP-8`, if known.
    QString connector;
    /// False if the display is known but doesn't answer DDC/CI right now.
    bool reachable = false;

    bool operator==(const DisplayInfo &) const = default;
};

/// Result of a VCP read. DDC/CI 1.1 "Get VCP Feature" replies carry both the
/// maximum and the current value.
struct VcpReading {
    VcpValue current = 0;
    VcpValue maximum = 0;

    bool operator==(const VcpReading &) const = default;
};

/// Reads and writes VCP codes on monitors. Implementations: DdcutilBackend
/// (real hardware, #15) and FakeBackend (tests).
///
/// Calls are synchronous and may block for a DDC/CI round trip (50–100 ms).
/// Callers run them off the UI thread. On failure the methods return
/// false/std::nullopt and put a message into @p error if it is not null.
class DisplayBackend
{
public:
    virtual ~DisplayBackend() = default;

    /// All displays the backend knows, reachable or not.
    virtual QList<DisplayInfo> displays() = 0;

    virtual std::optional<VcpReading> readVcp(const QString &displayId, VcpCode code, QString *error) = 0;

    virtual bool writeVcp(const QString &displayId, VcpCode code, VcpValue value, QString *error) = 0;
};

/// Sets the brightness that KDE (PowerDevil) stores for a display, so Plasma
/// doesn't later reset the monitor to an old value. Implementations:
/// KdeScreenBrightness (#16) and FakeBrightness (tests).
class BrightnessSync
{
public:
    enum class Result {
        /// KDE set the brightness (and wrote it to the monitor).
        Done,
        /// KDE has no matching display; the caller should write 0x10 itself.
        NoMatch,
        /// KDE has the display but setting the brightness failed.
        Failed,
    };

    virtual ~BrightnessSync() = default;

    /// Sets @p display to @p value on the monitor's VCP scale 0…@p maximum.
    /// The implementation scales it to KDE's range.
    virtual Result setBrightness(const DisplayInfo &display, VcpValue value, VcpValue maximum, QString *error) = 0;
};

} // namespace Pmmt
