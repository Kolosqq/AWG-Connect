#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "core/models/serverDescription.h"
#include "secureQSettings.h"
#include "ui/models/containerProps.h"
#include "ui/models/installedAppsModel.h"
#include "ui/utils/pageEnum.h"
#include "utils/testCoreController.h"
#include "vpnConnection.h"

using namespace amnezia;

class TestUiAppSTModelAndController : public QObject
{
    Q_OBJECT

private:
    TestCoreController *m_coreController;
    SecureQSettings *m_settings;

private slots:
    void testPageLoads()
    {
        QQuickStyle::setStyle("Basic");
        PageLoader::declareQmlPageEnum();
        qmlRegisterType<InstalledAppsModel>("InstalledAppsModel", 1, 0, "InstalledAppsModel");
        ContainerProps containerProps;
        qmlRegisterSingletonInstance("ContainerProps", 1, 0, "ContainerProps", &containerProps);
        QQmlApplicationEngine engine;
        const QString qmlDirectory = qEnvironmentVariable("AMNEZIA_TEST_QML_DIR", QStringLiteral(AMNEZIA_TEST_QML_DIR));
        engine.addImportPath(qmlDirectory + "/Modules");
        engine.rootContext()->setContextProperty("windowHeight", 680);
        engine.rootContext()->setContextProperty("windowWidth", 380);
        auto vpnConnection = QSharedPointer<VpnConnection>::create(nullptr, nullptr);
        TestCoreController controller(vpnConnection, m_settings, &engine);
        QQmlComponent component(&engine, QUrl::fromLocalFile(qmlDirectory + "/Pages2/PageSettingsAppSplitTunneling.qml"));
        QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 10000);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> page(component.create());
        QVERIFY2(page, qPrintable(component.errorString()));
        QQuickWindow window;
        window.resize(380, 680);
        auto pageItem = qobject_cast<QQuickItem *>(page.data());
        QVERIFY(pageItem);
        pageItem->setParentItem(window.contentItem());
        window.show();
        QVERIFY(page->property("pageEnabled").toBool());
        auto selector = page->findChild<QObject *>("appRouteModeSelector");
        QVERIFY(selector);
#ifdef Q_OS_WIN
        QVERIFY(selector->property("enabled").toBool());
        auto choices = page->findChild<QObject *>("appSelectionDrawer");
        auto processes = page->findChild<QObject *>("installedAppsDrawer");
        QVERIFY(choices);
        QVERIFY(processes);
        QVERIFY(QMetaObject::invokeMethod(choices, "open"));
        QTRY_VERIFY(choices->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(choices, "close"));
        QVERIFY(QMetaObject::invokeMethod(choices, "runningAppsRequested"));
        QTRY_VERIFY(processes->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(processes, "close"));
#endif
        QVERIFY(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 0)));
        QCOMPARE(controller.m_appSplitTunnelingUiController->getRouteMode(), int(AppsRouteMode::VpnOnlyForwardApps));
        QCOMPARE(page->property("routeModeIndex").toInt(), 0);
        controller.m_appSplitTunnelingUiController->setRouteMode(AppsRouteMode::VpnAllExceptApps);
        QCOMPARE(page->property("routeModeIndex").toInt(), 1);
        page->setProperty("pageEnabled", false);
        QVERIFY(!selector->property("enabled").toBool());
        page.reset();
#ifdef Q_OS_WIN
        controller.m_appSplitTunnelingUiController->toggleSplitTunneling(true);
        controller.m_appSplitTunnelingUiController->setRouteMode(AppsRouteMode::VpnOnlyForwardApps);
        QQmlComponent killSwitchComponent(&engine,
                                          QUrl::fromLocalFile(qmlDirectory + "/Pages2/PageSettingsKillSwitch.qml"));
        QTRY_VERIFY_WITH_TIMEOUT(killSwitchComponent.status() != QQmlComponent::Loading, 10000);
        QVERIFY2(killSwitchComponent.isReady(), qPrintable(killSwitchComponent.errorString()));
        QScopedPointer<QObject> killSwitchPage(killSwitchComponent.create());
        QVERIFY2(killSwitchPage, qPrintable(killSwitchComponent.errorString()));
        auto strictChoice = killSwitchPage->findChild<QObject *>("strictKillSwitchChoice");
        QVERIFY(strictChoice);
        QVERIFY(strictChoice->property("visible").toBool());
        QVERIFY(strictChoice->property("enabled").toBool());
        controller.m_appSplitTunnelingUiController->setRouteMode(AppsRouteMode::VpnAllExceptApps);
        QVERIFY(!strictChoice->property("visible").toBool());
#endif
    }

    void testSeparateApplicationLists()
    {
        auto controller = m_coreController->m_appSplitTunnelingUiController;
        auto model = m_coreController->m_appSplitTunnelingModel;
        controller->setRouteMode(AppsRouteMode::VpnOnlyForwardApps);
        QCOMPARE(model->rowCount(), 0);
        controller->addApp(QCoreApplication::applicationFilePath());
        QCOMPARE(model->rowCount(), 1);
        controller->setRouteMode(AppsRouteMode::VpnAllExceptApps);
        QCOMPARE(model->rowCount(), 0);
        controller->setRouteMode(AppsRouteMode::VpnOnlyForwardApps);
        QCOMPARE(model->rowCount(), 1);
        controller->removeApp(0);
        QCOMPARE(controller->getRouteMode(), int(AppsRouteMode::VpnOnlyForwardApps));
        QCOMPARE(model->rowCount(), 0);
    }

    void testFolderAndRunningProcessSelection()
    {
#ifdef Q_OS_WIN
        auto controller = m_coreController->m_appSplitTunnelingUiController;
        auto model = m_coreController->m_appSplitTunnelingModel;
        controller->setRouteMode(AppsRouteMode::VpnOnlyForwardApps);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        controller->addFolder(QUrl::fromLocalFile(folder.path()));
        controller->updateModel();
        QCOMPARE(model->rowCount(), 1);
        const QString stored =
                m_coreController->m_appSettingsRepository->vpnApps(AppsRouteMode::VpnOnlyForwardApps).first().appPath;
        QVERIFY(stored.endsWith('/'));
        QCOMPARE(QFileInfo(stored).canonicalFilePath(), QFileInfo(folder.path()).canonicalFilePath());
        controller->addFolder(QUrl::fromLocalFile(folder.path()));
        controller->updateModel();
        QCOMPARE(model->rowCount(), 1);
        controller->removeApp(0);
        InstalledAppsModel processes;
        processes.updateModel();
        int current = -1;
        QSet<QString> paths;
        for (int row = 0; row < processes.rowCount(); ++row) {
            const QString path = processes.data(processes.index(row, 0), InstalledAppsModel::PackageNameRole).toString();
            QVERIFY(!paths.contains(path.toCaseFolded()));
            paths.insert(path.toCaseFolded());
            if (path.compare(QCoreApplication::applicationFilePath(), Qt::CaseInsensitive) == 0) {
                current = row;
            }
        }
        QVERIFY(current >= 0);
        processes.selectedStateChanged(current, true);
        controller->addApps(processes.getSelectedAppsInfo());
        controller->updateModel();
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(m_coreController->m_appSettingsRepository->vpnApps(AppsRouteMode::VpnOnlyForwardApps).first().appPath,
                 QCoreApplication::applicationFilePath());
        processes.selectedStateChanged(current, true);
        processes.updateModel();
        QVERIFY(processes.getSelectedAppsInfo().isEmpty());
        controller->removeApp(0);
#endif
    }

    void initTestCase()
    {
        QString testOrg = "AmneziaVPN-Test-" + QUuid::createUuid().toString();
        m_settings = new SecureQSettings(testOrg, "amnezia-client", nullptr, false);

        auto vpnConnection = QSharedPointer<VpnConnection>::create(nullptr, nullptr);

        m_coreController = new TestCoreController(vpnConnection, m_settings, nullptr, this);
    }

    void cleanupTestCase()
    {
        m_settings->clearSettings();
        delete m_coreController;
        delete m_settings;
    }

    void init()
    {
        m_settings->clearSettings();
        m_coreController->m_appSplitTunnelingUiController->setRouteMode(AppsRouteMode::VpnOnlyForwardApps);
        if (m_coreController->m_serversModel) {
            m_coreController->m_serversModel->updateModel(QVector<ServerDescription>(), QString{});
        }
    }

    void testRolesAndSignals()
    {
        QSignalSpy finishedSpy(m_coreController->m_appSplitTunnelingUiController, &AppSplitTunnelingUiController::finished);
        QSignalSpy errorOccurredSpy(m_coreController->m_appSplitTunnelingUiController, &AppSplitTunnelingUiController::errorOccurred);
        QSignalSpy isSplitTunnelingChangedSpy(m_coreController->m_appSplitTunnelingUiController, &AppSplitTunnelingUiController::isSplitTunnelingEnabledChanged);

        m_coreController->m_appSplitTunnelingUiController->toggleSplitTunneling(true);
        QVERIFY2(isSplitTunnelingChangedSpy.count() == 1, "isSplitTunnelingChangedSpy signal should be emitted");
        QVERIFY2(m_coreController->m_appSplitTunnelingUiController->isSplitTunnelingEnabled() == true, "AppSplitTunneling should be enabled");

        m_coreController->m_appSplitTunnelingUiController->toggleSplitTunneling(false);
        QVERIFY2(isSplitTunnelingChangedSpy.count() == 2, "isSplitTunnelingChangedSpy signal should be emitted 2nd time");
        QVERIFY2(m_coreController->m_appSplitTunnelingUiController->isSplitTunnelingEnabled() == false, "AppSplitTunneling should be disabled");

        const QString app = QCoreApplication::applicationFilePath();

        m_coreController->m_appSplitTunnelingUiController->addApp(app);
        m_coreController->m_appSplitTunnelingUiController->updateModel();
        QVERIFY2(finishedSpy.count() == 1, "finished signal should be emitted");
        QVERIFY2(m_coreController->m_appSplitTunnelingModel->rowCount() == 1, "AppSplitTunnelingModel should have 1 row");

        QModelIndex appSTModelIndex = m_coreController->m_appSplitTunnelingModel->index(0, 0);
        QVERIFY2(appSTModelIndex.isValid(), "Site model index should be valid");

        auto appPath = m_coreController->m_appSplitTunnelingModel->data(appSTModelIndex, AppSplitTunnelingModel::AppPathRole);
        QString msg = QString("app path should be %1, got %2").arg(app, appPath.toString());
        QVERIFY2(app.contains(appPath.toString()) == true, msg.toLocal8Bit().constData());

        auto pkgAppName = m_coreController->m_appSplitTunnelingModel->data(appSTModelIndex, AppSplitTunnelingModel::PackageAppNameRole);
        QVERIFY2(pkgAppName == true, "app name should be set");

        auto pkgAppIcon = m_coreController->m_appSplitTunnelingModel->data(appSTModelIndex, AppSplitTunnelingModel::PackageAppIconRole);
        QVERIFY2(pkgAppIcon == true, "app image should be set");

        m_coreController->m_appSplitTunnelingUiController->addApp(app);
        m_coreController->m_appSplitTunnelingUiController->updateModel();
        QVERIFY2(errorOccurredSpy.count() == 1, "errorOccurred signal should be emitted");
        QVERIFY2(m_coreController->m_appSplitTunnelingModel->rowCount() == 1, "AppSplitTunnelingModel should have 3 rows (same app should not be added)");

        m_coreController->m_appSplitTunnelingUiController->removeApp(0);
        m_coreController->m_appSplitTunnelingUiController->updateModel();
        QVERIFY2(finishedSpy.count() == 2, "finished signal should be emitted");
        QVERIFY2(m_coreController->m_appSplitTunnelingModel->rowCount() == 0, "AppSplitTunnelingModel should have 0 rows");
    }
};

QTEST_MAIN(TestUiAppSTModelAndController)
#include "testUiAppSTModelAndController.moc"
