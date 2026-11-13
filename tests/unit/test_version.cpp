// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "version.h"

#include "pmmt_version.h"

#include <QRegularExpression>
#include <QTest>

class TestVersion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void versionMatchesProject()
    {
        QCOMPARE(Pmmt::versionString(), QStringLiteral(PMMT_VERSION_STRING));
    }

    void versionIsSemVer()
    {
        // Semantic Versioning 2.0.0, §2: MAJOR.MINOR.PATCH, non-negative
        // integers without leading zeroes.
        static const QRegularExpression semver(QStringLiteral(R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$)"));
        QVERIFY2(semver.match(Pmmt::versionString()).hasMatch(), qPrintable(Pmmt::versionString()));
    }
};

QTEST_GUILESS_MAIN(TestVersion)

#include "test_version.moc"
