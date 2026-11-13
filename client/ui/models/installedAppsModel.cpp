#include "installedAppsModel.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QtConcurrent>

#ifdef Q_OS_WIN
// clang-format off
    #include <windows.h>
    #include <tlhelp32.h>
// clang-format on
#endif

#ifdef Q_OS_ANDROID
    #include "platforms/android/android_controller.h"
#endif

InstalledAppsModel::InstalledAppsModel(QObject *parent) : QAbstractListModel(parent)
{
}

int InstalledAppsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_installedApps.size();
}

QVariant InstalledAppsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rowCount()))
        return QVariant();

    switch (role) {
    case AppNameRole: {
        auto appName = m_installedApps.at(index.row()).toObject().value("name").toString();
        auto packageName = m_installedApps.at(index.row()).toObject().value("package").toString();
        if (appName.isEmpty()) {
            appName = packageName;
        }
        return appName;
    }
    case AppIconRole: {
        const auto app = m_installedApps.at(index.row()).toObject().value("package").toString();
#ifdef Q_OS_WIN
        return QString::fromLatin1(app.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
#else
        return app;
#endif
    }
    case PackageNameRole: {
        return m_installedApps.at(index.row()).toObject().value("package");
    }
    case IsAppSelectedRole: {
        return m_selectedAppIndexes.contains(index.row());
    }
    }

    return QVariant();
}

void InstalledAppsModel::selectedStateChanged(const int index, const bool selected)
{
    if (selected) {
        m_selectedAppIndexes.insert(index);
    } else {
        m_selectedAppIndexes.remove(index);
    }
}

QVector<QPair<QString, QString>> InstalledAppsModel::getSelectedAppsInfo()
{
    QVector<QPair<QString, QString>> appsInfo;
    for (const auto i : m_selectedAppIndexes) {
        QString packageName = data(index(i, 0), PackageNameRole).toString();
        QString appName = data(index(i, 0), AppNameRole).toString();
        if (appName.isEmpty()) {
            appName = packageName;
        }

        appsInfo.push_back({ appName, packageName });
    }

    m_selectedAppIndexes.clear();
    return appsInfo;
}

void InstalledAppsModel::updateModel()
{
#ifdef Q_OS_WIN
    QJsonArray apps;
    QSet<QString> seen;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry { };
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
                if (!process) {
                    continue;
                }
                wchar_t buffer[32768];
                DWORD length = 32768;
                if (QueryFullProcessImageNameW(process, 0, buffer, &length)) {
                    const QString path = QDir::fromNativeSeparators(QString::fromWCharArray(buffer, length));
                    if (!seen.contains(path.toCaseFolded())) {
                        seen.insert(path.toCaseFolded());
                        apps.append(QJsonObject { { "name", QFileInfo(path).fileName() }, { "package", path } });
                    }
                }
                CloseHandle(process);
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    beginResetModel();
    m_selectedAppIndexes.clear();
    m_installedApps = apps;
    endResetModel();
#endif
#ifdef Q_OS_ANDROID
    QFuture<QJsonArray> future = QtConcurrent::run([]() {
        return AndroidController::instance()->getAppList();
    });

    QFutureWatcher<QJsonArray> watcher;
    QEventLoop wait;
    connect(&watcher, &QFutureWatcher<QJsonArray>::finished, &wait, &QEventLoop::quit);
    watcher.setFuture(future);
    wait.exec();

    beginResetModel();
    m_installedApps = future.result();
    endResetModel();
#endif
}

QHash<int, QByteArray> InstalledAppsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[AppNameRole] = "appName";
    roles[AppIconRole] = "appIcon";
    roles[PackageNameRole] = "packageName";
    roles[IsAppSelectedRole] = "isAppSelected";
    return roles;
}
