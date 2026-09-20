// keyCounter - 主题实现
#include "Theme.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>
#include <QWidget>

#include <cmath>

namespace kc
{
namespace
{

QString pickFamily()
{
    const QStringList preferred = {QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
                                   QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei")};
    const QStringList families = QFontDatabase::families();
    for (const QString& candidate : preferred)
    {
        if (families.contains(candidate, Qt::CaseInsensitive))
        {
            return candidate;
        }
    }
    return QString();
}

double channelToLinear(int c)
{
    const double s = static_cast<double>(c) / 255.0;
    return (s <= 0.04045) ? (s / 12.92) : std::pow((s + 0.055) / 1.055, 2.4);
}

}  // namespace

Theme& Theme::instance()
{
    static Theme theme;
    return theme;
}

CardPanel::CardPanel(QWidget* parent, qreal radius, bool raised)
    : QFrame(parent), m_radius(radius), m_raised(raised)
{
    setAttribute(Qt::WA_StyledBackground, false);
}

void CardPanel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    Theme::drawCard(painter, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Theme::instance().palette(), m_radius,
                    m_raised);
}

Theme::Theme() : m_family(pickFamily())
{
    if (QGuiApplication::styleHints() != nullptr)
    {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { setMode(m_mode); });
    }
    rebuild();
}

QColor Theme::systemAccent()
{
    // 1) 注册表里的用户强调色（ABGR）
    QSettings dwm(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM"), QSettings::NativeFormat);
    const QVariant accent = dwm.value(QStringLiteral("AccentColor"));
    if (accent.isValid() && accent.toUInt() != 0)
    {
        const quint32 value = accent.toUInt();
        const int r = static_cast<int>(value & 0xFF);
        const int g = static_cast<int>((value >> 8) & 0xFF);
        const int b = static_cast<int>((value >> 16) & 0xFF);
        if (r + g + b > 60)
        {
            return QColor(r, g, b);
        }
    }
    // 2) 回退：中性蓝
    return QColor(0x4C, 0x8D, 0xFF);
}

void Theme::setMode(int mode)
{
    m_mode = (mode >= 0 && mode <= 2) ? mode : System;
    rebuild();
    emit changed();
}

void Theme::rebuild()
{
    bool dark = true;
    switch (m_mode)
    {
        case Dark:
            dark = true;
            break;
        case Light:
            dark = false;
            break;
        default:
            dark = (QGuiApplication::styleHints() != nullptr) &&
                   (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
            break;
    }

    Palette p;
    p.dark = dark;
    // 强调色统一微调，避免注册表里过饱和的取值显得刺眼
    QColor accent = systemAccent();
    accent = QColor::fromHsl(accent.hslHue(), qBound(120, accent.hslSaturation(), 235), qBound(130, accent.lightness(), 215));

    if (dark)
    {
        p.window = QColor(0x0F, 0x11, 0x16);
        p.surface = QColor(0x17, 0x1A, 0x21);
        p.surfaceAlt = QColor(0x1D, 0x21, 0x29);
        p.border = QColor(0x24, 0x28, 0x33);
        p.keycap = QColor(0x23, 0x26, 0x2E);
        p.keycapEdge = QColor(0x2E, 0x32, 0x3C);
        p.text = QColor(0xE6, 0xE9, 0xF0);
        p.textDim = QColor(0x99, 0xA2, 0xB0);
        p.textFaint = QColor(0x6A, 0x73, 0x82);
        p.accent = accent.lighter(115);
        p.danger = QColor(0xE5, 0x48, 0x4D);
        p.ok = QColor(0x2B, 0xB6, 0x73);
    }
    else
    {
        p.window = QColor(0xF4, 0xF5, 0xF8);
        p.surface = QColor(0xFF, 0xFF, 0xFF);
        p.surfaceAlt = QColor(0xEC, 0xEE, 0xF3);
        p.border = QColor(0xDD, 0xE0, 0xE7);
        p.keycap = QColor(0xF7, 0xF8, 0xFA);
        p.keycapEdge = QColor(0xD3, 0xD7, 0xDE);
        p.text = QColor(0x1A, 0x1D, 0x24);
        p.textDim = QColor(0x55, 0x5C, 0x69);
        p.textFaint = QColor(0x8A, 0x91, 0x9E);
        p.accent = accent.darker(115);
        p.danger = QColor(0xD1, 0x2C, 0x31);
        p.ok = QColor(0x1E, 0x9E, 0x62);
    }
    m_palette = p;
}

QFont Theme::font(int pixelSize, QFont::Weight weight) const
{
    QFont f = m_family.isEmpty() ? QFont() : QFont(m_family);
    f.setPixelSize(pixelSize);
    f.setWeight(weight);
    f.setHintingPreference(QFont::PreferFullHinting);
    return f;
}

QFont Theme::numberFont(int pixelSize, QFont::Weight weight) const
{
    QFont f = font(pixelSize, weight);
    f.setFeature(QFont::Tag("tnum"), 1);  // 等宽数字，数值跳动不抖
    return f;
}

void Theme::drawCard(QPainter& painter, const QRectF& rect, const Palette& palette, qreal radius, bool raised)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(raised ? palette.surfaceAlt : palette.surface);
    painter.drawRoundedRect(rect, radius, radius);

    // 顶部微高光
    QLinearGradient highlight(rect.topLeft(), QPointF(rect.left(), rect.top() + rect.height() * 0.5));
    highlight.setColorAt(0.0, alpha(palette.dark ? QColor(Qt::white) : QColor(Qt::black), palette.dark ? 12 : 6));
    highlight.setColorAt(1.0, Qt::transparent);
    painter.setBrush(highlight);
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    painter.drawPath(path);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(palette.border, 1.0));
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}

QColor Theme::blend(const QColor& a, const QColor& b, double t)
{
    const double clamped = qBound(0.0, t, 1.0);
    return QColor(static_cast<int>(std::lround(a.red() + (b.red() - a.red()) * clamped)),
                  static_cast<int>(std::lround(a.green() + (b.green() - a.green()) * clamped)),
                  static_cast<int>(std::lround(a.blue() + (b.blue() - a.blue()) * clamped)),
                  static_cast<int>(std::lround(a.alpha() + (b.alpha() - a.alpha()) * clamped)));
}

QColor Theme::alpha(const QColor& color, int a)
{
    QColor copy = color;
    copy.setAlpha(qBound(0, a, 255));
    return copy;
}

double Theme::luminance(const QColor& color)
{
    return 0.2126 * channelToLinear(color.red()) + 0.7152 * channelToLinear(color.green()) +
           0.0722 * channelToLinear(color.blue());
}

QColor Theme::readableText(const QColor& background, const QColor& light, const QColor& dark)
{
    return luminance(background) > 0.36 ? dark : light;
}

void Theme::applyToWidget(QWidget* widget) const
{
    if (widget == nullptr)
    {
        return;
    }
    const Palette& p = m_palette;
    QPalette qp = widget->palette();
    qp.setColor(QPalette::Window, p.window);
    qp.setColor(QPalette::WindowText, p.text);
    qp.setColor(QPalette::Base, p.surface);
    qp.setColor(QPalette::AlternateBase, p.surfaceAlt);
    qp.setColor(QPalette::Text, p.text);
    qp.setColor(QPalette::Button, p.surfaceAlt);
    qp.setColor(QPalette::ButtonText, p.text);
    qp.setColor(QPalette::ToolTipBase, p.surfaceAlt);
    qp.setColor(QPalette::ToolTipText, p.text);
    qp.setColor(QPalette::Highlight, p.accent);
    qp.setColor(QPalette::HighlightedText, p.dark ? QColor(0x0F, 0x11, 0x16) : Qt::white);
    qp.setColor(QPalette::PlaceholderText, p.textFaint);
    widget->setPalette(qp);
    widget->setFont(font(13));

    widget->setStyleSheet(
        QStringLiteral("QTabWidget::pane{border:1px solid %1;border-radius:8px;top:-1px;}"
                       "QTabBar::tab{background:transparent;color:%2;padding:6px 14px;border-radius:6px;}"
                       "QTabBar::tab:selected{background:%3;color:%4;}"
                       "QComboBox,QSpinBox,QKeySequenceEdit{background:%5;border:1px solid %1;border-radius:6px;"
                       "padding:4px 8px;color:%4;}"
                       "QComboBox QAbstractItemView{background:%5;color:%4;selection-background-color:%3;"
                       "border:1px solid %1;}"
                       "QPushButton{background:%5;border:1px solid %1;border-radius:6px;padding:6px 14px;color:%4;}"
                       "QPushButton:hover{border-color:%6;}"
                       "QPushButton:disabled{color:%7;}"
                       "QCheckBox,QRadioButton{color:%4;spacing:8px;}"
                       "QGroupBox{border:1px solid %1;border-radius:8px;margin-top:14px;padding-top:10px;color:%2;}"
                       "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 4px;}"
                       "QScrollArea{background:transparent;border:none;}"
                       "QLabel{color:%4;}"
                       "QToolTip{background:%5;color:%4;border:1px solid %1;}")
            .arg(p.border.name(), p.textDim.name(), p.surfaceAlt.name(), p.text.name(), p.surface.name(),
                 p.accent.name(), p.textFaint.name()));
}

}  // namespace kc
