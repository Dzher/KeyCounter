// keyCounter - 热力虚拟键盘实现
#include "KeyboardHeatmapWidget.h"

#include <QFontMetricsF>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include "core/KeyCatalog.h"
#include "ui/HeatRamp.h"
#include "ui/Theme.h"

namespace kc
{
namespace
{

constexpr double kOuterMargin = 6.0;
constexpr double kLegendHeight = 34.0;
constexpr double kMaxRadius = 7.0;
constexpr int kMinLabelPx = 8;
constexpr int kMaxLabelPx = 17;

QString formatCount(quint64 value)
{
    return QLocale::system().toString(static_cast<qlonglong>(value));
}

/// 键帽内可用的文本宽度（左右各留约 8% 呼吸空间）
double labelWidth(const QRectF& cell)
{
    return qMax(8.0, cell.width() - qMax(4.0, cell.width() * 0.16));
}

/// 挑选键帽字号：先按键高取上限，再按文本宽度收缩。
/// 小键盘 Enter 这种「窄而高」的键只按高度算会得到 17 px，足以让 5 个字符溢出键帽。
int fitLabelPx(const QString& text, const QRectF& cell, QFont::Weight weight, int maxPx = kMaxLabelPx)
{
    const int byHeight = qBound(kMinLabelPx, static_cast<int>(cell.height() * 0.30), maxPx);
    if (text.isEmpty() || cell.width() <= 0.0)
    {
        return byHeight;
    }

    const double available = labelWidth(cell);
    int px = byHeight;
    for (int attempt = 0; attempt < 3 && px > kMinLabelPx; ++attempt)
    {
        const QFontMetricsF metrics(Theme::instance().font(px, weight));
        const double advance = metrics.horizontalAdvance(text);
        if (advance <= available)
        {
            break;
        }
        px = qMax(kMinLabelPx, static_cast<int>(px * available / advance));
    }
    return px;
}

}  // namespace

KeyboardHeatmapWidget::KeyboardHeatmapWidget(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumHeight(200);
    connect(&Theme::instance(), &Theme::changed, this,
            [this]
            {
                invalidateLayer();
                update();
            });
}

void KeyboardHeatmapWidget::setKeyboardLayout(std::shared_ptr<const KeyboardLayout> layout)
{
    m_layout = std::move(layout);
    m_geometryDirty = true;
    m_layerDirty = true;
    m_hoverIndex = -1;
    updateGeometry();
    update();
}

void KeyboardHeatmapWidget::setStats(const QHash<quint16, KeyStat>& keys, quint64 totalPress, int rampKind,
                                     bool showCorrections)
{
    m_stats = keys;
    m_totalPress = totalPress;
    m_ramp = rampKind;
    m_showCorrections = showCorrections;
    m_p97 = qMax<quint32>(1, HeatRamp::percentile(keys, 0.97));
    m_layerDirty = true;
    update();
}

void KeyboardHeatmapWidget::clearStats()
{
    m_stats.clear();
    m_totalPress = 0;
    m_p97 = 1;
    m_layerDirty = true;
    update();
}

void KeyboardHeatmapWidget::setLegendVisible(bool visible)
{
    if (m_legendVisible == visible)
    {
        return;
    }
    m_legendVisible = visible;
    m_geometryDirty = true;
    m_layerDirty = true;
    update();
}

QSize KeyboardHeatmapWidget::sizeHint() const
{
    if (!m_layout)
    {
        return QSize(760, 300);
    }
    const double unit = 36.0;
    return QSize(static_cast<int>(m_layout->widthU * unit) + 40,
                 static_cast<int>(m_layout->heightU * unit) + 40 + static_cast<int>(kLegendHeight));
}

QSize KeyboardHeatmapWidget::minimumSizeHint() const
{
    if (!m_layout)
    {
        return QSize(420, 180);
    }
    return QSize(static_cast<int>(m_layout->widthU * 17.0), static_cast<int>(m_layout->heightU * 17.0) + 30);
}

void KeyboardHeatmapWidget::invalidateLayer()
{
    m_layerDirty = true;
    m_geometryDirty = true;
}

void KeyboardHeatmapWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_geometryDirty = true;
    m_layerDirty = true;
}

void KeyboardHeatmapWidget::rebuildGeometry()
{
    m_keyRects.clear();
    if (!m_layout || m_layout->keys.isEmpty() || width() <= 0 || height() <= 0)
    {
        m_geometryDirty = false;
        return;
    }

    const double legendSpace = m_legendVisible ? kLegendHeight : 0.0;
    const double availWidth = qMax(1.0, width() - 2.0 * kOuterMargin);
    const double availHeight = qMax(1.0, height() - 2.0 * kOuterMargin - legendSpace);

    m_scale = qMin(availWidth / m_layout->widthU, availHeight / m_layout->heightU);
    const double boardWidth = m_scale * m_layout->widthU;
    const double boardHeight = m_scale * m_layout->heightU;
    m_origin = QPointF((width() - boardWidth) / 2.0, kOuterMargin + (availHeight - boardHeight) / 2.0);

    m_keyRects.reserve(m_layout->keys.size());
    for (const KeyGeometry& key : m_layout->keys)
    {
        m_keyRects.append(QRectF(m_origin.x() + key.cell.x() * m_scale, m_origin.y() + key.cell.y() * m_scale,
                                 key.cell.width() * m_scale, key.cell.height() * m_scale));
    }
    m_geometryDirty = false;
}

int KeyboardHeatmapWidget::keyIndexAt(const QPointF& pos) const
{
    if (!m_layout)
    {
        return -1;
    }
    for (int i = 0; i < m_keyRects.size(); ++i)
    {
        if (m_keyRects.at(i).contains(pos))
        {
            return i;
        }
    }
    return -1;
}

QString KeyboardHeatmapWidget::keyDisplayName(const KeyGeometry& key) const
{
    const QString label = key.label.trimmed();
    if (!label.isEmpty())
    {
        return label;
    }
    return QString::fromLatin1(keyInfo(key.pid).name);
}

void KeyboardHeatmapWidget::renderLayer()
{
    const Palette& palette = Theme::instance().palette();
    const qreal dpr = devicePixelRatioF();

    m_layer = QPixmap(QSize(qMax(1, static_cast<int>(width() * dpr)), qMax(1, static_cast<int>(height() * dpr))));
    m_layer.setDevicePixelRatio(dpr);
    m_layer.fill(Qt::transparent);

    QPainter painter(&m_layer);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (!m_layout || m_keyRects.isEmpty())
    {
        painter.setPen(palette.textFaint);
        painter.setFont(Theme::instance().font(14));
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("暂无数据\n开始打字即可统计 · 数据仅保存在本机"));
        m_layerDirty = false;
        return;
    }

    const double keyGap = qMax(1.0, m_layout->gapU * m_scale);
    const QLocale locale = QLocale::system();

    // 第一遍：键帽底 + 热力填充
    for (int i = 0; i < m_layout->keys.size(); ++i)
    {
        const KeyGeometry& key = m_layout->keys.at(i);
        const QRectF cell = m_keyRects.at(i).adjusted(keyGap / 2.0, keyGap / 2.0, -keyGap / 2.0, -keyGap / 2.0);
        const double radius = qMin(kMaxRadius, cell.height() * 0.22);
        const KeyStat stat = m_stats.value(key.pid);
        const bool heat = key.countable && stat.press > 0;

        if (heat)
        {
            const QColor color = HeatRamp::colorFor(HeatRamp::normalize(stat.press, m_p97), m_ramp);
            QLinearGradient gradient(cell.topLeft(), cell.bottomLeft());
            gradient.setColorAt(0.0, color.lighter(112));
            gradient.setColorAt(1.0, color.darker(112));
            painter.setBrush(gradient);
            painter.setPen(QPen(color.darker(150), 1.0));
        }
        else
        {
            QLinearGradient gradient(cell.topLeft(), cell.bottomLeft());
            const QColor base = key.countable ? palette.keycap : palette.keycap.darker(112);
            gradient.setColorAt(0.0, base.lighter(palette.dark ? 112 : 100));
            gradient.setColorAt(1.0, base.darker(palette.dark ? 110 : 104));
            painter.setBrush(gradient);
            painter.setPen(QPen(palette.keycapEdge, 1.0));
        }
        painter.drawRoundedRect(cell.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

        // 被打错过的键：右上角红色小点（归因口径）
        if (m_showCorrections && key.countable && stat.corr > 0 && cell.width() > 18.0)
        {
            const double dot = qBound(3.0, cell.height() * 0.1, 5.0);
            const QRectF dotRect(cell.right() - dot - 2.0, cell.top() + 2.0, dot, dot);
            painter.setPen(Qt::NoPen);
            painter.setBrush(palette.danger);
            painter.drawEllipse(dotRect);
        }
    }

    // 第二遍：文字（避免被后画的键帽覆盖）
    for (int i = 0; i < m_layout->keys.size(); ++i)
    {
        const KeyGeometry& key = m_layout->keys.at(i);
        const QRectF cell = m_keyRects.at(i).adjusted(keyGap / 2.0, keyGap / 2.0, -keyGap / 2.0, -keyGap / 2.0);
        if (cell.height() < 12.0 || cell.width() < 10.0)
        {
            continue;
        }
        const KeyStat stat = m_stats.value(key.pid);
        const bool heat = key.countable && stat.press > 0;

        QColor labelColor = key.countable ? palette.text : palette.textFaint;
        if (heat)
        {
            const QColor fill = HeatRamp::colorFor(HeatRamp::normalize(stat.press, m_p97), m_ramp);
            labelColor = Theme::readableText(fill, palette.text, QColor(0x0D, 0x0F, 0x14));
        }

        const QString label = keyDisplayName(key);
        const QFont::Weight weight = heat ? QFont::DemiBold : QFont::Normal;
        const int fontPx = fitLabelPx(label, cell, weight);
        QFont font = Theme::instance().font(fontPx, weight);
        painter.setFont(font);
        painter.setPen(labelColor);

        const bool showSub = !key.sub.isEmpty() && cell.width() > 26.0 && cell.height() > 26.0;
        QRectF textRect = cell;
        if (showSub)
        {
            textRect.setTop(cell.top() + cell.height() * 0.16);
            painter.setFont(Theme::instance().font(qMax(8, fontPx - 3)));
            painter.setPen(heat ? Theme::alpha(labelColor, 200) : palette.textFaint);
            painter.drawText(QRectF(cell.left() + 4.0, cell.top() + 1.0, cell.width() - 6.0, cell.height() * 0.34),
                             Qt::AlignRight | Qt::AlignTop, key.sub);
            painter.setFont(font);
            painter.setPen(labelColor);
        }
        // 极端窄键兜底：字号已到下限仍放不下时省略
        const QFontMetricsF labelMetrics(font);
        const double textSpace = qMax(4.0, textRect.width() - 4.0);
        const QString drawn = labelMetrics.horizontalAdvance(label) > textSpace
                                  ? labelMetrics.elidedText(label, Qt::ElideRight, textSpace)
                                  : label;
        painter.drawText(textRect, Qt::AlignCenter, drawn);

        // 次数：键够大且已计数时显示
        if (heat && cell.width() > 34.0 && cell.height() > 34.0)
        {
            painter.setFont(Theme::instance().numberFont(qMax(8, fontPx - 4)));
            painter.setPen(Theme::alpha(labelColor, 190));
            painter.drawText(QRectF(cell.left(), cell.bottom() - cell.height() * 0.36, cell.width(), cell.height() * 0.3),
                             Qt::AlignHCenter | Qt::AlignBottom, locale.toString(stat.press));
        }
    }

    // 图例
    if (m_legendVisible)
    {
        const double barY = height() - kLegendHeight + 6.0;
        const double barX = kOuterMargin + 2.0;
        const double barW = qMax(60.0, width() - 2.0 * barX - 150.0);
        const double barH = 9.0;

        QLinearGradient gradient(barX, 0, barX + barW, 0);
        const QVector<QPair<double, QColor>> stops = HeatRamp::stops(m_ramp);
        for (const auto& stop : stops)
        {
            gradient.setColorAt(qBound(0.0, stop.first, 1.0), stop.second);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawRoundedRect(QRectF(barX, barY, barW, barH), barH / 2.0, barH / 2.0);

        painter.setFont(Theme::instance().font(11));
        painter.setPen(palette.textFaint);
        painter.drawText(QRectF(barX, barY + barH + 2.0, 60.0, 16.0), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("0（无色）"));
        painter.drawText(QRectF(barX + barW - 70.0, barY + barH + 2.0, 70.0, 16.0), Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("%1+").arg(locale.toString(m_p97)));
        painter.drawText(QRectF(barX, barY - 14.0, barW, 14.0), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("对数刻度 · 数值越高越红"));
        painter.setPen(palette.textDim);
        painter.drawText(QRectF(barX + barW + 10.0, barY - 6.0, 140.0, 20.0), Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("本期 %1 次键击").arg(formatCount(m_totalPress)));
    }

    m_layerDirty = false;
}

void KeyboardHeatmapWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (m_geometryDirty)
    {
        rebuildGeometry();
    }
    const QSize scaledSize(qRound(width() * devicePixelRatioF()), qRound(height() * devicePixelRatioF()));
    if (m_layerDirty || m_layer.size() != scaledSize)
    {
        renderLayer();
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawPixmap(0, 0, m_layer);

    if (m_hoverIndex >= 0 && m_hoverIndex < m_keyRects.size() && m_layout)
    {
        const Palette& palette = Theme::instance().palette();
        const double keyGap = qMax(1.0, m_layout->gapU * m_scale);
        const QRectF rect = m_keyRects.at(m_hoverIndex).adjusted(keyGap / 2.0 - 1.0, keyGap / 2.0 - 1.0, -keyGap / 2.0 + 1.0,
                                                             -keyGap / 2.0 + 1.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(palette.accent, 2.0));
        painter.drawRoundedRect(rect, qMin(kMaxRadius, rect.height() * 0.22), qMin(kMaxRadius, rect.height() * 0.22));
    }
}

void KeyboardHeatmapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_geometryDirty)
    {
        rebuildGeometry();
    }
    const int index = keyIndexAt(event->position());
    if (index != m_hoverIndex)
    {
        const int previous = m_hoverIndex;
        m_hoverIndex = index;
        if (previous >= 0 && previous < m_keyRects.size())
        {
            update(m_keyRects.at(previous).adjusted(-3, -3, 3, 3).toAlignedRect());
        }
        if (m_hoverIndex >= 0)
        {
            update(m_keyRects.at(m_hoverIndex).adjusted(-3, -3, 3, 3).toAlignedRect());
        }
        setCursor(m_hoverIndex >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    if (m_hoverIndex >= 0)
    {
        showTooltip(m_hoverIndex, event->globalPosition().toPoint());
    }
    else
    {
        QToolTip::hideText();
    }
    QWidget::mouseMoveEvent(event);
}

void KeyboardHeatmapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }
    if (m_geometryDirty)
    {
        rebuildGeometry();
    }
    const int index = keyIndexAt(event->position());
    if (index >= 0 && m_layout)
    {
        emit keyActivated(m_layout->keys.at(index).pid);
    }
    QWidget::mousePressEvent(event);
}

void KeyboardHeatmapWidget::leaveEvent(QEvent* event)
{
    if (m_hoverIndex >= 0 && m_hoverIndex < m_keyRects.size())
    {
        update(m_keyRects.at(m_hoverIndex).adjusted(-3, -3, 3, 3).toAlignedRect());
    }
    m_hoverIndex = -1;
    QToolTip::hideText();
    setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(event);
}

void KeyboardHeatmapWidget::showTooltip(int index, const QPoint& globalPos)
{
    if (!m_layout || index < 0 || index >= m_layout->keys.size())
    {
        return;
    }
    const KeyGeometry& key = m_layout->keys.at(index);
    const Palette& palette = Theme::instance().palette();
    const KeyStat stat = m_stats.value(key.pid);
    const QLocale locale = QLocale::system();

    QString text = QStringLiteral("<b>%1</b>").arg(keyDisplayName(key).toHtmlEscaped());
    if (!key.countable)
    {
        text += QStringLiteral("<br/>不由系统上报，无法统计");
    }
    else
    {
        text += QStringLiteral("<br/>按下 %1 次").arg(locale.toString(static_cast<qlonglong>(stat.press)));
        if (m_totalPress > 0 && stat.press > 0)
        {
            text += QStringLiteral(" · 占比 %1%").arg(static_cast<double>(stat.press) * 100.0 / static_cast<double>(m_totalPress), 0,
                                                   'f', 2);
        }
        if (stat.corr > 0)
        {
            text += QStringLiteral("<br/><span style='color:%1'>打错修正 %2 次</span>")
                        .arg(palette.danger.name())
                        .arg(stat.corr);
        }
    }
    QToolTip::showText(globalPos + QPoint(12, 12), text, this);
}

}  // namespace kc
