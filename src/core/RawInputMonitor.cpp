// keyCounter - Win32 采集层实现
#include "RawInputMonitor.h"

#include <QDebug>

#include <windows.h>
#include <wtsapi32.h>

#include "KeyCatalog.h"

namespace kc
{
namespace
{

const wchar_t* const kWindowClass = L"keyCounter.RawInput.Window";

quint32 toVirtualKey(int key) noexcept
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
    {
        return static_cast<quint32>(key);  // VK 与 ASCII 大写字母一致
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
    {
        return static_cast<quint32>(key);  // '0'..'9'
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
    {
        return VK_F1 + static_cast<quint32>(key - Qt::Key_F1);
    }
    switch (key)
    {
        case Qt::Key_Space:
            return VK_SPACE;
        case Qt::Key_Escape:
            return VK_ESCAPE;
        case Qt::Key_Tab:
            return VK_TAB;
        case Qt::Key_Backspace:
            return VK_BACK;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            return VK_RETURN;
        case Qt::Key_Home:
            return VK_HOME;
        case Qt::Key_End:
            return VK_END;
        case Qt::Key_PageUp:
            return VK_PRIOR;
        case Qt::Key_PageDown:
            return VK_NEXT;
        case Qt::Key_Insert:
            return VK_INSERT;
        case Qt::Key_Delete:
            return VK_DELETE;
        case Qt::Key_Left:
            return VK_LEFT;
        case Qt::Key_Right:
            return VK_RIGHT;
        case Qt::Key_Up:
            return VK_UP;
        case Qt::Key_Down:
            return VK_DOWN;
        case Qt::Key_Comma:
            return VK_OEM_COMMA;
        case Qt::Key_Period:
            return VK_OEM_PERIOD;
        case Qt::Key_Minus:
            return VK_OEM_MINUS;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            return VK_OEM_PLUS;
        case Qt::Key_BracketLeft:
            return VK_OEM_4;
        case Qt::Key_BracketRight:
            return VK_OEM_6;
        case Qt::Key_Semicolon:
            return VK_OEM_1;
        case Qt::Key_QuoteLeft:
            return VK_OEM_3;
        case Qt::Key_Apostrophe:
            return VK_OEM_7;
        case Qt::Key_Slash:
            return VK_OEM_2;
        case Qt::Key_Backslash:
            return VK_OEM_5;
        default:
            return 0;
    }
}

}  // namespace

RawInputMonitor::RawInputMonitor(QObject* parent) : QObject(parent) {}

RawInputMonitor::~RawInputMonitor()
{
    stop();
}

bool RawInputMonitor::start(QString* error)
{
    if (m_hwnd != nullptr)
    {
        return true;
    }

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reinterpret_cast<WNDPROC>(&RawInputMonitor::wndProc);
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("注册窗口类失败（错误码 %1）").arg(GetLastError());
        }
        return false;
    }

    // 不可见顶层窗口：既能收到 Raw Input，也能收到电源/会话广播
    const HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kWindowClass, L"keyCounter", WS_POPUP, 0, 0, 0,
                                       0, nullptr, nullptr, instance, this);
    if (hwnd == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("创建消息窗口失败（错误码 %1）").arg(GetLastError());
        }
        return false;
    }
    m_hwnd = hwnd;

    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;   // Generic Desktop Controls
    rid.usUsage = 0x06;       // Keyboard
    rid.dwFlags = RIDEV_INPUTSINK;  // 本进程无焦点时也接收
    rid.hwndTarget = hwnd;
    if (RegisterRawInputDevices(&rid, 1, sizeof(rid)) == FALSE)
    {
        const DWORD code = GetLastError();
        if (error != nullptr)
        {
            *error = QStringLiteral("注册键盘采集失败（错误码 %1）").arg(code);
        }
        DestroyWindow(hwnd);
        m_hwnd = nullptr;
        return false;
    }

    m_sessionNotification = WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION) != FALSE;
    return true;
}

void RawInputMonitor::stop()
{
    unregisterHotkey();
    if (m_hwnd == nullptr)
    {
        return;
    }

    const HWND hwnd = static_cast<HWND>(m_hwnd);
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x06;
    rid.dwFlags = RIDEV_REMOVE;
    rid.hwndTarget = nullptr;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    if (m_sessionNotification)
    {
        WTSUnRegisterSessionNotification(hwnd);
        m_sessionNotification = false;
    }
    DestroyWindow(hwnd);
    m_hwnd = nullptr;
}

void RawInputMonitor::handleRawInput(void* rawInputHandle)
{
    UINT size = 0;
    if (GetRawInputData(static_cast<HRAWINPUT>(rawInputHandle), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0)
    {
        return;
    }
    if (size == 0 || size > sizeof(RAWINPUT))
    {
        return;
    }

    RAWINPUT raw{};
    UINT written = sizeof(RAWINPUT);
    const UINT copied =
        GetRawInputData(static_cast<HRAWINPUT>(rawInputHandle), RID_INPUT, &raw, &written, sizeof(RAWINPUTHEADER));
    if (copied == static_cast<UINT>(-1) || raw.header.dwType != RIM_TYPEKEYBOARD)
    {
        return;
    }

    const RAWKEYBOARD& kb = raw.data.keyboard;

    // 伪事件过滤
    if (kb.VKey == 0xFF || kb.MakeCode == 0xFF)
    {
        return;  // VK__none_ / KEYBOARD_OVERRUN_MAKE_CODE
    }
    const bool extended = (kb.Flags & RI_KEY_E0) != 0;
    const bool e1 = (kb.Flags & RI_KEY_E1) != 0;
    if (extended && (kb.MakeCode == 0x2A || kb.MakeCode == 0x36))
    {
        return;  // PrintScreen 的伪 Shift（真实左右 Shift 不带 E0）
    }

    const bool down = (kb.Flags & RI_KEY_BREAK) == 0;
    emit rawKey(encodePid(kb.MakeCode, extended, e1), down, kb.VKey);
}

bool RawInputMonitor::registerHotkey(const QKeySequence& sequence, QString* error)
{
    unregisterHotkey();
    if (m_hwnd == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("采集未启动");
        }
        return false;
    }
    if (sequence.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("组合键为空");
        }
        return false;
    }

    const QKeyCombination combination = sequence[0];
    const Qt::KeyboardModifiers mods = combination.keyboardModifiers();
    UINT flags = MOD_NOREPEAT;
    if (mods.testFlag(Qt::ControlModifier))
    {
        flags |= MOD_CONTROL;
    }
    if (mods.testFlag(Qt::AltModifier))
    {
        flags |= MOD_ALT;
    }
    if (mods.testFlag(Qt::ShiftModifier))
    {
        flags |= MOD_SHIFT;
    }
    if (mods.testFlag(Qt::MetaModifier))
    {
        flags |= MOD_WIN;
    }
    if (flags == MOD_NOREPEAT)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("全局快捷键必须包含至少一个修饰键（Ctrl/Alt/Shift/Win）");
        }
        return false;
    }

    const quint32 vk = toVirtualKey(combination.key());
    if (vk == 0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("暂不支持该主键作为全局快捷键");
        }
        return false;
    }

    if (RegisterHotKey(static_cast<HWND>(m_hwnd), 1, flags, vk) == FALSE)
    {
        const DWORD code = GetLastError();
        if (error != nullptr)
        {
            *error = (code == ERROR_HOTKEY_ALREADY_REGISTERED)
                         ? QStringLiteral("该组合键已被其他程序占用，请换一个")
                         : QStringLiteral("注册失败（错误码 %1）").arg(code);
        }
        return false;
    }
    m_hotkeyRegistered = true;
    return true;
}

void RawInputMonitor::unregisterHotkey()
{
    if (m_hotkeyRegistered && m_hwnd != nullptr)
    {
        UnregisterHotKey(static_cast<HWND>(m_hwnd), 1);
        m_hotkeyRegistered = false;
    }
}

long long __stdcall RawInputMonitor::wndProc(void* hwnd, unsigned int msg, unsigned long long wp, long long lp)
{
    HWND handle = static_cast<HWND>(hwnd);
    auto* self = reinterpret_cast<RawInputMonitor*>(GetWindowLongPtrW(handle, GWLP_USERDATA));

    if (msg == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<RawInputMonitor*>(create->lpCreateParams);
        SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    switch (msg)
    {
        case WM_INPUT:
            if (self != nullptr)
            {
                self->handleRawInput(reinterpret_cast<HRAWINPUT>(lp));
            }
            return 0;
        case WM_HOTKEY:
            if (self != nullptr)
            {
                emit self->hotkeyTriggered();
            }
            return 0;
        case WM_POWERBROADCAST:
            if (self != nullptr)
            {
                if (wp == PBT_APMSUSPEND)
                {
                    emit self->systemSuspending();
                }
                else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND)
                {
                    emit self->systemResumed();
                }
            }
            return TRUE;
        case WM_WTSSESSION_CHANGE:
            if (self != nullptr)
            {
                if (wp == WTS_SESSION_LOCK || wp == WTS_SESSION_LOGOFF)
                {
                    emit self->sessionLocked(true);
                }
                else if (wp == WTS_SESSION_UNLOCK || wp == WTS_SESSION_LOGON)
                {
                    emit self->sessionLocked(false);
                }
            }
            return 0;
        case WM_QUERYENDSESSION:
            if (self != nullptr)
            {
                emit self->systemSuspending();
            }
            return TRUE;
        case WM_ENDSESSION:
            if (self != nullptr && wp != 0)
            {
                emit self->systemSuspending();
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(handle, msg, wp, lp);
}

}  // namespace kc
