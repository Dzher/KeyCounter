// keyCounter - 概览卡片实现
#include "SummaryCardsWidget.h"

#include <QPainter>
#include <QPainterPath>

#include "ui/Theme.h"

namespace kc
{
namespace
{
constexpr double kGap = 12.0;
constexpr double kRadius = 12.0;
}  // namespace

SummaryCardsWidget::SummaryCardsWidget(QWidget* parent) : QWidget(parent)
{
    connect(&Theme::instance(), &Theme::changed, this, QOverload<>::of(&QWidget::update));
}

void SummaryCardsWidget::setCards(const QVector<SummaryCard>& cards)
{
    m_cards = cards;
    update();
}

QSize SummaryCardsWidget::sizeHint() const
{
    return QSize(4 * 180 + 3 * static_cast<int>(kGap), 96);
}

QSize SummaryCardsWidget::minimumSizeHint() const
{
    return QSize(320, 90);
}

void SummaryCardsWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (m_cards.isEmpty())
    {
        return;
    }
    const Palette& palette = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int count = m_cards.size();
    const double cardWidth = (static_cast<double>(width()) - kGap * (count - 1)) / count;
    const QRectF full(0, 0, cardWidth, static_cast<double>(height()));

    for (int i = 0; i < count; ++i)
    {
        const SummaryCard& card = m_cards.at(i);
        const QRectF box = full.translated(static_cast<double>(i) * (cardWidth + kGap), 0.0);
        Theme::drawCard(painter, box, palette, kRadius);

        // 顶部 2px 强调条
        painter.save();
        QPainterPath clip;
        clip.addRoundedRect(box, kRadius, kRadius);
        painter.setClipPath(clip);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::alpha(card.accent, 210));
        painter.drawRect(QRectF(box.left(), box.top(), box.width(), 2.0));
        painter.restore();

        const double padding = 16.0;
        painter.setPen(palette.textDim);
        painter.setFont(Theme::instance().font(12));
        painter.drawText(QRectF(box.left() + padding, box.top() + 12.0, box.width() - padding * 2, 16),
                         Qt::AlignLeft | Qt::AlignVCenter, card.caption);

        painter.setPen(palette.text);
        painter.setFont(Theme::instance().numberFont(26, QFont::DemiBold));
        painter.drawText(QRectF(box.left() + padding, box.top() + 32.0, box.width() - padding * 2, 34),
                         Qt::AlignLeft | Qt::AlignVCenter, card.value);

        if (!card.hint.isEmpty())
        {
            painter.setPen(palette.textFaint);
            painter.setFont(Theme::instance().font(11));
            painter.drawText(QRectF(box.left() + padding, box.bottom() - 28.0, box.width() - padding * 2, 16),
                             Qt::AlignLeft | Qt::AlignVCenter, card.hint);
        }
    }
}

}  // namespace kc
