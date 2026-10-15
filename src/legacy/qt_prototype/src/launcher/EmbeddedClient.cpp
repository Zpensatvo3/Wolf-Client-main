#include "EmbeddedClient.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

QString thedorius::launcher::EmbeddedClient::extract(QString& error)
{
#ifndef THEDORIUS_HAS_EMBEDDED_CLIENT
    error = QStringLiteral("This build does not contain Thedorius-Lite.dll");
    return {};
#else
    QFile resource(QStringLiteral(":/embedded/Thedorius-Lite.dll"));
    if (!resource.open(QIODevice::ReadOnly))
    {
        error = resource.errorString();
        return {};
    }
    const QByteArray bytes = resource.readAll();
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/Runtime/0.3/") + hash;
    if (!QDir().mkpath(root))
    {
        error = QStringLiteral("Runtime directory could not be created");
        return {};
    }
    const QString path = root + QStringLiteral("/Thedorius-Lite.dll");
    if (QFileInfo(path).size() == bytes.size()) return path;
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit())
    {
        error = output.errorString();
        return {};
    }
    return path;
#endif
}

void thedorius::launcher::EmbeddedClient::cleanOldVersions()
{
    const QDir runtime(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/Runtime"));
    if (!runtime.exists()) return;
    // Loaded DLLs remain locked by Windows. Cleanup is deliberately best effort.
    for (const QFileInfo& entry : runtime.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (entry.fileName() != QStringLiteral("0.3")) QDir(entry.absoluteFilePath()).removeRecursively();
}
