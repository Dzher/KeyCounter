// keyCounter - Win32 采集层：Raw Input（不注入、不挂钩）+ 电源/会话通知 + 全局快捷键
#pragma once

#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QtGlobal>

namespace kc
{

/// 已解析的键盘事件
struct RawKeyEvent
{
    quint16 pid = 0;  ///< (E1<<9)|(E0<<8)|makeCode
    bool down = false;
    quint8 vkey = 0;
};

/// 解析 RAWKEYBOARD 字段（纯函数，便于单测）。
/// @param flags RAWKEYBOARD::Flags（RI_KEY_BREAK=1, RI_KEY_E0=2, RI_KEY_E1=4）
/// @return false 表示该事件必须丢弃（VK__none_ / overrun / PrintScreen 伪 Shift）
bool decodeRawKeyboard(quint8 makeCode, quint16 flags, quint8 vkey, RawKeyEvent* out);

/// 隐藏消息窗口 + Raw Input 注册。全部工作在主线程消息循环内，无额外线程、无轮询。
class RawInputMonitor : public QObject
{
    Q_OBJECT
public:
    explicit RawInputMonitor(QObject* parent = nullptr);
    ~RawInputMonitor() override;

    /// 注册 Raw Input 与电源/会话通知
    bool start(QString* error = nullptr);
    void stop();
    bool isRunning() const noexcept { return m_hwnd != nullptr; }

    /// 全局快捷键（可选功能）：失败时返回 false 并在 error 中给出原因
    bool registerHotkey(const QKeySequence& sequence, QString* error = nullptr);
    void unregisterHotkey();

signals:
    void rawKey(quint16 pid, bool down, quint32 vk);
    void hotkeyTriggered();
    /// 休眠/关机前：立即落盘
    void systemSuspending();
    void systemResumed();
    void sessionLocked(bool locked);

private:
    void handleRawInput(void* rawInputHandle);
    static long long __stdcall wndProc(void* hwnd, unsigned int msg, unsigned long long wp, long long lp);

    void* m_hwnd = nullptr;
    bool m_hotkeyRegistered = false;
    bool m_sessionNotification = false;
};

}  // namespace kc
