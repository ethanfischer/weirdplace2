Add-Type -AssemblyName System.Windows.Forms
$proc = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Host 'UnrealEditor not running'; exit 1 }
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class WinAPI {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
}
'@
[WinAPI]::ShowWindow($proc.MainWindowHandle, 9)
Start-Sleep -Milliseconds 300
[WinAPI]::SetForegroundWindow($proc.MainWindowHandle)
Start-Sleep -Milliseconds 500
[System.Windows.Forms.SendKeys]::SendWait('^%{F11}')
Write-Host 'Sent Ctrl+Alt+F11 to UnrealEditor'
