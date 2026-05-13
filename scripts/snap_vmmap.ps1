<#
.SYNOPSIS
    Takes two VMMap snapshots of UnrealEditor.exe separated by an interval.

.DESCRIPTION
    Captures snap1, sleeps, captures snap2, then prints a one-liner showing
    how to open the comparison in VMMap's GUI.

    Snapshots go to Saved/Profiling/VMMap/. Use VMMap GUI's File → Open and
    File → Compare to diff them; the Type column tells us which Windows
    memory category leaked (Heap / Private Data / Image / Mapped File / etc.).

.PARAMETER WaitMinutes
    Minutes between the two snapshots. Default 15.
#>

param(
    [int]$WaitMinutes = 15
)

$ErrorActionPreference = "Stop"

$Vmmap = "C:\Users\ethan\Tools\Sysinternals\vmmap64.exe"
if (-not (Test-Path $Vmmap)) {
    Write-Error "vmmap64.exe not found at $Vmmap"
    exit 1
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$OutDir = Join-Path $ProjectRoot "Saved\Profiling\VMMap"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$proc = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue
if (-not $proc) {
    Write-Error "UnrealEditor.exe is not running. Launch it from Rider first."
    exit 1
}
if ($proc -is [array]) {
    Write-Error "Multiple UnrealEditor.exe instances found. Close all but one."
    exit 1
}
$pid_ue = $proc.Id
Write-Host "Found UnrealEditor.exe PID $pid_ue"

function Take-Snapshot {
    param([string]$Path, [int]$Pid_, [int]$TimeoutSec = 30)
    Remove-Item $Path -ErrorAction SilentlyContinue
    Start-Process -FilePath $Vmmap -ArgumentList "-accepteula","-p",$Pid_,"`"$Path`""
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path $Path) {
            # File exists; wait briefly for write to complete then verify size is stable.
            Start-Sleep -Milliseconds 500
            $sz1 = (Get-Item $Path).Length
            Start-Sleep -Milliseconds 500
            $sz2 = (Get-Item $Path).Length
            if ($sz1 -eq $sz2 -and $sz1 -gt 0) { return $true }
        }
        Start-Sleep -Milliseconds 500
    }
    return $false
}

$stamp1 = Get-Date -Format "yyyyMMdd_HHmmss"
$snap1 = Join-Path $OutDir "snap1_$stamp1.mmp"
Write-Host "Taking snap1 -> $snap1"
if (-not (Take-Snapshot -Path $snap1 -Pid_ $pid_ue)) {
    Write-Error "snap1 was not written within timeout. Try running as Admin."
    exit 1
}
Write-Host ("  size: {0:N0} bytes" -f (Get-Item $snap1).Length)

$sleepSec = $WaitMinutes * 60
Write-Host "Sleeping $WaitMinutes minutes..."
Start-Sleep -Seconds $sleepSec

# Verify UE is still alive.
$proc2 = Get-Process -Id $pid_ue -ErrorAction SilentlyContinue
if (-not $proc2) {
    Write-Error "UnrealEditor.exe exited during the wait. No snap2 taken."
    exit 1
}

$stamp2 = Get-Date -Format "yyyyMMdd_HHmmss"
$snap2 = Join-Path $OutDir "snap2_$stamp2.mmp"
Write-Host "Taking snap2 -> $snap2"
if (-not (Take-Snapshot -Path $snap2 -Pid_ $pid_ue)) {
    Write-Error "snap2 was not written within timeout."
    exit 1
}
Write-Host ("  size: {0:N0} bytes" -f (Get-Item $snap2).Length)

Write-Host ""
Write-Host "Done. To compare:"
Write-Host "  1. Launch VMMap: $Vmmap"
Write-Host "  2. File -> Open... -> select $snap2"
Write-Host "  3. View -> Compare -> select $snap1 as the baseline"
Write-Host "  4. Sort by 'Size' delta descending; check the 'Type' column."
