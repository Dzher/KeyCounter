// keyCounter - 托盘图标与菜单
#pragma once

#include <QObject>

class QAction;
#include <QString>

class QActionGroup;
class QMenu;
class QSystemTrayIcon;
class QTimer;

namespace kc
{

class CounterEngine;
class Settings;
class StatsQuery;

class TrayController : public QObject
{
    Q_OBJECT
public:
    TrayController(Settings& settings, CounterEngine& engine, StatsQuery& query, QObject* parent = nullptr);
    ~TrayController() override;

    bool available() const;
    void setPaused(bool paused);
    void setHotkeyText(const QString& text);
    void setHotkeyRegistered(bool registered);
    void showMessage(const QString& title, const QString& body, bool warning = false);

public slots:
    /// 重绘图标 + 更新 tooltip（每分钟调用一次）
    void refresh();

signals:
    void showPanelRequested();
    /// 0 = 恢复，60 = 暂停一小时，-1 = 暂停到明天，-2 = 暂停直到手动恢复
    void pauseRequested(int minutes);
    void layoutChangeRequested(const QString& id);
    void settingsRequested();
    void quitRequested();

private:
    void rebuildMenu();
    void updateIcon();
    double activityRatio() const;

    Settings& m_settings;
    CounterEngine& m_engine;
    StatsQuery& m_query;

    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_menu = nullptr;
    QMenu* m_pauseMenu = nullptr;
    QMenu* m_layoutMenu = nullptr;
    QActionGroup* m_pauseGroup = nullptr;
    QActionGroup* m_layoutGroup = nullptr;
    QAction* m_infoAction = nullptr;
    QAction* m_autostartAction = nullptr;
    QAction* m_hotkeyAction = nullptr;
    QTimer* m_iconTimer = nullptr;
    QString m_hotkeyText;
    bool m_paused = false;
    bool m_hotkeyRegistered = false;
    bool m_autostartHandled = false;
};

}  // namespace kc
