#include "appSplitTunnelingUiController.h"

#include <QFileInfo>

#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"

AppSplitTunnelingUiController::AppSplitTunnelingUiController(AppSplitTunnelingController* appSplitTunnelingController,
                                                              AppSplitTunnelingModel* appSplitTunnelingModel,
                                                              QObject *parent)
    : QObject(parent),
      m_appSplitTunnelingController(appSplitTunnelingController),
      m_appSplitTunnelingModel(appSplitTunnelingModel)
{
    m_appSplitTunnelingModel->updateModel(m_appSplitTunnelingController->getApps());
}

bool AppSplitTunnelingUiController::addApp(const QString &appPath)
{
    amnezia::InstalledAppInfo appInfo { "", "", appPath };
    if (!appPath.isEmpty()) {
        QFileInfo fileInfo(appPath);
        appInfo.appName = fileInfo.fileName();
    }

    if (m_appSplitTunnelingController->addApp(appInfo)) {
        emit finished(tr("Application added: %1").arg(appInfo.appName));
        return true;
    } else {
        emit errorOccurred(tr("The application has already been added"));
    }
    return false;
}

bool AppSplitTunnelingUiController::addFolder(const QUrl &folder)
{
    if (m_appSplitTunnelingController->addFolder(folder.toLocalFile())) {
        emit finished(tr("Folder added"));
        return true;
    } else {
        emit errorOccurred(tr("The folder is unavailable or has already been added"));
    }
    return false;
}

bool AppSplitTunnelingUiController::addApps(QVector<QPair<QString, QString>> apps)
{
    bool changed = false;
    for (const auto &app : apps) {
#ifdef Q_OS_WIN
        amnezia::InstalledAppInfo appInfo { app.first, "", app.second };
#else
        amnezia::InstalledAppInfo appInfo { app.first, app.second, "" };
#endif
        changed |= m_appSplitTunnelingController->addApp(appInfo);
    }
    if (!changed) {
        return false;
    }
    emit finished(tr("The selected applications have been added"));
    return true;
}

bool AppSplitTunnelingUiController::removeApp(const int index)
{
    auto modelIndex = m_appSplitTunnelingModel->index(index);
    if (!modelIndex.isValid()) {
        return false;
    }
    auto appPath = m_appSplitTunnelingModel->data(modelIndex, AppSplitTunnelingModel::Roles::AppPathRole).toString();
    m_appSplitTunnelingController->removeApp(index);

    QFileInfo fileInfo(appPath);
    emit finished(tr("Application removed: %1").arg(fileInfo.fileName()));
    return true;
}

void AppSplitTunnelingUiController::toggleSplitTunneling(bool enabled)
{
    m_appSplitTunnelingController->toggleSplitTunneling(enabled);
    emit isSplitTunnelingEnabledChanged();
}

void AppSplitTunnelingUiController::setRouteMode(int routeMode)
{
    m_appSplitTunnelingController->setRouteMode(static_cast<amnezia::AppsRouteMode>(routeMode));
    emit routeModeChanged();
}

int AppSplitTunnelingUiController::getRouteMode() const
{
    return static_cast<int>(m_appSplitTunnelingController->getRouteMode());
}

bool AppSplitTunnelingUiController::isSplitTunnelingEnabled() const
{
    return m_appSplitTunnelingController->isSplitTunnelingEnabled();
}

void AppSplitTunnelingUiController::updateModel()
{
    m_appSplitTunnelingModel->updateModel(m_appSplitTunnelingController->getApps());
}

