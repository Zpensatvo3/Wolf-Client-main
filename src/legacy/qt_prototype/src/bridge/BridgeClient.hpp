#pragma once

#include "Protocol.hpp"

#include <QLocalSocket>
#include <QObject>
#include <QTimer>

namespace thedorius::bridge
{
class BridgeClient final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
public:
    explicit BridgeClient(QObject* parent = nullptr);

    bool connected() const;
    void configure(QString serverName, QString nonce);
    Q_INVOKABLE void connectNow();
    Q_INVOKABLE void disconnectNow();
    void send(const QString& type, const QJsonObject& payload = {}, const QString& requestId = {});

signals:
    void connectedChanged();
    void messageReceived(const thedorius::bridge::Message& message);
    void transportError(const QString& message);

private:
    void consume();

    QLocalSocket socket_;
    QTimer reconnectTimer_;
    QByteArray receiveBuffer_;
    QString serverName_;
    QString nonce_;
};
}

Q_DECLARE_METATYPE(thedorius::bridge::Message)
