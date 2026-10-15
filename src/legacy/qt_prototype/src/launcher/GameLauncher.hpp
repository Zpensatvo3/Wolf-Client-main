#pragma once

#include <QString>
#include <QVariantList>

namespace thedorius::launcher
{
    class GameLauncher final
    {
    public:
        static QVariantList installedLaunchers();
        static bool launch(const QString& id, const QString& profile, QString& error);
    };
}
