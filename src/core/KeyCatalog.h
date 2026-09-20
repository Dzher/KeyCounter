// keyCounter - 物理键目录：pid 常量、英文名、按键分类
#pragma once

#include <QtGlobal>

#include "KeyTypes.h"

namespace kc
{

/// 按键分类位（用于纠错归因与统计口径）
enum KeyClassFlag : quint8
{
    KeyClassNone = 0x00,
    KeyClassTypeable = 0x01,    ///< 字母/数字/符号/空格：可作为纠错归因对象
    KeyClassModifier = 0x02,    ///< Shift/Ctrl/Alt/Win/Caps：不打断输入语境
    KeyClassCorrection = 0x04,  ///< Backspace / Delete：纠错动作
    KeyClassBreak = 0x08,       ///< Enter/Esc/Tab/方向/编辑簇/F 区：清空归因语境
    KeyClassFunction = 0x10,    ///< F1-F12 等功能键（同样视为打断）
};

struct KeyInfo
{
    const char* name = "Unknown";
    quint8 cls = KeyClassNone;
};

/// O(1) 查询（首次调用时构建 1024 项查表）
const KeyInfo& keyInfo(quint16 pid) noexcept;

inline bool isTypeable(quint16 pid) noexcept { return (keyInfo(pid).cls & KeyClassTypeable) != 0; }
inline bool isModifier(quint16 pid) noexcept { return (keyInfo(pid).cls & KeyClassModifier) != 0; }
inline bool isCorrection(quint16 pid) noexcept { return (keyInfo(pid).cls & KeyClassCorrection) != 0; }
inline bool isBreak(quint16 pid) noexcept { return (keyInfo(pid).cls & (KeyClassBreak | KeyClassFunction)) != 0; }

/// 用当前键盘布局把 pid 还原成虚拟键（仅诊断/导出用，不参与统计主键）
quint32 pidToVirtualKey(quint16 pid) noexcept;

// ---------------------------------------------------------------------------
// pid 常量表（scan code set 1；E0/E1 前缀体现在高位）
// ---------------------------------------------------------------------------
namespace pk
{

// 顶行
inline constexpr quint16 Escape = 0x001;
inline constexpr quint16 Num1 = 0x002;
inline constexpr quint16 Num2 = 0x003;
inline constexpr quint16 Num3 = 0x004;
inline constexpr quint16 Num4 = 0x005;
inline constexpr quint16 Num5 = 0x006;
inline constexpr quint16 Num6 = 0x007;
inline constexpr quint16 Num7 = 0x008;
inline constexpr quint16 Num8 = 0x009;
inline constexpr quint16 Num9 = 0x00A;
inline constexpr quint16 Num0 = 0x00B;
inline constexpr quint16 Minus = 0x00C;
inline constexpr quint16 Equal = 0x00D;
inline constexpr quint16 Backspace = 0x00E;
inline constexpr quint16 Tab = 0x00F;
inline constexpr quint16 Q = 0x010;
inline constexpr quint16 W = 0x011;
inline constexpr quint16 E = 0x012;
inline constexpr quint16 R = 0x013;
inline constexpr quint16 T = 0x014;
inline constexpr quint16 Y = 0x015;
inline constexpr quint16 U = 0x016;
inline constexpr quint16 I = 0x017;
inline constexpr quint16 O = 0x018;
inline constexpr quint16 P = 0x019;
inline constexpr quint16 BracketLeft = 0x01A;
inline constexpr quint16 BracketRight = 0x01B;
inline constexpr quint16 Enter = 0x01C;
inline constexpr quint16 LeftCtrl = 0x01D;
inline constexpr quint16 A = 0x01E;
inline constexpr quint16 S = 0x01F;
inline constexpr quint16 D = 0x020;
inline constexpr quint16 F = 0x021;
inline constexpr quint16 G = 0x022;
inline constexpr quint16 H = 0x023;
inline constexpr quint16 J = 0x024;
inline constexpr quint16 K = 0x025;
inline constexpr quint16 L = 0x026;
inline constexpr quint16 Semicolon = 0x027;
inline constexpr quint16 Quote = 0x028;
inline constexpr quint16 Grave = 0x029;
inline constexpr quint16 LeftShift = 0x02A;
inline constexpr quint16 Backslash = 0x02B;
inline constexpr quint16 Z = 0x02C;
inline constexpr quint16 X = 0x02D;
inline constexpr quint16 C = 0x02E;
inline constexpr quint16 V = 0x02F;
inline constexpr quint16 B = 0x030;
inline constexpr quint16 N = 0x031;
inline constexpr quint16 M = 0x032;
inline constexpr quint16 Comma = 0x033;
inline constexpr quint16 Period = 0x034;
inline constexpr quint16 Slash = 0x035;
inline constexpr quint16 RightShift = 0x036;
inline constexpr quint16 NumpadMultiply = 0x037;
inline constexpr quint16 LeftAlt = 0x038;
inline constexpr quint16 Space = 0x039;
inline constexpr quint16 CapsLock = 0x03A;
inline constexpr quint16 F1 = 0x03B;
inline constexpr quint16 F2 = 0x03C;
inline constexpr quint16 F3 = 0x03D;
inline constexpr quint16 F4 = 0x03E;
inline constexpr quint16 F5 = 0x03F;
inline constexpr quint16 F6 = 0x040;
inline constexpr quint16 F7 = 0x041;
inline constexpr quint16 F8 = 0x042;
inline constexpr quint16 F9 = 0x043;
inline constexpr quint16 F10 = 0x044;
inline constexpr quint16 NumLock = 0x045;
inline constexpr quint16 ScrollLock = 0x046;
inline constexpr quint16 Numpad7 = 0x047;
inline constexpr quint16 Numpad8 = 0x048;
inline constexpr quint16 Numpad9 = 0x049;
inline constexpr quint16 NumpadSubtract = 0x04A;
inline constexpr quint16 Numpad4 = 0x04B;
inline constexpr quint16 Numpad5 = 0x04C;
inline constexpr quint16 Numpad6 = 0x04D;
inline constexpr quint16 NumpadAdd = 0x04E;
inline constexpr quint16 Numpad1 = 0x04F;
inline constexpr quint16 Numpad2 = 0x050;
inline constexpr quint16 Numpad3 = 0x051;
inline constexpr quint16 Numpad0 = 0x052;
inline constexpr quint16 NumpadDecimal = 0x053;
inline constexpr quint16 F11 = 0x057;
inline constexpr quint16 F12 = 0x058;

// E0 扩展
inline constexpr quint16 NumpadEnter = 0x11C;
inline constexpr quint16 RightCtrl = 0x11D;
inline constexpr quint16 NumpadDivide = 0x135;
inline constexpr quint16 PrintScreen = 0x137;
inline constexpr quint16 RightAlt = 0x138;
inline constexpr quint16 Home = 0x147;
inline constexpr quint16 ArrowUp = 0x148;
inline constexpr quint16 PageUp = 0x149;
inline constexpr quint16 ArrowLeft = 0x14B;
inline constexpr quint16 ArrowRight = 0x14D;
inline constexpr quint16 End = 0x14F;
inline constexpr quint16 ArrowDown = 0x150;
inline constexpr quint16 PageDown = 0x151;
inline constexpr quint16 Insert = 0x152;
inline constexpr quint16 Delete = 0x153;
inline constexpr quint16 LeftWin = 0x15B;
inline constexpr quint16 RightWin = 0x15C;
inline constexpr quint16 Menu = 0x15D;

// E1 前缀（Pause/Break）
inline constexpr quint16 Pause = 0x245;

}  // namespace pk

}  // namespace kc
