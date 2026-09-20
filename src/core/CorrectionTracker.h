// keyCounter - 纠错归因：把 Backspace/Delete 归因到"刚打错的那个键"
#pragma once

#include <QtGlobal>

#include "KeyCatalog.h"
#include "KeyTypes.h"

namespace kc
{

/// 只保存最近若干次可归因按键，纯内存、绝不落盘。
/// 规则：
///   - 可输入键（字母/数字/符号/空格）入栈
///   - 修饰键（Shift/Ctrl/Alt/Win/Caps/Num/Scroll）忽略，不打断语境
///   - 导航/编辑/F 区/Enter/Esc 等打断键清空语境
///   - 超过 windowMs 未输入视为新语境（默认 10 s）
class CorrectionTracker
{
public:
    static constexpr int kCapacity = 32;

    explicit CorrectionTracker(qint64 windowMs = 10000) noexcept : m_windowMs(windowMs) {}

    void reset() noexcept;
    void setWindowMs(qint64 ms) noexcept { m_windowMs = ms; }

    /// 处理一次按下事件；返回本次应被记一次纠错的 pid（无可归因对象时返回 kInvalidPid）
    quint16 registerKeyDown(quint16 pid, qint64 monotonicMs) noexcept;

private:
    void push(quint16 pid) noexcept;

    quint16 m_recent[kCapacity]{};
    int m_size = 0;
    qint64 m_lastTypedMs = 0;
    qint64 m_windowMs = 10000;
};

}  // namespace kc
