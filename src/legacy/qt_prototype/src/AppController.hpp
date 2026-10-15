#pragma once

#include "bridge/BridgeClient.hpp"
#include "launcher/ProcessDiscovery.hpp"
#include "launcher/SessionBootstrap.hpp"

#include <QObject>
#include <QRect>
#include <QTimer>
#include <QVariantList>

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool mockMode READ mockMode CONSTANT)
    Q_PROPERTY(QString initialState READ initialState CONSTANT)
    Q_PROPERTY(bool menuVisible READ menuVisible WRITE setMenuVisible NOTIFY menuVisibleChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString activeCategory READ activeCategory NOTIFY activeCategoryChanged)
    Q_PROPERTY(QString activePage READ activePage NOTIFY activePageChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString selectedModuleId READ selectedModuleId NOTIFY selectedModuleChanged)
    Q_PROPERTY(QVariantMap selectedModule READ selectedModule NOTIFY selectedModuleChanged)
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)
    Q_PROPERTY(QVariantList allModules READ allModules NOTIFY modulesChanged)
    Q_PROPERTY(QVariantList visibleModules READ visibleModules NOTIFY modulesChanged)
    Q_PROPERTY(QVariantList configs READ configs NOTIFY configsChanged)
    Q_PROPERTY(QVariantList serverPlayers READ serverPlayers NOTIFY friendsChanged)
    Q_PROPERTY(QVariantList friends READ friends NOTIFY friendsChanged)
    Q_PROPERTY(QVariantList enabledModuleNames READ enabledModuleNames NOTIFY modulesChanged)
    Q_PROPERTY(QVariantList processes READ processes NOTIFY processesChanged)
    Q_PROPERTY(QVariantList launchers READ launchers CONSTANT)
    Q_PROPERTY(QRect targetRect READ targetRect NOTIFY targetRectChanged)
    Q_PROPERTY(bool targetVisible READ targetVisible NOTIFY targetRectChanged)
    Q_PROPERTY(QString toastText READ toastText NOTIFY toastChanged)
    Q_PROPERTY(bool toastSuccess READ toastSuccess NOTIFY toastChanged)
    Q_PROPERTY(QString playerName READ playerName NOTIFY hudChanged)
    Q_PROPERTY(int ping READ ping NOTIFY hudChanged)

public:
    explicit AppController(bool mockMode, QObject* parent = nullptr);

    bool mockMode() const { return mockMode_; }
    QString initialState() const { return initialState_; }
    bool menuVisible() const { return menuVisible_; }
    bool connected() const { return mockMode_ || bridge_.connected(); }
    QString activeCategory() const { return activeCategory_; }
    QString activePage() const { return activePage_; }
    QString searchText() const { return searchText_; }
    QString selectedModuleId() const { return selectedModuleId_; }
    QVariantMap selectedModule() const;
    QVariantList categories() const;
    QVariantList allModules() const { return modules_; }
    QVariantList visibleModules() const;
    QVariantList configs() const { return configs_; }
    QVariantList serverPlayers() const { return serverPlayers_; }
    QVariantList friends() const { return friends_; }
    QVariantList enabledModuleNames() const;
    QVariantList processes() const { return processes_; }
    QVariantList launchers() const;
    QRect targetRect() const { return targetRect_; }
    bool targetVisible() const { return targetVisible_; }
    QString toastText() const { return toastText_; }
    bool toastSuccess() const { return toastSuccess_; }
    QString playerName() const { return playerName_; }
    int ping() const { return ping_; }

    void setMenuVisible(bool value);
    void setSearchText(const QString& value);

    Q_INVOKABLE void setCategory(const QString& category);
    Q_INVOKABLE void setPage(const QString& page);
    Q_INVOKABLE void selectModule(const QString& id);
    Q_INVOKABLE void closeSettings();
    Q_INVOKABLE void setModuleEnabled(const QString& id, bool enabled);
    Q_INVOKABLE void setSetting(const QString& moduleId, const QString& settingId, const QVariant& value);
    Q_INVOKABLE void beginKeybind(const QString& moduleId);
    Q_INVOKABLE void beginSettingKey(const QString& moduleId, const QString& settingId);
    Q_INVOKABLE void scanProcesses();
    Q_INVOKABLE void attachToProcess(quint32 processId);
    Q_INVOKABLE bool launchProgram(const QString& executable, const QStringList& arguments = {});
    Q_INVOKABLE bool launchGame(const QString& launcherId, const QString& profile = {});
    Q_INVOKABLE void refreshConfigs();
    Q_INVOKABLE void loadConfig(int id);
    Q_INVOKABLE void deleteConfig(int id);
    Q_INVOKABLE void publishConfig(int updateId, const QString& name, const QString& description, const QString& author);
    Q_INVOKABLE void toggleFavourite(int id);
    Q_INVOKABLE void addFriend(const QString& name);
    Q_INVOKABLE void removeFriend(const QString& name);
    Q_INVOKABLE void showToast(const QString& text, bool success = true);

signals:
    void menuVisibleChanged();
    void connectedChanged();
    void activeCategoryChanged();
    void activePageChanged();
    void searchTextChanged();
    void selectedModuleChanged();
    void modulesChanged();
    void configsChanged();
    void friendsChanged();
    void processesChanged();
    void targetRectChanged();
    void toastChanged();
    void hudChanged();

private:
    void buildMockData();
    void handleBridgeMessage(const thedorius::bridge::Message& message);
    void updateTargetRect();
    int moduleIndex(const QString& id) const;

    bool mockMode_ = true;
    QString initialState_ = QStringLiteral("modules");
    bool menuVisible_ = true;
    QString activeCategory_ = QStringLiteral("Combat");
    QString activePage_ = QStringLiteral("modules");
    QString searchText_;
    QString selectedModuleId_;
    QVariantList modules_;
    QVariantList configs_;
    QVariantList serverPlayers_;
    QVariantList friends_;
    QVariantList processes_;
    QRect targetRect_{ 80, 60, 1280, 720 };
    quint32 targetPid_ = 0;
    bool targetVisible_ = true;
    QString toastText_;
    bool toastSuccess_ = true;
    QString playerName_ = QStringLiteral("Thessaryn");
    int ping_ = 84;
    QTimer targetTimer_;
    QTimer toastTimer_;
    thedorius::bridge::BridgeClient bridge_;
    thedorius::launcher::ProcessDiscovery discovery_;
    thedorius::launcher::SessionBootstrap sessionBootstrap_;
};
