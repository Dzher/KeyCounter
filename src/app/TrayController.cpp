// keyCounter - 托盘实现
#include "TrayController.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QVector>

#include <QTimer>
#include "layout/LayoutModel.h"

#include "app/Icons.h"
#include "app/Settings.h"
#include "core/CounterEngine.h"
#include "core/Periods.h"
#include "core/StatsQuery.h"
#include "ui/HeatRamp.h"
#include "ui/Theme.h"

namespace kc
{
namespace
{
constexpr int kIconRefreshMs = 60000;
constexpr int kActivityWindowDays = 30;
}  // namespace

TrayController::TrayController(Settings& settings, CounterEngine& engine, StatsQuery& query, QObject* parent)
    : QObject(parent), m_settings(settings), m_engine(engine), m_query(query) {
    m_tray = new QSystemTrayIcon(this);
    m_tray->setToolTip(QStringLiteral("keyCounter · 按键统计"));
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                    emit showPanelRequested();
                }
            });

    m_menu = new QMenu();
    connect(m_menu, &QMenu::aboutToShow, this, &TrayController::rebuildMenu);
    m_tray->setContextMenu(m_menu);

    connect(&Theme::instance(), &Theme::changed, this,
            [this]
            {
                // 深/浅色切换后重新配色（菜单是顶层弹出控件，不会自动跟随）
                Theme::instance().applyToMenu(m_menu);
                refresh();
            });

    // 活跃度环开关 / 颜色等设置变化时立即重绘图标（QSettings 持久化由 Settings 完成）
    connect(&m_settings, &Settings::changed, this, &TrayController::refresh);

    m_iconTimer = new QTimer(this);
    m_iconTimer->setInterval(kIconRefreshMs);
    connect(m_iconTimer, &QTimer::timeout, this, &TrayController::refresh);
    m_iconTimer->start();

    refresh();
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray->show();
    }
}

TrayController::~TrayController() {
    delete m_menu;
}

bool TrayController::available() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayController::setPaused(bool paused) {
    m_paused = paused;
    refresh();
}

void TrayController::setHotkeyText(const QString& text) {
    m_hotkeyText = text;
}

void TrayController::setHotkeyRegistered(bool registered) {
    m_hotkeyRegistered = registered;
}

double TrayController::activityRatio() const {
    const quint64 today = m_engine.todayPress();
    if (today == 0) {
        return 0.0;
    }

    const QDate todayDate = QDate::currentDate();
    const Range range = makeRange(PeriodKind::All, todayDate, m_settings.weekStart(),
                                  static_cast<int>(todayDate.addDays(-(kActivityWindowDays - 1)).toJulianDay()));
    const PeriodStats stats = m_query.query(range);
    const double average = static_cast<double>(stats.totalPress) / static_cast<double>(kActivityWindowDays);
    if (average <= 0.5) {
        return 0.0;
    }
    return static_cast<double>(today) / average;
}

void TrayController::updateIcon() {
    const QColor accent = m_hotkeyRegistered ? Theme::instance().palette().accent : Theme::instance().palette().accent;
    const double ratio = activityRatio();
    const QColor ringColor = m_settings.trayRingColor();
    const bool ringEnabled = m_settings.trayRingEnabled();
    QIcon icon;
    // 原生 16/24 保证 100% 缩放下逐像素锐利；48/64 供高 DPI 托盘取用（较初版 16/24/32 整体翻倍）
    for (int size : {16, 24, 32, 48, 64}) {
        icon.addPixmap(icons::trayIcon(accent, ratio, m_paused, size, ringColor, ringEnabled));
    }
    m_tray->setIcon(icon);
}

void TrayController::refresh() {
    updateIcon();

    const QLocale locale = QLocale::system();
    const quint64 today = m_engine.todayPress();
    const quint64 corr = m_engine.todayCorr();
    QString tooltip =
        QStringLiteral("keyCounter · %1\n今天 %2 次 · 打错修正 %3 次")
            .arg(m_paused ? QStringLiteral("已暂停") : QStringLiteral("正在统计"),
                 locale.toString(static_cast<qlonglong>(today)), locale.toString(static_cast<qlonglong>(corr)));
    if (!m_hotkeyText.isEmpty()) {
        tooltip += QStringLiteral("\n快捷键 %1").arg(m_hotkeyText);
    }
    m_tray->setToolTip(tooltip);
}

void TrayController::rebuildMenu() {
    // 每次弹出都重建菜单；QMenu::clear() 只删 action，不会删掉子菜单控件，
    // 必须在清空前显式释放上一轮的子菜单（连同其 QActionGroup），否则每次右键都留下孤儿控件
    const QList<QMenu*> staleSubmenus = m_menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);
    for (QMenu* submenu : staleSubmenus) {
        delete submenu;
    }
    m_menu->clear();
    m_pauseMenu = nullptr;
    m_layoutMenu = nullptr;
    m_pauseGroup = nullptr;
    m_layoutGroup = nullptr;

    const QLocale locale = QLocale::system();

    auto* openAction = m_menu->addAction(QStringLiteral("打开统计面板"));
    connect(openAction, &QAction::triggered, this, &TrayController::showPanelRequested);

    m_infoAction = m_menu->addAction(QStringLiteral("今天 %1 次 · 打错 %2 次")
                                         .arg(locale.toString(static_cast<qlonglong>(m_engine.todayPress())))
                                         .arg(locale.toString(static_cast<qlonglong>(m_engine.todayCorr()))));
    m_infoAction->setEnabled(false);

    m_menu->addSeparator();

    // 暂停
    m_pauseMenu = m_menu->addMenu(QStringLiteral("暂停统计"));
    m_pauseGroup = new QActionGroup(m_pauseMenu);
    m_pauseGroup->setExclusive(true);
    const QVector<QPair<QString, int>> pauseOptions = {{QStringLiteral("恢复统计"), 0},
                                                       {QStringLiteral("暂停 1 小时"), 60},
                                                       {QStringLiteral("暂停到明天"), -1},
                                                       {QStringLiteral("暂停直到手动恢复"), -2}};
    for (const auto& option : pauseOptions) {
        auto* action = m_pauseMenu->addAction(option.first);
        action->setCheckable(true);
        action->setChecked((option.second == 0) ? !m_paused : false);
        m_pauseGroup->addAction(action);
        const int minutes = option.second;
        connect(action, &QAction::triggered, this, [this, minutes] { emit pauseRequested(minutes); });
    }

    // 键盘布局
    m_layoutMenu = m_menu->addMenu(QStringLiteral("键盘布局"));
    m_layoutGroup = new QActionGroup(m_layoutMenu);
    m_layoutGroup->setExclusive(true);
    const LayoutRepository& repo = LayoutRepository::instance();
    for (const QString& id : repo.availableIds()) {
        auto* action = m_layoutMenu->addAction(repo.displayName(id));
        action->setCheckable(true);
        action->setChecked(id == m_settings.layoutId());
        m_layoutGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, id] { emit layoutChangeRequested(id); });
    }

    // 开机自启
    m_autostartAction = m_menu->addAction(QStringLiteral("开机静默启动"));
    m_autostartAction->setCheckable(true);
    m_autostartAction->setChecked(m_settings.autostart());
    connect(m_autostartAction, &QAction::triggered, this,
            [this](bool checked)
            {
                QString error;
                if (!m_settings.setAutostart(checked, &error)) {
                    showMessage(QStringLiteral("开机自启设置失败"), error, true);
                }
                m_autostartHandled = true;
            });

    if (!m_hotkeyText.isEmpty()) {
        m_hotkeyAction = m_menu->addAction(QStringLiteral("全局快捷键：%1").arg(m_hotkeyText));
        m_hotkeyAction->setEnabled(false);
    }

    m_menu->addSeparator();

    auto* settingsAction = m_menu->addAction(QStringLiteral("设置…"));
    connect(settingsAction, &QAction::triggered, this, &TrayController::settingsRequested);

    auto* quitAction = m_menu->addAction(QStringLiteral("退出 keyCounter"));
    connect(quitAction, &QAction::triggered, this, &TrayController::quitRequested);

    // 子菜单是本轮新建的，配色要放在最后统一套用
    Theme::instance().applyToMenu(m_menu);
}

void TrayController::showMessage(const QString& title, const QString& body, bool warning) {
    m_tray->showMessage(title, body, warning ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information, 6000);
}

}  // namespace kc
