#ifndef UPDATECONTROLLER_H
#define UPDATECONTROLLER_H

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QTemporaryDir>
#include <QVersionNumber>
#include <memory>

#include "core/repositories/secureAppSettingsRepository.h"

namespace UpdateState
{
    Q_NAMESPACE
    enum class State {
        Idle = 0,
        Downloading,
        ReadyToInstall,
        DownloadError
    };
    Q_ENUM_NS(State)

    inline void declareQmlUpdateStateEnum()
    {
        qmlRegisterUncreatableMetaObject(UpdateState::staticMetaObject, "UpdateState", 1, 0, "UpdateState",
                                         "Error: only enums");
    }
}

class UpdateController : public QObject
{
    Q_OBJECT
public:
    explicit UpdateController(SecureAppSettingsRepository* appSettingsRepository, QObject *parent = nullptr);

    QString getVersion() const;
    QString getReleaseDate() const;
    QString getDescription() const;
    QStringList getTags() const;
    QStringList getNewFeatures() const;
    QStringList getImprovements() const;
    QStringList getBugFixes() const;
    UpdateState::State getUpdateState() const;
    bool isStoreUpdate() const;
    bool isUpdateCheckRunning() const;
    bool canInstall() const;
    bool isUpstreamNotice() const;
    void markUpstreamNoticeAsRead();
    bool isPreviousVersion() const;
    QStringList previousVersions() const;
    void checkPreviousVersions();
    void selectPreviousVersion(const QString &version);
    void downloadPreviousVersion(const QString &destination);
    int progress() const;

public slots:
    void checkForUpdates(bool automatic = false);
    void startUpdate();
    void installUpdate();

signals:
    void updateFound();
    void progressChanged();
    void updateNotFound();
    void updateCheckFailed();
    void updateStateChanged();
    void updateCheckRunningChanged();
    void previousVersionsFound();
    void previousVersionSaved();

private:
    void setUpdateState(UpdateState::State state);
    void setUpdateCheckRunning(bool running);
    void downloadInstaller();
    QString composeDownloadUrl() const;
    void openStorePage() const;

    SecureAppSettingsRepository* m_appSettingsRepository;

    QString m_version;
    QString m_releaseDate;
    QString m_description;
    QStringList m_tags;
    QStringList m_newFeatures;
    QStringList m_improvements;
    QStringList m_bugFixes;
    QString m_downloadBaseUrl;
    QString m_releasePageUrl;

    UpdateState::State m_updateState = UpdateState::State::Idle;
    bool m_updateCheckRunning = false;

#if defined(Q_OS_WIN)
    void checkGithubRelease(bool upstream, bool automatic);
    void showWindowsRelease(const QJsonObject &release, const QString &version, bool upstream, bool previous);
    void downloadWindowsPackage(const QString &destination = { });
    void downloadWindowsSignature();
    void installWindowsPackage();
    std::unique_ptr<QTemporaryDir> m_staging;
    QString m_packageUrl;
    QString m_packagePath;
    qint64 m_packageSize = 0;
    int m_progress = 0;
    bool m_upstreamNotice = false;
    bool m_previousVersion = false;
    QMap<QVersionNumber, QJsonObject> m_previousReleases;
    QString m_previousPackageDestination;
#elif defined(Q_OS_MACOS) && !defined(MACOS_NE)
    int runMacInstaller(const QString &installerPath);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    int runLinuxInstaller(const QString &installerPath);
#endif
};

#endif // UPDATECONTROLLER_H
