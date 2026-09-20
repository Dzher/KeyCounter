// keyCounter - 周期分段控件（今天/本周/本月/今年/累计）
#pragma once

#include <QStringList>
#include <QWidget>

class QVariantAnimation;

namespace kc
{

class PeriodSelector : public QWidget
{
    Q_OBJECT
public:
    explicit PeriodSelector(QWidget* parent = nullptr);

    void setCurrentKind(int kind);
    int currentKind() const noexcept { return m_current; }
    void setReduceMotion(bool reduce);

    QSize sizeHint() const override;

signals:
    void kindChanged(int kind);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF segmentRect(int index) const;
    int indexAt(const QPointF& pos) const;

    QStringList m_labels;
    int m_current = 0;
    int m_hover = -1;
    double m_indicatorX = 0.0;
    QVariantAnimation* m_animation = nullptr;
    bool m_reduceMotion = false;
};

}  // namespace kc
