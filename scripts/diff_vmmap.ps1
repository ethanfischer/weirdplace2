<#
.SYNOPSIS
    Diffs two VMMap .mmp snapshots and prints commit deltas by memory type.

.DESCRIPTION
    Each .mmp is XML with <Region Type="..." Commit="..." .../> entries.
    Aggregates committed bytes per Type, computes snap2 - snap1, sorts
    descending. The dominant growth Type tells us what kind of Windows
    memory is leaking.
#>

param(
    [string]$Snap1,
    [string]$Snap2
)

$ErrorActionPreference = "Stop"

if (-not $Snap1 -or -not $Snap2) {
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $ProjectRoot = Split-Path -Parent $ScriptDir
    $Dir = Join-Path $ProjectRoot "Saved\Profiling\VMMap"
    if (-not $Snap1) {
        $Snap1 = (Get-ChildItem -Path $Dir -Filter "snap1_*.mmp" | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
    }
    if (-not $Snap2) {
        $Snap2 = (Get-ChildItem -Path $Dir -Filter "snap2_*.mmp" | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
    }
}

Write-Host "Loading $Snap1 ..."
$xmlA = [xml](Get-Content -Path $Snap1 -Raw)
$snapA = $xmlA.root.Snapshots.Snapshot
if ($snapA -is [array]) { $snapA = $snapA[0] }
$byTypeA = @{}
foreach ($r in $snapA.MemoryRegions.Region) {
    $t = $r.Type
    $c = [int64]$r.Commit
    if (-not $byTypeA.ContainsKey($t)) { $byTypeA[$t] = [int64]0 }
    $byTypeA[$t] += $c
}
$tsA = $snapA.Timestamp

Write-Host "Loading $Snap2 ..."
$xmlB = [xml](Get-Content -Path $Snap2 -Raw)
$snapB = $xmlB.root.Snapshots.Snapshot
if ($snapB -is [array]) { $snapB = $snapB[0] }
$byTypeB = @{}
foreach ($r in $snapB.MemoryRegions.Region) {
    $t = $r.Type
    $c = [int64]$r.Commit
    if (-not $byTypeB.ContainsKey($t)) { $byTypeB[$t] = [int64]0 }
    $byTypeB[$t] += $c
}
$tsB = $snapB.Timestamp

$t1 = [datetime]::FromFileTime([int64]$tsA)
$t2 = [datetime]::FromFileTime([int64]$tsB)
$elapsedMin = [math]::Round(($t2 - $t1).TotalMinutes, 1)
Write-Host ""
Write-Host "Snap1: $t1"
Write-Host "Snap2: $t2"
Write-Host "Elapsed: $elapsedMin min"
Write-Host ""

$types = @($byTypeA.Keys) + @($byTypeB.Keys) | Sort-Object -Unique
$rows = @()
foreach ($t in $types) {
    $a = if ($byTypeA.ContainsKey($t)) { $byTypeA[$t] } else { 0 }
    $b = if ($byTypeB.ContainsKey($t)) { $byTypeB[$t] } else { 0 }
    $delta = $b - $a
    $rows += [pscustomobject]@{
        Type     = $t
        StartMB  = [math]::Round($a / 1MB, 1)
        EndMB    = [math]::Round($b / 1MB, 1)
        DeltaMB  = [math]::Round($delta / 1MB, 1)
    }
}

$rows = $rows | Sort-Object -Property DeltaMB -Descending

Write-Host ("{0,-22} {1,14} {2,14} {3,14}" -f "Type", "Start MB", "End MB", "Delta MB")
Write-Host ("-" * 68)
foreach ($r in $rows) {
    $sign = if ($r.DeltaMB -ge 0) { "+" } else { "" }
    $deltaStr = "{0}{1:N1}" -f $sign, $r.DeltaMB
    Write-Host ("{0,-22} {1,14:N1} {2,14:N1} {3,14}" -f $r.Type, $r.StartMB, $r.EndMB, $deltaStr)
}

$totalDelta = ($rows | Measure-Object -Property DeltaMB -Sum).Sum
Write-Host ""
if ($elapsedMin -gt 0) {
    Write-Host ("Total committed delta: {0:N1} MB over {1} min  ({2:N1} MB/min)" -f $totalDelta, $elapsedMin, ($totalDelta/$elapsedMin))
}
