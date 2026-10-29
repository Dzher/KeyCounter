// keyCounter - 主面板：周期切换 + 概览卡片 + 热力键盘 + 排行 + 趋势
#pragma once

#include <QHash>
#include <QMainWindow>

#include "core/KeyTypes.h"
#include "ui/TrendChartWidget.h"
#include "core/Periods.h"

class QEvent;
class QLabel;
class QMoveEvent;
class QResizeEvent;
class QTimer;
class QToolButton;

namespace kc
{

class CounterEngine;
class Settings;
class StatsQuery;
class KeyboardHeatmapWidget;
class SummaryCardsWidget;
class TopKeysWidget;
class TrendChartWidget;
class PeriodSelector;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(Settings& settings, StatsQuery& query, CounterEngine& engine, QWidget* parent = nullptr);

    /// 重新查询并刷新全部视图
    void refresh();
    void setPaused(bool paused);
    void showPanel();
    void applyPreferences();
    /// 立即把窗口几何（含最大化状态）写入设置；退出前兜底调用
    void persistGeometry();

signals:
    void settingsRequested();
    void pauseToggleRequested();
    void hiddenToTray();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void scheduleGeometrySave();
    static QString topKeyTooltip(const QString& label, const KeyStat& stat, bool pressPrimary);
    void onPeriodChanged(int kind);
    void updateStatusLine();
    void updateCards(const PeriodStats& stats);
    void updateTopKeys(const PeriodStats& stats);
    void updateTrend(const PeriodStats& stats);
    void updateFocusBar(const PeriodStats& stats);
    void setFocusKey(quint16 pid);    static QVector<TrendChartWidget::Point> timelinePoints(const QVector<QPair<int, KeyStat>>& sparse, const Range& range);
    static QVector<TrendChartWidget::Point> hourPoints(const std::array<quint32, 24>& hours);
    quint64 errorCount(const PeriodStats& stats) const;
    QString keyLabel(quint16 pid) const;

    Settings& m_settings;
    StatsQuery& m_query;
    CounterEngine& m_engine;

    PeriodSelector* m_periodSelector = nullptr;
    SummaryCardsWidget* m_cards = nullptr;
    KeyboardHeatmapWidget* m_heatmap = nullptr;
    TopKeysWidget* m_topKeys = nullptr;
    TrendChartWidget* m_trend = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_footerLabel = nullptr;
    QToolButton* m_pauseButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QWidget* m_focusBar = nullptr;
    QLabel* m_focusLabel = nullptr;

    QTimer* m_tickTimer = nullptr;
    QTimer* m_geometryTimer = nullptr;
    Range m_range;
    PeriodKind m_kind = PeriodKind::Today;
    quint16 m_focusPid = 0;
    int m_topKeysTab = 0;  ///< 0 = 高频，1 = 打错
    int m_trendMode = 0;    ///< 0 = 按天/按月，1 = 时段
    QString m_layoutId;
    int m_tickCounter = 0;
    bool m_paused = false;
    bool m_trayHintShown = false;
};

}  // namespace kc
