// keyCounter - 应用装配实现
#include "Application.h"

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QThread>
#include <QTimer>

#include "app/Settings.h"
#include "app/TrayController.h"
#include "core/CounterEngine.h"
#include "core/RawInputMonitor.h"
#include "core/StatsQuery.h"
#include "core/StatsStore.h"
#include "ui/MainWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

namespace kc
{

Application::Application(QObject* parent) : QObject(parent) {}

Application::~Application()
{
    shutdown();
}

bool Application::initialize(const QStringList& arguments, QString* error)
{
    qRegisterMetaType<kc::DayBatch>("kc::DayBatch");

    m_settings = new Settings(this);
    Theme::instance().setMode(static_cast<int>(m_settings->themeMode()));
    // 自启自愈：便携版移动目录后自动修正注册表里的路径
    if (m_settings->autostart())
    {
        QString ignored;
        m_settings->setAutostart(true, &ignored);
    }

    m_engine = new CounterEngine(this);
    m_query = new StatsQuery(this);
    m_pauseTimer = new QTimer(this);
    m_pauseTimer->setSingleShot(true);
    connect(m_pauseTimer, &QTimer::timeout, this, [this] { onPauseRequested(0); });

    m_monitor = new RawInputMonitor(this);
    connect(m_monitor, &RawInputMonitor::rawKey, m_engine, &CounterEngine::onRawKey);
    connect(m_monitor, &RawInputMonitor::systemSuspending, m_engine, &CounterEngine::flush);
    connect(m_monitor, &RawInputMonitor::sessionLocked, this,
            [this](bool locked)
            {
                if (locked || !locked)
                {
                    m_engine->flush();
                }
            });
    connect(m_monitor, &RawInputMonitor::hotkeyTriggered, this, &Application::showPanel);

    QString monitorError;
    const bool monitoring = m_monitor->start(&monitorError);

    m_tray = new TrayController(*m_settings, *m_engine, *m_query, this);
    connect(m_tray, &TrayController::showPanelRequested, this, &Application::showPanel);
    connect(m_tray, &TrayController::pauseRequested, this, &Application::onPauseRequested);
    connect(m_tray, &TrayController::settingsRequested, this, &Application::openSettings);
    connect(m_tray, &TrayController::quitRequested, qApp, &QApplication::quit);
    connect(m_tray, &TrayController::layoutChangeRequested, this,
            [this](const QString& id)
            {
                m_settings->setLayoutId(id);
                if (m_window != nullptr)
                {
                    m_window->applyPreferences();
                }
            });

    m_window = new MainWindow(*m_settings, *m_query, *m_engine);
    connect(m_window, &MainWindow::settingsRequested, this, &Application::openSettings);
    connect(m_window, &MainWindow::pauseToggleRequested, this,
            [this]
            {
                onPauseRequested(m_engine->isPaused() ? 0 : -2);
            });
    connect(m_window, &MainWindow::hiddenToTray, this,
            [this]
            {
                m_tray->showMessage(QStringLiteral("keyCounter 仍在后台统计"),
                                    QStringLiteral("数据每 15 秒自动落盘；右键托盘图标可再次打开或退出。"));
            });

    startStoreThread();
    applyHotkey();

    const bool silent = arguments.contains(QStringLiteral("--silent"), Qt::CaseInsensitive);
    if (!silent)
    {
        m_window->showPanel();
    }

    if (!m_settings->onboarded())
    {
        m_settings->setOnboarded(true);
        m_tray->showMessage(QStringLiteral("keyCounter 已在后台开始统计"),
                            QStringLiteral("只保存逐键的聚合计数，不含按键内容。点托盘图标查看热力图。"));
    }

    if (!monitoring)
    {
        m_tray->showMessage(QStringLiteral("键盘采集启动失败"),
                            monitorError + QStringLiteral("（界面仍可查看历史数据）"), true);
    }

    Q_UNUSED(error);
    return true;
}

void Application::startStoreThread()
{
    m_storeThread = new QThread(this);
    m_storeThread->setObjectName(QStringLiteral("keyCounterStore"));
    m_store = new StatsStore();
    m_store->moveToThread(m_storeThread);

    const QString dbPath = StatsStore::defaultDatabasePath();
    connect(m_storeThread, &QThread::started, m_store, [this, dbPath] { m_store->open(dbPath); });
    connect(m_engine, &CounterEngine::batchReady, m_store, &StatsStore::applyBatch);
    connect(m_store, &StatsStore::opened, this, &Application::onStoreOpened);
    connect(m_store, &StatsStore::batchingApplied, this, &Application::onBatchApplied);
    connect(m_store, &StatsStore::integrityWarning, this,
            [this](const QString& message)
            {
                m_tray->showMessage(QStringLiteral("数据文件校验失败"), message, true);
            });
    connect(m_store, &StatsStore::failed, this,
            [this](const QString& message)
            {
                m_tray->showMessage(QStringLiteral("统计写入失败"), message, true);
            });

    m_storeThread->start();
}

void Application::onStoreOpened(bool ok, const QString& error, int firstDataDay)
{
    if (!ok)
    {
        QMessageBox::critical(nullptr, QStringLiteral("keyCounter 无法启动"), error);
        QCoreApplication::exit(1);
        return;
    }

    QString queryError;
    if (!m_query->open(StatsStore::defaultDatabasePath(), &queryError))
    {
        m_tray->showMessage(QStringLiteral("读取统计数据失败"), queryError, true);
        return;
    }

    m_engine->setFirstDataDay(firstDataDay);
    if (m_window != nullptr)
    {
        m_window->refresh();
    }
}

void Application::onBatchApplied(quint64 revision, int day)
{
    m_query->onDataChanged(revision, day);
    if (m_window != nullptr && m_window->isVisible())
    {
        m_window->refresh();
    }
    m_tray->refresh();
}

void Application::onPauseRequested(int minutes)
{
    if (minutes == 0)
    {
        m_pausedIndefinitely = false;
        m_pauseUntil = QDateTime();
        m_pauseTimer->stop();
        m_engine->setPaused(false);
        m_tray->setPaused(false);
        if (m_window != nullptr)
        {
            m_window->setPaused(false);
        }
        return;
    }

    m_engine->setPaused(true);
    m_tray->setPaused(true);
    if (m_window != nullptr)
    {
        m_window->setPaused(true);
    }

    m_pauseTimer->stop();
    if (minutes == -1)
    {
        m_pausedIndefinitely = false;
        m_pauseUntil = QDateTime(QDate::currentDate().addDays(1), QTime(0, 0));
        m_pauseTimer->start(static_cast<int>(qMax<qint64>(1000, QDateTime::currentDateTime().msecsTo(m_pauseUntil))));
    }
    else if (minutes == -2)
    {
        m_pausedIndefinitely = true;
        m_pauseUntil = QDateTime();
    }
    else
    {
        m_pausedIndefinitely = false;
        m_pauseUntil = QDateTime::currentDateTime().addSecs(minutes * 60);
        m_pauseTimer->start(minutes * 60 * 1000);
    }
}

void Application::applyHotkey()
{
    const bool enabled = m_settings->hotkeyEnabled();
    const QKeySequence sequence = m_settings->hotkey();
    const QString text = sequence.toString(QKeySequence::NativeText);

    m_tray->setHotkeyText(enabled ? text : QString());

    if (!enabled)
    {
        m_monitor->unregisterHotkey();
        m_tray->setHotkeyRegistered(false);
        if (m_settingsDialog != nullptr)
        {
            m_settingsDialog->setHotkeyStatus(true, QString());
        }
        return;
    }

    QString error;
    const bool ok = m_monitor->registerHotkey(sequence, &error);
    m_tray->setHotkeyRegistered(ok);
    if (!ok)
    {
        m_settings->setHotkeyEnabled(false);
        m_tray->setHotkeyText(QString());
        m_tray->showMessage(QStringLiteral("全局快捷键未生效"), error + QStringLiteral("（已在设置中关闭）"), true);
    }
    if (m_settingsDialog != nullptr)
    {
        m_settingsDialog->setHotkeyStatus(ok, ok ? QString() : error);
    }
}

void Application::openSettings()
{
    if (m_settingsDialog == nullptr)
    {
        m_settingsDialog = new SettingsDialog(*m_settings, *m_engine, m_window);
        m_settingsDialog->setAttribute(Qt::WA_DeleteOnClose, false);
        connect(m_settingsDialog, &SettingsDialog::preferencesChanged, this,
                [this]
                {
                    if (m_window != nullptr)
                    {
                        m_window->applyPreferences();
                    }
                });
        connect(m_settingsDialog, &SettingsDialog::hotkeyChanged, this,
                [this](bool enabled, const QKeySequence& sequence)
                {
                    m_settings->setHotkeyEnabled(enabled);
                    m_settings->setHotkey(sequence);
                    applyHotkey();
                });
        connect(m_settingsDialog, &SettingsDialog::clearDataRequested, this,
                [this]
                {
                    if (m_store != nullptr)
                    {
                        QMetaObject::invokeMethod(m_store, "clearAll", Qt::QueuedConnection);
                    }
                });
    }
    m_settingsDialog->setHotkeyStatus(m_monitor->isRunning(), QString());
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void Application::showPanel()
{
    if (m_window != nullptr)
    {
        m_window->showPanel();
    }
}

void Application::shutdown()
{
    if (m_shuttingDown)
    {
        return;
    }
    m_shuttingDown = true;

    if (m_monitor != nullptr)
    {
        m_monitor->stop();
    }
    if (m_engine != nullptr)
    {
        m_engine->flush();
    }
    if (m_store != nullptr && m_storeThread != nullptr && m_storeThread->isRunning())
    {
        QMetaObject::invokeMethod(m_store, "shutdown", Qt::BlockingQueuedConnection);
        m_storeThread->quit();
        m_storeThread->wait(3000);
    }
    delete m_store;
    m_store = nullptr;
}

}  // namespace kc
