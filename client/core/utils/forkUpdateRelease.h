#pragma once

#include "forkUpdateVersion.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

namespace forkUpdate
{
    inline QString windowsMsiName(const QString &version)
    {
        return QStringLiteral("AWG-Connect_%1_windows_x64.msi").arg(version);
    }

    inline qint64 windowsMsiSize(const QJsonArray &assets, const QString &version)
    {
        const auto name = windowsMsiName(version);
        const auto signatureName = name + QStringLiteral(".sig");
        qint64 size = 0;
        bool hasSignature = false;
        for (const auto &entry : assets) {
            const auto asset = entry.toObject();
            const auto assetName = asset.value("name").toString();
            const auto assetSize = asset.value("size").toInteger();
            if (assetName == name && assetSize > 0 && assetSize <= 512 * 1024 * 1024) {
                size = assetSize;
            }
            if (assetName == signatureName && assetSize == 384) {
                hasSignature = true;
            }
        }
        return hasSignature ? size : 0;
    }

    inline QMap<QVersionNumber, QJsonObject> previousWindowsReleases(const QJsonArray &releases,
                                                                     const QVersionNumber &installed)
    {
        QMap<QVersionNumber, QJsonObject> previous;
        for (const auto &item : releases) {
            const auto release = item.toObject();
            if (release.value("draft") != QJsonValue(false) || release.value("prerelease") != QJsonValue(false)) {
                continue;
            }
            const auto tag = release.value("tag_name").toString();
            const auto version = stableVersion(tag);
            if (!version.isNull() && version < installed && tag == QStringLiteral("v") + version.toString()
                && windowsMsiSize(release.value("assets").toArray(), version.toString()) > 0) {
                previous.insert(version, release);
            }
        }
        return previous;
    }
}
