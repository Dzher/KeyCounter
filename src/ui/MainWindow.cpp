// keyCounter - 主面板实现
#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "app/Icons.h"
#include "app/Settings.h"
#include "core/CounterEngine.h"
#include "core/KeyCatalog.h"
#include "core/StatsQuery.h"
#include "core/StatsStore.h"
#include "layout/LayoutModel.h"
#include "ui/HeatRamp.h"
#include "ui/KeyboardHeatmapWidget.h"
#include "ui/PeriodSelector.h"
#include "ui/SummaryCardsWidget.h"
#include "ui/Theme.h"
#include "ui/TopKeysWidget.h"
#include "ui/TrendChartWidget.h"

namespace kc
{
namespace
{

constexpr int kTickIntervalMs = 2000;
constexpr int kTicksPerRefresh = 8;  ///< 约 16 s 全量刷新一次（与落盘周期对齐）
constexpr int kTopKeyCount = 12;

QToolButton* makeToolButton(QWidget* parent, const QIcon& icon, const QString& tooltip)
{
    auto* button = new QToolButton(parent);
    button->setIcon(icon);
    button->setIconSize(QSize(18, 18));
    button->setAutoRaise(true);
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(32, 30);
    return button;
}

}  // namespace

MainWindow::MainWindow(Settings& settings, StatsQuery& query, CounterEngine& engine, QWidget* parent)
    : QMainWindow(parent), m_settings(settings), m_query(query), m_engine(engine)
{
    buildUi();
    setPaused(m_engine.isPaused());

    connect(&Theme::instance(), &Theme::changed, this,
            [this]
            {
                m_settingsButton->setIcon(icons::gear(Theme::instance().palette().textDim, 18));
                m_pauseButton->setIcon(m_paused ? icons::play(Theme::instance().palette().ok, 18)
                                                : icons::pause(Theme::instance().palette().textDim, 18));
                update();
            });

    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(kTickIntervalMs);
    connect(m_tickTimer, &QTimer::timeout, this,
            [this]
            {
                updateStatusLine();
                if (++m_tickCounter >= kTicksPerRefresh)
                {
                    m_tickCounter = 0;
                    refresh();
                }
            });

    refresh();
}

void MainWindow::buildUi()
{
    const Palette& palette = Theme::instance().palette();

    setWindowTitle(QStringLiteral("keyCounter · 按键统计"));
    setWindowIcon(QIcon(icons::appIcon(palette.accent, 64)));
    setMinimumSize(920, 620);
    resize(1120, 780);

    auto* central = new QWidget(this);
    central->setAutoFillBackground(true);
    QPalette centralPalette = central->palette();
    centralPalette.setColor(QPalette::Window, palette.window);
    central->setPalette(centralPalette);
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 12, 16, 10);
    root->setSpacing(12);

    // ---- 顶部栏 ----
    auto* header = new QHBoxLayout();
    header->setSpacing(10);

    m_statusLabel = new QLabel(central);
    m_statusLabel->setFont(Theme::instance().font(13, QFont::DemiBold));
    header->addWidget(m_statusLabel);
    header->addStretch(1);

    m_periodSelector = new PeriodSelector(central);
    m_periodSelector->setReduceMotion(m_settings.reduceMotion());
    connect(m_periodSelector, &PeriodSelector::kindChanged, this, &MainWindow::onPeriodChanged);
    header->addWidget(m_periodSelector);

    m_pauseButton = makeToolButton(central, icons::pause(palette.textDim, 18), QStringLiteral("暂停 / 恢复统计"));
    connect(m_pauseButton, &QToolButton::clicked, this, &MainWindow::pauseToggleRequested);
    header->addWidget(m_pauseButton);

    m_settingsButton = makeToolButton(central, icons::gear(palette.textDim, 18), QStringLiteral("设置"));
    connect(m_settingsButton, &QToolButton::clicked, this, &MainWindow::settingsRequested);
    header->addWidget(m_settingsButton);

    root->addLayout(header);

    // ---- 概览卡片 ----
    m_cards = new SummaryCardsWidget(central);
    m_cards->setFixedHeight(96);
    root->addWidget(m_cards);

    // ---- 中部：热力键盘 + 排行 ----
    auto* middle = new QHBoxLayout();
    middle->setSpacing(12);

    auto* boardCard = new CardPanel(central);
    auto* boardLayout = new QVBoxLayout(boardCard);
    boardLayout->setContentsMargins(14, 12, 14, 10);
    m_heatmap = new KeyboardHeatmapWidget(boardCard);
    boardLayout->addWidget(m_heatmap);
    connect(m_heatmap, &KeyboardHeatmapWidget::keyActivated, this, &MainWindow::setFocusKey);
    middle->addWidget(boardCard, 3);

    auto* listCard = new CardPanel(central);
    listCard->setMinimumWidth(268);
    listCard->setMaximumWidth(360);
    auto* listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(16, 12, 12, 10);
    m_topKeys = new TopKeysWidget(listCard);
    m_topKeys->setTabs({QStringLiteral("高频"), QStringLiteral("打错")}, 0);
    connect(m_topKeys, &TopKeysWidget::tabChangeRequested, this,
            [this](int tab)
            {
                m_topKeysTab = tab;
                refresh();
            });
    connect(m_topKeys, &TopKeysWidget::entryActivated, this, &MainWindow::setFocusKey);
    listLayout->addWidget(m_topKeys);
    middle->addWidget(listCard, 1);

    root->addLayout(middle, 4);

    // ---- 底部：趋势 ----
    auto* trendCard = new CardPanel(central);
    auto* trendLayout = new QVBoxLayout(trendCard);
    trendLayout->setContentsMargins(14, 8, 14, 6);
    trendLayout->setSpacing(4);

    auto* focusRow = new QHBoxLayout();
    focusRow->setSpacing(8);
    m_focusBar = new QWidget(trendCard);
    auto* focusLayout = new QHBoxLayout(m_focusBar);
    focusLayout->setContentsMargins(0, 0, 0, 0);
    focusLayout->setSpacing(8);
    m_focusLabel = new QLabel(m_focusBar);
    m_focusLabel->setFont(Theme::instance().font(11));
    focusLayout->addWidget(m_focusLabel);
    auto* backButton = new QToolButton(m_focusBar);
    backButton->setText(QStringLiteral("返回总览"));
    backButton->setAutoRaise(true);
    backButton->setCursor(Qt::PointingHandCursor);
    backButton->setFont(Theme::instance().font(11));
    connect(backButton, &QToolButton::clicked, this, [this] { setFocusKey(kInvalidPid); });
    focusLayout->addWidget(backButton);
    focusLayout->addStretch(1);
    m_focusBar->setVisible(false);
    focusRow->addWidget(m_focusBar);
    focusRow->addStretch(1);
    trendLayout->addLayout(focusRow);

    m_trend = new TrendChartWidget(trendCard);
    connect(m_trend, &TrendChartWidget::modeChangeRequested, this,
            [this](int mode)
            {
                m_trendMode = mode;
                refresh();
            });
    trendLayout->addWidget(m_trend);
    root->addWidget(trendCard, 2);

    // ---- 底部状态 ----
    m_footerLabel = new QLabel(central);
    m_footerLabel->setFont(Theme::instance().font(10));
    m_footerLabel->setStyleSheet(QStringLiteral("color:%1;").arg(palette.textFaint.name()));
    root->addWidget(m_footerLabel);

    const QByteArray geometry = m_settings.windowGeometry();
    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
        m_geometryRestored = true;
    }
    m_kind = static_cast<PeriodKind>(m_settings.lastPeriod());
    m_periodSelector->setCurrentKind(static_cast<int>(m_kind));
}

void MainWindow::applyPreferences()
{
    m_periodSelector->setReduceMotion(m_settings.reduceMotion());
    if (m_settings.layoutId() != m_layoutId)
    {
        m_layoutId = m_settings.layoutId();
        m_heatmap->setKeyboardLayout(LayoutRepository::instance().layout(m_layoutId));
    }
    m_trend->setTimelineEnabled(true);
    m_trend->setHourlyEnabled(m_settings.hourlyChart());
    if (!m_settings.hourlyChart() && m_trendMode == 1)
    {
        m_trendMode = 0;
    }
    refresh();
}

void MainWindow::setPaused(bool paused)
{
    m_paused = paused;
    const Palette& palette = Theme::instance().palette();
    m_pauseButton->setIcon(paused ? icons::play(palette.ok, 18) : icons::pause(palette.textDim, 18));
    m_pauseButton->setToolTip(paused ? QStringLiteral("已暂停 · 点击恢复统计") : QStringLiteral("正在统计 · 点击暂停"));
    updateStatusLine();
}

void MainWindow::showPanel()
{
    if (isMinimized())
    {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    show();
    raise();
    activateWindow();
    refresh();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_settings.setWindowGeometry(saveGeometry());
    m_settings.sync();
    event->ignore();
    hide();
    if (!m_trayHintShown)
    {
        m_trayHintShown = true;
        emit hiddenToTray();
    }
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    m_tickTimer->start();
    refresh();
}

void MainWindow::hideEvent(QHideEvent* event)
{
    QMainWindow::hideEvent(event);
    m_tickTimer->stop();
    m_settings.setWindowGeometry(saveGeometry());
    m_settings.sync();
}

void MainWindow::onPeriodChanged(int kind)
{
    m_kind = static_cast<PeriodKind>(qBound(0, kind, 4));
    m_settings.setLastPeriod(kind);
    m_focusPid = kInvalidPid;
    refresh();
}

void MainWindow::setFocusKey(quint16 pid)
{
    m_focusPid = (pid == m_focusPid) ? kInvalidPid : pid;
    refresh();
}

QString MainWindow::keyLabel(quint16 pid) const
{
    if (const KeyboardLayout* layout = m_heatmap->keyboardLayout())
    {
        if (const KeyGeometry* key = layout->find(pid))
        {
            if (!key->label.trimmed().isEmpty())
            {
                return key->label;
            }
        }
    }
    return QString::fromLatin1(keyInfo(pid).name);
}

quint64 MainWindow::errorCount(const PeriodStats& stats) const
{
    if (m_settings.errorMode() == Settings::ErrorMode::Literal)
    {
        return static_cast<quint64>(stats.keys.value(pk::Backspace).press) + stats.keys.value(pk::Delete).press;
    }
    return stats.totalCorr;
}

void MainWindow::refresh()
{
    m_kind = static_cast<PeriodKind>(m_periodSelector->currentKind());
    m_range = makeRange(m_kind, QDate::currentDate(), m_settings.weekStart(), m_engine.firstDataDay());
    const PeriodStats stats = m_query.query(m_range);

    if (m_settings.layoutId() != m_layoutId)
    {
        m_layoutId = m_settings.layoutId();
        m_heatmap->setKeyboardLayout(LayoutRepository::instance().layout(m_layoutId));
    }
    m_heatmap->setStats(stats.keys, stats.totalPress, m_settings.heatRamp(),
                        m_settings.errorMode() == Settings::ErrorMode::Attributed);

    updateCards(stats);
    updateTopKeys(stats);
    updateTrend(stats);
    updateFocusBar(stats);
    updateStatusLine();
}

void MainWindow::updateStatusLine()
{
    const Palette& palette = Theme::instance().palette();
    const QLocale locale = QLocale::system();
    const quint64 todayPress = m_engine.todayPress();
    const QColor dot = m_paused ? palette.textFaint : palette.ok;
    m_statusLabel->setText(QStringLiteral("<span style='color:%1'>●</span> %2 · 今天 %3 次")
                               .arg(dot.name(), m_paused ? QStringLiteral("已暂停统计") : QStringLiteral("正在统计"),
                                    locale.toString(static_cast<qlonglong>(todayPress))));

    if (m_footerLabel != nullptr)
    {
        const int firstDataDay = m_engine.firstDataDay();
        const QString since = firstDataDay > 0 ? QDate::fromJulianDay(firstDataDay).toString(QStringLiteral("yyyy-MM-dd"))
                                               : QStringLiteral("—");
        m_footerLabel->setText(QStringLiteral("数据目录 %1 · 自 %2 起统计 · 仅保存逐键计数，不含按键内容")
                                   .arg(StatsStore::dataDir(), since));
    }
}

void MainWindow::updateCards(const PeriodStats& stats)
{
    const Palette& palette = Theme::instance().palette();
    const QLocale locale = QLocale::system();

    quint64 totalPress = stats.totalPress;
    if (m_kind == PeriodKind::Today)
    {
        totalPress = qMax<quint64>(totalPress, m_engine.todayPress());
    }

    const quint64 errors = errorCount(stats);
    const double errorRate = totalPress > 0 ? static_cast<double>(errors) * 100.0 / static_cast<double>(totalPress) : 0.0;

    int elapsedDays = m_range.dayCount();
    if (m_kind != PeriodKind::All)
    {
        const int todayDay = static_cast<int>(QDate::currentDate().toJulianDay());
        elapsedDays = qMax(1, qMin(m_range.endDay, todayDay) - m_range.startDay + 1);
    }
    else
    {
        elapsedDays = qMax(1, elapsedDays - 1);  // "累计"按已过去的天数平均
    }

    QVector<SummaryCard> cards;
    cards.append({QStringLiteral("本期键击"), locale.toString(static_cast<qlonglong>(totalPress)),
                  rangeSubtitle(m_range, QDate::currentDate()), palette.accent});

    cards.append({QStringLiteral("打错修正"),
                  locale.toString(static_cast<qlonglong>(errors)),
                  QStringLiteral("错误率 %1%").arg(errorRate, 0, 'f', 2), palette.danger});

    const QString topKey = stats.maxKeyPid != kInvalidPid ? keyLabel(stats.maxKeyPid) : QStringLiteral("—");
    const double share = totalPress > 0 ? static_cast<double>(stats.maxKeyPress) * 100.0 / static_cast<double>(totalPress) : 0.0;
    cards.append({QStringLiteral("最高频键"), topKey,
                  stats.maxKeyPress > 0 ? QStringLiteral("%1 次 · 占比 %2%")
                                              .arg(locale.toString(static_cast<qlonglong>(stats.maxKeyPress)))
                                              .arg(share, 0, 'f', 1)
                                        : QStringLiteral("暂无数据"),
                  HeatRamp::colorFor(0.95, m_settings.heatRamp())});

    const double daily = static_cast<double>(totalPress) / static_cast<double>(elapsedDays);
    cards.append({QStringLiteral("日均键击"), locale.toString(static_cast<qlonglong>(std::llround(daily))),
                  QStringLiteral("活跃 %1 天").arg(stats.activeDays), palette.ok});

    m_cards->setCards(cards);
}

void MainWindow::updateTopKeys(const PeriodStats& stats)
{
    QVector<TopKeysWidget::Entry> entries;

    if (m_topKeysTab == 0)
    {
        QVector<QPair<quint16, KeyStat>> items;
        items.reserve(stats.keys.size());
        for (auto it = stats.keys.constBegin(); it != stats.keys.constEnd(); ++it)
        {
            if (it.value().press > 0)
            {
                items.append({it.key(), it.value()});
            }
        }
        std::sort(items.begin(), items.end(),
                  [](const QPair<quint16, KeyStat>& a, const QPair<quint16, KeyStat>& b)
                  { return a.second.press > b.second.press; });
        for (const auto& item : items.mid(0, kTopKeyCount))
        {
            entries.append({item.first, keyLabel(item.first), item.second.press, item.second.corr});
        }
        m_topKeys->setTitle(QStringLiteral("高频键 · Top %1").arg(kTopKeyCount));
        m_topKeys->setEmptyHint(QStringLiteral("本期还没有数据"));
    }
    else
    {
        QVector<QPair<quint16, KeyStat>> items;
        for (auto it = stats.keys.constBegin(); it != stats.keys.constEnd(); ++it)
        {
            if (it.value().corr > 0)
            {
                items.append({it.key(), it.value()});
            }
        }
        std::sort(items.begin(), items.end(),
                  [](const QPair<quint16, KeyStat>& a, const QPair<quint16, KeyStat>& b)
                  { return a.second.corr > b.second.corr; });
        for (const auto& item : items.mid(0, kTopKeyCount))
        {
            entries.append({item.first, keyLabel(item.first), item.second.corr, item.second.press});
        }
        m_topKeys->setTitle(QStringLiteral("最常打错的键"));
        m_topKeys->setEmptyHint(QStringLiteral("还没有检测到打错后删除的按键"));
    }

    m_topKeys->setEntries(entries, m_settings.heatRamp(), m_topKeysTab == 0);
    m_topKeys->setTabs({QStringLiteral("高频"), QStringLiteral("打错")}, m_topKeysTab);
}

QVector<TrendChartWidget::Point> MainWindow::timelinePoints(const QVector<QPair<int, KeyStat>>& sparse, const Range& range)
{
    const QLocale locale = QLocale::system();
    QVector<TrendChartWidget::Point> points;

    if (range.dayCount() <= 120)
    {
        QHash<int, KeyStat> map;
        for (const auto& item : sparse)
        {
            map.insert(item.first, item.second);
        }
        points.reserve(range.dayCount());
        for (int day = range.startDay; day <= range.endDay; ++day)
        {
            const KeyStat stat = map.value(day);
            TrendChartWidget::Point point;
            point.key = day;
            point.press = stat.press;
            point.corr = stat.corr;
            const QDate date = QDate::fromJulianDay(day);
            point.label = date.toString(QStringLiteral("MM-dd"));
            QString tooltip = QStringLiteral("%1（%2）\n%3 次键击")
                                  .arg(date.toString(QStringLiteral("yyyy-MM-dd")), date.toString(QStringLiteral("dddd")),
                                       locale.toString(static_cast<qlonglong>(stat.press)));
            if (stat.corr > 0)
            {
                tooltip += QStringLiteral("\n打错修正 %1 次").arg(stat.corr);
            }
            point.tooltip = tooltip;
            points.append(point);
        }
        return points;
    }

    // 区间较长时按月聚合
    const QDate startDate = QDate::fromJulianDay(range.startDay);
    const QDate endDate = QDate::fromJulianDay(range.endDay);
    const bool singleYear = (startDate.year() == endDate.year());

    QMap<QPair<int, int>, KeyStat> monthly;
    for (const auto& item : sparse)
    {
        const QDate date = QDate::fromJulianDay(item.first);
        KeyStat& stat = monthly[{date.year(), date.month()}];
        stat.press += item.second.press;
        stat.corr += item.second.corr;
    }

    int year = startDate.year();
    int month = startDate.month();
    while (year * 12 + month <= endDate.year() * 12 + endDate.month())
    {
        const KeyStat stat = monthly.value({year, month});
        TrendChartWidget::Point point;
        point.key = year * 12 + month;
        point.press = stat.press;
        point.corr = stat.corr;
        point.label = singleYear ? QStringLiteral("%1月").arg(month)
                                 : QStringLiteral("%1-%2").arg(year % 100, 2, 10, QLatin1Char('0'))
                                       .arg(month, 2, 10, QLatin1Char('0'));
        QString tooltip = QStringLiteral("%1 年 %2 月\n%3 次键击")
                              .arg(year)
                              .arg(month)
                              .arg(locale.toString(static_cast<qlonglong>(stat.press)));
        if (stat.corr > 0)
        {
            tooltip += QStringLiteral("\n打错修正 %1 次").arg(stat.corr);
        }
        point.tooltip = tooltip;
        points.append(point);

        if (++month > 12)
        {
            month = 1;
            ++year;
        }
    }
    return points;
}

QVector<TrendChartWidget::Point> MainWindow::hourPoints(const std::array<quint32, 24>& hours)
{
    const QLocale locale = QLocale::system();
    QVector<TrendChartWidget::Point> points;
    points.reserve(24);
    for (int hour = 0; hour < 24; ++hour)
    {
        TrendChartWidget::Point point;
        point.key = hour;
        point.press = hours[static_cast<size_t>(hour)];
        point.label = QStringLiteral("%1时").arg(hour);
        point.tooltip = QStringLiteral("%1:00 - %2:00\n%3 次键击")
                            .arg(hour, 2, 10, QLatin1Char('0'))
                            .arg(hour + 1, 2, 10, QLatin1Char('0'))
                            .arg(locale.toString(static_cast<qlonglong>(point.press)));
        points.append(point);
    }
    return points;
}

void MainWindow::updateTrend(const PeriodStats& stats)
{
    if (m_focusPid != kInvalidPid)
    {
        const QVector<QPair<int, KeyStat>> series = m_query.keySeries(m_focusPid, m_range);
        m_trend->setSeries(timelinePoints(series, m_range), TrendChartWidget::Timeline,
                           QStringLiteral("键 %1 的走势").arg(keyLabel(m_focusPid)));
        return;
    }

    if (m_trendMode == 1)
    {
        m_trend->setSeries(hourPoints(stats.hours), TrendChartWidget::Hours,
                           rangeSubtitle(m_range, QDate::currentDate()));
    }
    else
    {
        m_trend->setSeries(timelinePoints(stats.days, m_range), TrendChartWidget::Timeline,
                           rangeSubtitle(m_range, QDate::currentDate()));
    }
}

void MainWindow::updateFocusBar(const PeriodStats& stats)
{
    const bool focused = (m_focusPid != kInvalidPid);
    m_focusBar->setVisible(focused);
    if (!focused)
    {
        return;
    }

    const QLocale locale = QLocale::system();
    const KeyStat stat = stats.keys.value(m_focusPid);
    const double share = stats.totalPress > 0
                             ? static_cast<double>(stat.press) * 100.0 / static_cast<double>(stats.totalPress)
                             : 0.0;
    QString text = QStringLiteral("已聚焦：<b>%1</b> · %2 次 · 占比 %3%")
                       .arg(keyLabel(m_focusPid), locale.toString(static_cast<qlonglong>(stat.press)))
                       .arg(share, 0, 'f', 2);
    if (stat.corr > 0)
    {
        text += QStringLiteral(" · <span style='color:%1'>打错 %2 次</span>")
                    .arg(Theme::instance().palette().danger.name())
                    .arg(stat.corr);
    }
    m_focusLabel->setText(text);
}

}  // namespace kc
