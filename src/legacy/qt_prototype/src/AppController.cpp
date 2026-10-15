#include "AppController.hpp"

#include "launcher/EmbeddedClient.hpp"
#include "launcher/Injector.hpp"
#include "launcher/GameLauncher.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonValue>
#include <QProcess>
#include <QSettings>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace
{
QVariantMap setting(QString id, QString label, QString type, QVariant value,
    double minimum = 0.0, double maximum = 1.0, QString unit = {}, QVariantList options = {})
{
    return { { "id", std::move(id) }, { "label", std::move(label) }, { "type", std::move(type) },
        { "value", std::move(value) }, { "minimum", minimum }, { "maximum", maximum },
        { "unit", std::move(unit) }, { "options", std::move(options) } };
}

QVariantMap module(QString id, QString name, QString description, QString category, bool enabled,
    QVariantList settings = {}, QString keybind = QStringLiteral("NONE"))
{
    return { { "id", std::move(id) }, { "name", std::move(name) },
        { "description", std::move(description) }, { "category", std::move(category) },
        { "enabled", enabled }, { "keybind", std::move(keybind) }, { "settings", std::move(settings) } };
}
}

AppController::AppController(bool mockMode, QObject* parent)
    : QObject(parent), mockMode_(mockMode), bridge_(this), discovery_(this)
{
    for (const QString& argument : QCoreApplication::arguments())
        if (argument.startsWith(QStringLiteral("--state="))) initialState_ = argument.mid(8).trimmed().toLower();
    buildMockData();
    targetTimer_.setInterval(120);
    connect(&targetTimer_, &QTimer::timeout, this, &AppController::updateTargetRect);
    toastTimer_.setSingleShot(true);
    toastTimer_.setInterval(2600);
    connect(&toastTimer_, &QTimer::timeout, this, [this]
    {
        toastText_.clear();
        emit toastChanged();
    });
    connect(&bridge_, &thedorius::bridge::BridgeClient::connectedChanged,
        this, [this]
        {
            emit connectedChanged();
            if (bridge_.connected()) bridge_.send(QStringLiteral("menu_visibility"), { { "visible", menuVisible_ } });
        });
    connect(&bridge_, &thedorius::bridge::BridgeClient::messageReceived,
        this, &AppController::handleBridgeMessage);
    connect(&bridge_, &thedorius::bridge::BridgeClient::transportError, this,
        [this](const QString& error) { if (!mockMode_) showToast(error, false); });
}

QVariantList AppController::categories() const
{
    return {
        QVariantMap{ { "id", "Combat" }, { "label", "Combat" }, { "icon", "⚔" } },
        QVariantMap{ { "id", "Movement" }, { "label", "Movement" }, { "icon", "➤" } },
        QVariantMap{ { "id", "Player" }, { "label", "Player" }, { "icon", "♙" } },
        QVariantMap{ { "id", "Visuals" }, { "label", "Visuals" }, { "icon", "◉" } },
        QVariantMap{ { "id", "Client" }, { "label", "Client" }, { "icon", "⚙" } }
    };
}

QVariantList AppController::launchers() const { return thedorius::launcher::GameLauncher::installedLaunchers(); }

QVariantMap AppController::selectedModule() const
{
    const int index = moduleIndex(selectedModuleId_);
    return index >= 0 ? modules_.at(index).toMap() : QVariantMap{};
}

QVariantList AppController::visibleModules() const
{
    QVariantList result;
    for (const QVariant& value : modules_)
    {
        const QVariantMap item = value.toMap();
        if (item.value("category").toString() != activeCategory_) continue;
        if (!searchText_.isEmpty())
        {
            const QString haystack = item.value("name").toString() + ' ' + item.value("description").toString();
            if (!haystack.contains(searchText_, Qt::CaseInsensitive)) continue;
        }
        result.push_back(item);
    }
    return result;
}

QVariantList AppController::enabledModuleNames() const
{
    QVariantList result;
    for (const QVariant& value : modules_)
    {
        const QVariantMap item = value.toMap();
        if (item.value("enabled").toBool() && item.value("id") != "watermark" && item.value("id") != "arraylist")
            result.push_back(item.value("name"));
    }
    return result;
}

void AppController::setMenuVisible(bool value)
{
    if (menuVisible_ == value) return;
    menuVisible_ = value;
    emit menuVisibleChanged();
#ifdef Q_OS_WIN
    if (!value && targetPid_)
    {
        const HWND target = reinterpret_cast<HWND>(thedorius::launcher::ProcessDiscovery::mainWindowForProcess(targetPid_));
        if (target) SetForegroundWindow(target);
    }
#endif
    if (!mockMode_) bridge_.send(QStringLiteral("menu_visibility"), { { "visible", value } });
}

void AppController::setSearchText(const QString& value)
{
    if (searchText_ == value) return;
    searchText_ = value;
    emit searchTextChanged();
    emit modulesChanged();
}

void AppController::setCategory(const QString& category)
{
    if (activeCategory_ == category && activePage_ == "modules") return;
    activeCategory_ = category;
    activePage_ = QStringLiteral("modules");
    selectedModuleId_.clear();
    emit activeCategoryChanged();
    emit activePageChanged();
    emit selectedModuleChanged();
    emit modulesChanged();
}

void AppController::setPage(const QString& page)
{
    if (activePage_ == page) return;
    activePage_ = page;
    selectedModuleId_.clear();
    emit activePageChanged();
    emit selectedModuleChanged();
}

void AppController::selectModule(const QString& id)
{
    if (selectedModuleId_ == id) return;
    selectedModuleId_ = id;
    emit selectedModuleChanged();
}

void AppController::closeSettings()
{
    if (selectedModuleId_.isEmpty()) return;
    selectedModuleId_.clear();
    emit selectedModuleChanged();
}

void AppController::setModuleEnabled(const QString& id, bool enabled)
{
    const int index = moduleIndex(id);
    if (index < 0) return;
    QVariantMap item = modules_[index].toMap();
    item["enabled"] = enabled;
    modules_[index] = item;
    emit modulesChanged();
    emit selectedModuleChanged();
    if (!mockMode_) bridge_.send(QStringLiteral("set_module_enabled"), { { "id", id }, { "enabled", enabled } });
}

void AppController::setSetting(const QString& moduleId, const QString& settingId, const QVariant& value)
{
    const int index = moduleIndex(moduleId);
    if (index < 0) return;
    QVariantMap item = modules_[index].toMap();
    QVariantList settings = item.value("settings").toList();
    for (int settingIndex = 0; settingIndex < settings.size(); ++settingIndex)
    {
        QVariantMap current = settings[settingIndex].toMap();
        if (current.value("id").toString() != settingId) continue;
        current["value"] = value;
        settings[settingIndex] = current;
        break;
    }
    item["settings"] = settings;
    modules_[index] = item;
    emit modulesChanged();
    emit selectedModuleChanged();
    if (!mockMode_) bridge_.send(QStringLiteral("set_setting"),
        { { "moduleId", moduleId }, { "settingId", settingId }, { "value", QJsonValue::fromVariant(value) } });
}

void AppController::beginKeybind(const QString& moduleId)
{
    if (!mockMode_) bridge_.send(QStringLiteral("begin_keybind"), { { "id", moduleId } });
    else showToast(QStringLiteral("Press a key - Esc to cancel"));
}

void AppController::beginSettingKey(const QString& moduleId, const QString& settingId)
{
    if (!mockMode_) bridge_.send(settingId == QStringLiteral("menu_key") ? QStringLiteral("begin_menu_key") : QStringLiteral("begin_keybind"),
        { { "id", moduleId }, { "settingId", settingId } });
    else showToast(QStringLiteral("Press a key - Esc to cancel"));
}

void AppController::scanProcesses()
{
    processes_ = discovery_.minecraftProcesses();
    emit processesChanged();
}

void AppController::attachToProcess(quint32 processId)
{
    QString error;
    if (!sessionBootstrap_.prepare(processId, error)) { showToast(error, false); return; }
    const QString dllPath = thedorius::launcher::EmbeddedClient::extract(error);
    if (dllPath.isEmpty()) { sessionBootstrap_.clear(); showToast(error, false); return; }
    const auto result = thedorius::launcher::injectLibrary(processId, dllPath);
    showToast(result.message, result.success);
    if (!result.success) { sessionBootstrap_.clear(); return; }
    targetPid_ = processId;
    targetTimer_.start();
    bridge_.configure(sessionBootstrap_.pipeName(), sessionBootstrap_.nonce());
    bridge_.connectNow();
}

bool AppController::launchProgram(const QString& executable, const QStringList& arguments)
{
    const bool started = QProcess::startDetached(executable, arguments);
    showToast(started ? QStringLiteral("Launcher opened - waiting for Fabric 1.21.1")
                      : QStringLiteral("Launcher could not be opened"), started);
    return started;
}

bool AppController::launchGame(const QString& launcherId, const QString& profile)
{
    QString error;
    const bool started = thedorius::launcher::GameLauncher::launch(launcherId, profile, error);
    showToast(started ? QStringLiteral("Launcher opened - waiting for Fabric 1.21.1") : error, started);
    return started;
}

void AppController::refreshConfigs()
{
    if (!mockMode_) bridge_.send(QStringLiteral("config_refresh"));
    showToast(QStringLiteral("Config library refreshed"));
}

void AppController::loadConfig(int id)
{
    if (!mockMode_) bridge_.send(QStringLiteral("config_load"), { { "id", id } });
    showToast(QStringLiteral("Config loaded"));
}

void AppController::deleteConfig(int id)
{
    if (!mockMode_) bridge_.send(QStringLiteral("config_delete"), { { "id", id } });
    for (qsizetype index = 0; index < configs_.size(); ++index)
        if (configs_[index].toMap().value("id").toInt() == id) { configs_.removeAt(index); break; }
    emit configsChanged();
    showToast(QStringLiteral("Config deleted"));
}

void AppController::publishConfig(int updateId, const QString& name, const QString& description, const QString& author)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty()) { showToast(QStringLiteral("Config name is required"), false); return; }
    if (!mockMode_)
    {
        bridge_.send(QStringLiteral("config_publish"), {
            { "id", updateId }, { "name", clean }, { "description", description.trimmed() }, { "author", author.trimmed() }
        });
    }
    else if (updateId > 0)
    {
        for (qsizetype index = 0; index < configs_.size(); ++index)
        {
            QVariantMap item = configs_[index].toMap(); if (item.value("id").toInt() != updateId) continue;
            item["name"] = clean; item["description"] = description.trimmed(); configs_[index] = item; break;
        }
    }
    else
    {
        configs_.push_front(QVariantMap{ { "id", 100 + configs_.size() }, { "name", clean },
            { "author", author.trimmed().isEmpty() ? QStringLiteral("Thessaryn") : author.trimmed() },
            { "description", description.trimmed() }, { "modules", enabledModuleNames().size() },
            { "downloads", 0 }, { "mine", true }, { "favourite", false }, { "updated", "Now" } });
    }
    emit configsChanged(); showToast(updateId > 0 ? QStringLiteral("Config updated") : QStringLiteral("Config published"));
}

void AppController::toggleFavourite(int id)
{
    QSettings settings;
    QStringList favourites = settings.value(QStringLiteral("configs/favourites")).toStringList();
    const QString key = QString::number(id);
    favourites.contains(key) ? favourites.removeAll(key) : favourites.push_back(key);
    settings.setValue(QStringLiteral("configs/favourites"), favourites);
    for (qsizetype index = 0; index < configs_.size(); ++index)
    {
        QVariantMap item = configs_[index].toMap();
        if (item.value("id").toInt() != id) continue;
        item["favourite"] = favourites.contains(key);
        configs_[index] = item;
        break;
    }
    emit configsChanged();
}

void AppController::addFriend(const QString& name)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty()) return;
    for (const QVariant& item : friends_) if (item.toMap().value("name").toString().compare(clean, Qt::CaseInsensitive) == 0) return;
    friends_.push_back(QVariantMap{ { "name", clean }, { "online", true } });
    emit friendsChanged();
    if (!mockMode_) bridge_.send(QStringLiteral("friends_action"), { { "action", "add" }, { "name", clean } });
}

void AppController::removeFriend(const QString& name)
{
    for (qsizetype index = 0; index < friends_.size(); ++index)
        if (friends_[index].toMap().value("name").toString() == name) { friends_.removeAt(index); break; }
    emit friendsChanged();
    if (!mockMode_) bridge_.send(QStringLiteral("friends_action"), { { "action", "remove" }, { "name", name } });
}

void AppController::showToast(const QString& text, bool success)
{
    toastText_ = text;
    toastSuccess_ = success;
    toastTimer_.start();
    emit toastChanged();
}

void AppController::handleBridgeMessage(const thedorius::bridge::Message& message)
{
    if (message.type == "module_snapshot")
    {
        modules_ = message.payload.value("modules").toArray().toVariantList();
        emit modulesChanged(); emit selectedModuleChanged();
    }
    else if (message.type == "configs_snapshot")
    {
        configs_ = message.payload.value("configs").toArray().toVariantList(); emit configsChanged();
    }
    else if (message.type == "friends_snapshot")
    {
        serverPlayers_ = message.payload.value("serverPlayers").toArray().toVariantList();
        friends_ = message.payload.value("friends").toArray().toVariantList(); emit friendsChanged();
    }
    else if (message.type == "hud_snapshot")
    {
        playerName_ = message.payload.value("playerName").toString(QStringLiteral("Player"));
        ping_ = message.payload.value("ping").toInt(); emit hudChanged();
    }
    else if (message.type == "menu_visibility") setMenuVisible(message.payload.value("visible").toBool());
    else if (message.type == "toast") showToast(message.payload.value("text").toString(), message.payload.value("success").toBool(true));
}

void AppController::updateTargetRect()
{
#ifdef Q_OS_WIN
    const HWND window = reinterpret_cast<HWND>(thedorius::launcher::ProcessDiscovery::mainWindowForProcess(targetPid_));
    const bool visible = window && IsWindowVisible(window) && !IsIconic(window);
    if (targetVisible_ != visible) { targetVisible_ = visible; emit targetRectChanged(); }
    if (!visible) return;
    RECT client{};
    if (!GetClientRect(window, &client)) return;
    POINT origin{};
    ClientToScreen(window, &origin);
    const QRect next(origin.x, origin.y, client.right - client.left, client.bottom - client.top);
    if (next == targetRect_) return;
    targetRect_ = next;
    emit targetRectChanged();
#endif
}

int AppController::moduleIndex(const QString& id) const
{
    for (qsizetype index = 0; index < modules_.size(); ++index)
        if (modules_[index].toMap().value("id").toString() == id) return static_cast<int>(index);
    return -1;
}

void AppController::buildMockData()
{
    const QVariantList targetOptions{ "Players", "Players + Mobs", "All Living" };
    const QVariantList aimOptions{ "Body", "Random", "Adaptive" };
    modules_ = {
        module("triggerbot", "Triggerbot", "Automatically attacks the entity under your crosshair", "Combat", true,
            { setting("cooldown", "Cooldown", "range", QVariantList{ 88, 97 }, 1, 100, "%"),
              setting("reaction", "Reaction", "range", QVariantList{ 18, 42 }, 0, 180, "ms"),
              setting("targets", "Targets", "enum", 0, 0, 2, {}, targetOptions),
              setting("prioritize_crits", "Prioritize crits", "bool", true),
              setting("on_click", "On click", "bool", false) }, "R"),
        module("aim_assist", "Aim Assist", "Smoothly follows valid targets while attacking", "Combat", true,
            { setting("fov", "FOV", "float", 72.0, 10, 180, "deg"),
              setting("distance", "Max distance", "float", 4.5, 1, 8, "blocks"),
              setting("strength", "Strength", "float", 0.34, 0, 1),
              setting("aim_point", "Aim point", "enum", 2, 0, 2, {}, aimOptions),
              setting("weapon", "Weapon only", "bool", true) }),
        module("shield_breaker", "Shield Breaker", "Switches to an axe when a target raises a shield", "Combat", false,
            { setting("range", "Range", "float", 4.2, 2, 6, "blocks"), setting("return", "Return weapon", "bool", true) }),
        module("velocity", "Velocity", "Controls horizontal and vertical knockback response", "Combat", false,
            { setting("horizontal", "Horizontal", "float", 0.65, 0, 1), setting("vertical", "Vertical", "float", 0.85, 0, 1) }),
        module("sprint", "Sprint", "Keeps sprint active while normal movement is possible", "Movement", true),
        module("bridge_assist", "Bridge Assist", "Sneaks near block edges while bridging", "Movement", false,
            { setting("offset", "Edge offset", "float", 0.14, 0, 0.27, "blocks"), setting("blocks", "Require blocks", "bool", true) }),
        module("scaffold", "Scaffold", "Places bridge blocks using configurable timing", "Movement", false),
        module("auto_tool", "Auto Tool", "Selects the best hotbar tool for the targeted block", "Player", true),
        module("auto_armor", "Auto Armor", "Equips stronger armor from an open inventory", "Player", false),
        module("chest_stealer", "Chest Stealer", "Moves useful chest contents into your inventory", "Player", false),
        module("player_esp", "Player ESP", "Highlights nearby players with configurable boxes", "Visuals", true,
            { setting("style", "Style", "enum", 0, 0, 2, {}, QVariantList{ "2D Box", "Corners", "3D Box" }),
              setting("thickness", "Thickness", "float", 1.5, 0.5, 5),
              setting("color", "ESP color", "color", QStringLiteral("#8F62FF")),
              setting("health", "Show health", "bool", true) }),
        module("storage_esp", "Storage ESP", "Highlights nearby storage blocks", "Visuals", true),
        module("block_esp", "Block ESP", "Highlights only selected block types", "Visuals", false),
        module("full_bright", "Full Bright", "Brightens dark areas without changing world time", "Visuals", true),
        module("nametag", "Nametag", "Replaces player labels with detailed information", "Visuals", true),
        module("click_gui", "Click GUI", "Controls language and the menu key", "Client", true,
            { setting("language", "Language", "enum", 0, 0, 1, {}, QVariantList{ "English", "Türkçe" }),
              setting("menu_key", "Menu key", "action", QStringLiteral("INSERT")) }),
        module("panic", "Panic", "Safely unloads Thedorius from the game", "Client", false),
        module("watermark", "Watermark", "Shows the client, player and latency status", "Hud", true),
        module("arraylist", "ArrayList", "Shows enabled modules as a connected staircase", "Hud", true)
    };
    configs_ = {
        QVariantMap{ { "id", 11 }, { "name", "Sword 1.9+" }, { "author", "Thessaryn" }, { "modules", 12 }, { "downloads", 248 }, { "mine", true }, { "favourite", true }, { "updated", "Today" } },
        QVariantMap{ { "id", 12 }, { "name", "Legit Practice" }, { "author", "Saburu" }, { "modules", 7 }, { "downloads", 93 }, { "mine", false }, { "favourite", false }, { "updated", "Yesterday" } },
        QVariantMap{ { "id", 13 }, { "name", "Crystal Session" }, { "author", "Krisis" }, { "modules", 9 }, { "downloads", 451 }, { "mine", false }, { "favourite", true }, { "updated", "2 days ago" } }
    };
    serverPlayers_ = {
        QVariantMap{ { "name", "Thessaryn" }, { "local", true }, { "friend", false } },
        QVariantMap{ { "name", "Saburu" }, { "local", false }, { "friend", true } },
        QVariantMap{ { "name", "Krisis" }, { "local", false }, { "friend", false } }
    };
    friends_ = { QVariantMap{ { "name", "Saburu" }, { "online", true } } };
}
