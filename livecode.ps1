Add-Type -AssemblyName System.Windows.Forms
$proc = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Host 'UnrealEditor not running'; exit 1 }
# Clean up rotated log backups from prior sessions — the active log is held
# open by UnrealEditor and can't be truncated while the editor is running.
Remove-Item "$PSScriptRoot\Saved\Logs\weirdplace2-backup-*.log" -ErrorAction SilentlyContinue
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
$log = "$PSScriptRoot\Saved\Logs\weirdplace2.log"
$startLen = (Get-Item $log -ErrorAction SilentlyContinue).Length
if ($null -eq $startLen) { $startLen = 0 }
[System.Windows.Forms.SendKeys]::SendWait('^%{F11}')
Write-Host 'Sent Ctrl+Alt+F11 to UnrealEditor'

# UE's own compile jingle has been broken for a while, so play the result
# ourselves: watch the log for the Live Coding outcome and chime accordingly.
$deadline = (Get-Date).AddMinutes(4)
$result = $null
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 1500
    $len = (Get-Item $log -ErrorAction SilentlyContinue).Length
    if ($null -eq $len -or $len -le $startLen) { continue }
    $fs = [System.IO.File]::Open($log, 'Open', 'Read', 'ReadWrite')
    $fs.Seek($startLen, 'Begin') | Out-Null
    $new = (New-Object System.IO.StreamReader($fs)).ReadToEnd()
    $fs.Close()
    if ($new -match 'Live coding succeeded') { $result = 'success'; break }
    if ($new -match 'Live coding failed|Live coding aborted') { $result = 'fail'; break }
    if ($new -match 'no code changes detected|Failed to find patchable') { $result = 'nochange'; break }
}
# Chimes are played by scripts/livecode_chime_watcher.ps1 (spawned at editor
# launch), so results here just report and set the exit code.
switch ($result) {
    'success'  { Write-Host 'Live coding succeeded' }
    'nochange' { Write-Host 'No code changes detected' }
    'fail'     { Write-Host 'Live coding FAILED'; exit 1 }
    default    { Write-Host 'Timed out waiting for Live Coding result'; exit 1 }
}
