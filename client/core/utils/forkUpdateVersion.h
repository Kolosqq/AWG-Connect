#pragma once

#include <QRegularExpression>
#include <QVersionNumber>

namespace forkUpdate
{
    inline QVersionNumber stableVersion(const QString &tag)
    {
        static const QRegularExpression format(QStringLiteral("\\Av?((?:0|[1-9][0-9]*)\\.(?:0|[1-9][0-9]*)\\.(?:0|[1-9]"
                                                              "[0-9]*))(?:\\+[0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)?\\z"));
        const auto match = format.match(tag);
        if (!match.hasMatch()) {
            return { };
        }
        const auto core = match.captured(1);
        const auto version = QVersionNumber::fromString(core);
        return version.toString() == core ? version : QVersionNumber();
    }
}
