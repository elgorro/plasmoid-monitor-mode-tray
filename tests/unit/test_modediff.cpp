// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "modediff.h"

#include <QTest>

using namespace Pmmt;

class TestModeDiff : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nothingChangedIsEmpty()
    {
        const VcpValues values{{0x10, 75}, {0x12, 50}};
        QVERIFY(diffValues(values, values).isEmpty());
    }

    void onlyChangedCodes()
    {
        const VcpValues target{{0x10, 75}, {0x12, 50}, {0x14, 0x0B}};
        const VcpValues current{{0x10, 75}, {0x12, 40}, {0x14, 0x0B}, {0x16, 50}};
        QCOMPARE(diffValues(target, current), (VcpValues{{0x12, 50}}));
    }

    void unknownCurrentValuesCountAsChanged()
    {
        const VcpValues target{{0x10, 75}, {0xE8, 2}};
        QCOMPARE(diffValues(target, {}), target);
        QCOMPARE(diffValues(target, VcpValues{{0x10, 75}}), (VcpValues{{0xE8, 2}}));
    }

    void writeOrderFollowsArchitecture()
    {
        // Every group, deliberately out of order by code.
        const VcpValues values{
            {0x10, 1}, // brightness: left out
            {0x12, 1}, // contrast: group 3
            {0x14, 1}, // colour preset: group 1
            {0x16, 1}, // gains: group 2
            {0x18, 1},
            {0x1A, 1},
            {0x87, 1}, // sharpness: group 3
            {0xDC, 1}, // display application: group 1
            {0xE8, 1}, // after_brightness: left out
            {0x02, 1}, // below contrast, still group 3 ascending
        };
        const QList<VcpCode> expected{0x14, 0xDC, 0x16, 0x18, 0x1A, 0x02, 0x12, 0x87};
        QCOMPARE(ddcWriteOrder(values, {0xE8}), expected);
    }

    void writeOrderWithoutAfterBrightness()
    {
        const VcpValues values{{0x10, 1}, {0xE8, 1}};
        QCOMPARE(ddcWriteOrder(values, {}), QList<VcpCode>{0xE8});
    }
};

QTEST_GUILESS_MAIN(TestModeDiff)

#include "test_modediff.moc"
