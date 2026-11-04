#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>
#include <QUuid>

#include "core/controllers/connectionController.h"
#include "core/controllers/gatewayController.h"
#include "core/utils/constants/apiKeys.h"
#include "core/utils/constants/configKeys.h"
#include "ui/controllers/connectionUiController.h"
#include "utils/testCoreController.h"
#include "vpnConnection.h"

class TestWindowsBuildReadiness : public QObject
{
    Q_OBJECT

private slots:
    void productionGatewayInitializes()
    {
        const QByteArray key(PROD_AGW_PUBLIC_KEY);
        QCOMPARE(QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex(),
                 QByteArray("8c470f207d071799be894e5562a171da73400268a195a32106e7fd56fe30bc36"));
        QVERIFY(!QByteArray(PROD_S3_ENDPOINT).isEmpty());
        QVERIFY(!QByteArray(FALLBACK_S3_ENDPOINT).isEmpty());
        GatewayController gateway("https://gateway.invalid", false, 1000, false, nullptr);
        QVERIFY(!gateway.m_publicKeyMissing);
        QVERIFY(gateway.m_client != 0);
    }

    void positiveConnectionValidation_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<bool>("wireguard");
        QTest::addColumn<QString>("scenario");
        const QList<QPair<QString, int>> kinds {
            { "premium", serverConfigUtils::AmneziaPremiumV2 }, { "free", serverConfigUtils::AmneziaFreeV3 },
            { "external", serverConfigUtils::ExternalPremium }, { "admin", serverConfigUtils::SelfHostedAdmin },
            { "shared", serverConfigUtils::SelfHostedUser },    { "native", serverConfigUtils::Native }
        };
        for (const auto &[name, kind] : kinds) {
            for (bool wireguard : { false, true }) {
                for (const QString &scenario : { "default-kill-switch", "strict-kill-switch", "sites", "partial-routes",
                                                 "supported", "exclusion" }) {
                    const QByteArray label = (name + (wireguard ? "-wg-" : "-awg-") + scenario).toUtf8();
                    QTest::newRow(label.constData()) << kind << wireguard << scenario;
                }
            }
        }
    }

    void positiveConnectionValidation()
    {
        QFETCH(int, kind);
        QFETCH(bool, wireguard);
        QFETCH(QString, scenario);
        SecureQSettings settings("AmneziaVPN-Test-" + QUuid::createUuid().toString(QUuid::Id128), "positive-validation",
                                 nullptr, false);
        auto vpn = QSharedPointer<VpnConnection>::create(nullptr, nullptr);
        TestCoreController core(vpn, &settings, nullptr);
        auto connection = core.findChild<ConnectionController *>();
        QVERIFY(connection);
        auto repository = core.m_serversRepository;
        auto appSettings = core.m_appSettingsRepository;
        auto ui = core.m_connectionUiController;
        disconnect(repository, &SecureServersRepository::serverAdded, nullptr, nullptr);
        disconnect(connection, &ConnectionController::openConnectionRequested, nullptr, nullptr);

        const QString protocol = wireguard ? configKey::wireguard : configKey::awg;
        const QString container = wireguard ? configKey::amneziaWireguard : configKey::amneziaAwg;
        const QJsonObject clientConfig {
            { configKey::config, "[Interface]\nAddress = 10.233.0.2/32\n[Peer]\nEndpoint = 192.0.2.1:51820\n" },
            { configKey::allowedIps, QJsonArray { scenario == "partial-routes" ? "10.0.0.0/8" : "0.0.0.0/0" } }
        };
        const QJsonObject protocolConfig { { configKey::lastConfig,
                                             QString::fromUtf8(QJsonDocument(clientConfig).toJson()) },
                                           { configKey::isThirdPartyConfig, kind == serverConfigUtils::Native } };
        QJsonObject server {
            { configKey::hostName, "192.0.2.1" },
            { configKey::defaultContainer, container },
            { configKey::containers,
              QJsonArray { QJsonObject { { configKey::container, container }, { protocol, protocolConfig } } } }
        };
        if (serverConfigUtils::isApiV2Subscription(static_cast<serverConfigUtils::ConfigType>(kind))) {
            const QString service = kind == serverConfigUtils::AmneziaPremiumV2 ? "amnezia-premium"
                    : kind == serverConfigUtils::AmneziaFreeV3                  ? "amnezia-free"
                                                                                : "external-premium";
            server.insert(configKey::configVersion, serverConfigUtils::AmneziaGateway);
            server.insert(apiDefs::key::apiConfig, QJsonObject { { apiDefs::key::serviceType, service } });
        } else if (kind == serverConfigUtils::SelfHostedAdmin) {
            server.insert(configKey::userName, "test-admin");
            server.insert(configKey::password, QString(8, 'x'));
            server.insert(configKey::port, 22);
        }
        const QString serverId = repository->addServer({ }, server, static_cast<serverConfigUtils::ConfigType>(kind));
        QCOMPARE(repository->serversCount(), 1);
        repository->setDefaultServer(serverId);
        appSettings->setAppsSplitTunnelingEnabled(true);
        appSettings->setAppsRouteMode(scenario == "exclusion" ? AppsRouteMode::VpnAllExceptApps
                                                              : AppsRouteMode::VpnOnlyForwardApps);
        if (scenario != "default-kill-switch" && scenario != "exclusion") {
            appSettings->setKillSwitchEnabled(false);
        }
        appSettings->setStrictKillSwitchEnabled(scenario == "strict-kill-switch");
        appSettings->setSitesSplitTunnelingEnabled(scenario == "sites");
        QSignalSpy activation(connection, &ConnectionController::openConnectionRequested);
        QSignalSpy errors(ui, &ConnectionUiController::connectionErrorOccurred);

        for (int attempt = 0; attempt < 2; ++attempt) {
            connection->setConnectionState(Vpn::ConnectionState::Disconnected);
            connection->setConnectionState(Vpn::ConnectionState::Preparing);
            QVERIFY(ui->isConnectionInProgress());
            ui->openConnection();
            if (scenario == "supported" || scenario == "exclusion" || scenario == "default-kill-switch"
                || scenario == "strict-kill-switch") {
                QCOMPARE(errors.count(), 0);
                QCOMPARE(activation.count(), attempt + 1);
            } else {
                QCOMPARE(activation.count(), 0);
                QCOMPARE(errors.count(), attempt + 1);
                QCOMPARE(qvariant_cast<ErrorCode>(errors.last().first()),
                         ErrorCode::PositiveSplitTunnelConfigurationError);
                QVERIFY(!ui->isConnectionInProgress());
                QCOMPARE(ui->connectionStateText(), ui->tr("Connect"));
            }
        }
        QVERIFY(!vpn->vpnProtocol());
        settings.clearSettings();
    }
};

QTEST_MAIN(TestWindowsBuildReadiness)
#include "testWindowsBuildReadiness.moc"
