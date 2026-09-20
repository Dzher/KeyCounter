#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "core/KeyCatalog.h"
#include "core/StatsQuery.h"
#include "core/StatsStore.h"

using namespace kc;

class TestStatsStore : public QObject
{
    Q_OBJECT
private slots:
    void roundTrip();
    void accumulatesAcrossBatches();
    void corruptFileIsQuarantined();
};

namespace
{

Range dayRange(int startDay, int endDay, PeriodKind kind = PeriodKind::Today)
{
    Range r;
    r.kind = kind;
    r.startDay = startDay;
    r.endDay = endDay;
    return r;
}

DayBatch makeBatch(int day)
{
    DayBatch batch;
    batch.day = day;
    batch.press[pk::A] = 5;
    batch.corr[pk::A] = 2;
    batch.press[pk::Space] = 40;
    batch.hours[10] = 45;
    return batch;
}

}  // namespace

void TestStatsStore::roundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("keycounter.db"));
    const int today = static_cast<int>(QDate::currentDate().toJulianDay());

    {
        StatsStore store;
        QSignalSpy opened(&store, &StatsStore::opened);
        store.open(dbPath);
        QCOMPARE(opened.count(), 1);
        QVERIFY2(opened.at(0).at(0).toBool(), qPrintable(opened.at(0).at(1).toString()));
        QCOMPARE(opened.at(0).at(2).toInt(), 0);  // 全新库尚无数据

        store.applyBatch(makeBatch(today));
        store.shutdown();
    }

    {
        // 重新打开：首个数据日应被正确回填
        StatsStore store;
        QSignalSpy opened(&store, &StatsStore::opened);
        store.open(dbPath);
        QCOMPARE(opened.count(), 1);
        QVERIFY(opened.at(0).at(0).toBool());
        QCOMPARE(opened.at(0).at(2).toInt(), today);
        store.shutdown();
    }

    StatsQuery query;
    QString error;
    QVERIFY2(query.open(dbPath, &error), qPrintable(error));

    const PeriodStats stats = query.query(dayRange(today, today));
    QVERIFY(stats.valid);
    QCOMPARE(stats.totalPress, quint64(45));
    QCOMPARE(stats.totalCorr, quint64(2));
    QCOMPARE(stats.keys.value(pk::A).press, quint32(5));
    QCOMPARE(stats.keys.value(pk::A).corr, quint32(2));
    QCOMPARE(stats.keys.value(pk::Space).press, quint32(40));
    QCOMPARE(stats.maxKeyPid, quint16(pk::Space));
    QCOMPARE(stats.hours[10], quint32(45));
    QCOMPARE(stats.days.size(), qsizetype(1));
    QCOMPARE(stats.activeDays, 1);

    // 区间外查询应为空
    const PeriodStats other = query.query(dayRange(today - 1, today - 1));
    QCOMPARE(other.totalPress, quint64(0));
    QVERIFY(other.keys.isEmpty());
}

void TestStatsStore::accumulatesAcrossBatches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("keycounter.db"));
    const int today = static_cast<int>(QDate::currentDate().toJulianDay());

    StatsStore store;
    QSignalSpy opened(&store, &StatsStore::opened);
    store.open(dbPath);
    QVERIFY(opened.at(0).at(0).toBool());

    // 同一批次重复落盘 → 累加而不是覆盖
    store.applyBatch(makeBatch(today));
    store.applyBatch(makeBatch(today));

    DayBatch yesterday;
    yesterday.day = today - 1;
    yesterday.press[pk::A] = 1;
    store.applyBatch(yesterday);

    store.shutdown();

    StatsQuery query;
    QVERIFY(query.open(dbPath));
    const PeriodStats todayStats = query.query(dayRange(today, today));
    QCOMPARE(todayStats.totalPress, quint64(90));
    QCOMPARE(todayStats.keys.value(pk::A).press, quint32(10));

    const PeriodStats week = query.query(dayRange(today - 1, today, PeriodKind::Week));
    QCOMPARE(week.totalPress, quint64(91));
    QCOMPARE(week.days.size(), qsizetype(2));
    QCOMPARE(week.activeDays, 2);
    QCOMPARE(week.keys.value(pk::A).press, quint32(11));
}

void TestStatsStore::corruptFileIsQuarantined()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("keycounter.db"));

    {
        QFile garbage(dbPath);
        QVERIFY(garbage.open(QIODevice::WriteOnly));
        garbage.write("this is definitely not a sqlite database file - garbage garbage garbage garbage");
        garbage.close();
    }

    StatsStore store;
    QSignalSpy opened(&store, &StatsStore::opened);
    QSignalSpy warnings(&store, &StatsStore::integrityWarning);
    store.open(dbPath);

    QCOMPARE(warnings.count(), 1);
    QCOMPARE(opened.count(), 1);
    QVERIFY2(opened.at(0).at(0).toBool(), qPrintable(opened.at(0).at(1).toString()));

    const QStringList quarantined =
        QDir(dir.path()).entryList({QStringLiteral("keycounter.corrupt-*.db")}, QDir::Files);
    QCOMPARE(quarantined.size(), 1);

    // 新库立即可用
    const int today = static_cast<int>(QDate::currentDate().toJulianDay());
    DayBatch batch;
    batch.day = today;
    batch.press[pk::B] = 3;
    store.applyBatch(batch);
    store.shutdown();

    StatsQuery query;
    QVERIFY(query.open(dbPath));
    QCOMPARE(query.query(dayRange(today, today)).keys.value(pk::B).press, quint32(3));
}

QTEST_GUILESS_MAIN(TestStatsStore)

#include "tst_store.moc"
