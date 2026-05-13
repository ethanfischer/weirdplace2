<#
.SYNOPSIS
    Takes a series of timestamped VMMap snapshots of UnrealEditor.exe at fixed
    intervals. Used for controlled A/B experiments where the user toggles one
    variable per interval.

.DESCRIPTION
    Writes snapNN_<timestamp>.mmp files to Saved/Profiling/VMMap/series/. Logs
    each snap to series.log with the wall-clock time so phases can be aligned
    with user actions.

.PARAMETER IntervalMinutes
    Minutes between snapshots. Default 5.

.PARAMETER Count
    Total snapshots to take (including snap00 at start). Default 6.
#>

param(
    [int]$IntervalMinutes = 5,
    [int]$Count = 6
)

$Vmmap = "C:\Users\ethan\Tools\Sysinternals\vmmap64.exe"
if (-not (Test-Path $Vmmap)) { Write-Error "vmmap64.exe not found"; exit 1 }

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$OutDir = Join-Path $ProjectRoot "Saved\Profiling\VMMap\series"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }
$Log = Join-Path $OutDir "series.log"
Remove-Item $Log -ErrorAction SilentlyContinue

function Log {
    param([string]$Msg)
    $line = "[" + (Get-Date -Format "HH:mm:ss") + "] $Msg"
    Write-Host $line
    Add-Content -Path $Log -Value $line -Encoding utf8
}

function Take-Snap {
    param([string]$Path, [int]$Pid_)
    Remove-Item $Path -ErrorAction SilentlyContinue
    Start-Process -FilePath $Vmmap -ArgumentList "-accepteula","-p",$Pid_,"`"$Path`""
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path $Path) {
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

Log "Snapshot series starting. Interval=${IntervalMinutes}min Count=$Count"

$proc = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue
if (-not $proc) { Log "UE not running"; exit 1 }
if ($proc -is [array]) { Log "Multiple UE instances"; exit 1 }
$pid_ue = $proc.Id
Log "UE PID=$pid_ue"

for ($i = 0; $i -lt $Count; $i++) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $idx = "{0:D2}" -f $i
    $snap = Join-Path $OutDir "snap${idx}_${stamp}.mmp"
    Log "Taking snap$idx -> $snap"
    $ok = Take-Snap -Path $snap -Pid_ $pid_ue
    if (-not $ok) { Log "  FAILED (vmmap may need admin or UE gone)"; exit 1 }
    Log ("  size: {0:N0} bytes" -f (Get-Item $snap).Length)

    if ($i -lt $Count - 1) {
        $sleepSec = $IntervalMinutes * 60
        Log "  sleeping ${IntervalMinutes}min until next snap"
        Start-Sleep -Seconds $sleepSec
        # Verify UE still alive.
        if (-not (Get-Process -Id $pid_ue -ErrorAction SilentlyContinue)) { Log "  UE exited"; exit 1 }
    }
}

Log "Series complete. Files in $OutDir"
