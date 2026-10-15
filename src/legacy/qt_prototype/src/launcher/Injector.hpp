#pragma once

#include <QString>

namespace thedorius::launcher
{
struct InjectionResult
{
    bool success = false;
    QString message;
};

InjectionResult injectLibrary(quint32 processId, const QString& absoluteDllPath);
}
