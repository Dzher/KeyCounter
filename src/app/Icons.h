// keyCounter - 运行时绘制的图标（不依赖图片资源，DPI 下永远锐利）
#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace kc
{

namespace icons
{

QIcon gear(const QColor& color, int size = 18);
QIcon pause(const QColor& color, int size = 18);
QIcon play(const QColor& color, int size = 18);
QIcon close(const QColor& color, int size = 18);
QIcon chevronLeft(const QColor& color, int size = 14);
QIcon data(const QColor& color, int size = 18);

/// 托盘图标：⌨️ emoji 键帽（Segoe UI Emoji，缺字体回退手绘键帽）+ 今日活跃度环
/// ringColor 无效时回退 accent；ringEnabled 为 false 时不绘制圆环
QPixmap trayIcon(const QColor& accent, double activityRatio, bool paused, int pixelSize, const QColor& ringColor,
                 bool ringEnabled);
/// 应用图标（窗口/任务栏）：键帽 + 三根上升柱
QPixmap appIcon(const QColor& accent, int pixelSize);

}  // namespace icons

}  // namespace kc
