<#
.SYNOPSIS
    Captures an ETW heap trace with callstacks for the running UnrealEditor.exe.

.DESCRIPTION
    Starts WPR with the Heap profile, sleeps for the specified duration, then
    stops and writes the .etl file under Saved/Profiling/Heap/.

    Requires heap_trace_setup.ps1 to have been run previously, and UE must have
    been relaunched after setup so stack tracing applies. Self-elevates via UAC.

.PARAMETER DurationMinutes
    How long to capture. Default 5.
#>

param(
    [int]$DurationMinutes = 5
)

$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]"Administrator")
if (-not $isAdmin) {
    Write-Host "Not running as admin; relaunching elevated..."
    Start-Process -FilePath "powershell.exe" -ArgumentList "-ExecutionPolicy","Bypass","-File","`"$PSCommandPath`"","-DurationMinutes",$DurationMinutes -Verb RunAs
    exit 0
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$OutDir = Join-Path $ProjectRoot "Saved\Profiling\Heap"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$proc = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue
if (-not $proc) {
    Write-Error "UnrealEditor.exe is not running. Launch it first."
    Read-Host "Press Enter to close"
    exit 1
}
Write-Host "UE PID: $($proc.Id)"

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$etl = Join-Path $OutDir "heap_$stamp.etl"

Write-Host "Starting WPR (Heap profile)..."
& wpr -start Heap -filemode
if ($LASTEXITCODE -ne 0) {
    Write-Error "wpr -start failed with exit code $LASTEXITCODE. Did you run heap_trace_setup.ps1 and restart UE since?"
    Read-Host "Press Enter to close"
    exit 1
}

Write-Host "Capturing for $DurationMinutes minutes..."
$totalSec = $DurationMinutes * 60
$elapsed = 0
while ($elapsed -lt $totalSec) {
    Start-Sleep -Seconds 30
    $elapsed += 30
    $remain = [math]::Max(0, $totalSec - $elapsed)
    Write-Host "  ${elapsed}s elapsed, ${remain}s remaining"
}

Write-Host "Stopping trace -> $etl"
& wpr -stop $etl
if ($LASTEXITCODE -ne 0) {
    Write-Error "wpr -stop failed with exit code $LASTEXITCODE"
    Read-Host "Press Enter to close"
    exit 1
}

$sizeMB = [math]::Round((Get-Item $etl).Length / 1MB, 1)
Write-Host ""
Write-Host "Done. Trace size: $sizeMB MB"
Write-Host "File: $etl"
Write-Host ""
Write-Host "To analyze:"
Write-Host "  C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\wpa.exe `"$etl`""
Write-Host "  In WPA: Graph Explorer -> Memory -> Heap Allocations (Outstanding Size)"
Write-Host "  Open the table view, group by Process then Stack, sort by Size desc."
Write-Host ""
Read-Host "Press Enter to close"
