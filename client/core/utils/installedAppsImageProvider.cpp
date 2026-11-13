#include "installedAppsImageProvider.h"

#ifdef Q_OS_WIN
    #include <QFileIconProvider>
    #include <QFileInfo>
    #include <QIcon>
#else
    #include "platforms/android/android_controller.h"
#endif

InstalledAppsImageProvider::InstalledAppsImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap InstalledAppsImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
#ifdef Q_OS_WIN
    const QString path = QString::fromUtf8(QByteArray::fromBase64(id.toLatin1(), QByteArray::Base64UrlEncoding));
    const QSize iconSize = requestedSize.isValid() ? requestedSize : QSize(32, 32);
    const QPixmap icon = QFileIconProvider().icon(QFileInfo(path)).pixmap(iconSize);
    if (size) {
        *size = icon.size();
    }
    return icon;
#else
    return AndroidController::instance()->getAppIcon(id, size, requestedSize);
#endif
}
