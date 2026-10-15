#include "BridgeClient.hpp"

#include <QUuid>

using namespace thedorius::bridge;

BridgeClient::BridgeClient(QObject* parent) : QObject(parent)
{
    reconnectTimer_.setInterval(750);
    reconnectTimer_.setSingleShot(true);
    connect(&reconnectTimer_, &QTimer::timeout, this, &BridgeClient::connectNow);
    connect(&socket_, &QLocalSocket::connected, this, [this]
    {
        emit connectedChanged();
        send(QStringLiteral("hello"), { { "ui", QStringLiteral("Thedorius") } });
    });
    connect(&socket_, &QLocalSocket::disconnected, this, [this]
    {
        emit connectedChanged();
        if (!serverName_.isEmpty()) reconnectTimer_.start();
    });
    connect(&socket_, &QLocalSocket::readyRead, this, &BridgeClient::consume);
    connect(&socket_, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError)
    {
        emit transportError(socket_.errorString());
        if (!serverName_.isEmpty() && socket_.state() == QLocalSocket::UnconnectedState)
            reconnectTimer_.start();
    });
}

bool BridgeClient::connected() const
{
    return socket_.state() == QLocalSocket::ConnectedState;
}

void BridgeClient::configure(QString serverName, QString nonce)
{
    serverName_ = std::move(serverName);
    nonce_ = std::move(nonce);
}

void BridgeClient::connectNow()
{
    if (serverName_.isEmpty() || socket_.state() != QLocalSocket::UnconnectedState)
        return;
    socket_.connectToServer(serverName_, QIODevice::ReadWrite);
}

void BridgeClient::disconnectNow()
{
    reconnectTimer_.stop();
    serverName_.clear();
    socket_.disconnectFromServer();
}

void BridgeClient::send(const QString& type, const QJsonObject& payload, const QString& requestId)
{
    if (!connected()) return;
    socket_.write(encode({ type,
        requestId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : requestId,
        nonce_, payload }));
}

void BridgeClient::consume()
{
    receiveBuffer_.append(socket_.readAll());
    while (true)
    {
        Message message;
        QString error;
        const qsizetype previousSize = receiveBuffer_.size();
        if (!tryDecode(receiveBuffer_, message, error))
        {
            if (!error.isEmpty()) emit transportError(error);
            if (receiveBuffer_.size() == previousSize) break;
            continue;
        }
        if (message.sessionNonce != nonce_)
        {
            emit transportError(QStringLiteral("Bridge session nonce mismatch"));
            socket_.disconnectFromServer();
            break;
        }
        emit messageReceived(message);
    }
}
