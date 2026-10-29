// keyCounter - 图标绘制实现
#include "Icons.h"

#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <cmath>

namespace kc
{
namespace icons
{
namespace
{

constexpr double kPi = 3.14159265358979323846;

QPixmap makeCanvas(int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

/// 齿轮路径（8 齿方波轮廓 − 中心孔）
QPainterPath cogPath(const QPointF& center, double outer, double inner, int teeth) {
    QPainterPath path;
    const int steps = teeth * 4;
    for (int i = 0; i <= steps; ++i) {
        const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(steps) - kPi / 2.0;
        const double radius = ((i % 4) < 2) ? outer : inner;
        const QPointF point(center.x() + radius * std::cos(angle), center.y() + radius * std::sin(angle));
        if (i == 0) {
            path.moveTo(point);
        }
        else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();

    QPainterPath hole;
    hole.addEllipse(center, inner * 0.42, inner * 0.42);
    return path.subtracted(hole);
}

QPainterPath keycapPath(const QRectF& rect, double radius) {
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    return path;
}

/// 托盘键帽主体：README 标题同款 ⌨️ emoji（Segoe UI Emoji 彩色渲染）。
/// 4 倍超采样渲染后平滑缩小，16px 下依然干净；系统缺该字体时返回空图，由调用方回退手绘键帽。
QPixmap emojiKeycap(int pixelSize, bool grayscale) {
    static const bool hasEmojiFont = QFontDatabase().families().contains(QStringLiteral("Segoe UI Emoji"));
    if (!hasEmojiFont || pixelSize < 4) {
        return {};
    }

    constexpr int kSupersample = 4;
    const int renderSize = pixelSize * kSupersample;
    QImage image(renderSize, renderSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QFont font(QStringLiteral("Segoe UI Emoji"));
        font.setPixelSize(renderSize);
        painter.setFont(font);
        // 按字形墨迹包围盒精确居中：emoji 的 em 框留白不对称，直接 AlignCenter 会偏上
        const QString text = QStringLiteral("\u2328\uFE0F");
        const QFontMetricsF metrics(font);
        const QRectF ink = metrics.boundingRect(text);
        if (ink.isValid() && !ink.isEmpty()) {
            const QPointF baseline((renderSize - ink.width()) / 2.0 - ink.left(),
                                   (renderSize - ink.height()) / 2.0 - ink.top());
            painter.drawText(baseline, text);
        }
        else {
            painter.drawText(image.rect(), Qt::AlignCenter, text);
        }
    }

    if (grayscale) {
        QImage gray(renderSize, renderSize, QImage::Format_ARGB32);
        for (int y = 0; y < renderSize; ++y) {
            for (int x = 0; x < renderSize; ++x) {
                const QColor color = image.pixelColor(x, y);
                const int g = qGray(color.rgb());
                gray.setPixelColor(x, y, QColor(g, g, g, color.alpha()));
            }
        }
        image = gray;
    }

    return QPixmap::fromImage(image.scaled(pixelSize, pixelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

}  // namespace

QIcon gear(const QColor& color, int size) {
    QPixmap pixmap = makeCanvas(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const QRectF box(1.5, 1.5, size - 3.0, size - 3.0);
    painter.drawPath(cogPath(box.center(), box.width() * 0.5, box.width() * 0.34, 8));
    return QIcon(pixmap);
}

QIcon pause(const QColor& color, int size) {
    QPixmap pixmap = makeCanvas(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const double barWidth = size * 0.24;
    const double gap = size * 0.16;
    const double top = size * 0.18;
    const double height = size * 0.64;
    painter.drawRoundedRect(QRectF(size / 2.0 - gap / 2.0 - barWidth, top, barWidth, height), 1.5, 1.5);
    painter.drawRoundedRect(QRectF(size / 2.0 + gap / 2.0, top, barWidth, height), 1.5, 1.5);
    return QIcon(pixmap);
}

QIcon play(const QColor& color, int size) {
    QPixmap pixmap = makeCanvas(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    QPolygonF triangle;
    triangle << QPointF(size * 0.28, size * 0.18) << QPointF(size * 0.82, size * 0.5)
             << QPointF(size * 0.28, size * 0.82);
    painter.drawPolygon(triangle);
    return QIcon(pixmap);
}

QIcon close(const QColor& color, int size) {
    QPixmap pixmap = makeCanvas(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, qMax(1.4, size * 0.1));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    const double inset = size * 0.28;
    painter.drawLine(QPointF(inset, inset), QPointF(size - inset, size - inset));
    painter.drawLine(QPointF(size - inset, inset), QPointF(inset, size - inset));
    return QIcon(pixmap);
}

QIcon chevronLeft(const QColor& color, int size) {
    QPixmap pixmap = makeCanvas(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, qMax(1.4, size * 0.14));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    QPainterPath path;
    path.moveTo(QPointF(size * 0.62, size * 0.22));
    path.lineTo(QPointF(size * 0.34, size * 0.5));
    path.lineTo(QPointF(size * 0.62, size * 0.78));
    painter.drawPath(path);
    return QIcon(pixmap);
}

QIcon data(const QColor& color, int size) {
    QPixmap pixmap = makeCanvas(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const double barWidth = size * 0.16;
    const double baseY = size * 0.8;
    const double heights[3] = {0.28, 0.46, 0.64};
    for (int i = 0; i < 3; ++i) {
        const double x = size * 0.2 + static_cast<double>(i) * (barWidth * 1.7);
        painter.drawRoundedRect(QRectF(x, baseY - size * heights[i], barWidth, size * heights[i]), 1.5, 1.5);
    }
    return QIcon(pixmap);
}

QPixmap trayIcon(const QColor& accent, double activityRatio, bool paused, int pixelSize, const QColor& ringColor,
                 bool ringEnabled) {
    QPixmap pixmap = makeCanvas(pixelSize);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const double ratio = qBound(0.0, activityRatio, 1.0);
    const QRectF outer(0.5, 0.5, pixelSize - 1.0, pixelSize - 1.0);

    // 活跃度环（可在设置中关闭 / 固化颜色；无效颜色跟随主题强调色）
    if (ringEnabled && !paused && ratio > 0.001) {
        const QColor effectiveRing = ringColor.isValid() ? ringColor : accent;
        QPen ring(ratio > 1.0 ? QColor(0xE5, 0x48, 0x4D) : effectiveRing, qMax(1.5, pixelSize * 0.09));
        ring.setCapStyle(Qt::RoundCap);
        painter.setPen(ring);
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(outer.adjusted(pixelSize * 0.04, pixelSize * 0.04, -pixelSize * 0.04, -pixelSize * 0.04),
                        static_cast<int>(90 * 16), static_cast<int>(-360.0 * 16.0 * qMin(1.0, ratio)));
    }

    // 键帽主体：⌨️ emoji（README 标题同款），缺字体时回退手绘键帽。
    // 环关闭时键帽放大铺满画布；环开启时收到环内、与环轻微交叠（环先绘制、键帽压在上面）
    const int capSize = qRound(pixelSize * (ringEnabled ? 0.85 : 1.0));
    const QPixmap cap = emojiKeycap(capSize, paused);
    if (!cap.isNull()) {
        painter.drawPixmap(QPoint((pixelSize - capSize) / 2, (pixelSize - capSize) / 2), cap);
        return pixmap;
    }

    const QRectF capRect = outer.adjusted(pixelSize * 0.19, pixelSize * 0.17, -pixelSize * 0.19, -pixelSize * 0.24);
    painter.setPen(Qt::NoPen);
    painter.setBrush(paused ? QColor(0x8A, 0x91, 0x9E) : accent);
    painter.drawPath(keycapPath(capRect, pixelSize * 0.16));

    // 键脚
    painter.setBrush(paused ? QColor(0x6A, 0x73, 0x82) : accent.darker(125));
    painter.drawRoundedRect(QRectF(capRect.center().x() - pixelSize * 0.12, capRect.bottom() + pixelSize * 0.03,
                                   pixelSize * 0.24, pixelSize * 0.08),
                            pixelSize * 0.03, pixelSize * 0.03);
    return pixmap;
}

QPixmap appIcon(const QColor& accent, int pixelSize) {
    QPixmap pixmap = makeCanvas(pixelSize);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF background(0.5, 0.5, pixelSize - 1.0, pixelSize - 1.0);
    QLinearGradient gradient(background.topLeft(), background.bottomRight());
    gradient.setColorAt(0.0, accent.lighter(118));
    gradient.setColorAt(1.0, accent.darker(135));
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(background, pixelSize * 0.22, pixelSize * 0.22);

    // 键帽
    QPainterPath cap;
    cap.addRoundedRect(QRectF(pixelSize * 0.16, pixelSize * 0.16, pixelSize * 0.68, pixelSize * 0.5), pixelSize * 0.12,
                       pixelSize * 0.12);
    painter.setBrush(QColor(255, 255, 255, 232));
    painter.drawPath(cap);

    // 键帽内的三根上升柱
    painter.setBrush(accent.darker(140));
    const double baseY = pixelSize * 0.56;
    const double barWidth = pixelSize * 0.1;
    const double heights[3] = {0.12, 0.2, 0.28};
    for (int i = 0; i < 3; ++i) {
        const double x = pixelSize * 0.27 + static_cast<double>(i) * (barWidth * 1.5);
        painter.drawRoundedRect(QRectF(x, baseY - pixelSize * heights[i], barWidth, pixelSize * heights[i]), 1.0, 1.0);
    }

    // 键脚
    painter.setBrush(QColor(255, 255, 255, 200));
    painter.drawRoundedRect(QRectF(pixelSize * 0.4, pixelSize * 0.7, pixelSize * 0.2, pixelSize * 0.14),
                            pixelSize * 0.04, pixelSize * 0.04);
    return pixmap;
}

}  // namespace icons

}  // namespace kc
