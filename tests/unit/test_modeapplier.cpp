// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "fakebackend.h"
#include "modeapplier.h"

#include <QTest>

using namespace Pmmt;
using namespace Qt::StringLiterals;

namespace
{

const QString acerId = u"ACR:SA243Y G0:FAKE0001"_s;

DisplayConfig acerConfig(QList<VcpCode> afterBrightness = {0xE8})
{
    return DisplayConfig{u"acer"_s, acerId, u"DP-8"_s, afterBrightness};
}

Mode mode(const QString &name, const VcpValues &values)
{
    return Mode{name, u"acer"_s, values};
}

QList<VcpCode> writtenCodes(const FakeBackend &backend)
{
    QList<VcpCode> codes;
    for (const auto &write : backend.writes()) {
        codes << write.code;
    }
    return codes;
}

/// The Acer SA243Y fixture, with FakeBrightness writing 0x10 like PowerDevil.
struct Rig {
    FakeBackend backend;
    FakeBrightness brightness{&backend};
    ModeApplier applier{backend, &brightness};

    Rig()
    {
        QString error;
        const QString id = backend.loadFixture(QStringLiteral(PMMT_FIXTURES_DIR "/monitors/acer-sa243y.toml"), &error);
        if (id != acerId) {
            qFatal("cannot load fixture: %s", qPrintable(error));
        }
    }
};

} // namespace

class TestModeApplier : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesAllValuesFirstTime()
    {
        Rig rig;
        const auto result = rig.applier.apply(mode(u"day"_s, {{0x10, 100}, {0x12, 60}}), acerConfig());
        QVERIFY2(result.ok(), qPrintable(result.message));
        QCOMPARE(result.written, (QList<VcpCode>{0x12, 0x10}));
        QCOMPARE(rig.backend.value(acerId, 0x10), 100);
        QCOMPARE(rig.backend.value(acerId, 0x12), 60);
        QVERIFY(result.warnings.isEmpty());
    }

    void applySkipsUnchangedValues()
    {
        Rig rig;
        const Mode day = mode(u"day"_s, {{0x10, 100}, {0x12, 60}});
        QVERIFY(rig.applier.apply(day, acerConfig()).ok());
        rig.backend.clearWrites();
        const int reads = rig.backend.readCount();

        const auto result = rig.applier.apply(day, acerConfig());
        QVERIFY(result.ok());
        QVERIFY(result.written.isEmpty());
        QVERIFY(rig.backend.writes().isEmpty());
        QCOMPARE(rig.backend.readCount(), reads);
        QCOMPARE(rig.brightness.calls().size(), 1);
    }

    void onlyChangedValuesAreWritten()
    {
        Rig rig;
        QVERIFY(rig.applier.apply(mode(u"a"_s, {{0x10, 100}, {0x12, 60}}), acerConfig()).ok());
        rig.backend.clearWrites();
        QVERIFY(rig.applier.apply(mode(u"a2"_s, {{0x10, 100}, {0x12, 40}}), acerConfig()).ok());
        QCOMPARE(writtenCodes(rig.backend), QList<VcpCode>{0x12});
    }

    void brightnessGoesToBrightnessSync()
    {
        Rig rig;
        const auto result = rig.applier.apply(mode(u"m"_s, {{0x10, 60}}), acerConfig());
        QVERIFY(result.ok());
        // Maximum comes from reading 0x10 (fixture: max 100).
        QCOMPARE(rig.brightness.calls(), (QList<FakeBrightness::Call>{{acerId, 60, 100}}));
        // The only 0x10 write is the one FakeBrightness does as PowerDevil.
        QCOMPARE(rig.backend.writes().size(), 1);
        QCOMPARE(result.written, QList<VcpCode>{0x10});
    }

    void colourPresetBeforeGainsAndBrightnessLast()
    {
        Rig rig;
        const auto result = rig.applier.apply(mode(u"evening"_s, {{0x10, 60}, {0x12, 55}, {0x14, 0x0B}, {0x16, 47}, {0x18, 50}, {0x1A, 36}}), acerConfig());
        QVERIFY2(result.ok(), qPrintable(result.message));
        const QList<VcpCode> expected{0x14, 0x16, 0x18, 0x1A, 0x12, 0x10};
        QCOMPARE(writtenCodes(rig.backend), expected);
        QCOMPARE(result.written, expected);
    }

    void afterBrightnessCodesComeLast()
    {
        Rig rig;
        const Mode lowblue = mode(u"lowblue"_s, {{0x10, 75}, {0x12, 50}, {0xE8, 2}});
        auto result = rig.applier.apply(lowblue, acerConfig());
        QVERIFY2(result.ok(), qPrintable(result.message));
        QVERIFY2(result.warnings.isEmpty(), qPrintable(result.warnings.join(u'\n')));
        // Fixture brightness is already 75, but the applier doesn't know that
        // yet, so it writes it.
        QCOMPARE(writtenCodes(rig.backend), (QList<VcpCode>{0x12, 0x10, 0xE8}));
        QCOMPARE(rig.backend.value(acerId, 0xE8), 2);
        QCOMPARE(rig.backend.value(acerId, 0x10), 75);
    }

    void afterBrightnessCodesAreAlwaysWritten()
    {
        Rig rig;
        const Mode lowblue = mode(u"lowblue"_s, {{0x10, 75}, {0xE8, 2}});
        QVERIFY(rig.applier.apply(lowblue, acerConfig()).ok());
        rig.backend.clearWrites();

        // Something (PowerDevil, the OSD) cleared LBL in the meantime.
        QString error;
        QVERIFY(rig.backend.writeVcp(acerId, 0x10, 75, &error));
        QCOMPARE(rig.backend.value(acerId, 0xE8), 0);
        rig.backend.clearWrites();

        const auto result = rig.applier.apply(lowblue, acerConfig());
        QVERIFY(result.ok());
        QCOMPARE(writtenCodes(rig.backend), QList<VcpCode>{0xE8});
        QCOMPARE(rig.backend.value(acerId, 0xE8), 2);
    }

    void wrongWriteOrderIsCaughtByFixture()
    {
        // Without `after_brightness`, 0xE8 goes before brightness and the
        // brightness write clears it again. This is the bug the setting
        // exists for; the fake must reproduce it.
        Rig rig;
        const auto result = rig.applier.apply(mode(u"lowblue"_s, {{0x10, 75}, {0xE8, 2}}), acerConfig({}));
        QVERIFY(result.ok());
        QCOMPARE(writtenCodes(rig.backend), (QList<VcpCode>{0xE8, 0x10}));
        QCOMPARE(rig.backend.value(acerId, 0xE8), 0);
    }

    void warnsWhenAfterBrightnessCodeChangesBrightness()
    {
        Rig rig;
        // LBL level 2 sets brightness 75, not 80.
        const auto result = rig.applier.apply(mode(u"lowblue"_s, {{0x10, 80}, {0xE8, 2}}), acerConfig());
        QVERIFY(result.ok());
        QCOMPARE(result.warnings.size(), 1);
        QVERIFY2(result.warnings.first().contains(u"reports brightness 75"_s), qPrintable(result.warnings.first()));
        QCOMPARE(writtenCodes(rig.backend), (QList<VcpCode>{0x10, 0xE8}));
        QCOMPARE(rig.applier.knownValues(acerId).value(0x10), 75);
    }

    void failingWriteStopsAndNamesCode()
    {
        Rig rig;
        rig.backend.failOn(0x18);
        const auto result = rig.applier.apply(mode(u"evening"_s, {{0x10, 60}, {0x14, 0x0B}, {0x16, 47}, {0x18, 50}, {0x1A, 36}}), acerConfig());
        QVERIFY(!result.ok());
        QCOMPARE(result.error, ModeApplier::Error::DdcFailed);
        QCOMPARE(result.failedCode, std::optional<VcpCode>(0x18));
        QVERIFY2(result.message.contains(u"0x18 (green_gain)"_s), qPrintable(result.message));
        QVERIFY2(result.message.contains(u"already written: 0x14, 0x16"_s), qPrintable(result.message));
        QCOMPARE(writtenCodes(rig.backend), (QList<VcpCode>{0x14, 0x16}));
        QVERIFY(rig.brightness.calls().isEmpty());

        // Codes written before the failure are known; the rest are not.
        QCOMPARE(rig.applier.knownValues(acerId), (VcpValues{{0x14, 0x0B}, {0x16, 47}}));
    }

    void failingBrightnessSyncIsAnError()
    {
        Rig rig;
        rig.brightness.setResult(BrightnessSync::Result::Failed);
        const auto result = rig.applier.apply(mode(u"m"_s, {{0x10, 60}, {0x12, 40}}), acerConfig());
        QCOMPARE(result.error, ModeApplier::Error::DdcFailed);
        QCOMPARE(result.failedCode, std::optional<VcpCode>(0x10));
        QVERIFY(result.message.contains(u"KDE"_s));
        QVERIFY(!rig.applier.knownValues(acerId).contains(0x10));
    }

    void fallsBackToDdcWhenKdeHasNoMatch()
    {
        Rig rig;
        rig.brightness.setResult(BrightnessSync::Result::NoMatch);
        const auto result = rig.applier.apply(mode(u"m"_s, {{0x10, 60}}), acerConfig());
        QVERIFY(result.ok());
        QCOMPARE(writtenCodes(rig.backend), QList<VcpCode>{0x10});
        QCOMPARE(rig.backend.value(acerId, 0x10), 60);
        QCOMPARE(result.warnings.size(), 1);
        QVERIFY(result.warnings.first().contains(u"KDE may overwrite it"_s));
    }

    void worksWithoutBrightnessSync()
    {
        FakeBackend backend;
        QString error;
        backend.loadFixture(QStringLiteral(PMMT_FIXTURES_DIR "/monitors/acer-sa243y.toml"), &error);
        ModeApplier applier(backend, nullptr);
        const auto result = applier.apply(mode(u"m"_s, {{0x10, 60}}), acerConfig());
        QVERIFY(result.ok());
        QCOMPARE(backend.value(acerId, 0x10), 60);
        QCOMPARE(result.warnings.size(), 1);
    }

    void verifyOffDoesNotReadBack()
    {
        Rig rig;
        QVERIFY(!rig.applier.verify());
        // A gain write switches the preset to User, so 0x14 ends up wrong.
        const auto result = rig.applier.apply(mode(u"m"_s, {{0x14, 0x06}, {0x16, 47}}), acerConfig());
        QVERIFY(result.ok());
        QVERIFY(result.warnings.isEmpty());
        QCOMPARE(rig.backend.readCount(), 0);
    }

    void verifyOnReportsMismatches()
    {
        Rig rig;
        rig.applier.setVerify(true);
        const auto result = rig.applier.apply(mode(u"m"_s, {{0x14, 0x06}, {0x16, 47}}), acerConfig());
        QVERIFY(result.ok());
        QCOMPARE(rig.backend.readCount(), 2);
        QCOMPARE(result.warnings.size(), 1);
        QVERIFY2(result.warnings.first().contains(u"0x14 (color_preset) reads 11 after writing 6"_s), qPrintable(result.warnings.first()));
        // The applier now knows the real value.
        QCOMPARE(rig.applier.knownValues(acerId).value(0x14), 0x0B);
    }

    void forgetsCodesTheNewModeDoesNotSet()
    {
        Rig rig;
        const Mode normal = mode(u"normal"_s, {{0x14, 0x06}});
        QVERIFY(rig.applier.apply(normal, acerConfig()).ok());
        // Gains switch the preset to User on the monitor.
        QVERIFY(rig.applier.apply(mode(u"warm"_s, {{0x16, 47}}), acerConfig()).ok());
        QCOMPARE(rig.backend.value(acerId, 0x14), 0x0B);
        rig.backend.clearWrites();

        QVERIFY(rig.applier.apply(normal, acerConfig()).ok());
        QCOMPARE(writtenCodes(rig.backend), QList<VcpCode>{0x14});
        QCOMPARE(rig.backend.value(acerId, 0x14), 0x06);
    }

    void forgetMakesNextApplyWriteEverything()
    {
        Rig rig;
        const Mode day = mode(u"day"_s, {{0x10, 100}, {0x12, 60}});
        QVERIFY(rig.applier.apply(day, acerConfig()).ok());
        rig.applier.forget(acerId);
        QVERIFY(rig.applier.knownValues(acerId).isEmpty());
        rig.backend.clearWrites();
        QVERIFY(rig.applier.apply(day, acerConfig()).ok());
        QCOMPARE(writtenCodes(rig.backend), (QList<VcpCode>{0x12, 0x10}));
    }

    void missingDisplayIsAnError_data()
    {
        QTest::addColumn<bool>("connected");
        QTest::newRow("not connected") << false;
        QTest::newRow("not reachable") << true;
    }

    void missingDisplayIsAnError()
    {
        QFETCH(bool, connected);
        Rig rig;
        DisplayConfig display = acerConfig();
        if (connected) {
            rig.backend.setReachable(acerId, false);
        } else {
            display.id = u"ACR:OTHER:1"_s;
        }
        const auto result = rig.applier.apply(mode(u"m"_s, {{0x10, 60}}), display);
        QCOMPARE(result.error, ModeApplier::Error::DisplayNotFound);
        QVERIFY(result.message.contains(connected ? u"not reachable"_s : u"not connected"_s));
        QVERIFY(rig.backend.writes().isEmpty());
        QVERIFY(rig.brightness.calls().isEmpty());
    }

    void modeForOtherDisplayIsAnError()
    {
        Rig rig;
        const auto result = rig.applier.apply(Mode{u"m"_s, u"dell"_s, {{0x10, 60}}}, acerConfig());
        QCOMPARE(result.error, ModeApplier::Error::DisplayNotFound);
        QVERIFY(rig.backend.writes().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestModeApplier)

#include "test_modeapplier.moc"
