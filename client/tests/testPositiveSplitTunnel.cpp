#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include "daemon/daemon.h"
#include "daemon/interfaceconfig.h"

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
