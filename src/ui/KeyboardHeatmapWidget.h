// keyCounter - 热力虚拟键盘（自绘，静态层缓存 + 悬停高亮）
#pragma once

#include <QHash>
#include <QPixmap>
#include <QWidget>

#include <memory>

#include "core/KeyTypes.h"
#include "layout/LayoutModel.h"

namespace kc
{

class KeyboardHeatmapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit KeyboardHeatmapWidget(QWidget* parent = nullptr);

    void setKeyboardLayout(std::shared_ptr<const KeyboardLayout> layout);
    const KeyboardLayout* keyboardLayout() const noexcept { return m_layout.get(); }

    /// @param includeCorrections 叠加显示"被打错的键"（归因口径）
    void setStats(const QHash<quint16, KeyStat>& keys, quint64 totalPress, int rampKind, bool showCorrections);
    void clearStats();
    void setLegendVisible(bool visible);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /// 当前显示的热力上限（97 分位），供卡片/图例复用
    quint32 heatReference() const noexcept { return m_p97; }

signals:
    void keyActivated(quint16 pid);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void invalidateLayer();
    void rebuildGeometry();
    void renderLayer();
    int keyIndexAt(const QPointF& pos) const;
    void showTooltip(int index, const QPoint& globalPos);
    QString keyDisplayName(const KeyGeometry& key) const;

    std::shared_ptr<const KeyboardLayout> m_layout;
    QHash<quint16, KeyStat> m_stats;
    QVector<QRectF> m_keyRects;  ///< 与 m_layout->keys 一一对应（逻辑像素）
    QPixmap m_layer;
    quint64 m_totalPress = 0;
    quint32 m_p97 = 1;
    int m_ramp = 0;
    int m_hoverIndex = -1;
    double m_scale = 1.0;
    QPointF m_origin;
    bool m_legendVisible = true;
    bool m_showCorrections = true;
    bool m_geometryDirty = true;
    bool m_layerDirty = true;
};

}  // namespace kc
