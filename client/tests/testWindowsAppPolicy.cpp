// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
// clang-format on

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QProcess>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include "killswitch.h"
#include "platforms/windows/daemon/windowsfirewall.h"
#include "version.h"

class TestWindowsAppPolicy : public QObject
{
    Q_OBJECT

    QTemporaryDir m_directory;
    QString m_selected;
    QString m_direct;
    bool m_initialized = false;

    int probe(const QString &executable)
    {
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("PATH",
                           QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) + ';'
                                   + environment.value("PATH"));
        process.setProcessEnvironment(environment);
        process.start(executable, { "--connect-probe" });
        if (!process.waitForFinished(5000)) {
            process.kill();
            process.waitForFinished();
            return 100;
        }
        return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 101;
    }

    QJsonObject policy(bool soft, bool strict)
    {
        return { { "positive", true }, { "soft", soft }, { "strict", strict }, { "apps", QJsonArray { m_selected } } };
    }

private slots:
    void initTestCase()
    {
        if (qEnvironmentVariable("GITHUB_ACTIONS") != "true" || qEnvironmentVariable("AMNEZIA_RUN_WFP_TESTS") != "1") {
            QSKIP("Requires the isolated Windows CI runner");
        }
        QSettings registry("HKEY_LOCAL_MACHINE\\Software\\" + QString(ORGANIZATION_NAME) + "\\"
                                   + QString(APPLICATION_NAME),
                           QSettings::NativeFormat);
        QVERIFY2(!registry.contains("strictKillSwitchEnabled"), "Refusing to modify an existing installation");
        QVERIFY(m_directory.isValid());
        m_selected = m_directory.filePath("selected.exe");
        m_direct = m_directory.filePath("direct.exe");
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(), m_selected));
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(), m_direct));
        QCOMPARE(probe(m_selected), 0);
        QCOMPARE(probe(m_direct), 0);
        m_initialized = true;
    }

    void normalAndStrictDisconnects()
    {
        auto guard = KillSwitch::instance();
        QVERIFY(guard->configureAppPolicy(policy(true, false)));
        QVERIFY(guard->beginPositiveConnection({ m_selected }, true));
        QCOMPARE(probe(m_selected), 2);
        QCOMPARE(probe(m_direct), 0);
        DWORD interfaceIndex = 0;
        QVERIFY(GetBestInterface(0x01010101, &interfaceIndex) == NO_ERROR);
        QVERIFY(guard->allowPositiveInterface(interfaceIndex));
        QCOMPARE(probe(m_selected), 0);
        QVERIFY(guard->endPositiveConnection(false));
        QCOMPARE(probe(m_selected), 2);
        QCOMPARE(probe(m_direct), 0);
        QVERIFY(guard->configureAppPolicy(policy(true, false)));
        QCOMPARE(probe(m_selected), 2);
        QVERIFY(guard->endPositiveConnection(true));
        QCOMPARE(probe(m_selected), 0);
        QVERIFY(guard->configureAppPolicy(policy(true, true)));
        QCOMPARE(probe(m_selected), 2);
        QVERIFY(guard->beginPositiveConnection({ m_selected }, true));
        QVERIFY(guard->allowPositiveInterface(interfaceIndex));
        QCOMPARE(probe(m_selected), 0);
        QVERIFY(guard->endPositiveConnection(true));
        QCOMPARE(probe(m_selected), 2);
        QCOMPARE(probe(m_direct), 0);
        QVERIFY(guard->configureAppPolicy(policy(false, false)));
        QCOMPARE(probe(m_selected), 0);
        QVERIFY(guard->beginPositiveConnection({ m_selected }, false));
        QVERIFY(guard->endPositiveConnection(false));
        QCOMPARE(probe(m_selected), 0);
    }

    void cleanupTestCase()
    {
        if (!m_initialized) {
            return;
        }
        QVERIFY(KillSwitch::instance()->endPositiveConnection(true));
        QVERIFY(KillSwitch::instance()->configureAppPolicy(policy(false, false)));
        QSettings registry("HKEY_LOCAL_MACHINE\\Software\\" + QString(ORGANIZATION_NAME) + "\\"
                                   + QString(APPLICATION_NAME),
                           QSettings::NativeFormat);
        for (const QString &key : { "strictKillSwitchEnabled", "positiveSplitTunnelEnabled", "positiveSplitTunnelApps" }) {
            registry.remove(key);
        }
    }
};

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().contains("--connect-probe")) {
        QTcpSocket socket;
        socket.connectToHost("1.1.1.1", 443);
        return socket.waitForConnected(2000) ? 0 : 2;
    }
    TestWindowsAppPolicy test;
    return QTest::qExec(&test, argc, argv);
}

#include "testWindowsAppPolicy.moc"
