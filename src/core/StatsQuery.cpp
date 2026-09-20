// keyCounter - 只读查询实现
#include "StatsQuery.h"

#include <QElapsedTimer>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>
#include <QVector>

namespace kc
{
namespace
{
const QString kConnectionName = QStringLiteral("keycounter-query");
constexpr int kCacheSize = 8;
}  // namespace

StatsQuery::StatsQuery(QObject* parent) : QObject(parent), m_cache(kCacheSize) {}

StatsQuery::~StatsQuery()
{
    m_cache.clear();
    m_cachedRanges.clear();
    if (m_db.isValid())
    {
        m_db.close();
    }
    m_db = QSqlDatabase();
    if (QSqlDatabase::contains(kConnectionName))
    {
        QSqlDatabase::removeDatabase(kConnectionName);
    }
}

bool StatsQuery::open(const QString& dbPath, QString* error)
{
    if (m_db.isValid())
    {
        m_db.close();
        m_db = QSqlDatabase();
        if (QSqlDatabase::contains(kConnectionName))
        {
            QSqlDatabase::removeDatabase(kConnectionName);
        }
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName);
    m_db.setDatabaseName(dbPath);
    m_db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=3000"));
    if (!m_db.open())
    {
        if (error != nullptr)
        {
            *error = m_db.lastError().text();
        }
        m_open = false;
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));

    m_open = true;
    return true;
}

quint64 StatsQuery::cacheKey(const Range& r)
{
    return (static_cast<quint64>(r.kind) << 56) ^ (static_cast<quint64>(r.startDay) << 24) ^
           static_cast<quint64>(r.endDay);
}

void StatsQuery::evictForDay(int day)
{
    const QList<quint64> keys = m_cache.keys();
    for (quint64 key : keys)
    {
        const Range r = m_cachedRanges.value(key);
        if (day == 0 || r.contains(day))
        {
            m_cache.remove(key);
            m_cachedRanges.remove(key);
        }
    }
}

void StatsQuery::onDataChanged(quint64 revision, int touchedDay)
{
    Q_UNUSED(revision);
    evictForDay(touchedDay);
}

PeriodStats StatsQuery::query(const Range& range)
{
    const quint64 key = cacheKey(range);
    if (const PeriodStats* hit = m_cache.object(key))
    {
        return *hit;
    }

    PeriodStats stats;
    if (!m_open)
    {
        return stats;
    }

    // 1) 逐键聚合
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT pid, SUM(press), SUM(corr) FROM key_daily "
                                 "WHERE day BETWEEN ?1 AND ?2 GROUP BY pid"));
        q.addBindValue(range.startDay);
        q.addBindValue(range.endDay);
        if (!q.exec())
        {
            return stats;
        }
        while (q.next())
        {
            const quint16 pid = static_cast<quint16>(q.value(0).toUInt());
            KeyStat st;
            st.press = q.value(1).toUInt();
            st.corr = q.value(2).toUInt();
            stats.keys.insert(pid, st);
            stats.totalPress += st.press;
            stats.totalCorr += st.corr;
            if (st.press > stats.maxKeyPress)
            {
                stats.maxKeyPress = st.press;
                stats.maxKeyPid = pid;
            }
        }
    }

    // 2) 逐日聚合（趋势图）
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT day, SUM(press), SUM(corr) FROM key_daily "
                                 "WHERE day BETWEEN ?1 AND ?2 GROUP BY day ORDER BY day"));
        q.addBindValue(range.startDay);
        q.addBindValue(range.endDay);
        if (!q.exec())
        {
            return stats;
        }
        while (q.next())
        {
            const int day = q.value(0).toInt();
            KeyStat st;
            st.press = q.value(1).toUInt();
            st.corr = q.value(2).toUInt();
            if (st.press > 0)
            {
                stats.days.append({day, st});
                ++stats.activeDays;
            }
        }
    }

    // 3) 时段分布
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT hour, SUM(press) FROM day_hour WHERE day BETWEEN ?1 AND ?2 GROUP BY hour"));
        q.addBindValue(range.startDay);
        q.addBindValue(range.endDay);
        if (!q.exec())
        {
            return stats;
        }
        while (q.next())
        {
            const int hour = q.value(0).toInt();
            if (hour >= 0 && hour < 24)
            {
                stats.hours[static_cast<size_t>(hour)] = q.value(1).toUInt();
            }
        }
    }

    stats.valid = true;
    m_cachedRanges.insert(key, range);
    m_cache.insert(key, new PeriodStats(stats));
    return stats;
}

}  // namespace kc
