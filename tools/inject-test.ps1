# keyCounter - 采集链路验证工具：注入 N 次 F13（真实键盘不会产生该键，不干扰其他程序）
# 用法： powershell -ExecutionPolicy Bypass -File tools/inject-test.ps1 -Count 3
param(
    [int]$Count = 3
)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class KeyInjector
{
    // x64 下 sizeof(INPUT) 必须是 40：type(4) + pad(4) + union(32, 由 MOUSEINPUT 决定)
    [StructLayout(LayoutKind.Sequential)]
    public struct MOUSEINPUT
    {
        public int dx;
        public int dy;
        public uint mouseData;
        public uint dwFlags;
        public uint time;
        public IntPtr dwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT
    {
        public ushort wVk;
        public ushort wScan;
        public uint dwFlags;
        public uint time;
        public IntPtr dwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct HARDWAREINPUT
    {
        public uint uMsg;
        public ushort wParamL;
        public ushort wParamH;
    }

    [StructLayout(LayoutKind.Explicit)]
    public struct InputUnion
    {
        [FieldOffset(0)] public MOUSEINPUT mi;
        [FieldOffset(0)] public KEYBDINPUT ki;
        [FieldOffset(0)] public HARDWAREINPUT hi;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT
    {
        public uint type;
        public InputUnion u;
    }

    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

    public static int Size()
    {
        return Marshal.SizeOf(typeof(INPUT));
    }

    public static string Press(ushort vk, ushort scan)
    {
        INPUT[] down = new INPUT[1];
        down[0].type = 1;
        down[0].u.ki.wVk = vk;
        down[0].u.ki.wScan = scan;
        down[0].u.ki.dwFlags = 0;

        INPUT[] up = new INPUT[1];
        up[0].type = 1;
        up[0].u.ki.wVk = vk;
        up[0].u.ki.wScan = scan;
        up[0].u.ki.dwFlags = 2; // KEYEVENTF_KEYUP

        int size = Marshal.SizeOf(typeof(INPUT));
        uint r1 = SendInput(1, down, size);
        int e1 = Marshal.GetLastWin32Error();
        uint r2 = SendInput(1, up, size);
        int e2 = Marshal.GetLastWin32Error();
        if (r1 != 1 || r2 != 1)
        {
            return string.Format("SendInput 失败：down={0}(err {1}) up={2}(err {3})", r1, e1, r2, e2);
        }
        return null;
    }
}
"@

$VK_F13 = 0x7C
$SC_F13 = 0x64
$failures = 0
for ($i = 0; $i -lt $Count; $i++) {
    $result = [KeyInjector]::Press($VK_F13, $SC_F13)
    if ($result) {
        Write-Output $result
        $failures++
    }
    Start-Sleep -Milliseconds 60
}
Write-Output "injected $($Count - $failures)/$Count x F13 (sizeof(INPUT)=$([KeyInjector]::Size()))"
