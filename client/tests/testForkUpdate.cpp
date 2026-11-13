#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "core/utils/forkUpdateVersion.h"
#include "core/utils/windowsUpdatePackage.h"

class TestForkUpdate : public QObject
{
    Q_OBJECT

private slots:
    void ordersStableVersions()
    {
        using forkUpdate::stableVersion;
        QVERIFY(stableVersion("v1.10.0") > stableVersion("v1.9.9"));
        QVERIFY(stableVersion("v2.0.0") > stableVersion("1.99.99"));
        QCOMPARE(stableVersion("1.2.3+build.12"), stableVersion("v1.2.3"));
        for (const auto &invalid : { "v1.2.3-beta.1", "1.2", "01.2.3", "1.2.3.4", "1.2.3\n", "v9999999999999.0.0" }) {
            QVERIFY(stableVersion(invalid).isNull());
        }
    }

    void rejectsCorruptOrMissingMsi()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile payload(directory.filePath("damaged.msi"));
        QVERIFY(payload.open(QIODevice::WriteOnly));
        QVERIFY(payload.write("not a Windows Installer database") > 0);
        payload.close();
        QVERIFY(!windowsUpdate::verifyPackage(payload.fileName().toStdWString(), L"1.2.0"));
        QVERIFY(!windowsUpdate::verifyPackage(directory.filePath("missing.msi").toStdWString(), L"1.2.0"));
    }
};

QTEST_GUILESS_MAIN(TestForkUpdate)
#include "testForkUpdate.moc"
