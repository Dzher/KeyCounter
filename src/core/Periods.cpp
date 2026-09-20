// keyCounter - 周期区间实现
#include "Periods.h"

#include <QVector>

namespace kc
{

Range makeRange(PeriodKind kind, const QDate& today, int weekStart, int firstDataDay)
{
    Range r;
    r.kind = kind;
    const int todayDay = static_cast<int>(today.toJulianDay());

    switch (kind)
    {
        case PeriodKind::Today:
        {
            r.startDay = todayDay;
            r.endDay = todayDay;
            break;
        }
        case PeriodKind::Week:
        {
            const int ws = (weekStart >= 1 && weekStart <= 7) ? weekStart : 1;
            const int dow = today.dayOfWeek();  // 1=Mon .. 7=Sun
            const int back = ((dow - ws) + 7) % 7;
            const QDate start = today.addDays(-back);
            r.startDay = static_cast<int>(start.toJulianDay());
            r.endDay = static_cast<int>(start.addDays(6).toJulianDay());
            break;
        }
        case PeriodKind::Month:
        {
            const QDate start(today.year(), today.month(), 1);
            const QDate end(today.year(), today.month(), today.daysInMonth());
            r.startDay = static_cast<int>(start.toJulianDay());
            r.endDay = static_cast<int>(end.toJulianDay());
            break;
        }
        case PeriodKind::Year:
        {
            const QDate start(today.year(), 1, 1);
            const QDate end(today.year(), 12, 31);
            r.startDay = static_cast<int>(start.toJulianDay());
            r.endDay = static_cast<int>(end.toJulianDay());
            break;
        }
        case PeriodKind::All:
        {
            r.startDay = (firstDataDay > 0 && firstDataDay <= todayDay) ? firstDataDay : todayDay;
            r.endDay = todayDay;
            break;
        }
    }
    return r;
}

QString periodName(PeriodKind kind)
{
    switch (kind)
    {
        case PeriodKind::Today:
            return QStringLiteral("今天");
        case PeriodKind::Week:
            return QStringLiteral("本周");
        case PeriodKind::Month:
            return QStringLiteral("本月");
        case PeriodKind::Year:
            return QStringLiteral("今年");
        case PeriodKind::All:
            return QStringLiteral("累计");
    }
    return QString();
}

QString rangeSubtitle(const Range& r, const QDate& today)
{
    const QDate start = QDate::fromJulianDay(r.startDay);
    const QDate end = QDate::fromJulianDay(r.endDay);

    switch (r.kind)
    {
        case PeriodKind::Today:
            return start.toString(QStringLiteral("yyyy-MM-dd dddd"));
        case PeriodKind::Week:
            return QStringLiteral("%1 ~ %2").arg(start.toString(QStringLiteral("MM-dd")), end.toString(QStringLiteral("MM-dd")));
        case PeriodKind::Month:
            return start.toString(QStringLiteral("yyyy 年 M 月"));
        case PeriodKind::Year:
            return QStringLiteral("%1 年").arg(start.year());
        case PeriodKind::All:
        {
            const int days = static_cast<int>(start.daysTo(today)) + 1;
            return QStringLiteral("%1 起 · 已统计 %2 天")
                .arg(start.toString(QStringLiteral("yyyy-MM-dd")))
                .arg(days);
        }
    }
    return QString();
}

QVector<QDate> rangeDates(const Range& r)
{
    QVector<QDate> dates;
    const int n = r.dayCount();
    if (n <= 0 || n > 40000)
    {
        return dates;
    }
    dates.reserve(n);
    for (int day = r.startDay; day <= r.endDay; ++day)
    {
        dates.append(QDate::fromJulianDay(day));
    }
    return dates;
}

}  // namespace kc
