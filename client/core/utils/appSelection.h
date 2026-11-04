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

    ResolvedApps resolve(const QStringList &paths);
}

#endif
