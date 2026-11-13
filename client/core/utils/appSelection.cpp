#include "appSelection.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QObject>
#include <QSet>
#include <QUrl>

QString appSelection::localPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    return QDir::fromNativeSeparators(trimmed.startsWith("file:", Qt::CaseInsensitive) ? QUrl(trimmed).toLocalFile()
                                                                                       : trimmed);
}

appSelection::ResolvedApps appSelection::resolve(const QStringList &paths)
{
    ResolvedApps result;
    QSet<QString> seen;
    auto addExecutable = [&](const QFileInfo &info) {
        const QString path = QDir::fromNativeSeparators(info.absoluteFilePath());
        if (info.suffix().compare("exe", Qt::CaseInsensitive) == 0 && !seen.contains(path.toCaseFolded())) {
            seen.insert(path.toCaseFolded());
            result.executables.append(path);
        }
    };
    for (const QString &entry : paths) {
        const QString path = localPath(entry);
        const QFileInfo info(path);
        if (info.isAbsolute() && !info.exists()) {
            if (path.endsWith('/')) {
                result.directories.append(path);
            }
            continue;
        }
        if (info.isDir()) {
            result.directories.append(QDir::fromNativeSeparators(info.absoluteFilePath()));
            QDirIterator files(info.absoluteFilePath(), QDir::Files | QDir::NoSymLinks | QDir::Hidden | QDir::System,
                               QDirIterator::Subdirectories);
            while (files.hasNext()) {
                files.next();
                addExecutable(files.fileInfo());
            }
        } else if (info.isFile() && info.suffix().compare("exe", Qt::CaseInsensitive) == 0) {
            addExecutable(info);
        } else {
            result.error = QObject::tr("Application or folder is unavailable: %1").arg(path);
            result.executables.clear();
            return result;
        }
    }
    result.executables.sort(Qt::CaseInsensitive);
    result.directories.removeDuplicates();
    return result;
}
