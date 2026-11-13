#ifndef APPSELECTION_H
#define APPSELECTION_H

#include <QStringList>

namespace appSelection
{
    struct ResolvedApps
    {
        QStringList executables;
        QStringList directories;
        QString error;
    };

    QString localPath(const QString &path);
    ResolvedApps resolve(const QStringList &paths);
}

#endif
