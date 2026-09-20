// keyCounter - 周期分段控件实现
#include "PeriodSelector.h"

#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>

#include "ui/Theme.h"

namespace kc
{
namespace
{
constexpr double kPadding = 3.0;
constexpr double kRadius = 9.0;
}  // namespace

PeriodSelector::PeriodSelector(QWidget* parent) : QWidget(parent)
{
    m_labels << QStringLiteral("今天") << QStringLiteral("本周") << QStringLiteral("本月") << QStringLiteral("今年")
             << QStringLiteral("累计");
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(34);
    m_animation = new QVariantAnimation(this);
    m_animation->setDuration(180);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value)
            {
                m_indicatorX = value.toDouble();
                update();
            });
    connect(&Theme::instance(), &Theme::changed, this, QOverload<>::of(&QWidget::update));
}

void PeriodSelector::setReduceMotion(bool reduce)
{
    m_reduceMotion = reduce;
    m_animation->setDuration(reduce ? 0 : 180);
}

QSize PeriodSelector::sizeHint() const
{
    const int width = 5 * 62 + static_cast<int>(kPadding * 2);
    return QSize(width, 34);
}

QRectF PeriodSelector::segmentRect(int index) const
{
    const double count = static_cast<double>(m_labels.size());
    const double width = (static_cast<double>(this->width()) - 2.0 * kPadding) / count;
    return QRectF(kPadding + width * index, kPadding, width, static_cast<double>(height()) - 2.0 * kPadding);
}

int PeriodSelector::indexAt(const QPointF& pos) const
{
    for (int i = 0; i < m_labels.size(); ++i)
    {
        if (segmentRect(i).contains(pos))
        {
            return i;
        }
    }
    return -1;
}

void PeriodSelector::setCurrentKind(int kind)
{
    const int bounded = qBound(0, kind, m_labels.size() - 1);
    if (bounded == m_current)
    {
        m_indicatorX = segmentRect(bounded).x();
        update();
        return;
    }
    m_current = bounded;
    const double target = segmentRect(m_current).x();
    if (m_reduceMotion || !isVisible())
    {
        m_indicatorX = target;
    }
    else
    {
        m_animation->stop();
        m_animation->setStartValue(m_indicatorX);
        m_animation->setEndValue(target);
        m_animation->start();
    }
    update();
}

void PeriodSelector::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const Palette& palette = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF background = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.surfaceAlt);
    painter.drawRoundedRect(background, kRadius + kPadding, kRadius + kPadding);

    const QRectF indicator = segmentRect(m_current);
    painter.setBrush(Theme::alpha(palette.accent, palette.dark ? 60 : 46));
    painter.setPen(QPen(Theme::alpha(palette.accent, 150), 1.0));
    painter.drawRoundedRect(indicator.translated(m_indicatorX - indicator.x(), 0.0), kRadius, kRadius);

    for (int i = 0; i < m_labels.size(); ++i)
    {
        const bool active = (i == m_current);
        QColor color = active ? palette.text : palette.textDim;
        if (!active && i == m_hover)
        {
            color = palette.text;
        }
        painter.setPen(color);
        painter.setFont(Theme::instance().font(13, active ? QFont::DemiBold : QFont::Normal));
        painter.drawText(segmentRect(i), Qt::AlignCenter, m_labels.at(i));
    }
}

void PeriodSelector::mousePressEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position());
    if (index >= 0 && index != m_current)
    {
        setCurrentKind(index);
        emit kindChanged(index);
    }
    QWidget::mousePressEvent(event);
}

void PeriodSelector::mouseMoveEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position());
    if (index != m_hover)
    {
        m_hover = index;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void PeriodSelector::leaveEvent(QEvent* event)
{
    m_hover = -1;
    update();
    QWidget::leaveEvent(event);
}

}  // namespace kc
