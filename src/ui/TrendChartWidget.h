// keyCounter - 趋势图（按天/按月柱状 + 时段分布），含悬停提示
#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

#include "core/Periods.h"

namespace kc
{

class TrendChartWidget : public QWidget
{
    Q_OBJECT
public:
    enum Mode
    {
        Timeline = 0,  ///< 按天/按月
        Hours = 1,     ///< 0-23 时段分布
    };

    struct Point
    {
        QString label;    ///< 横轴短标签
        QString tooltip;  ///< 悬停详情
        quint32 press = 0;
        quint32 corr = 0;
        int key = 0;      ///< 天序号或小时
    };

    explicit TrendChartWidget(QWidget* parent = nullptr);

    void setSeries(QVector<Point> points, Mode mode, const QString& subtitle);
    void setRamp(int kind);
    void setTimelineEnabled(bool enabled);
    void setHourlyEnabled(bool enabled);
    Mode mode() const noexcept { return m_mode; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void modeChangeRequested(int mode);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QRectF plotRect() const;
    QRectF modeTabRect(int which) const;
    int indexAt(const QPointF& pos) const;

    QVector<Point> m_points;
    Mode m_mode = Timeline;
    QString m_subtitle;
    int m_ramp = 0;
    int m_hover = -1;
    bool m_timelineEnabled = true;
    bool m_hourlyEnabled = true;
};

}  // namespace kc
