#include <roapi.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Storage.h>

#include "codexPackageFolders.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>
#include <optional>

#include "core/utils/appSelection.h"

#pragma comment(lib, "windowsapp.lib")

namespace
{
    QString familyKey(const QString &path)
    {
        static const QRegularExpression package(
                R"(^([A-Z]:/(?:[^/]+/)*WindowsApps/)(OpenAI\.Codex|OpenAI\.CodexPrimaryRuntime\.v[0-9]+(?:-[0-9]+){3})_[0-9]+(?:\.[0-9]+){3}_(x64|x86|arm64|neutral)__([a-z0-9]{13})/$)",
                QRegularExpression::CaseInsensitiveOption);
        const auto match = package.match(path);
        if (!match.hasMatch()) {
            return { };
        }
        // Codex Runtime includes its build number in the package name as well as the package version.
        const QString name = match.captured(2).section(".v", 0, 0, QString::SectionCaseInsensitiveSeps);
        return (match.captured(1) + name + '_' + match.captured(3) + '_' + match.captured(4)).toCaseFolded();
    }
}

QStringList codexPackageFolders::registeredFolders()
{
    const HRESULT initialized = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) {
        return { };
    }
    const auto apartment = qScopeGuard([initialized] {
        if (SUCCEEDED(initialized)) {
            RoUninitialize();
        }
    });
    QStringList folders;
    try {
        const winrt::Windows::Management::Deployment::PackageManager manager;
        for (const auto &package : manager.FindPackagesForUser(L"")) {
            const auto id = package.Id();
            const QString name = QString::fromWCharArray(id.Name().c_str());
            if (name != "OpenAI.Codex" && !name.startsWith("OpenAI.CodexPrimaryRuntime.v")) {
                continue;
            }
            const auto path =
                    QDir::fromNativeSeparators(QString::fromWCharArray(package.InstalledLocation().Path().c_str()));
            if (QFileInfo(path).fileName().compare(QString::fromWCharArray(id.FullName().c_str()), Qt::CaseInsensitive)
                == 0) {
                folders.append(path + '/');
            }
        }
    } catch (const winrt::hresult_error &) {
        return { };
    }
    folders.sort(Qt::CaseInsensitive);
    return folders;
}

QVector<amnezia::InstalledAppInfo> codexPackageFolders::currentSelection(const QVector<amnezia::InstalledAppInfo> &saved,
                                                                         const std::function<QStringList()> &lookup)
{
    QVector<amnezia::InstalledAppInfo> result;
    std::optional<QStringList> registered;
    QSet<QString> included;
    for (const auto &entry : saved) {
        const QString path = appSelection::localPath(entry.appPath);
        const QString key = familyKey(path);
        if (key.isEmpty()) {
            result.append(entry);
            continue;
        }
        if (!registered) {
            registered = lookup();
        }
        QStringList versions;
        for (const auto &folder : *registered) {
            if (familyKey(folder) == key) {
                versions.append(folder);
            }
        }
        // An earlier version can still have running processes while Windows stages the update.
        if (versions.isEmpty() || QFileInfo(path).isDir()) {
            versions.append(path);
        }
        for (const auto &folder : versions) {
            const QString normalized = folder.toCaseFolded();
            if (!included.contains(normalized)) {
                auto app = entry;
                app.appPath = folder;
                app.appName = QDir(folder).dirName();
                result.append(app);
                included.insert(normalized);
            }
        }
    }
    return result;
}
