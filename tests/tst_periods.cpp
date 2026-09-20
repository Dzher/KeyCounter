#include <QDate>
#include <QTest>

#include "core/Periods.h"

using namespace kc;

class TestPeriods : public QObject
{
    Q_OBJECT
private slots:
    void weekStartMonday();
    void weekStartSunday();
    void monthAndYear();
    void leapFebruary();
    void allPeriod();
};

void TestPeriods::weekStartMonday()
{
    const QDate today(2025, 5, 29);  // 周四
    QCOMPARE(today.dayOfWeek(), 4);

    const Range r = makeRange(PeriodKind::Week, today, 1, 0);
    QCOMPARE(r.dayCount(), 7);
    QCOMPARE(QDate::fromJulianDay(r.startDay), QDate(2025, 5, 26));  // 周一
    QCOMPARE(QDate::fromJulianDay(r.endDay), QDate(2025, 6, 1));     // 周日
    QVERIFY(r.contains(static_cast<int>(today.toJulianDay())));

    // 周日当天（周起始为周一时，周日属于本周最后一天）
    const QDate sunday(2025, 6, 1);
    const Range r2 = makeRange(PeriodKind::Week, sunday, 1, 0);
    QCOMPARE(QDate::fromJulianDay(r2.startDay), QDate(2025, 5, 26));
    QCOMPARE(QDate::fromJulianDay(r2.endDay), QDate(2025, 6, 1));
}

void TestPeriods::weekStartSunday()
{
    const QDate today(2025, 5, 29);  // 周四
    const Range r = makeRange(PeriodKind::Week, today, 7, 0);
    QCOMPARE(QDate::fromJulianDay(r.startDay), QDate(2025, 5, 25));  // 周日
    QCOMPARE(QDate::fromJulianDay(r.endDay), QDate(2025, 5, 31));    // 周六
}

void TestPeriods::monthAndYear()
{
    const QDate today(2025, 5, 29);

    const Range month = makeRange(PeriodKind::Month, today, 1, 0);
    QCOMPARE(QDate::fromJulianDay(month.startDay), QDate(2025, 5, 1));
    QCOMPARE(QDate::fromJulianDay(month.endDay), QDate(2025, 5, 31));

    const Range year = makeRange(PeriodKind::Year, today, 1, 0);
    QCOMPARE(QDate::fromJulianDay(year.startDay), QDate(2025, 1, 1));
    QCOMPARE(QDate::fromJulianDay(year.endDay), QDate(2025, 12, 31));

    // 12 月 31 日：本周可能跨年
    const QDate newYearEve(2025, 12, 31);
    const Range week = makeRange(PeriodKind::Week, newYearEve, 1, 0);
    QCOMPARE(QDate::fromJulianDay(week.startDay), QDate(2025, 12, 29));
    QCOMPARE(QDate::fromJulianDay(week.endDay), QDate(2026, 1, 4));
}

void TestPeriods::leapFebruary()
{
    const QDate today(2024, 2, 15);
    const Range month = makeRange(PeriodKind::Month, today, 1, 0);
    QCOMPARE(QDate::fromJulianDay(month.startDay), QDate(2024, 2, 1));
    QCOMPARE(QDate::fromJulianDay(month.endDay), QDate(2024, 2, 29));
    QCOMPARE(month.dayCount(), 29);

    const QDate normal(2025, 2, 15);
    QCOMPARE(makeRange(PeriodKind::Month, normal, 1, 0).dayCount(), 28);
}

void TestPeriods::allPeriod()
{
    const QDate today(2025, 5, 29);

    // 无数据时退化为"今天"
    const Range empty = makeRange(PeriodKind::All, today, 1, 0);
    QCOMPARE(empty.startDay, empty.endDay);
    QCOMPARE(empty.startDay, static_cast<int>(today.toJulianDay()));

    // 有数据时从首个数据日开始
    const QDate first(2025, 1, 1);
    const Range all = makeRange(PeriodKind::All, today, 1, static_cast<int>(first.toJulianDay()));
    QCOMPARE(QDate::fromJulianDay(all.startDay), first);
    QCOMPARE(QDate::fromJulianDay(all.endDay), today);

    const QVector<QDate> dates = rangeDates(all);
    QCOMPARE(dates.size(), all.dayCount());
    QCOMPARE(dates.first(), first);
    QCOMPARE(dates.last(), today);

    // 未来日期（首数据日晚于今天）不应导致非法区间
    const Range weird = makeRange(PeriodKind::All, today, 1, static_cast<int>(today.toJulianDay()) + 10);
    QVERIFY(weird.startDay <= weird.endDay);
    QVERIFY(weird.dayCount() >= 1);
}

QTEST_GUILESS_MAIN(TestPeriods)

#include "tst_periods.moc"
