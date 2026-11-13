// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#pragma once

#include "displaybackend.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QString>

#include <optional>

namespace Pmmt
{

/// In-memory monitors for unit and smoke tests. Reproduces the monitor-side
/// effects listed in a fixture (e.g. "writing 0x10 clears 0xE8"), so tests
/// catch wrong write orders. Never sleeps: delays only advance elapsedMs().
///
/// Fixture format: tests/fixtures/monitors/*.toml, described in
/// docs/development/unit-tests.md, "Fakes".
class FakeBackend : public DisplayBackend
{
public:
    struct Feature {
        VcpValue value = 0;
        /// Maximum for continuous features; 0 if the feature is
        /// non-continuous (then `allowed` lists the valid values).
        VcpValue maximum = 0;
        QList<VcpValue> allowed;
        bool writable = true;
    };

    /// When @c onWrite is written (with @c value, if set), the monitor sets
    /// the codes in @c sets by itself.
    struct Effect {
        VcpCode onWrite = 0;
        std::optional<VcpValue> value;
        VcpValues sets;
    };

    struct Write {
        QString displayId;
        VcpCode code = 0;
        VcpValue value = 0;

        bool operator==(const Write &) const = default;
    };

    /// Adds the display described by the fixture at @p path. Returns its ID,
    /// or an empty string and a message in @p error.
    QString loadFixture(const QString &path, QString *error);

    void addDisplay(const DisplayInfo &info, const QMap<VcpCode, Feature> &features = {}, const QList<Effect> &effects = {});
    void setFeature(const QString &displayId, VcpCode code, const Feature &feature);
    void setReachable(const QString &displayId, bool reachable);

    /// Makes every read and write of @p code fail, on all displays.
    void failOn(VcpCode code);
    void clearFailures();

    /// Simulated duration of each read and write.
    void setDelayMs(int ms);
    /// Simulated time spent in reads and writes so far.
    qint64 elapsedMs() const;

    /// Successful writes, in order. Writes from FakeBrightness are included.
    QList<Write> writes() const;
    void clearWrites();
    int readCount() const;

    /// The value the monitor holds now, without counting as a read.
    std::optional<VcpValue> value(const QString &displayId, VcpCode code) const;

    QList<DisplayInfo> displays() override;
    std::optional<VcpReading> readVcp(const QString &displayId, VcpCode code, QString *error) override;
    bool writeVcp(const QString &displayId, VcpCode code, VcpValue value, QString *error) override;

private:
    struct Monitor {
        DisplayInfo info;
        QMap<VcpCode, Feature> features;
        QList<Effect> effects;
    };

    Monitor *access(const QString &displayId, VcpCode code, QString *error);

    QList<Monitor> m_monitors;
    QList<VcpCode> m_failing;
    QList<Write> m_writes;
    int m_reads = 0;
    int m_delayMs = 0;
    qint64 m_elapsedMs = 0;
};

/// BrightnessSync stand-in. Records calls and, like PowerDevil, writes the
/// value to the monitor (through @p backend, if given) when it succeeds.
class FakeBrightness : public BrightnessSync
{
public:
    struct Call {
        QString displayId;
        VcpValue value = 0;
        VcpValue maximum = 0;

        bool operator==(const Call &) const = default;
    };

    explicit FakeBrightness(FakeBackend *backend = nullptr);

    /// What setBrightness() returns from now on. Default: Done.
    void setResult(Result result);

    QList<Call> calls() const;

    Result setBrightness(const DisplayInfo &display, VcpValue value, VcpValue maximum, QString *error) override;

private:
    FakeBackend *m_backend;
    Result m_result = Result::Done;
    QList<Call> m_calls;
};

} // namespace Pmmt
