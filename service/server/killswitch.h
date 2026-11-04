#ifndef KILLSWITCH_H
#define KILLSWITCH_H

#include <QJsonObject>
#include <QSharedPointer>

#include "secureQSettings.h"

class KillSwitch : public QObject
{
    Q_OBJECT
public:
    static KillSwitch *instance();
    bool init();
    bool refresh(bool enabled);
    bool disableKillSwitch();
    bool disableAllTraffic();
    bool enablePeerTraffic(const QJsonObject &configStr);
    bool enableKillSwitch(const QJsonObject &configStr, int vpnAdapterIndex);
    bool resetAllowedRange(const QStringList &ranges);
    bool addAllowedRange(const QStringList &ranges);
    bool isStrictKillSwitchEnabled();
#ifdef Q_OS_WIN
    bool configureAppPolicy(const QJsonObject &policy);
    bool beginPositiveConnection(const QStringList &paths, bool softKillSwitch);
    bool allowPositiveInterface(int interfaceIndex);
    bool endPositiveConnection(bool manual);
#endif

private:
    KillSwitch(QObject* parent) {};
    QStringList m_allowedRanges;
    QSharedPointer<SecureQSettings> m_appSettigns;
#ifdef Q_OS_WIN
    bool m_positiveMode = false;
    bool m_positiveActive = false;
    bool m_positiveHold = false;
    bool m_positiveSoftKillSwitch = false;
    int m_positiveInterface = 0;
    QStringList m_selectedApps;
#endif
};

#endif // KILLSWITCH_H
