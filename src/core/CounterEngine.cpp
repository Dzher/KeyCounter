// keyCounter - 统计内核实现
#include "CounterEngine.h"

#include <QDate>
#include <QDateTime>
#include <QTimer>

namespace kc
{
namespace
{
constexpr int kFlushIntervalMs = 15000;
constexpr qint64 kSecsPerDay = 86400;
}  // namespace

CounterEngine::CounterEngine(QObject* parent) : QObject(parent)
{
    const QDateTime now = QDateTime::currentDateTime();
    m_today.day = static_cast<int>(now.date().toJulianDay());
    m_delta.day = m_today.day;
    m_offsetSecs = now.offsetFromUtc();
    const qint64 secs = now.toSecsSinceEpoch();
    m_nextMidnight = secs + (kSecsPerDay - ((secs + m_offsetSecs) % kSecsPerDay));

    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_flushTimer, &QTimer::timeout, this, &CounterEngine::flush);
}

CounterEngine::~CounterEngine() = default;

void CounterEngine::onRawKey(quint16 pid, bool isDown, quint32 vk)
{
    Q_UNUSED(vk);
    if (pid >= kMaxPid)
    {
        return;
    }

    const quint16 mask = static_cast<quint16>(1u << (pid & 15));
    quint16& word = m_down[pid >> 4];

    if (!isDown)
    {
        word &= static_cast<quint16>(~mask);
        return;
    }
    if ((word & mask) != 0)
    {
        return;  // 长按自动重复：不计次
    }
    word |= mask;

    if (m_paused)
    {
        return;
    }

    // --- 热路径：纯整数运算 ---
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now >= m_nextMidnight)
    {
        refreshCalendar();
    }
    const qint64 localSecs = now + m_offsetSecs;
    const int hour = static_cast<int>((localSecs % kSecsPerDay) / 3600);

    ++m_delta.press[pid];
    ++m_delta.hours[hour];
    ++m_today.press[pid];
    ++m_today.hours[hour];

    const quint16 victim = m_tracker.registerKeyDown(pid, now * 1000);
    if (victim != kInvalidPid)
    {
        ++m_delta.corr[victim];
        ++m_today.corr[victim];
    }

    armFlushTimer();
}

void CounterEngine::armFlushTimer()
{
    if (!m_flushTimer->isActive())
    {
        m_flushTimer->start(kFlushIntervalMs);
    }
}

void CounterEngine::refreshCalendar()
{
    const QDateTime now = QDateTime::currentDateTime();
    const int day = static_cast<int>(now.date().toJulianDay());
    if (day != m_today.day)
    {
        rolloverTo(day);
    }
    m_offsetSecs = now.offsetFromUtc();
    const qint64 secs = now.toSecsSinceEpoch();
    const qint64 intoDay = ((secs + m_offsetSecs) % kSecsPerDay + kSecsPerDay) % kSecsPerDay;
    m_nextMidnight = secs + (kSecsPerDay - intoDay);
}

void CounterEngine::rolloverTo(int newDay)
{
    if (!m_delta.isEmpty())
    {
        emit batchReady(m_delta);
    }
    m_delta.clear();
    m_delta.day = newDay;
    m_today.clear();
    m_today.day = newDay;
    emit dayRolled(newDay);
}

void CounterEngine::flush()
{
    if (m_flushTimer->isActive())
    {
        m_flushTimer->stop();
    }

    // 顺带刷新日历缓存：跨零点 / 系统时钟或时区变化 / 夏令时切换都在这里收口
    if (QDateTime::currentSecsSinceEpoch() >= m_nextMidnight)
    {
        refreshCalendar();
    }

    if (!m_delta.isEmpty())
    {
        emit batchReady(m_delta);
        m_delta.clear();
        m_delta.day = m_today.day;
    }
}

void CounterEngine::setPaused(bool paused)
{
    if (m_paused == paused)
    {
        return;
    }
    m_paused = paused;
    if (!m_paused)
    {
        return;
    }
    // 暂停前把已统计的数据落盘（按下位图继续维护，暂停期间按住的键不会在恢复时被误计）
    flush();
}

quint64 CounterEngine::todayPress() const noexcept
{
    quint64 sum = 0;
    for (quint32 v : m_today.press)
    {
        sum += v;
    }
    return sum;
}

quint64 CounterEngine::todayCorr() const noexcept
{
    quint64 sum = 0;
    for (quint32 v : m_today.corr)
    {
        sum += v;
    }
    return sum;
}

}  // namespace kc
