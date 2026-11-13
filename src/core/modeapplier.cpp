// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "modeapplier.h"

#include "modediff.h"

#include <algorithm>

namespace Pmmt
{

namespace
{

QString describe(VcpCode code)
{
    const QString key = keyForVcpCode(code);
    return key.isEmpty() ? formatVcpCode(code) : QStringLiteral("%1 (%2)").arg(formatVcpCode(code), key);
}

QString codeList(const QList<VcpCode> &codes)
{
    QStringList out;
    for (VcpCode code : codes) {
        out << formatVcpCode(code);
    }
    return out.join(QLatin1String(", "));
}

} // namespace

ModeApplier::ModeApplier(DisplayBackend &backend, BrightnessSync *brightness)
    : m_backend(backend)
    , m_brightness(brightness)
{
}

void ModeApplier::setVerify(bool verify)
{
    m_verify = verify;
}

bool ModeApplier::verify() const
{
    return m_verify;
}

VcpValues ModeApplier::knownValues(const QString &displayId) const
{
    return m_known.value(displayId);
}

void ModeApplier::forget(const QString &displayId)
{
    m_known.remove(displayId);
    m_brightnessMax.remove(displayId);
}

ModeApplier::Result ModeApplier::apply(const Mode &mode, const DisplayConfig &display)
{
    Result result;

    // Step 1: find the display.
    if (mode.display != display.key) {
        result.error = Error::DisplayNotFound;
        result.message = QStringLiteral("mode `%1` belongs to display `%2`, not `%3`").arg(mode.name, mode.display, display.key);
        return result;
    }
    std::optional<DisplayInfo> info;
    for (const auto &candidate : m_backend.displays()) {
        if (candidate.id == display.id) {
            info = candidate;
            break;
        }
    }
    if (!info || !info->reachable) {
        result.error = Error::DisplayNotFound;
        result.message = QStringLiteral("display `%1` (%2) is %3")
                             .arg(display.key, display.id, info ? QStringLiteral("not reachable over DDC/CI") : QStringLiteral("not connected"));
        return result;
    }
    const QString id = display.id;

    // Steps 2–3: diff against what we know. Only values confirmed in this
    // apply are kept afterwards; see the class comment.
    const VcpValues previous = m_known.take(id);
    const VcpValues changed = diffValues(mode.values, previous);
    VcpValues known;
    for (auto it = mode.values.cbegin(); it != mode.values.cend(); ++it) {
        if (!changed.contains(it.key())) {
            known.insert(it.key(), it.value());
        }
    }
    auto finish = [&]() -> Result {
        m_known.insert(id, known);
        return result;
    };

    // Step 4: everything except brightness and `after_brightness` codes.
    for (VcpCode code : ddcWriteOrder(changed, display.afterBrightness)) {
        if (!write(id, code, changed.value(code), result)) {
            return finish();
        }
        known.insert(code, changed.value(code));
    }

    // Step 5: brightness, through KDE.
    if (changed.contains(Vcp::Luminance)) {
        if (!setBrightness(*info, changed.value(Vcp::Luminance), result)) {
            return finish();
        }
        known.insert(Vcp::Luminance, changed.value(Vcp::Luminance));
    }

    // Step 6: codes the monitor clears when brightness or contrast is
    // written. Always written, since step 4 or 5 (or PowerDevil) may have
    // cleared them even if our cache says otherwise.
    QList<VcpCode> after;
    for (VcpCode code : display.afterBrightness) {
        if (mode.values.contains(code)) {
            after.append(code);
        }
    }
    std::sort(after.begin(), after.end());
    for (VcpCode code : after) {
        if (!write(id, code, mode.values.value(code), result)) {
            known.remove(code);
            return finish();
        }
        known.insert(code, mode.values.value(code));
    }
    if (!after.isEmpty() && mode.values.contains(Vcp::Luminance)) {
        // Some `after_brightness` codes set brightness themselves (Acer LBL).
        // Report a mismatch, but don't write 0x10 again: that would clear
        // them.
        QString error;
        if (const auto reading = m_backend.readVcp(id, Vcp::Luminance, &error)) {
            m_brightnessMax.insert(id, reading->maximum);
            known.insert(Vcp::Luminance, reading->current);
            if (reading->current != mode.values.value(Vcp::Luminance)) {
                result.warnings << QStringLiteral("after writing %1 the monitor reports brightness %2, but mode `%3` sets %4; set the mode's brightness to %2")
                                       .arg(codeList(after))
                                       .arg(reading->current)
                                       .arg(mode.name)
                                       .arg(mode.values.value(Vcp::Luminance));
            }
        } else {
            known.remove(Vcp::Luminance);
            result.warnings << QStringLiteral("could not read back brightness: %1").arg(error);
        }
    }

    // Step 7: optional read-back.
    if (m_verify) {
        for (auto it = mode.values.cbegin(); it != mode.values.cend(); ++it) {
            QString error;
            const auto reading = m_backend.readVcp(id, it.key(), &error);
            if (!reading) {
                known.remove(it.key());
                result.warnings << QStringLiteral("could not read back %1: %2").arg(describe(it.key()), error);
            } else {
                known.insert(it.key(), reading->current);
                if (reading->current != it.value()) {
                    result.warnings << QStringLiteral("%1 reads %2 after writing %3").arg(describe(it.key())).arg(reading->current).arg(it.value());
                }
            }
        }
    }

    return finish();
}

std::optional<VcpValue> ModeApplier::brightnessMaximum(const QString &displayId, Result &result)
{
    if (const auto it = m_brightnessMax.constFind(displayId); it != m_brightnessMax.cend()) {
        return it.value();
    }
    QString error;
    const auto reading = m_backend.readVcp(displayId, Vcp::Luminance, &error);
    if (!reading) {
        result.error = Error::DdcFailed;
        result.failedCode = Vcp::Luminance;
        result.message = QStringLiteral("reading %1 failed: %2").arg(describe(Vcp::Luminance), error);
        return std::nullopt;
    }
    m_brightnessMax.insert(displayId, reading->maximum);
    return reading->maximum;
}

bool ModeApplier::write(const QString &displayId, VcpCode code, VcpValue value, Result &result)
{
    QString error;
    if (m_backend.writeVcp(displayId, code, value, &error)) {
        result.written << code;
        return true;
    }
    result.error = Error::DdcFailed;
    result.failedCode = code;
    result.message = QStringLiteral("writing %1 = %2 failed: %3").arg(describe(code)).arg(value).arg(error);
    if (!result.written.isEmpty()) {
        result.message += QStringLiteral(" (already written: %1)").arg(codeList(result.written));
    }
    return false;
}

bool ModeApplier::setBrightness(const DisplayInfo &display, VcpValue value, Result &result)
{
    BrightnessSync::Result sync = BrightnessSync::Result::NoMatch;
    QString error;
    if (m_brightness) {
        const auto maximum = brightnessMaximum(display.id, result);
        if (!maximum) {
            return false;
        }
        sync = m_brightness->setBrightness(display, value, *maximum, &error);
    }

    switch (sync) {
    case BrightnessSync::Result::Done:
        result.written << Vcp::Luminance;
        return true;
    case BrightnessSync::Result::NoMatch:
        // architecture.md, "Brightness and KDE": fall back to DDC/CI.
        result.warnings << QStringLiteral("KDE doesn't know display `%1`; brightness was written over DDC/CI and KDE may overwrite it").arg(display.label);
        return write(display.id, Vcp::Luminance, value, result);
    case BrightnessSync::Result::Failed:
        break;
    }
    result.error = Error::DdcFailed;
    result.failedCode = Vcp::Luminance;
    result.message = QStringLiteral("setting brightness through KDE failed: %1").arg(error);
    if (!result.written.isEmpty()) {
        result.message += QStringLiteral(" (already written: %1)").arg(codeList(result.written));
    }
    return false;
}

} // namespace Pmmt
