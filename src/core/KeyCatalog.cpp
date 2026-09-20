// keyCounter - 物理键目录实现
#include "KeyCatalog.h"

#include <array>
#include <memory>

#include <windows.h>

namespace kc
{
namespace
{

using F = quint8;

struct Entry
{
    quint16 pid;
    const char* name;
    F cls;
};

// 仅列出标准 104 键（含 TKL / 65% 子集）——非常规键（多媒体等）不在表内，按 Unknown 处理
constexpr Entry kEntries[] = {
    {pk::Escape, "Escape", KeyClassBreak},
    {pk::Num1, "1", KeyClassTypeable},
    {pk::Num2, "2", KeyClassTypeable},
    {pk::Num3, "3", KeyClassTypeable},
    {pk::Num4, "4", KeyClassTypeable},
    {pk::Num5, "5", KeyClassTypeable},
    {pk::Num6, "6", KeyClassTypeable},
    {pk::Num7, "7", KeyClassTypeable},
    {pk::Num8, "8", KeyClassTypeable},
    {pk::Num9, "9", KeyClassTypeable},
    {pk::Num0, "0", KeyClassTypeable},
    {pk::Minus, "-", KeyClassTypeable},
    {pk::Equal, "=", KeyClassTypeable},
    {pk::Backspace, "Backspace", KeyClassCorrection},
    {pk::Tab, "Tab", KeyClassBreak},
    {pk::Q, "Q", KeyClassTypeable},
    {pk::W, "W", KeyClassTypeable},
    {pk::E, "E", KeyClassTypeable},
    {pk::R, "R", KeyClassTypeable},
    {pk::T, "T", KeyClassTypeable},
    {pk::Y, "Y", KeyClassTypeable},
    {pk::U, "U", KeyClassTypeable},
    {pk::I, "I", KeyClassTypeable},
    {pk::O, "O", KeyClassTypeable},
    {pk::P, "P", KeyClassTypeable},
    {pk::BracketLeft, "[", KeyClassTypeable},
    {pk::BracketRight, "]", KeyClassTypeable},
    {pk::Backslash, "\\", KeyClassTypeable},
    {pk::Enter, "Enter", KeyClassBreak},
    {pk::LeftCtrl, "Left Ctrl", KeyClassModifier},
    {pk::A, "A", KeyClassTypeable},
    {pk::S, "S", KeyClassTypeable},
    {pk::D, "D", KeyClassTypeable},
    {pk::F, "F", KeyClassTypeable},
    {pk::G, "G", KeyClassTypeable},
    {pk::H, "H", KeyClassTypeable},
    {pk::J, "J", KeyClassTypeable},
    {pk::K, "K", KeyClassTypeable},
    {pk::L, "L", KeyClassTypeable},
    {pk::Semicolon, ";", KeyClassTypeable},
    {pk::Quote, "'", KeyClassTypeable},
    {pk::Grave, "`", KeyClassTypeable},
    {pk::LeftShift, "Left Shift", KeyClassModifier},
    {pk::Z, "Z", KeyClassTypeable},
    {pk::X, "X", KeyClassTypeable},
    {pk::C, "C", KeyClassTypeable},
    {pk::V, "V", KeyClassTypeable},
    {pk::B, "B", KeyClassTypeable},
    {pk::N, "N", KeyClassTypeable},
    {pk::M, "M", KeyClassTypeable},
    {pk::Comma, ",", KeyClassTypeable},
    {pk::Period, ".", KeyClassTypeable},
    {pk::Slash, "/", KeyClassTypeable},
    {pk::RightShift, "Right Shift", KeyClassModifier},
    {pk::LeftAlt, "Left Alt", KeyClassModifier},
    {pk::Space, "Space", KeyClassTypeable},
    {pk::CapsLock, "Caps Lock", KeyClassModifier},
    {pk::F1, "F1", KeyClassFunction},
    {pk::F2, "F2", KeyClassFunction},
    {pk::F3, "F3", KeyClassFunction},
    {pk::F4, "F4", KeyClassFunction},
    {pk::F5, "F5", KeyClassFunction},
    {pk::F6, "F6", KeyClassFunction},
    {pk::F7, "F7", KeyClassFunction},
    {pk::F8, "F8", KeyClassFunction},
    {pk::F9, "F9", KeyClassFunction},
    {pk::F10, "F10", KeyClassFunction},
    {pk::F11, "F11", KeyClassFunction},
    {pk::F12, "F12", KeyClassFunction},
    {pk::ScrollLock, "Scroll Lock", KeyClassModifier},
    {pk::NumLock, "Num Lock", KeyClassModifier},
    {pk::Pause, "Pause", KeyClassBreak},
    {pk::PrintScreen, "Print Screen", KeyClassBreak},
    {pk::Numpad0, "Numpad 0", KeyClassTypeable},
    {pk::Numpad1, "Numpad 1", KeyClassTypeable},
    {pk::Numpad2, "Numpad 2", KeyClassTypeable},
    {pk::Numpad3, "Numpad 3", KeyClassTypeable},
    {pk::Numpad4, "Numpad 4", KeyClassTypeable},
    {pk::Numpad5, "Numpad 5", KeyClassTypeable},
    {pk::Numpad6, "Numpad 6", KeyClassTypeable},
    {pk::Numpad7, "Numpad 7", KeyClassTypeable},
    {pk::Numpad8, "Numpad 8", KeyClassTypeable},
    {pk::Numpad9, "Numpad 9", KeyClassTypeable},
    {pk::NumpadDecimal, "Numpad .", KeyClassTypeable},
    {pk::NumpadAdd, "Numpad +", KeyClassTypeable},
    {pk::NumpadSubtract, "Numpad -", KeyClassTypeable},
    {pk::NumpadMultiply, "Numpad *", KeyClassTypeable},
    {pk::NumpadDivide, "Numpad /", KeyClassTypeable},
    {pk::NumpadEnter, "Numpad Enter", KeyClassBreak},
    {pk::RightCtrl, "Right Ctrl", KeyClassModifier},
    {pk::RightAlt, "Right Alt", KeyClassModifier},
    {pk::LeftWin, "Left Win", KeyClassModifier},
    {pk::RightWin, "Right Win", KeyClassModifier},
    {pk::Menu, "Menu", KeyClassBreak},
    {pk::Insert, "Insert", KeyClassBreak},
    {pk::Delete, "Delete", KeyClassCorrection},
    {pk::Home, "Home", KeyClassBreak},
    {pk::End, "End", KeyClassBreak},
    {pk::PageUp, "Page Up", KeyClassBreak},
    {pk::PageDown, "Page Down", KeyClassBreak},
    {pk::ArrowUp, "Up", KeyClassBreak},
    {pk::ArrowDown, "Down", KeyClassBreak},
    {pk::ArrowLeft, "Left", KeyClassBreak},
    {pk::ArrowRight, "Right", KeyClassBreak},
};

std::array<KeyInfo, kMaxPid> buildTable()
{
    std::array<KeyInfo, kMaxPid> table{};
    for (const Entry& e : kEntries)
    {
        table[e.pid] = KeyInfo{e.name, e.cls};
    }
    return table;
}

}  // namespace

const KeyInfo& keyInfo(quint16 pid) noexcept
{
    static const std::array<KeyInfo, kMaxPid> table = buildTable();
    return table[pid & (kMaxPid - 1)];
}

quint32 pidToVirtualKey(quint16 pid) noexcept
{
    const quint32 sc = pidMakeCode(pid) | (pidIsExtended(pid) ? 0xE000u : 0u);
    return static_cast<quint32>(MapVirtualKeyExW(sc, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0)));
}

}  // namespace kc
