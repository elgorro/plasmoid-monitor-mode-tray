// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "fakebackend.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Pmmt;
using namespace Qt::StringLiterals;

namespace
{

const QString acerId = u"ACR:SA243Y G0:FAKE0001"_s;

QString acerFixture()
{
    return QStringLiteral(PMMT_FIXTURES_DIR "/monitors/acer-sa243y.toml");
}

} // namespace

class TestFakeBackend : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadsFixture()
    {
        FakeBackend backend;
        QString error;
        QCOMPARE(backend.loadFixture(acerFixture(), &error), acerId);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const auto displays = backend.displays();
        QCOMPARE(displays.size(), 1);
        QCOMPARE(displays.first().id, acerId);
        QCOMPARE(displays.first().label, u"SA243Y G0"_s);
        QCOMPARE(displays.first().connector, u"DP-8"_s);
        QVERIFY(displays.first().reachable);

        QCOMPARE(backend.readVcp(acerId, 0x10, &error), (VcpReading{75, 100}));
        QCOMPARE(backend.readVcp(acerId, 0x14, &error), (VcpReading{0x06, 0}));
        QCOMPARE(backend.readCount(), 2);
    }

    void rejectsBrokenFixture()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"bad.toml"_s);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("version = 1\n[display]\nmanufacturer = \"ACR\"\nmodel = \"X\"\ncolour = 1\n");
        file.close();

        FakeBackend backend;
        QString error;
        QVERIFY(backend.loadFixture(path, &error).isEmpty());
        QVERIFY2(error.contains(u"line 5"_s), qPrintable(error));
        QVERIFY(backend.displays().isEmpty());
    }

    void recordsWritesAndAppliesEffects()
    {
        FakeBackend backend;
        QString error;
        backend.loadFixture(acerFixture(), &error);

        // Fixture: E8 = 2 sets brightness 75; a brightness write clears E8.
        QVERIFY(backend.writeVcp(acerId, 0xE8, 2, &error));
        QCOMPARE(backend.value(acerId, 0x10), 75);
        QVERIFY(backend.writeVcp(acerId, 0x10, 75, &error));
        QCOMPARE(backend.value(acerId, 0xE8), 0);

        // A gain write switches the colour preset to User.
        QVERIFY(backend.writeVcp(acerId, 0x16, 40, &error));
        QCOMPARE(backend.value(acerId, 0x14), 0x0B);

        const QList<FakeBackend::Write> expected{{acerId, 0xE8, 2}, {acerId, 0x10, 75}, {acerId, 0x16, 40}};
        QCOMPARE(backend.writes(), expected);
    }

    void rejectsInvalidWrites_data()
    {
        QTest::addColumn<int>("code");
        QTest::addColumn<int>("value");
        QTest::addColumn<QString>("message");
        QTest::newRow("read-only") << 0xE1 << 1 << u"read-only"_s;
        QTest::newRow("above maximum") << 0x10 << 101 << u"above the maximum"_s;
        QTest::newRow("not allowed") << 0x14 << 0x07 << u"does not accept"_s;
        QTest::newRow("unsupported") << 0xDC << 1 << u"not supported"_s;
    }

    void rejectsInvalidWrites()
    {
        QFETCH(int, code);
        QFETCH(int, value);
        QFETCH(QString, message);
        FakeBackend backend;
        QString error;
        backend.loadFixture(acerFixture(), &error);
        QVERIFY(!backend.writeVcp(acerId, static_cast<VcpCode>(code), static_cast<VcpValue>(value), &error));
        QVERIFY2(error.contains(message), qPrintable(error));
        QVERIFY(backend.writes().isEmpty());
    }

    void injectedFailure()
    {
        FakeBackend backend;
        QString error;
        backend.loadFixture(acerFixture(), &error);
        backend.failOn(0x12);
        QVERIFY(!backend.writeVcp(acerId, 0x12, 40, &error));
        QVERIFY(error.contains(u"0x12"_s));
        QVERIFY(!backend.readVcp(acerId, 0x12, &error));
        QCOMPARE(backend.value(acerId, 0x12), 50);
        QVERIFY(backend.writeVcp(acerId, 0x10, 40, &error));

        backend.clearFailures();
        QVERIFY(backend.writeVcp(acerId, 0x12, 40, &error));
    }

    void unreachableDisplay()
    {
        FakeBackend backend;
        QString error;
        backend.loadFixture(acerFixture(), &error);
        backend.setReachable(acerId, false);
        QVERIFY(!backend.displays().first().reachable);
        QVERIFY(!backend.readVcp(acerId, 0x10, &error));
        QVERIFY(!backend.writeVcp(acerId, 0x10, 40, &error));
        QVERIFY(!backend.readVcp(u"ACR:NOPE:1"_s, 0x10, &error));
    }

    void delaysAreSimulated()
    {
        FakeBackend backend;
        QString error;
        backend.loadFixture(acerFixture(), &error);
        backend.setDelayMs(80);
        backend.readVcp(acerId, 0x10, &error);
        backend.writeVcp(acerId, 0x10, 40, &error);
        QCOMPARE(backend.elapsedMs(), 160);
    }

    void fakeBrightnessWritesThrough()
    {
        FakeBackend backend;
        QString error;
        backend.loadFixture(acerFixture(), &error);
        FakeBrightness brightness(&backend);
        const DisplayInfo display = backend.displays().first();

        QCOMPARE(brightness.setBrightness(display, 60, 100, &error), BrightnessSync::Result::Done);
        QCOMPARE(backend.value(acerId, 0x10), 60);
        QCOMPARE(brightness.calls(), (QList<FakeBrightness::Call>{{acerId, 60, 100}}));

        brightness.setResult(BrightnessSync::Result::NoMatch);
        QCOMPARE(brightness.setBrightness(display, 70, 100, &error), BrightnessSync::Result::NoMatch);
        QCOMPARE(backend.value(acerId, 0x10), 60);

        brightness.setResult(BrightnessSync::Result::Failed);
        QCOMPARE(brightness.setBrightness(display, 70, 100, &error), BrightnessSync::Result::Failed);
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestFakeBackend)

#include "test_fakebackend.moc"
