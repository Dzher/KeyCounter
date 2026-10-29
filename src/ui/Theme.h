// keyCounter - 主题与绘制辅助（深色/浅色/跟随系统）
#pragma once

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QObject>
#include <QPainter>
#include <QRectF>

class QAbstractButton;
class QMenu;

namespace kc
{

struct Palette
{
    bool dark = true;
    QColor window;
    QColor surface;
    QColor surfaceAlt;
    QColor border;
    QColor keycap;
    QColor keycapEdge;
    QColor text;
    QColor textDim;
    QColor textFaint;
    QColor accent;
    QColor danger;
    QColor ok;
};

/// 全局主题单例：颜色、字体、绘制工具
class Theme : public QObject
{
    Q_OBJECT
public:
    enum Mode
    {
        System = 0,
        Dark = 1,
        Light = 2,
    };

    static Theme& instance();

    const Palette& palette() const noexcept { return m_palette; }
    bool isDark() const noexcept { return m_palette.dark; }
    int mode() const noexcept { return m_mode; }
    void setMode(int mode);

    QFont font(int pixelSize, QFont::Weight weight = QFont::Normal) const;
    QFont numberFont(int pixelSize, QFont::Weight weight = QFont::DemiBold) const;

    /// 圆角卡片：1 px 描边 + 顶部微高光（不用阴影，零性能代价）
    static void drawCard(QPainter& painter, const QRectF& rect, const Palette& palette, qreal radius = 12.0,
                         bool raised = false);

    static QColor blend(const QColor& a, const QColor& b, double t);
    static QColor alpha(const QColor& color, int a);
    /// 依据背景亮度返回可读的文字色
    static QColor readableText(const QColor& background, const QColor& light, const QColor& dark);
    static double luminance(const QColor& color);

    /// 给标准控件（对话框等）套上统一暗色/浅色外观
    void applyToWidget(QWidget* widget) const;

    /// 给弹出菜单（托盘右键菜单等顶层 QMenu）套上主题；递归应用到已有子菜单
    void applyToMenu(QMenu* menu) const;

signals:
    void changed();

private:
    Theme();
    void rebuild();
    /// 把调色板与弹窗样式表应用到 qApp：顶层弹窗（QMessageBox / 非原生 QFileDialog / QToolTip）
    /// 不继承主界面的样式表，不补主题就会出现「浅底 + 浅色文字」
    void applyToApplication() const;
    static QColor systemAccent();

    Palette m_palette;
    int m_mode = System;
    QString m_family;
};

/// 圆角卡片容器（自绘背景，内部可放任意布局）
class CardPanel : public QFrame
{
public:
    explicit CardPanel(QWidget* parent = nullptr, qreal radius = 12.0, bool raised = false);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    qreal m_radius;
    bool m_raised;
};

}  // namespace kc
