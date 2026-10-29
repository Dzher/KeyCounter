// keyCounter - 应用装配：设置 / 采集 / 存储 / 托盘 / 面板 / 快捷键的生命周期
#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

class QThread;
class QTimer;

namespace kc
{

class CounterEngine;
class MainWindow;
class RawInputMonitor;
class Settings;
class SettingsDialog;
class StatsQuery;
class StatsStore;
class TrayController;

class Application : public QObject
{
    Q_OBJECT
public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    /// 装配整个应用；失败时 error 给出原因
    bool initialize(const QStringList& arguments, QString* error);
    /// 退出前收口：落盘 + 关库 + 注销快捷键
    void shutdown();

public slots:
    void showPanel();

private:
    void startStoreThread();
    void onStoreOpened(bool ok, const QString& error, int firstDataDay);
    void onBatchApplied(quint64 revision, int day);
    void onPauseRequested(int minutes);
    void applyHotkey();
    void openSettings();

    /// 切换数据目录：先把待落盘数据交给存储线程并关库，再搬移文件、切换设置、重新打开
    /// @param migrateExisting true = 把当前数据搬到新目录；false = 直接接管新目录已有的数据
    bool switchDataDirectory(const QString& dir, bool migrateExisting, QString* error);
    void reopenStore(const QString& dbPath);

    Settings* m_settings = nullptr;
    CounterEngine* m_engine = nullptr;
    RawInputMonitor* m_monitor = nullptr;
    StatsQuery* m_query = nullptr;
    StatsStore* m_store = nullptr;
    QThread* m_storeThread = nullptr;
    TrayController* m_tray = nullptr;
    MainWindow* m_window = nullptr;
    SettingsDialog* m_settingsDialog = nullptr;
    QTimer* m_pauseTimer = nullptr;

    QString m_dataDir;  ///< 生效的数据目录（可能是自定义位置）
    QString m_dbPath;   ///< 生效的数据库路径
    QDateTime m_pauseUntil;
    bool m_pausedIndefinitely = false;
    bool m_shuttingDown = false;
};

}  // namespace kc
