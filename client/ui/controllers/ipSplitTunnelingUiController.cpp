#include "ipSplitTunnelingUiController.h"

#include <QDebug>

#include "systemController.h"
#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"

IpSplitTunnelingUiController::IpSplitTunnelingUiController(IpSplitTunnelingController* ipSplitTunnelingController,
                                                          IpSplitTunnelingModel* ipSplitTunnelingModel, QObject *parent)
    : QObject(parent),
      m_ipSplitTunnelingController(ipSplitTunnelingController),
      m_ipSplitTunnelingModel(ipSplitTunnelingModel)
{
    m_ipSplitTunnelingModel->updateModel(m_ipSplitTunnelingController->getCurrentSites());
}

bool IpSplitTunnelingUiController::addSite(QString hostname)
{
    if (m_ipSplitTunnelingController->addSite(hostname)) {
        emit finished(tr("New site added: %1").arg(hostname));
        return true;
    }
    return false;
}

bool IpSplitTunnelingUiController::removeSite(int index)
{
    auto modelIndex = m_ipSplitTunnelingModel->index(index);
    auto hostname = m_ipSplitTunnelingModel->data(modelIndex, IpSplitTunnelingModel::Roles::UrlRole).toString();
    if (m_ipSplitTunnelingController->removeSite(hostname)) {
        emit finished(tr("Site removed: %1").arg(hostname));
        return true;
    }
    return false;
}

void IpSplitTunnelingUiController::removeSites()
{
    m_ipSplitTunnelingController->removeSites();
    emit finished(tr("Site list cleared!"));
}

bool IpSplitTunnelingUiController::importSites(const QString &fileName, bool replaceExisting)
{
    QByteArray jsonData;
    if (!SystemController::readFile(fileName, jsonData)) {
        emit errorOccurred(tr("Can't open file: %1").arg(fileName));
        return false;
    }

    QString errorMessage;
    if (m_ipSplitTunnelingController->importSitesFromJson(jsonData, replaceExisting, errorMessage)) {
        emit finished(tr("Import completed"));
        return true;
    } else {
        emit errorOccurred(errorMessage);
    }
    return false;
}

void IpSplitTunnelingUiController::exportSites(const QString &fileName)
{
    QByteArray jsonData = m_ipSplitTunnelingController->exportSitesToJson();
    if (!SystemController::saveFile(fileName, jsonData)) {
        qInfo() << "IpSplitTunnelingUiController::exportSites: save or share was cancelled or failed";
        return;
    }
    emit finished(tr("Export completed"));
}

void IpSplitTunnelingUiController::toggleSplitTunneling(bool enabled)
{
    m_ipSplitTunnelingController->toggleSplitTunneling(enabled);
    emit isSplitTunnelingEnabledChanged();
}

void IpSplitTunnelingUiController::setRouteMode(int routeMode)
{
    m_ipSplitTunnelingController->setRouteMode(static_cast<amnezia::RouteMode>(routeMode));
    emit routeModeChanged();
}

int IpSplitTunnelingUiController::getRouteMode() const
{
    return static_cast<int>(m_ipSplitTunnelingController->getRouteMode());
}

bool IpSplitTunnelingUiController::isSplitTunnelingEnabled() const
{
    return m_ipSplitTunnelingController->isSplitTunnelingEnabled();
}

void IpSplitTunnelingUiController::updateModel()
{
    m_ipSplitTunnelingModel->updateModel(m_ipSplitTunnelingController->getCurrentSites());
}
