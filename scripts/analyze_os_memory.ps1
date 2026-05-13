<#
.SYNOPSIS
    Summarizes the overnight OS memory CSV produced by monitor_os_memory.ps1.

.DESCRIPTION
    Prints first sample, last sample, deltas, and peak system commit. Quick
    way to answer "did the OS run out of commit" without scrolling thousands
    of rows. Pass -Csv to point at a specific file; otherwise the most recent
    file under Saved/Profiling/OSMemory/ is used.
#>

param(
    [string]$Csv
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

if (-not $Csv) {
    $dir = Join-Path $ProjectRoot "Saved\Profiling\OSMemory"
    if (-not (Test-Path $dir)) {
        Write-Error "No OS memory CSVs at $dir. Did monitor_os_memory.ps1 run?"
        exit 1
    }
    $latest = Get-ChildItem -Path $dir -Filter "os_memory_*.csv" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $latest) {
        Write-Error "No os_memory_*.csv files in $dir."
        exit 1
    }
    $Csv = $latest.FullName
}

Write-Host "File: $Csv"
$rows = Import-Csv -Path $Csv
if ($rows.Count -lt 2) {
    Write-Error "Need at least 2 rows; got $($rows.Count)."
    exit 1
}
Write-Host "Rows: $($rows.Count)"
Write-Host ""

$first = $rows[0]
$last  = $rows[-1]

function ToNum($v) {
    if ([string]::IsNullOrWhiteSpace($v)) { return $null }
    return [double]$v
}

$cols = @(
    "SystemCommitUsedMB",
    "SystemCommitLimitMB",
    "PageFileUsedMB",
    "UE_WorkingSetMB",
    "UE_PrivateBytesMB",
    "UE_VirtualMB",
    "UE_Handles",
    "UE_Threads"
)

"{0,-22} {1,15} {2,15} {3,15}" -f "Metric", "First", "Last", "Delta" | Write-Host
"-" * 70 | Write-Host
foreach ($col in $cols) {
    $a = ToNum $first.$col
    $b = ToNum $last.$col
    if ($null -eq $a -or $null -eq $b) {
        "{0,-22} {1,15} {2,15} {3,15}" -f $col, "(n/a)", "(n/a)", "" | Write-Host
        continue
    }
    $delta = $b - $a
    $sign = if ($delta -ge 0) { "+" } else { "" }
    $deltaStr = "{0}{1:N1}" -f $sign, $delta
    "{0,-22} {1,15:N1} {2,15:N1} {3,15}" -f $col, $a, $b, $deltaStr | Write-Host
}

Write-Host ""
# Peaks.
$peakCommit = ($rows | ForEach-Object { ToNum $_.SystemCommitUsedMB } | Where-Object { $_ -ne $null } | Measure-Object -Maximum).Maximum
$peakUE     = ($rows | ForEach-Object { ToNum $_.UE_PrivateBytesMB }  | Where-Object { $_ -ne $null } | Measure-Object -Maximum).Maximum
$limit      = ToNum $last.SystemCommitLimitMB

Write-Host ("Peak system commit:    {0:N1} MB" -f $peakCommit)
if ($limit) {
    $pct = ($peakCommit / $limit) * 100.0
    Write-Host ("Commit limit at end:   {0:N1} MB  ({1:N1}% peak)" -f $limit, $pct)
}
Write-Host ("Peak UE private bytes: {0:N1} MB" -f $peakUE)

# Detect editor disappearance (UE columns going blank).
$ueDropoff = $null
for ($i = 0; $i -lt $rows.Count; $i++) {
    if ([string]::IsNullOrWhiteSpace($rows[$i].UE_PrivateBytesMB)) {
        $ueDropoff = $rows[$i].Timestamp
        break
    }
}
if ($ueDropoff) {
    Write-Host ""
    Write-Host "WARNING: UnrealEditor.exe was not running at $ueDropoff and possibly later."
}
