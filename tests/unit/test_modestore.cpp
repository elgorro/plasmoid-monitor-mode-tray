// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "modestore.h"

#include <QTemporaryDir>
#include <QTest>

using namespace Pmmt;
using namespace Qt::StringLiterals;

namespace
{

QString fixture(const QString &name)
{
    return QStringLiteral(PMMT_FIXTURES_DIR "/config/") + name;
}

const QByteArray header = QByteArrayLiteral(
    "version = 1\n"
    "[displays.acer]\n"
    "id = \"ACR:SA243Y:1234ABCD\"\n");

QString errorList(const ModeStore &store)
{
    QStringList out;
    for (const auto &e : store.errors()) {
        out << e.toString();
    }
    return out.join(QLatin1Char('\n'));
}

} // namespace

class TestModeStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadsValidConfig()
    {
        ModeStore store;
        QVERIFY2(store.load(fixture(QStringLiteral("valid.toml"))), qPrintable(errorList(store)));
        QVERIFY(store.errors().isEmpty());

        QCOMPARE(store.displays().size(), 1);
        const DisplayConfig acer = store.displays().first();
        QCOMPARE(acer.key, QStringLiteral("acer"));
        QCOMPARE(acer.id, QStringLiteral("ACR:SA243Y:1234ABCD"));
        QCOMPARE(acer.connector, QStringLiteral("DP-8"));
        QCOMPARE(acer.afterBrightness, QList<VcpCode>{0xE8});

        const auto evening = store.mode(QStringLiteral("evening"));
        QVERIFY(evening);
        QCOMPARE(evening->display, QStringLiteral("acer"));
        const VcpValues expected{{0x10, 60}, {0x14, 0x0B}, {0x16, 47}, {0x18, 50}, {0x1A, 36}};
        QCOMPARE(evening->values, expected);

        const auto lowblue = store.mode(QStringLiteral("lowblue"));
        QVERIFY(lowblue);
        QCOMPARE(lowblue->values, (VcpValues{{0x10, 75}, {0xE8, 2}}));

        QVERIFY(!store.mode(QStringLiteral("nope")));
        QVERIFY(store.display(QStringLiteral("acer")));
        QVERIFY(!store.display(QStringLiteral("nope")));
    }

    void keepsFileOrder()
    {
        ModeStore store;
        QVERIFY2(store.load(fixture(QStringLiteral("valid.toml"))), qPrintable(errorList(store)));
        QStringList names;
        for (const auto &mode : store.modes()) {
            names << mode.name;
        }
        QCOMPARE(names, (QStringList{u"day"_s, u"evening"_s, u"lowblue"_s, u"cinema"_s}));
    }

    void namedKeyMapsToVcpCode_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<int>("code");
        // docs/usage/configuration.md, VESA MCCS 2.2a.
        QTest::newRow("brightness") << u"brightness"_s << 0x10;
        QTest::newRow("contrast") << u"contrast"_s << 0x12;
        QTest::newRow("color_preset") << u"color_preset"_s << 0x14;
        QTest::newRow("red_gain") << u"red_gain"_s << 0x16;
        QTest::newRow("green_gain") << u"green_gain"_s << 0x18;
        QTest::newRow("blue_gain") << u"blue_gain"_s << 0x1A;
    }

    void namedKeyMapsToVcpCode()
    {
        QFETCH(QString, key);
        QFETCH(int, code);
        ModeStore store;
        const QByteArray toml = header + "[modes.m]\ndisplay = \"acer\"\n" + key.toUtf8() + " = 42\n";
        QVERIFY2(store.loadFromData(toml, u"t.toml"_s), qPrintable(errorList(store)));
        QCOMPARE(store.mode(u"m"_s)->values, (VcpValues{{static_cast<VcpCode>(code), 42}}));
    }

    void rawVcpKeys()
    {
        ModeStore store;
        const QByteArray toml = header
            + "[modes.m]\ndisplay = \"acer\"\n"
              "vcp = { \"0xE8\" = 2, \"0x87\" = 3, \"0xdc\" = 0xFFFF }\n";
        QVERIFY2(store.loadFromData(toml, u"t.toml"_s), qPrintable(errorList(store)));
        QCOMPARE(store.mode(u"m"_s)->values, (VcpValues{{0x87, 3}, {0xDC, 0xFFFF}, {0xE8, 2}}));
    }

    void emptyConfigIsValid()
    {
        ModeStore store;
        QVERIFY2(store.loadFromData("version = 1\n", u"t.toml"_s), qPrintable(errorList(store)));
        QVERIFY(store.displays().isEmpty());
        QVERIFY(store.modes().isEmpty());
    }

    void displayWithoutSerial()
    {
        ModeStore store;
        QVERIFY2(store.loadFromData("version = 1\n[displays.x]\nid = \"DEL:U2720Q:\"\n", u"t.toml"_s), qPrintable(errorList(store)));
    }

    void rejectsInvalidConfig_data()
    {
        QTest::addColumn<QByteArray>("toml");
        QTest::addColumn<int>("line");
        QTest::addColumn<QString>("message");

        const QByteArray mode = header + "[modes.m]\ndisplay = \"acer\"\n"; // modes.m starts on line 4

        QTest::newRow("unknown top-level key") << QByteArray("version = 1\nfoo = 1\n") << 2 << u"unknown key `foo`"_s;
        QTest::newRow("unknown mode key") << QByteArray(mode + "brightnes = 1\n") << 6 << u"unknown key `brightnes` in mode `m`"_s;
        QTest::newRow("unknown display key") << QByteArray(header + "conector = \"DP-8\"\n") << 4 << u"unknown key `conector` in display `acer`"_s;
        QTest::newRow("value too large") << QByteArray(mode + "brightness = 65536\n") << 6 << u"out of range"_s;
        QTest::newRow("negative value") << QByteArray(mode + "contrast = -1\n") << 6 << u"out of range"_s;
        QTest::newRow("value not integer") << QByteArray(mode + "brightness = \"75\"\n") << 6 << u"must be an integer"_s;
        QTest::newRow("float value") << QByteArray(mode + "brightness = 7.5\n") << 6 << u"must be an integer"_s;
        QTest::newRow("unknown display ref") << QByteArray(header + "[modes.m]\ndisplay = \"dell\"\nbrightness = 1\n") << 5 << u"unknown display `dell`"_s;
        QTest::newRow("missing display ref") << QByteArray(header + "[modes.m]\nbrightness = 1\n") << 4 << u"has no `display`"_s;
        QTest::newRow("mode without values") << QByteArray(mode) << 4 << u"sets no values"_s;
        QTest::newRow("missing version") << QByteArray("[displays.a]\nid = \"ACR:X:1\"\n") << 1 << u"missing `version`"_s;
        QTest::newRow("version mismatch") << QByteArray("version = 2\n") << 1 << u"unsupported config version"_s;
        QTest::newRow("version not integer") << QByteArray("version = \"1\"\n") << 1 << u"unsupported config version"_s;
        QTest::newRow("bad raw code") << QByteArray(mode + "vcp = { \"E8\" = 1 }\n") << 6 << u"not a VCP code"_s;
        QTest::newRow("raw code too long") << QByteArray(mode + "vcp = { \"0x0E8\" = 1 }\n") << 6 << u"not a VCP code"_s;
        QTest::newRow("raw code with name") << QByteArray(mode + "vcp = { \"0x10\" = 1 }\n") << 6 << u"use `brightness` instead"_s;
        QTest::newRow("raw value too large") << QByteArray(mode + "vcp = { \"0xE8\" = 70000 }\n") << 6 << u"out of range"_s;
        QTest::newRow("display without id") << QByteArray("version = 1\n[displays.a]\nconnector = \"DP-1\"\n") << 2 << u"has no `id`"_s;
        QTest::newRow("malformed display id") << QByteArray("version = 1\n[displays.a]\nid = \"acer\"\n") << 3 << u"not of the form"_s;
        QTest::newRow("lower-case manufacturer") << QByteArray("version = 1\n[displays.a]\nid = \"acr:X:1\"\n") << 3 << u"not of the form"_s;
        QTest::newRow("duplicate display id") << QByteArray(header + "[displays.b]\nid = \"ACR:SA243Y:1234ABCD\"\n") << 5
                                              << u"already used by display `acer`"_s;
        QTest::newRow("after_brightness not array") << QByteArray(header + "after_brightness = \"0xE8\"\n") << 4 << u"must be an array"_s;
        QTest::newRow("after_brightness bad code") << QByteArray(header + "after_brightness = [0xE8]\n") << 4 << u"must be VCP codes"_s;
        QTest::newRow("after_brightness 0x10") << QByteArray(header + "after_brightness = [\"0x10\"]\n") << 4 << u"brightness (0x10) itself"_s;
        QTest::newRow("after_brightness twice") << QByteArray(header + "after_brightness = [\"0xE8\", \"0xe8\"]\n") << 4 << u"listed twice"_s;
        QTest::newRow("modes not a table") << QByteArray("version = 1\nmodes = 1\n") << 2 << u"`modes` must be a table"_s;
        QTest::newRow("syntax error") << QByteArray("version = 1\n[modes.m\n") << 2 << QString();
    }

    void rejectsInvalidConfig()
    {
        QFETCH(QByteArray, toml);
        QFETCH(int, line);
        QFETCH(QString, message);

        ModeStore store;
        QVERIFY(!store.loadFromData(toml, u"modes.toml"_s));
        QVERIFY(!store.errors().isEmpty());
        const ConfigError error = store.errors().first();
        QCOMPARE(error.file, u"modes.toml"_s);
        QCOMPARE(error.line, line);
        QVERIFY(error.column > 0);
        QVERIFY2(error.message.contains(message), qPrintable(error.message));
    }

    void reportsAllErrors()
    {
        ModeStore store;
        const QByteArray toml = header + "[modes.a]\ndisplay = \"acer\"\nfoo = 1\n[modes.b]\ndisplay = \"acer\"\nbar = 1\n";
        QVERIFY(!store.loadFromData(toml, u"t.toml"_s));
        QCOMPARE(store.errors().size(), 2);
    }

    void errorsFromFileHavePath()
    {
        ModeStore store;
        const QString path = fixture(QStringLiteral("invalid-unknown-key.toml"));
        QVERIFY(!store.load(path));
        QCOMPARE(store.errors().size(), 1);
        QCOMPARE(store.errors().first().toString(), path + u":8:1: unknown key `brightnes` in mode `day`"_s);
    }

    void syntaxErrorFromFile()
    {
        ModeStore store;
        const QString path = fixture(QStringLiteral("invalid-syntax.toml"));
        QVERIFY(!store.load(path));
        QCOMPARE(store.errors().size(), 1);
        QCOMPARE(store.errors().first().file, path);
        QCOMPARE(store.errors().first().line, 3);
    }

    void missingFileIsAnError()
    {
        QTemporaryDir dir;
        ModeStore store;
        const QString path = dir.filePath(QStringLiteral("modes.toml"));
        QVERIFY(!store.load(path));
        QCOMPARE(store.errors().size(), 1);
        QCOMPARE(store.errors().first().line, 0);
        QVERIFY(store.errors().first().toString().startsWith(path + u": "_s));
    }

    void failedLoadKeepsPreviousConfig()
    {
        ModeStore store;
        QVERIFY(store.load(fixture(QStringLiteral("valid.toml"))));
        const auto modes = store.modes();
        QVERIFY(!store.load(fixture(QStringLiteral("invalid-unknown-key.toml"))));
        QCOMPARE(store.modes(), modes);
        QCOMPARE(store.displays().size(), 1);

        QVERIFY(store.loadFromData("version = 1\n", u"t.toml"_s));
        QVERIFY(store.errors().isEmpty());
        QVERIFY(store.modes().isEmpty());
    }

    void defaultPathFollowsXdgConfigHome()
    {
        QTemporaryDir dir;
        const QByteArray old = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
        const QString path = ModeStore::defaultConfigPath();
        qputenv("XDG_CONFIG_HOME", old);
        QCOMPARE(path, dir.path() + u"/pmmt/modes.toml"_s);
    }

    void formatsVcpCodes()
    {
        QCOMPARE(formatVcpCode(0x0E), u"0x0E"_s);
        QCOMPARE(formatVcpCode(0xE8), u"0xE8"_s);
        QCOMPARE(parseVcpCode(u"0xe8"), std::optional<VcpCode>(0xE8));
        QCOMPARE(parseVcpCode(u"0Xe8"), std::nullopt);
        QCOMPARE(parseVcpCode(u"0xg8"), std::nullopt);
    }
};

QTEST_GUILESS_MAIN(TestModeStore)

#include "test_modestore.moc"
