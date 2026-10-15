#pragma once

#include <QObject>
#include <QVariantList>

namespace thedorius::launcher
{
class ProcessDiscovery final : public QObject
{
    Q_OBJECT
public:
    explicit ProcessDiscovery(QObject* parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE QVariantList minecraftProcesses() const;
    static quintptr mainWindowForProcess(quint32 processId);
};
}
