#pragma once

#include <QString>

namespace thedorius::launcher
{
class EmbeddedClient
{
public:
    static QString extract(QString& error);
    static void cleanOldVersions();
};
}
