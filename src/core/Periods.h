// keyCounter - 统计周期（今天/本周/本月/今年/累计）区间计算
#pragma once

#include <QDate>
#include <QString>

namespace kc
{

enum class PeriodKind
{
    Today = 0,
    Week,
    Month,
    Year,
    All,
};

/// 闭区间 [startDay, endDay]，day = QDate::toJulianDay()
struct Range
{
    PeriodKind kind = PeriodKind::Today;
    int startDay = 0;
    int endDay = 0;

    int dayCount() const noexcept { return endDay - startDay + 1; }
    bool contains(int day) const noexcept { return day >= startDay && day <= endDay; }
    bool operator==(const Range& o) const noexcept
    {
        return kind == o.kind && startDay == o.startDay && endDay == o.endDay;
    }
};

/// @param weekStart 1 = 周一，7 = 周日
/// @param firstDataDay 库中最早的一天（用于 All；无数据时传 today）
Range makeRange(PeriodKind kind, const QDate& today, int weekStart, int firstDataDay);

QString periodName(PeriodKind kind);
QString rangeSubtitle(const Range& r, const QDate& today);

/// 生成用于图表的日期序列（升序，含无数据的日期）
QVector<QDate> rangeDates(const Range& r);

}  // namespace kc
