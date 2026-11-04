/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "windowsdaemon.h"

#include <Windows.h>
#include <qassert.h>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocalSocket>
#include <QNetworkInterface>
#include <QTextStream>
#include <QtGlobal>

#include "daemon/daemonerrors.h"
#include "dnsutilswindows.h"
#include "killswitch.h"
#include "leakdetector.h"
#include "logger.h"
#include "platforms/windows/daemon/windowsfirewall.h"
#include "platforms/windows/daemon/windowssplittunnel.h"
#include "windowsfirewall.h"

#include "core/utils/appSelection.h"
#include "core/utils/networkUtilities.h"

namespace {
Logger logger("WindowsDaemon");
}

WindowsDaemon::WindowsDaemon() : Daemon(nullptr) {
  MZ_COUNT_CTOR(WindowsDaemon);
  m_firewallManager = WindowsFirewall::create(this);
  Q_ASSERT(m_firewallManager != nullptr);

  m_wgutils = WireguardUtilsWindows::create(m_firewallManager, this);
  m_dnsutils = new DnsUtilsWindows(this);
  m_splitTunnelManager = WindowsSplitTunnel::create(m_firewallManager);
  m_appSelectionTimer.setInterval(2000);
  connect(&m_appSelectionTimer, &QTimer::timeout, this, &WindowsDaemon::refreshAppSelection);

  connect(m_wgutils.get(), &WireguardUtilsWindows::backendFailure, this,
          &WindowsDaemon::monitorBackendFailure);
  connect(this, &WindowsDaemon::activationFailure,
          [this]() { m_firewallManager->disableKillSwitch(); });
}

WindowsDaemon::~WindowsDaemon() {
  MZ_COUNT_DTOR(WindowsDaemon);
  logger.debug() << "Daemon released";
}

bool WindowsDaemon::activate(const InterfaceConfig &config)
{
    m_appSelectionTimer.stop();
    const auto selection = appSelection::resolve(config.m_vpnDisabledApps);
    if (!selection.error.isEmpty()) {
        emit activationFailure();
        emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_EXCLUDE_FAILURE);
        return false;
    }
    InterfaceConfig resolved = config;
    resolved.m_vpnDisabledApps = selection.executables;
    if (config.m_positiveSplitTunnel) {
        if ((!m_connections.isEmpty() && !deactivate(false))
            || !KillSwitch::instance()->beginPositiveConnection(config.m_vpnDisabledApps, config.m_killSwitchEnabled)) {
            emit activationFailure();
            emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_EXCLUDE_FAILURE);
            return false;
        }
    }
    const bool activated = Daemon::activate(resolved);
    if (!activated && config.m_positiveSplitTunnel) {
        if (m_splitTunnelManager) {
            m_splitTunnelManager->stop();
        }
        if (!deactivate(false)) {
            logger.error() << "Failed to clean up positive split tunneling activation";
        }
    }
    if (activated) {
        m_appSelection = config.m_vpnDisabledApps;
        if (!selection.directories.isEmpty()) {
            m_appSelectionTimer.start();
        }
    }
    return activated;
}

bool WindowsDaemon::deactivate(bool emitSignals)
{
    m_appSelectionTimer.stop();
    if (!KillSwitch::instance()->endPositiveConnection(false)) {
        return false;
    }
    return Daemon::deactivate(emitSignals);
}

bool WindowsDaemon::deactivateManually()
{
    if (!KillSwitch::instance()->endPositiveConnection(true)) {
        return false;
    }
    return deactivate();
}

void WindowsDaemon::refreshAppSelection()
{
    if (m_connections.isEmpty()) {
        return;
    }
    const auto selection = appSelection::resolve(m_appSelection);
    auto &config = m_connections.begin().value().m_config;
    if (selection.error.isEmpty() && selection.executables == config.m_vpnDisabledApps) {
        return;
    }
    if (!selection.error.isEmpty() || !m_splitTunnelManager) {
        monitorBackendFailure();
        return;
    }
    if (selection.executables.isEmpty()) {
        m_splitTunnelManager->stop();
    } else if (!m_splitTunnelManager->start(m_inetAdapterIndex, 0, config.m_positiveSplitTunnel)
               || !m_splitTunnelManager->excludeApps(selection.executables, true) || !m_splitTunnelManager->isRunning()) {
        monitorBackendFailure();
        return;
    }
    config.m_vpnDisabledApps = selection.executables;
}

void WindowsDaemon::prepareActivation(const InterfaceConfig& config, int inetAdapterIndex) {
  // Before creating the interface we need to check which adapter
  // routes to the server endpoint
  if (inetAdapterIndex == 0) {
      auto serveraddr = QHostAddress(config.m_serverIpv4AddrIn);
      m_inetAdapterIndex = NetworkUtilities::AdapterIndexTo(serveraddr);
  } else {
      m_inetAdapterIndex = inetAdapterIndex;
  }
}

void WindowsDaemon::activateSplitTunnel(const InterfaceConfig& config, int vpnAdapterIndex) {
    if (m_splitTunnelManager == nullptr) {
        if (config.m_vpnDisabledApps.length() > 0) {
            logger.error() << "Split tunnel manager is not initialized";
            emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_INIT_FAILURE);
        }
        return;
    }

  if (config.m_vpnDisabledApps.length() > 0) {
      if (!m_splitTunnelManager->start(m_inetAdapterIndex, vpnAdapterIndex, config.m_positiveSplitTunnel)) {
          logger.error() << "Failed to start split tunnel";
          emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_START_FAILURE);
          return;
      }
      if (!m_splitTunnelManager->excludeApps(config.m_vpnDisabledApps, config.m_positiveSplitTunnel)) {
          logger.error() << "Failed to apply split tunnel app exclusions";
          emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_EXCLUDE_FAILURE);
          return;
      }
      if (!m_splitTunnelManager->isRunning()) {
          logger.error() << "Split tunnel did not reach running state";
          emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_START_FAILURE);
      }
  } else {
      m_splitTunnelManager->stop();
  }
}

bool WindowsDaemon::run(Op op, const InterfaceConfig& config) {
  if (!m_splitTunnelManager) {
    if (config.m_vpnDisabledApps.length() > 0) {
      // The Client has sent us a list of disabled apps, but we failed
      // to init the the split tunnel driver.
      // So let the client know this was not possible
      logger.error() << "Split tunnel manager is not initialized";
      emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_INIT_FAILURE);
      return false;
    }
    return true;
  }

  if (op == Down) {
    m_splitTunnelManager->stop();
    return true;
  }
  if (config.m_vpnDisabledApps.length() > 0) {
      if (!m_splitTunnelManager->start(m_inetAdapterIndex, 0, config.m_positiveSplitTunnel)) {
          logger.error() << "Split tunnel start failed";
          emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_START_FAILURE);
          return false;
      };
      if (!m_splitTunnelManager->excludeApps(config.m_vpnDisabledApps, config.m_positiveSplitTunnel)) {
          logger.error() << "Split tunnel app exclusion failed";
          emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_EXCLUDE_FAILURE);
          return false;
      };
    // Now the driver should be running (State == 4)
    if (!m_splitTunnelManager->isRunning()) {
      logger.error() << "Split tunnel did not reach running state";
      emit backendFailure(DaemonError::ERROR_SPLIT_TUNNEL_START_FAILURE);
      return false;
    }
    return true;
  }
  m_splitTunnelManager->stop();

  return true;
}

void WindowsDaemon::monitorBackendFailure() {
  logger.warning() << "Tunnel service is down";

  emit backendFailure();
  deactivate();
}
