// keyCounter - 纠错归因实现
#include "CorrectionTracker.h"

#include <cstring>

namespace kc
{

void CorrectionTracker::reset() noexcept
{
    m_size = 0;
    m_lastTypedMs = 0;
}

void CorrectionTracker::push(quint16 pid) noexcept
{
    if (m_size >= kCapacity)
    {
        std::memmove(m_recent, m_recent + 1, sizeof(quint16) * (kCapacity - 1));
        m_recent[kCapacity - 1] = pid;
        return;
    }
    m_recent[m_size++] = pid;
}

quint16 CorrectionTracker::registerKeyDown(quint16 pid, qint64 monotonicMs) noexcept
{
    if (isCorrection(pid))
    {
        // 语境过期：不归因（但删除键本身仍会被计入按下次数）
        if (m_size == 0 || (monotonicMs - m_lastTypedMs) > m_windowMs)
        {
            reset();
            return kInvalidPid;
        }
        const quint16 victim = m_recent[--m_size];
        m_lastTypedMs = monotonicMs;
        return victim;
    }

    if (isTypeable(pid))
    {
        push(pid);
        m_lastTypedMs = monotonicMs;
        return kInvalidPid;
    }

    if (isModifier(pid))
    {
        return kInvalidPid;  // 不打断语境
    }

    if (isBreak(pid))
    {
        reset();
    }
    return kInvalidPid;
}

}  // namespace kc
