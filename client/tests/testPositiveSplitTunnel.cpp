#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "core/utils/appSelection.h"
#include "core/utils/routeModes.h"
#include "daemon/daemon.h"
#include "daemon/interfaceconfig.h"
#include "mozilla/localsocketcontroller.h"

class ConfigOnlyDaemon : public Daemon
{
public:
    ConfigOnlyDaemon() : Daemon(nullptr)
    {
    }

    bool canReuseInterface(const InterfaceConfig &current, const InterfaceConfig &next)
    {
        m_connections.insert(current.m_hopType, ConnectionState(current));
        return supportServerSwitching(next);
    }

protected:
    WireguardUtils *wgutils() const override
    {
        return nullptr;
    }
};

class TestPositiveSplitTunnel : public QObject
{
    Q_OBJECT

private:
    static InterfaceConfig configuration()
    {
        InterfaceConfig config;
        config.m_hopType = InterfaceConfig::SingleHop;
        config.m_killSwitchEnabled = false;
        config.m_privateKey = QStringLiteral("");
        config.m_serverPublicKey = QStringLiteral("");
        config.m_deviceIpv4Address = QStringLiteral("10.233.0.2");
        config.m_serverIpv4AddrIn = QStringLiteral("192.0.2.1");
        config.m_serverIpv4Gateway = config.m_serverIpv4AddrIn;
        config.m_primaryDnsServer = QStringLiteral("192.0.2.53");
        config.m_allowedIPAddressRanges.append(IPAddress(QStringLiteral("0.0.0.0/0")));
        return config;
    }

private slots:
    void distinguishesManualDisconnect()
    {
        QLocalServer server;
        const auto name = "amnezia-disconnect-test-" + QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY(server.listen(name));
        LocalSocketController controller;
        controller.m_daemonState = LocalSocketController::eInitializing;
        controller.m_socket->connectToServer(name);
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QLocalSocket> peer(server.nextPendingConnection());
        QTRY_VERIFY(peer->canReadLine());
        peer->readLine();
        controller.m_daemonState = LocalSocketController::eReady;
        controller.deactivate();
        QTRY_VERIFY(peer->canReadLine());
        const auto automatic = QJsonDocument::fromJson(peer->readLine()).object();
        QCOMPARE(automatic.value("type").toString(), QString("deactivate"));
        QVERIFY(!automatic.value("manual").toBool());
        controller.deactivateManually();
        QTRY_VERIFY(peer->canReadLine());
        const auto manual = QJsonDocument::fromJson(peer->readLine()).object();
        QCOMPARE(manual.value("type").toString(), QString("deactivate"));
        QVERIFY(manual.value("manual").toBool());
    }

    void resolvesFoldersRecursivelyAndRefreshes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir root(directory.path());
        QVERIFY(root.mkpath("selected/nested"));
        QVERIFY(root.mkpath("sibling"));
        const QString first = root.filePath("selected/first.exe");
        const QString nested = root.filePath("selected/nested/second.EXE");
        for (const auto &path :
             { first, nested, root.filePath("selected/readme.txt"), root.filePath("sibling/other.exe") }) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const QString folder = root.filePath("selected/");
        auto result = appSelection::resolve({ folder, nested });
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.executables, (QStringList { first, nested }));
        QCOMPARE(result.directories.size(), 1);
        const QString added = root.filePath("selected/new.exe");
        QFile file(added);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        result = appSelection::resolve({ folder });
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.executables.contains(added));
        QVERIFY(QFile::remove(first));
        result = appSelection::resolve({ folder });
        QVERIFY(result.error.isEmpty());
        QVERIFY(!result.executables.contains(first));
        result = appSelection::resolve({ nested, root.filePath("missing/") });
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.executables.isEmpty());
        QVERIFY(root.mkdir("empty"));
        result = appSelection::resolve({ root.filePath("empty/") });
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.executables.isEmpty());
        QCOMPARE(result.directories.size(), 1);
    }

    void negotiatesServiceSupport_data()
    {
        QTest::addColumn<int>("version");
        QTest::addColumn<bool>("folder");
        QTest::newRow("original-service") << 0 << false;
        QTest::newRow("release-1.0-service") << 1 << false;
        QTest::newRow("folder-only-beta-service") << 2 << false;
        QTest::newRow("matching-service") << 3 << false;
        QTest::newRow("folder-original-service") << 0 << true;
        QTest::newRow("folder-release-1.0-service") << 1 << true;
        QTest::newRow("folder-matching-service") << 3 << true;
    }

    void negotiatesServiceSupport()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows-only mode");
#else
        QFETCH(int, version);
        QFETCH(bool, folder);
        const bool supported = version >= 3;
        QLocalServer server;
        const auto name = "amnezia-positive-test-" + QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY(server.listen(name));
        LocalSocketController controller;
        QSignalSpy failed(&controller, &ControllerImpl::positiveSplitTunnelUnsupported);
        controller.m_daemonState = LocalSocketController::eInitializing;
        controller.m_socket->connectToServer(name);
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QLocalSocket> peer(server.nextPendingConnection());
        QTRY_VERIFY(peer->canReadLine());
        QCOMPARE(QJsonDocument::fromJson(peer->readLine()).object().value("type").toString(), QString("status"));

        QJsonObject raw;
        raw.insert("appSplitTunnelType", int(amnezia::AppsRouteMode::VpnOnlyForwardApps));
        const QJsonArray apps { folder ? "example/" : "example.exe" };
        raw.insert("splitTunnelApps", apps);
        controller.activate(raw);
        QVERIFY(!controller.m_pendingPositiveActivation.isEmpty());
        QJsonObject status { { "type", "status" }, { "connected", false } };
        if (version > 0) {
            status.insert("positiveSplitTunnelSupported", true);
            status.insert("appSelectionVersion", version);
        }
        peer->write(QJsonDocument(status).toJson(QJsonDocument::Compact) + '\n');
        peer->flush();
        if (supported) {
            QTRY_VERIFY(peer->canReadLine());
            const auto activation = QJsonDocument::fromJson(peer->readLine()).object();
            QCOMPARE(activation.value("type").toString(), QString("activate"));
            QVERIFY(activation.value("positiveSplitTunnel").toBool());
            QCOMPARE(activation.value("vpnDisabledApps").toArray(), apps);
            QCOMPARE(failed.count(), 0);
        } else {
            QTRY_COMPARE(failed.count(), 1);
            QCOMPARE(peer->bytesAvailable(), qint64(0));
        }
        QVERIFY(controller.m_pendingPositiveActivation.isEmpty());
#endif
    }

    void recreatesInterfaceForPositiveReconnect()
    {
        ConfigOnlyDaemon daemon;
        auto config = configuration();
        QVERIFY(daemon.canReuseInterface(config, config));
        config.m_positiveSplitTunnel = true;
        QVERIFY(!daemon.canReuseInterface(config, config));
        auto negative = config;
        negative.m_positiveSplitTunnel = false;
        QVERIFY(!daemon.canReuseInterface(config, negative));
    }

    void legacyIpcRemainsExclusionMode()
    {
        auto json = configuration().toJson();
        json.remove("positiveSplitTunnel");
        json.insert("vpnDisabledApps", QJsonArray { QStringLiteral("example.exe") });
        InterfaceConfig parsed;
        QVERIFY(Daemon::parseConfig(json, parsed));
        QVERIFY(!parsed.m_positiveSplitTunnel);
        QCOMPARE(parsed.m_vpnDisabledApps, QStringList { QStringLiteral("example.exe") });
    }

    void modeRoundTripWithEmptyList()
    {
        auto config = configuration();
        config.m_positiveSplitTunnel = true;
        InterfaceConfig parsed;
#ifdef Q_OS_WIN
        QVERIFY(Daemon::parseConfig(config.toJson(), parsed));
        QVERIFY(parsed.m_positiveSplitTunnel);
        QVERIFY(parsed.m_vpnDisabledApps.isEmpty());
#else
        QVERIFY(!Daemon::parseConfig(config.toJson(), parsed));
#endif
    }

    void rejectsMalformedMode_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("string") << QJsonValue("true");
        QTest::newRow("integer") << QJsonValue(1);
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("array") << QJsonValue(QJsonArray { });
    }

    void rejectsMalformedMode()
    {
        QFETCH(QJsonValue, value);
        auto json = configuration().toJson();
        json.insert("positiveSplitTunnel", value);
        InterfaceConfig parsed;
        QVERIFY(!Daemon::parseConfig(json, parsed));
    }

    void keepsSystemDnsInPositiveMode()
    {
        auto config = configuration();
        const QMap<QString, QString> options { { "Table", "off" } };
        QVERIFY(config.toWgConf(options).contains("DNS = 192.0.2.53"));
        config.m_positiveSplitTunnel = true;
        const QString wg = config.toWgConf(options);
        QVERIFY(!wg.isEmpty());
        QVERIFY(!wg.contains("DNS ="));
        QVERIFY(wg.contains("Table = off"));
        QVERIFY(wg.contains("AllowedIPs = 0.0.0.0/0"));
    }
};

QTEST_GUILESS_MAIN(TestPositiveSplitTunnel)
#include "testPositiveSplitTunnel.moc"
