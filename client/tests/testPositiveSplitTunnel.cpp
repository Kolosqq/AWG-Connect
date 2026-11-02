#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QSignalSpy>
#include <QTest>
#include <QUuid>

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
    void negotiatesServiceSupport_data()
    {
        QTest::addColumn<bool>("supported");
        QTest::newRow("original-service") << false;
        QTest::newRow("matching-service") << true;
    }

    void negotiatesServiceSupport()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows-only mode");
#else
        QFETCH(bool, supported);
        QLocalServer server;
        const auto name = "amnezia-positive-test-" + QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY(server.listen(name));
        LocalSocketController controller;
        QSignalSpy failed(&controller, &ControllerImpl::activationFailed);
        controller.m_daemonState = LocalSocketController::eInitializing;
        controller.m_socket->connectToServer(name);
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QLocalSocket> peer(server.nextPendingConnection());
        QTRY_VERIFY(peer->canReadLine());
        QCOMPARE(QJsonDocument::fromJson(peer->readLine()).object().value("type").toString(), QString("status"));

        QJsonObject raw;
        raw.insert("appSplitTunnelType", int(amnezia::AppsRouteMode::VpnOnlyForwardApps));
        raw.insert("splitTunnelApps", QJsonArray { "example.exe" });
        controller.activate(raw);
        QVERIFY(!controller.m_pendingPositiveActivation.isEmpty());
        QJsonObject status { { "type", "status" }, { "connected", false } };
        if (supported) {
            status.insert("positiveSplitTunnelSupported", true);
        }
        peer->write(QJsonDocument(status).toJson(QJsonDocument::Compact) + '\n');
        peer->flush();
        if (supported) {
            QTRY_VERIFY(peer->canReadLine());
            const auto activation = QJsonDocument::fromJson(peer->readLine()).object();
            QCOMPARE(activation.value("type").toString(), QString("activate"));
            QVERIFY(activation.value("positiveSplitTunnel").toBool());
            QCOMPARE(activation.value("vpnDisabledApps").toArray(), QJsonArray { "example.exe" });
            QCOMPARE(failed.count(), 0);
        } else {
            QTRY_COMPARE(failed.count(), 1);
            QCOMPARE(failed.first().first().value<amnezia::ErrorCode>(),
                     amnezia::ErrorCode::PositiveSplitTunnelServiceError);
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
