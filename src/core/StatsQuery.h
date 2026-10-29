// keyCounter - 只读查询（GUI 线程，独立连接 + 结果缓存）
#pragma once

#include <QCache>
#include <QHash>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include "KeyTypes.h"
#include "Periods.h"

namespace kc
{

class StatsQuery : public QObject
{
    Q_OBJECT
public:
    explicit StatsQuery(QObject* parent = nullptr);
    ~StatsQuery() override;

    bool open(const QString& dbPath, QString* error = nullptr);
    /// 释放连接（切换数据目录前必须调用，否则 Windows 下文件被占用）
    void close();
    bool isOpen() const noexcept { return m_open; }

    /// 区间聚合（命中缓存时不触库）；数据量很小，GUI 线程同步查询可接受
    PeriodStats query(const Range& range);

    /// 单个键的逐日序列（下钻用），只含有数据的日期
    QVector<QPair<int, KeyStat>> keySeries(quint16 pid, const Range& range);

    /// 存储层落盘后调用：失效受影响的缓存
    void onDataChanged(quint64 revision, int touchedDay);

private:
    static quint64 cacheKey(const Range& r);
    void evictForDay(int day);

    QCache<quint64, PeriodStats> m_cache;
    QHash<quint64, Range> m_cachedRanges;
    QSqlDatabase m_db;
    bool m_open = false;
};

}  // namespace kc
