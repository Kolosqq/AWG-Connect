#pragma once

#include <QStringList>
#include <QVector>
#include <functional>

#include "core/utils/commonStructs.h"

namespace codexPackageFolders
{
    QStringList registeredFolders();
    QVector<amnezia::InstalledAppInfo> currentSelection(const QVector<amnezia::InstalledAppInfo> &saved,
                                                        const std::function<QStringList()> &lookup = registeredFolders);
}
