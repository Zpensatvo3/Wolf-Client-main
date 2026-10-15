#include "Protocol.hpp"

#include <QDataStream>
#include <QJsonDocument>

QByteArray thedorius::bridge::encode(const Message& message)
{
    QJsonObject root{
        { "protocol", protocol_version },
        { "messageType", message.type },
        { "requestId", message.requestId },
        { "sessionNonce", message.sessionNonce },
        { "payload", message.payload }
    };
    const QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QByteArray framed;
    QDataStream stream(&framed, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint32(body.size());
    framed.append(body);
    return framed;
}

bool thedorius::bridge::tryDecode(QByteArray& buffer, Message& message, QString& error)
{
    constexpr qsizetype headerSize = sizeof(quint32);
    if (buffer.size() < headerSize)
        return false;

    QByteArray header = buffer.left(headerSize);
    QDataStream stream(&header, QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 bodySize = 0;
    stream >> bodySize;
    if (bodySize == 0 || bodySize > 1024U * 1024U)
    {
        error = QStringLiteral("Invalid bridge frame length");
        buffer.clear();
        return false;
    }
    if (buffer.size() < headerSize + bodySize)
        return false;

    const QByteArray body = buffer.mid(headerSize, bodySize);
    buffer.remove(0, headerSize + bodySize);
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        error = QStringLiteral("Invalid bridge JSON: %1").arg(parseError.errorString());
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("protocol")).toInt() != protocol_version)
    {
        error = QStringLiteral("Unsupported bridge protocol");
        return false;
    }
    message.type = root.value(QStringLiteral("messageType")).toString();
    message.requestId = root.value(QStringLiteral("requestId")).toString();
    message.sessionNonce = root.value(QStringLiteral("sessionNonce")).toString();
    message.payload = root.value(QStringLiteral("payload")).toObject();
    return true;
}
