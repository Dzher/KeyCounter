// keyCounter - 热力色带（对数归一化 + OKLab 感知插值）
#pragma once

#include <QColor>
#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

#include "core/KeyTypes.h"

namespace kc
{

class HeatRamp
{
public:
    enum Kind
    {
        Ember = 0,      ///< 青→绿→黄→橙→红→深红（默认，符合"越红越高频"）
        Inferno = 1,    ///< 深红→猩红→橙→金（亮度越高越热）
        Colorblind = 2, ///< 蓝→青→黄→砖红（红绿色盲友好）
    };

    /// 归一化：0 表示"未按过"（保持键帽本色），1 表示最高频
    static double normalize(quint32 count, quint32 p97);
    /// 取键频分布的 q 分位（q=0.97 时忽略极值影响）
    static quint32 percentile(const QHash<quint16, KeyStat>& keys, double q);
    static QColor colorFor(double t, int kind);
    static QColor colorForCount(quint32 count, quint32 p97, int kind);
    static QVector<QPair<double, QColor>> stops(int kind);
    static QString name(int kind);
    static QString description(int kind);
};

}  // namespace kc
