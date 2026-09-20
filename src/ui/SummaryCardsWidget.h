// keyCounter - 概览卡片（本期键击 / 错误率 / 最高频键 / 日均）
#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

namespace kc
{

struct SummaryCard
{
    QString caption;
    QString value;
    QString hint;
    QColor accent;
};

class SummaryCardsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SummaryCardsWidget(QWidget* parent = nullptr);

    void setCards(const QVector<SummaryCard>& cards);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<SummaryCard> m_cards;
};

}  // namespace kc
