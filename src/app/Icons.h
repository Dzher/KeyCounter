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

/// 托盘图标：键帽剪影 + 今日活跃度环
QPixmap trayIcon(const QColor& accent, double activityRatio, bool paused, int pixelSize);
/// 应用图标（窗口/任务栏）：键帽 + 三根上升柱
QPixmap appIcon(const QColor& accent, int pixelSize);

}  // namespace icons

}  // namespace kc
