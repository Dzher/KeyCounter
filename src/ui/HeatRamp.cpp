// keyCounter - 热力色带实现（sRGB ↔ OKLab）
#include "HeatRamp.h"

#include <QVector>

#include <algorithm>
#include <cmath>

namespace kc
{
namespace
{

struct Stop
{
    double t;
    QColor color;
};

const QVector<Stop>& emberStops()
{
    static const QVector<Stop> stops = {
        {0.00, QColor(0x12, 0x3A, 0x5E)}, {0.14, QColor(0x1C, 0x7F, 0xA8)}, {0.32, QColor(0x2B, 0xB6, 0x73)},
        {0.50, QColor(0xC9, 0xD9, 0x3B)}, {0.68, QColor(0xF2, 0xA9, 0x3B)}, {0.85, QColor(0xE5, 0x48, 0x4D)},
        {1.00, QColor(0x8C, 0x1C, 0x2B)},
    };
    return stops;
}

const QVector<Stop>& infernoStops()
{
    static const QVector<Stop> stops = {
        {0.00, QColor(0x2A, 0x12, 0x20)}, {0.30, QColor(0x7A, 0x1F, 0x3D)}, {0.60, QColor(0xC4, 0x3C, 0x39)},
        {0.85, QColor(0xEE, 0x7A, 0x2E)}, {1.00, QColor(0xFF, 0xC8, 0x5C)},
    };
    return stops;
}

const QVector<Stop>& colorblindStops()
{
    static const QVector<Stop> stops = {
        {0.00, QColor(0x1B, 0x3A, 0x6B)}, {0.33, QColor(0x4E, 0xA5, 0xD9)}, {0.62, QColor(0xF2, 0xE1, 0x4C)},
        {0.85, QColor(0xE4, 0x57, 0x2E)}, {1.00, QColor(0x8C, 0x1C, 0x2B)},
    };
    return stops;
}

const QVector<Stop>& stopsFor(int kind)
{
    switch (kind)
    {
        case HeatRamp::Inferno:
            return infernoStops();
        case HeatRamp::Colorblind:
            return colorblindStops();
        default:
            return emberStops();
    }
}

double toLinear(double channel)
{
    return (channel <= 0.04045) ? (channel / 12.92) : std::pow((channel + 0.055) / 1.055, 2.4);
}

double fromLinear(double channel)
{
    const double c = qBound(0.0, channel, 1.0);
    return (c <= 0.0031308) ? (c * 12.92) : (1.055 * std::pow(c, 1.0 / 2.4) - 0.055);
}

void toOklab(const QColor& color, double& L, double& a, double& b)
{
    const double r = toLinear(color.redF());
    const double g = toLinear(color.greenF());
    const double bl = toLinear(color.blueF());

    const double l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * bl;
    const double m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * bl;
    const double s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * bl;

    const double l_ = std::cbrt(l);
    const double m_ = std::cbrt(m);
    const double s_ = std::cbrt(s);

    L = 0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_;
    a = 1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_;
    b = 0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_;
}

QColor fromOklab(double L, double a, double b)
{
    const double l_ = L + 0.3963377774 * a + 0.2158037573 * b;
    const double m_ = L - 0.1055613458 * a - 0.0638541728 * b;
    const double s_ = L - 0.0894841775 * a - 1.2914855480 * b;

    const double l = l_ * l_ * l_;
    const double m = m_ * m_ * m_;
    const double s = s_ * s_ * s_;

    const double r = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    const double g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    const double bl = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;

    return QColor::fromRgbF(fromLinear(r), fromLinear(g), fromLinear(bl));
}

}  // namespace

double HeatRamp::normalize(quint32 count, quint32 p97)
{
    if (count == 0)
    {
        return 0.0;
    }
    const double denominator = std::log1p(static_cast<double>(p97 > 0 ? p97 : 1));
    if (denominator <= 0.0)
    {
        return 0.0;
    }
    const double t = std::log1p(static_cast<double>(count)) / denominator;
    return qBound(0.0, t, 1.0);
}

quint32 HeatRamp::percentile(const QHash<quint16, KeyStat>& keys, double q)
{
    if (keys.isEmpty())
    {
        return 0;
    }
    QVector<quint32> counts;
    counts.reserve(keys.size());
    for (const KeyStat& stat : keys)
    {
        if (stat.press > 0)
        {
            counts.append(stat.press);
        }
    }
    if (counts.isEmpty())
    {
        return 0;
    }
    std::sort(counts.begin(), counts.end());
    const int index = qBound(0, static_cast<int>(std::lround(q * (counts.size() - 1))), counts.size() - 1);
    return counts.at(index);
}

QColor HeatRamp::colorFor(double t, int kind)
{
    const QVector<Stop>& stops = stopsFor(kind);
    const double clamped = qBound(0.0, t, 1.0);

    if (clamped <= stops.first().t)
    {
        return stops.first().color;
    }
    if (clamped >= stops.last().t)
    {
        return stops.last().color;
    }

    for (int i = 1; i < stops.size(); ++i)
    {
        if (clamped <= stops[i].t)
        {
            const Stop& a = stops[i - 1];
            const Stop& b = stops[i];
            const double span = b.t - a.t;
            const double local = span > 0.0 ? (clamped - a.t) / span : 0.0;

            double la = 0.0, aa = 0.0, ba = 0.0;
            double lb = 0.0, ab = 0.0, bb = 0.0;
            toOklab(a.color, la, aa, ba);
            toOklab(b.color, lb, ab, bb);
            return fromOklab(la + (lb - la) * local, aa + (ab - aa) * local, ba + (bb - ba) * local);
        }
    }
    return stops.last().color;
}

QColor HeatRamp::colorForCount(quint32 count, quint32 p97, int kind)
{
    if (count == 0)
    {
        return QColor(Qt::transparent);
    }
    return colorFor(normalize(count, p97), kind);
}

QVector<QPair<double, QColor>> HeatRamp::stops(int kind)
{
    QVector<QPair<double, QColor>> result;
    for (const Stop& stop : stopsFor(kind))
    {
        result.append({stop.t, stop.color});
    }
    return result;
}

QString HeatRamp::name(int kind)
{
    switch (kind)
    {
        case Inferno:
            return QStringLiteral("热焰 Inferno");
        case Colorblind:
            return QStringLiteral("色盲友好");
        default:
            return QStringLiteral("熔岩 Ember");
    }
}

QString HeatRamp::description(int kind)
{
    switch (kind)
    {
        case Inferno:
            return QStringLiteral("深红 → 橙 → 金黄，越亮越热");
        case Colorblind:
            return QStringLiteral("蓝 → 青 → 黄 → 砖红，红绿色盲可辨");
        default:
            return QStringLiteral("青 → 绿 → 黄 → 橙 → 红 → 深红，越红越热");
    }
}

}  // namespace kc
