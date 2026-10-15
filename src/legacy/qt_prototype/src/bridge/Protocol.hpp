#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace thedorius::bridge
{
inline constexpr int protocol_version = 1;

struct Message
{
    QString type;
    QString requestId;
    QString sessionNonce;
    QJsonObject payload;
};

QByteArray encode(const Message& message);
bool tryDecode(QByteArray& buffer, Message& message, QString& error);
}
