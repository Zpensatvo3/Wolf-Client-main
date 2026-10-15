#include "GameLauncher.hpp"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QDesktopServices>

namespace
{
    struct candidate { const char* id; const char* name; QString path; bool profiles; };
    QList<candidate> candidates()
    {
        const QString appData = qEnvironmentVariable("APPDATA");
        const QString local = qEnvironmentVariable("LOCALAPPDATA");
        const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)");
        return {
            { "prism", "Prism Launcher", QDir(appData).filePath("PrismLauncher/PrismLauncher.exe"), true },
            { "prism", "Prism Launcher", QDir(local).filePath("Programs/PrismLauncher/PrismLauncher.exe"), true },
            { "multimc", "MultiMC", QDir(appData).filePath("MultiMC/MultiMC.exe"), true },
            { "official", "Minecraft Launcher", QDir(programFilesX86).filePath("Minecraft Launcher/MinecraftLauncher.exe"), false }
        };
    }
}

QVariantList thedorius::launcher::GameLauncher::installedLaunchers()
{
    QVariantList result; QSet<QString> seen;
    for (const candidate& entry : candidates())
    {
        if (!QFileInfo::exists(entry.path) || seen.contains(entry.id)) continue;
        seen.insert(entry.id);
        result.push_back(QVariantMap{ { "id", entry.id }, { "name", entry.name }, { "path", entry.path }, { "profiles", entry.profiles } });
    }
    if (!seen.contains(QStringLiteral("official")))
        result.push_back(QVariantMap{ { "id", "official" }, { "name", "Minecraft Launcher" }, { "path", "minecraft://" }, { "profiles", false } });
    return result;
}

bool thedorius::launcher::GameLauncher::launch(const QString& id, const QString& profile, QString& error)
{
    for (const candidate& entry : candidates())
    {
        if (id != entry.id || !QFileInfo::exists(entry.path)) continue;
        QStringList arguments;
        if (entry.profiles && !profile.trimmed().isEmpty()) arguments << QStringLiteral("--launch") << profile.trimmed();
        const bool started = QProcess::startDetached(entry.path, arguments);
        if (!started) error = QStringLiteral("%1 could not be started").arg(entry.name);
        return started;
    }
    if (id == QStringLiteral("official"))
    {
        const bool started = QDesktopServices::openUrl(QUrl(QStringLiteral("minecraft://")));
        if (!started) error = QStringLiteral("Minecraft Launcher could not be opened");
        return started;
    }
    error = QStringLiteral("Launcher is not installed"); return false;
}
