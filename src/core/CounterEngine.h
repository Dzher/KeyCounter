// keyCounter - 统计内核：按键热路径（无锁、无堆分配、无 I/O）
#pragma once

#include <QObject>

#include "CorrectionTracker.h"
#include "KeyTypes.h"

class QTimer;

namespace kc
{

/// 单日计数累加 + 脏批次移交。
/// 热路径 onRawKey() 只做：读一次系统时间 → 整数比较 → 数组自增（目标 < 2 µs）。
class CounterEngine : public QObject
{
    Q_OBJECT
public:
    explicit CounterEngine(QObject* parent = nullptr);
    ~CounterEngine() override;

    /// 由 RawInputMonitor 在消息循环中调用
    void onRawKey(quint16 pid, bool isDown, quint32 vk = 0);

    void setPaused(bool paused);
    bool isPaused() const noexcept { return m_paused; }

    /// 立即把增量交给存储层（定时 15 s / 跨零点 / 休眠 / 锁屏 / 退出时调用）
    void flush();

    /// 内存中的"今天"（含尚未落盘的部分）
    quint64 todayPress() const noexcept;
    quint64 todayCorr() const noexcept;
    int currentDay() const noexcept { return m_today.day; }

    /// 库中最早的数据日（用于"累计"区间），由 StatsStore 在打开后回填
    void setFirstDataDay(int day) noexcept { m_firstDataDay = day; }
    int firstDataDay() const noexcept { return m_firstDataDay; }

signals:
    /// 队列连接到存储线程（批次按值传递，8 KB）
    void batchReady(const kc::DayBatch& batch);
    void dayRolled(int newDay);

private:
    void refreshCalendar();  ///< 低频：刷新本地偏移、跨零点、下一个零点时刻
    void rolloverTo(int newDay);
    void armFlushTimer();

    DayBatch m_delta;  ///< 待落盘增量
    DayBatch m_today;  ///< 今日累计（仅用于展示）
    CorrectionTracker m_tracker;
    QTimer* m_flushTimer = nullptr;
    quint16 m_down[kMaxPid / 16]{};  ///< 按下位图：过滤长按自动重复
    qint64 m_offsetSecs = 0;         ///< 本地时区偏移（秒）
    qint64 m_nextMidnight = 0;       ///< 下一个本地零点（epoch 秒）
    int m_firstDataDay = 0;
    bool m_paused = false;
};

}  // namespace kc
