<#
.SYNOPSIS
    Samples system-wide and UnrealEditor.exe memory once per minute.

.DESCRIPTION
    Run before walking away for the night. Writes a CSV to
    Saved/Profiling/OSMemory/os_memory_<timestamp>.csv with:
      Timestamp, SystemCommitUsedMB, SystemCommitLimitMB, PageFileUsedMB,
      UE_WorkingSetMB, UE_PrivateBytesMB, UE_VirtualMB, UE_Handles, UE_Threads.

    Stops on Ctrl-C. Intended to be run in its own PowerShell window.

.PARAMETER IntervalSeconds
    Sampling interval. Defaults to 60.

.PARAMETER ProcessName
    Target process name (no .exe). Defaults to UnrealEditor. If the editor
    crashes mid-night the UE columns will be empty for those rows, which is
    itself useful signal.
#>

param(
    [int]$IntervalSeconds = 60,
    [string]$ProcessName = "UnrealEditor"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$OutDir = Join-Path $ProjectRoot "Saved\Profiling\OSMemory"
if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$OutFile = Join-Path $OutDir "os_memory_$Stamp.csv"

"Timestamp,SystemCommitUsedMB,SystemCommitLimitMB,PageFileUsedMB,UE_WorkingSetMB,UE_PrivateBytesMB,UE_VirtualMB,UE_Handles,UE_Threads" |
    Out-File -FilePath $OutFile -Encoding utf8

Write-Host "OS memory sampler started."
Write-Host "  Output:   $OutFile"
Write-Host "  Interval: ${IntervalSeconds}s"
Write-Host "  Process:  $ProcessName"
Write-Host "Ctrl-C to stop."
Write-Host ""

# Cache perf counter paths; resolving them is cheap but not free.
$CommitUsedCounter  = "\Memory\Committed Bytes"
$CommitLimitCounter = "\Memory\Commit Limit"
$PageFileCounter    = "\Paging File(_Total)\% Usage"

while ($true) {
    $now = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

    $commitUsedMB  = ""
    $commitLimitMB = ""
    $pageFileMB    = ""
    try {
        $counters = Get-Counter -Counter $CommitUsedCounter, $CommitLimitCounter -ErrorAction Stop
        $commitUsedMB  = [math]::Round($counters.CounterSamples[0].CookedValue / 1MB, 1)
        $commitLimitMB = [math]::Round($counters.CounterSamples[1].CookedValue / 1MB, 1)
    } catch {
        Write-Host "  [warn] commit counter failed: $($_.Exception.Message)"
    }
    try {
        # PageFile % Usage * size = bytes used. Use Win32_PageFileUsage for direct MB.
        $pf = Get-CimInstance -ClassName Win32_PageFileUsage -ErrorAction Stop
        if ($pf) {
            $pageFileMB = ($pf | Measure-Object -Property CurrentUsage -Sum).Sum
        }
    } catch {
        Write-Host "  [warn] pagefile query failed: $($_.Exception.Message)"
    }

    $workingMB = ""
    $privateMB = ""
    $virtualMB = ""
    $handles   = ""
    $threads   = ""
    $proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
    if ($proc) {
        # If multiple instances, sum them.
        $workingMB = [math]::Round((($proc | Measure-Object WorkingSet64 -Sum).Sum) / 1MB, 1)
        $privateMB = [math]::Round((($proc | Measure-Object PrivateMemorySize64 -Sum).Sum) / 1MB, 1)
        $virtualMB = [math]::Round((($proc | Measure-Object VirtualMemorySize64 -Sum).Sum) / 1MB, 1)
        $handles   = ($proc | Measure-Object HandleCount -Sum).Sum
        # Threads is a collection per-process, not a numeric property; sum the counts manually.
        $threads = 0
        foreach ($p in $proc) { $threads += $p.Threads.Count }
    }

    $line = "$now,$commitUsedMB,$commitLimitMB,$pageFileMB,$workingMB,$privateMB,$virtualMB,$handles,$threads"
    Add-Content -Path $OutFile -Value $line -Encoding utf8

    # Brief stdout heartbeat so the window shows progress.
    Write-Host "$now  commit=${commitUsedMB}/${commitLimitMB}MB  ue_ws=${workingMB}MB  ue_priv=${privateMB}MB  handles=$handles"

    Start-Sleep -Seconds $IntervalSeconds
}
