<#
.SYNOPSIS
    Launches Unreal Insights against the latest .utrace under Saved/TraceSessions/.

.DESCRIPTION
    Pass -Trace to point at a specific file. With no args, picks the most
    recently modified .utrace.

    Memory traces are large (tens of GB). Insights can take several minutes
    to load and will use a chunk of RAM. Be patient.
#>

param(
    [string]$Trace
)

$ErrorActionPreference = "Stop"

$Insights = "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealInsights.exe"
if (-not (Test-Path $Insights)) {
    Write-Error "UnrealInsights.exe not found at $Insights"
    exit 1
}

if (-not $Trace) {
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $ProjectRoot = Split-Path -Parent $ScriptDir
    $TraceDir = Join-Path $ProjectRoot "Saved\TraceSessions"
    $latest = Get-ChildItem -Path $TraceDir -Filter "*.utrace" -ErrorAction Stop |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $latest) {
        Write-Error "No .utrace files under $TraceDir"
        exit 1
    }
    $Trace = $latest.FullName
}

$sizeGB = [math]::Round((Get-Item $Trace).Length / 1GB, 2)
Write-Host "Opening: $Trace"
Write-Host "Size:    $sizeGB GB"
Write-Host "(Insights may take a few minutes to load and digest this trace.)"

Start-Process -FilePath $Insights -ArgumentList "`"$Trace`""
