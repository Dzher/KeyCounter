// keyCounter - 高频键排行实现
#include "TopKeysWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

#include "ui/HeatRamp.h"
#include "ui/Theme.h"

namespace kc
{
namespace
{
constexpr int kMaxRows = 12;
}

TopKeysWidget::TopKeysWidget(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    connect(&Theme::instance(), &Theme::changed, this, QOverload<>::of(&QWidget::update));
}

void TopKeysWidget::setTitle(const QString& title)
{
    m_title = title;
    update();
}

void TopKeysWidget::setTabs(const QStringList& tabs, int current)
{
    m_tabs = tabs;
    m_tab = qBound(0, current, qMax(0, tabs.size() - 1));
    update();
}

QRectF TopKeysWidget::tabRect(int index) const
{
    const double tabWidth = 46.0;
    const double right = static_cast<double>(width()) - 2.0;
    return QRectF(right - tabWidth * (m_tabs.size() - index), 5.0, tabWidth - 4.0, 19.0);
}

void TopKeysWidget::setEntries(const QVector<Entry>& entries, int rampKind)
{
    m_entries = entries.mid(0, kMaxRows);
    m_ramp = rampKind;
    m_hover = -1;
    setToolTip(QString());
    update();
}

void TopKeysWidget::setEmptyHint(const QString& hint)
{
    m_emptyHint = hint;
    update();
}

QSize TopKeysWidget::sizeHint() const
{
    return QSize(300, 380);
}

QSize TopKeysWidget::minimumSizeHint() const
{
    return QSize(210, 220);
}

void TopKeysWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const Palette& palette = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const double top = 34.0;
    painter.setPen(palette.textDim);
    painter.setFont(Theme::instance().font(12, QFont::DemiBold));
    painter.drawText(QRectF(2.0, 4.0, width() - 4.0, 20.0), Qt::AlignLeft | Qt::AlignVCenter, m_title);

    for (int i = 0; i < m_tabs.size(); ++i)
    {
        const QRectF tab = tabRect(i);
        const bool active = (i == m_tab);
        if (active)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::alpha(palette.accent, palette.dark ? 46 : 32));
            painter.drawRoundedRect(tab, 5.0, 5.0);
        }
        painter.setPen(active ? palette.text : palette.textFaint);
        painter.setFont(Theme::instance().font(10, active ? QFont::DemiBold : QFont::Normal));
        painter.drawText(tab, Qt::AlignCenter, m_tabs.at(i));
    }

    if (m_entries.isEmpty())
    {
        painter.setPen(palette.textFaint);
        painter.setFont(Theme::instance().font(12));
        painter.drawText(QRectF(0, top, width(), static_cast<double>(height()) - top), Qt::AlignCenter, m_emptyHint);
        return;
    }

    const double rowHeight =
        qMin(30.0, (static_cast<double>(height()) - top) / static_cast<double>(m_entries.size()));
    const double chipWidth = 52.0;
    const double countWidth = 62.0;
    const double barLeft = chipWidth + 10.0;
    const double barRight = width() - countWidth - 6.0;
    const double maxCount = static_cast<double>(m_entries.first().count);

    const QLocale locale = QLocale::system();

    for (int i = 0; i < m_entries.size(); ++i)
    {
        const Entry& entry = m_entries.at(i);
        const QRectF row(0.0, top + rowHeight * i, static_cast<double>(width()), rowHeight);
        const bool hovered = (i == m_hover);

        if (hovered)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::alpha(palette.accent, palette.dark ? 26 : 18));
            painter.drawRoundedRect(row.adjusted(-4.0, 1.0, 0.0, -1.0), 6.0, 6.0);
        }

        // 键名徽标
        const double chipHeight = qMin(22.0, rowHeight - 4.0);
        const QRectF chip(row.left(), row.center().y() - chipHeight / 2.0, chipWidth, chipHeight);
        const double t = (maxCount > 0.0) ? HeatRamp::normalize(entry.count, static_cast<quint32>(maxCount)) : 0.0;
        const QColor heat = HeatRamp::colorFor(qBound(0.15, t, 1.0), m_ramp);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::alpha(heat, palette.dark ? 90 : 70));
        painter.drawRoundedRect(chip, 6.0, 6.0);
        painter.setPen(palette.text);
        painter.setFont(Theme::instance().font(11, QFont::DemiBold));
        painter.drawText(chip, Qt::AlignCenter, entry.label);

        // 条形
        const double barHeight = qMin(10.0, chipHeight * 0.55);
        const QRectF track(barLeft, row.center().y() - barHeight / 2.0, qMax(10.0, barRight - barLeft), barHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::alpha(palette.text, palette.dark ? 18 : 14));
        painter.drawRoundedRect(track, barHeight / 2.0, barHeight / 2.0);

        const double ratio = (maxCount > 0.0) ? qBound(0.0, static_cast<double>(entry.count) / maxCount, 1.0) : 0.0;
        QRectF fill = track;
        fill.setWidth(qMax(barHeight, track.width() * ratio));
        QLinearGradient gradient(fill.topLeft(), fill.topRight());
        gradient.setColorAt(0.0, heat.lighter(115));
        gradient.setColorAt(1.0, heat);
        painter.setBrush(gradient);
        painter.drawRoundedRect(fill, barHeight / 2.0, barHeight / 2.0);

        // 次数
        painter.setPen(palette.text);
        painter.setFont(Theme::instance().numberFont(12, QFont::DemiBold));
        painter.drawText(QRectF(barRight + 4.0, row.top(), countWidth - 6.0, rowHeight), Qt::AlignRight | Qt::AlignVCenter,
                         locale.toString(static_cast<qlonglong>(entry.count)));
    }
}

void TopKeysWidget::mouseMoveEvent(QMouseEvent* event)
{
    const double top = 34.0;
    const double rowHeight =
        m_entries.isEmpty() ? 0.0 : qMin(30.0, (static_cast<double>(height()) - top) / static_cast<double>(m_entries.size()));
    int index = -1;
    if (rowHeight > 0.0 && event->position().y() >= top)
    {
        const int candidate = static_cast<int>((event->position().y() - top) / rowHeight);
        if (candidate >= 0 && candidate < m_entries.size())
        {
            index = candidate;
        }
    }
    if (index != m_hover)
    {
        m_hover = index;
        setCursor(m_hover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        setToolTip(m_hover >= 0 ? m_entries.at(m_hover).tooltip : QString());
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void TopKeysWidget::leaveEvent(QEvent* event)
{
    m_hover = -1;
    setCursor(Qt::ArrowCursor);
    setToolTip(QString());
    update();
    QWidget::leaveEvent(event);
}

void TopKeysWidget::mousePressEvent(QMouseEvent* event)
{
    for (int i = 0; i < m_tabs.size(); ++i)
    {
        if (tabRect(i).contains(event->position()) && i != m_tab)
        {
            m_tab = i;
            emit tabChangeRequested(i);
            return;
        }
    }
    if (event->button() == Qt::LeftButton && m_hover >= 0 && m_hover < m_entries.size())
    {
        emit entryActivated(m_entries.at(m_hover).pid);
    }
    QWidget::mousePressEvent(event);
}

}  // namespace kc
