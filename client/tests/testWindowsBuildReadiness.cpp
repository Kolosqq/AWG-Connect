#include <QCryptographicHash>
#include <QTest>

#include "core/controllers/gatewayController.h"

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
};

QTEST_MAIN(TestWindowsBuildReadiness)
#include "testWindowsBuildReadiness.moc"
