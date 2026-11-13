#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on

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

    void verifiesDetachedSignature()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray bytes("test update payload");
        QFile payload(directory.filePath("update.msi"));
        QFile signature(directory.filePath("update.msi.sig"));
        QVERIFY(payload.open(QIODevice::WriteOnly));
        QCOMPARE(payload.write(bytes), bytes.size());
        payload.close();

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_KEY_HANDLE key = nullptr;
        QCOMPARE(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0), 0L);
        QCOMPARE(BCryptGenerateKeyPair(algorithm, &key, 3072, 0), 0L);
        QCOMPARE(BCryptFinalizeKeyPair(key, 0), 0L);
        QByteArray publicKey(411, Qt::Uninitialized);
        ULONG length = 0;
        QCOMPARE(BCryptExportKey(key, nullptr, BCRYPT_RSAPUBLIC_BLOB, reinterpret_cast<PUCHAR>(publicKey.data()),
                                 publicKey.size(), &length, 0),
                 0L);
        QCOMPARE(length, 411UL);
        auto digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
        QByteArray signedBytes(384, Qt::Uninitialized);
        BCRYPT_PSS_PADDING_INFO padding { BCRYPT_SHA256_ALGORITHM, 32 };
        QCOMPARE(BCryptSignHash(key, &padding, reinterpret_cast<PUCHAR>(digest.data()), digest.size(),
                                reinterpret_cast<PUCHAR>(signedBytes.data()), signedBytes.size(), &length, BCRYPT_PAD_PSS),
                 0L);
        QCOMPARE(length, 384UL);
        BCryptDestroyKey(key);
        BCryptCloseAlgorithmProvider(algorithm, 0);

        QVERIFY(signature.open(QIODevice::WriteOnly));
        QCOMPARE(signature.write(signedBytes), signedBytes.size());
        signature.close();
        const auto packagePath = payload.fileName().toStdWString();
        const auto signaturePath = signature.fileName().toStdWString();
        const auto pin = publicKey.toHex().toStdString();
        QVERIFY(windowsUpdate::verifySignature(packagePath, signaturePath, pin));
        auto wrongKey = publicKey;
        wrongKey[100] = char(wrongKey.at(100) ^ 1);
        QVERIFY(!windowsUpdate::verifySignature(packagePath, signaturePath, wrongKey.toHex().toStdString()));
        QVERIFY(signature.open(QIODevice::Append));
        QCOMPARE(signature.write("x"), qint64(1));
        signature.close();
        QVERIFY(!windowsUpdate::verifySignature(packagePath, signaturePath, pin));
        QVERIFY(signature.open(QIODevice::WriteOnly));
        QCOMPARE(signature.write(signedBytes), signedBytes.size());
        signature.close();
        QVERIFY(payload.open(QIODevice::Append));
        QCOMPARE(payload.write("x"), qint64(1));
        payload.close();
        QVERIFY(!windowsUpdate::verifySignature(packagePath, signaturePath, pin));
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
