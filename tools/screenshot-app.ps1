# keyCounter - dev helper: capture the running main window to a PNG (PrintWindow, no focus needed)
# Usage: powershell -ExecutionPolicy Bypass -File tools/screenshot-app.ps1 -Output shot.png
# NOTE: keep this file ASCII-only (PowerShell 5.1 reads .ps1 as ANSI without a BOM).
param(
    [string]$Output = "shot.png",
    [string]$ProcessName = "keyCounter"
)

Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class WinCap
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern int GetWindowTextW(IntPtr hWnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int cmd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    public static List<IntPtr> WindowsOf(int pid, bool onlyVisible)
    {
        List<IntPtr> result = new List<IntPtr>();
        EnumWindows(delegate(IntPtr h, IntPtr l)
        {
            uint owner;
            GetWindowThreadProcessId(h, out owner);
            if (owner == (uint)pid)
            {
                if (!onlyVisible || IsWindowVisible(h))
                {
                    result.Add(h);
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static string Title(IntPtr h)
    {
        StringBuilder sb = new StringBuilder(400);
        GetWindowTextW(h, sb, 400);
        return sb.ToString();
    }
}
"@

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) {
    Write-Output "process not found: $ProcessName"
    exit 1
}

$handles = [WinCap]::WindowsOf($proc.Id, $true)
if ($handles.Count -eq 0) {
    Write-Output "no visible window for pid $($proc.Id)"
    exit 1
}

$target = $handles[0]
foreach ($h in $handles) {
    if ([WinCap]::Title($h) -ne "") { $target = $h; break }
}

[void][WinCap]::ShowWindow($target, 9)
[void][WinCap]::SetForegroundWindow($target)
Start-Sleep -Milliseconds 900

$rect = New-Object WinCap+RECT
[void][WinCap]::GetWindowRect($target, [ref]$rect)
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
Write-Output ("window hwnd=0x{0:X} size={1}x{2} title='{3}'" -f [int64]$target, $width, $height, [WinCap]::Title($target))

Add-Type -AssemblyName System.Drawing
$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$hdc = $graphics.GetHdc()
$ok = [WinCap]::PrintWindow($target, $hdc, 2)
$graphics.ReleaseHdc($hdc)
$graphics.Dispose()
$bitmap.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()
Write-Output "PrintWindow=$ok saved $Output"
