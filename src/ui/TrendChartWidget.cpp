// keyCounter - 趋势图实现
#include "TrendChartWidget.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <cmath>

#include "ui/HeatRamp.h"
#include "ui/Theme.h"

namespace kc
{
namespace
{
constexpr double kHeaderHeight = 40.0;
constexpr double kAxisWidth = 48.0;
constexpr double kAxisHeight = 22.0;

QString compactNumber(quint64 value)
{
    if (value >= 1000000)
    {
        return QStringLiteral("%1M").arg(static_cast<double>(value) / 1000000.0, 0, 'f', 1);
    }
    if (value >= 10000)
    {
        return QStringLiteral("%1k").arg(static_cast<double>(value) / 1000.0, 0, 'f', 1);
    }
    return QLocale::system().toString(static_cast<qlonglong>(value));
}

}  // namespace

TrendChartWidget::TrendChartWidget(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    connect(&Theme::instance(), &Theme::changed, this, QOverload<>::of(&QWidget::update));
}

void TrendChartWidget::setSeries(QVector<Point> points, Mode mode, const QString& subtitle)
{
    m_points = std::move(points);
    m_mode = mode;
    m_subtitle = subtitle;
    m_hover = -1;
    update();
}

void TrendChartWidget::setRamp(int kind)
{
    m_ramp = kind;
    update();
}

void TrendChartWidget::setTimelineEnabled(bool enabled)
{
    m_timelineEnabled = enabled;
    if (!enabled && m_mode == Timeline)
    {
        m_mode = Hours;
    }
    update();
}

void TrendChartWidget::setHourlyEnabled(bool enabled)
{
    m_hourlyEnabled = enabled;
    if (!enabled && m_mode == Hours)
    {
        m_mode = Timeline;
    }
    update();
}

QSize TrendChartWidget::sizeHint() const
{
    return QSize(600, 220);
}

QSize TrendChartWidget::minimumSizeHint() const
{
    return QSize(320, 150);
}

QRectF TrendChartWidget::plotRect() const
{
    return QRectF(kAxisWidth, kHeaderHeight, qMax(10.0, static_cast<double>(width()) - kAxisWidth - 10.0),
                  qMax(10.0, static_cast<double>(height()) - kHeaderHeight - kAxisHeight));
}

QRectF TrendChartWidget::modeTabRect(int which) const
{
    const double tabWidth = 52.0;
    const double x = static_cast<double>(width()) - 8.0 - tabWidth * (which == 0 ? 2 : 1);
    return QRectF(x, 8.0, tabWidth, 24.0);
}

int TrendChartWidget::indexAt(const QPointF& pos) const
{
    const QRectF plot = plotRect();
    if (!plot.contains(pos) || m_points.isEmpty())
    {
        return -1;
    }
    const double step = plot.width() / static_cast<double>(m_points.size());
    const int index = static_cast<int>((pos.x() - plot.left()) / step);
    return (index >= 0 && index < m_points.size()) ? index : -1;
}

void TrendChartWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const Palette& palette = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 标题
    painter.setPen(palette.textDim);
    painter.setFont(Theme::instance().font(12, QFont::DemiBold));
    painter.drawText(QRectF(2.0, 6.0, width() - 130.0, 18.0), Qt::AlignLeft | Qt::AlignVCenter,
                     m_mode == Hours ? QStringLiteral("时段分布") : QStringLiteral("趋势"));
    if (!m_subtitle.isEmpty())
    {
        painter.setPen(palette.textFaint);
        painter.setFont(Theme::instance().font(11));
        painter.drawText(QRectF(60.0, 6.0, width() - 200.0, 18.0), Qt::AlignLeft | Qt::AlignVCenter, m_subtitle);
    }

    // 模式切换
    const QStringList tabs = {QStringLiteral("按天"), QStringLiteral("时段")};
    for (int i = 0; i < tabs.size(); ++i)
    {
        const bool available = (i == 1) ? m_hourlyEnabled : m_timelineEnabled;
        const bool active = (static_cast<int>(m_mode) == i);
        const QRectF tab = modeTabRect(i);
        if (active)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::alpha(palette.accent, palette.dark ? 52 : 36));
            painter.drawRoundedRect(tab, 6.0, 6.0);
        }
        painter.setPen(available ? (active ? palette.text : palette.textDim) : palette.textFaint);
        painter.setFont(Theme::instance().font(11, active ? QFont::DemiBold : QFont::Normal));
        painter.drawText(tab, Qt::AlignCenter, tabs.at(i));
    }

    const QRectF plot = plotRect();

    if (m_points.isEmpty())
    {
        painter.setPen(palette.textFaint);
        painter.setFont(Theme::instance().font(12));
        painter.drawText(plot, Qt::AlignCenter, QStringLiteral("本期没有数据"));
        return;
    }

    quint32 maxPress = 1;
    for (const Point& point : m_points)
    {
        maxPress = qMax(maxPress, point.press);
    }
    // 让柱顶不至于顶到边框
    const double scaleMax = static_cast<double>(maxPress) * 1.12;

    // 网格 + Y 轴刻度
    painter.setFont(Theme::instance().font(10));
    for (int i = 0; i <= 3; ++i)
    {
        const double ratio = static_cast<double>(i) / 3.0;
        const double y = plot.bottom() - plot.height() * ratio;
        painter.setPen(QPen(Theme::alpha(palette.text, i == 0 ? 40 : 22), 1.0));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(palette.textFaint);
        painter.drawText(QRectF(2.0, y - 8.0, kAxisWidth - 8.0, 16.0), Qt::AlignRight | Qt::AlignVCenter,
                         compactNumber(static_cast<quint64>(scaleMax * ratio)));
    }

    const double step = plot.width() / static_cast<double>(m_points.size());
    const double barWidth = qBound(2.0, step * 0.62, 26.0);
    const int labelStride = qMax(1, static_cast<int>(std::ceil(static_cast<double>(m_points.size()) / 8.0)));

    for (int i = 0; i < m_points.size(); ++i)
    {
        const Point& point = m_points.at(i);
        const double centerX = plot.left() + step * (static_cast<double>(i) + 0.5);
        const double height = plot.height() * (static_cast<double>(point.press) / scaleMax);
        const QRectF bar(centerX - barWidth / 2.0, plot.bottom() - height, barWidth, qMax(0.0, height));

        if (point.press > 0 && bar.height() >= 1.0)
        {
            const QColor color = HeatRamp::colorFor(
                0.25 + 0.75 * HeatRamp::normalize(point.press, maxPress), m_ramp);
            QLinearGradient gradient(bar.topLeft(), bar.bottomLeft());
            gradient.setColorAt(0.0, color.lighter(118));
            gradient.setColorAt(1.0, color.darker(112));
            painter.setPen(Qt::NoPen);
            painter.setBrush(gradient);
            painter.drawRoundedRect(bar, qMin(3.5, barWidth / 2.0), qMin(3.5, barWidth / 2.0));

            // 错误占比（底部红色段）
            if (point.corr > 0 && bar.height() > 6.0)
            {
                const double ratio = qBound(0.0, static_cast<double>(point.corr) / static_cast<double>(point.press), 0.6);
                const double errorHeight = qMax(2.0, bar.height() * ratio);
                painter.setBrush(Theme::alpha(palette.danger, 225));
                painter.drawRoundedRect(QRectF(bar.left(), bar.bottom() - errorHeight, bar.width(), errorHeight), 2.0, 2.0);
            }
        }
        else
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::alpha(palette.text, palette.dark ? 16 : 12));
            painter.drawRoundedRect(QRectF(bar.left(), plot.bottom() - 2.0, bar.width(), 2.0), 1.0, 1.0);
        }

        // 悬停指示
        if (i == m_hover)
        {
            painter.setPen(QPen(Theme::alpha(palette.accent, 130), 1.0, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(QPointF(centerX, plot.top()), QPointF(centerX, plot.bottom()));
        }

        if (i % labelStride == 0)
        {
            painter.setPen(i == m_hover ? palette.text : palette.textFaint);
            painter.setFont(Theme::instance().font(10));
            painter.drawText(QRectF(centerX - 30.0, plot.bottom() + 3.0, 60.0, 16.0), Qt::AlignCenter, point.label);
        }
    }
}

void TrendChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position());
    if (index != m_hover)
    {
        m_hover = index;
        update();
    }
    if (m_hover >= 0 && m_hover < m_points.size())
    {
        QToolTip::showText(event->globalPosition().toPoint() + QPoint(12, 12), m_points.at(m_hover).tooltip, this);
    }
    else
    {
        QToolTip::hideText();
    }
    QWidget::mouseMoveEvent(event);
}

void TrendChartWidget::leaveEvent(QEvent* event)
{
    m_hover = -1;
    QToolTip::hideText();
    update();
    QWidget::leaveEvent(event);
}

void TrendChartWidget::mousePressEvent(QMouseEvent* event)
{
    for (int i = 0; i < 2; ++i)
    {
        if (modeTabRect(i).contains(event->position()))
        {
            if ((i == 0 && !m_timelineEnabled) || (i == 1 && !m_hourlyEnabled))
            {
                return;
            }
            if (static_cast<int>(m_mode) != i)
            {
                emit modeChangeRequested(i);
            }
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

}  // namespace kc
