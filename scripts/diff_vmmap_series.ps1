<#
.SYNOPSIS
    Diffs adjacent snapshots in a VMMap series and reports the leak rate per
    interval. Use after snap_vmmap_series.ps1 has produced multiple snaps.
#>

$Dir = "C:\Users\ethan\repos\weirdplace2\Saved\Profiling\VMMap\series"
$snaps = Get-ChildItem -Path $Dir -Filter "snap*.mmp" | Sort-Object Name
if ($snaps.Count -lt 2) { Write-Error "Need >=2 snaps in $Dir"; exit 1 }

function Aggregate-Heap {
    param([string]$Path)
    $xml = [xml](Get-Content -Path $Path -Raw)
    $snap = $xml.root.Snapshots.Snapshot
    if ($snap -is [array]) { $snap = $snap[0] }
    $heapBytes = [int64]0
    $totalCommit = [int64]0
    foreach ($r in $snap.MemoryRegions.Region) {
        $c = [int64]$r.Commit
        $totalCommit += $c
        if ($r.Type -like "Heap*") { $heapBytes += $c }
    }
    return @{ Heap = $heapBytes; Total = $totalCommit; Timestamp = $snap.Timestamp; Path = $Path }
}

$series = @()
foreach ($s in $snaps) {
    $series += @(Aggregate-Heap -Path $s.FullName)[0]
}

Write-Host ("{0,-8} {1,-20} {2,12} {3,12} {4,12} {5,12} {6,12}" -f "Snap","Time","HeapMB","HeapDelta","TotalMB","TotDelta","Rate (MB/min)")
Write-Host ("-" * 95)

for ($i = 0; $i -lt $series.Count; $i++) {
    $r = $series[$i]
    $t = [datetime]::FromFileTime([int64]$r.Timestamp)
    $heapMB = [math]::Round($r.Heap / 1MB, 1)
    $totMB  = [math]::Round($r.Total / 1MB, 1)
    $label = "snap{0:D2}" -f $i
    if ($i -eq 0) {
        Write-Host ("{0,-8} {1,-20} {2,12:N1} {3,12} {4,12:N1} {5,12} {6,12}" -f $label, $t.ToString("HH:mm:ss"), $heapMB, "(base)", $totMB, "(base)", "-")
    } else {
        $prev = $series[$i-1]
        $heapDelta = ($r.Heap - $prev.Heap) / 1MB
        $totDelta = ($r.Total - $prev.Total) / 1MB
        $minElapsed = ([datetime]::FromFileTime([int64]$r.Timestamp) - [datetime]::FromFileTime([int64]$prev.Timestamp)).TotalMinutes
        $rate = if ($minElapsed -gt 0) { $heapDelta / $minElapsed } else { 0 }
        $hSign = if ($heapDelta -ge 0) { "+" } else { "" }
        $tSign = if ($totDelta -ge 0) { "+" } else { "" }
        $hd = "{0}{1:N1}" -f $hSign, $heapDelta
        $td = "{0}{1:N1}" -f $tSign, $totDelta
        Write-Host ("{0,-8} {1,-20} {2,12:N1} {3,12} {4,12:N1} {5,12} {6,12:N1}" -f $label, $t.ToString("HH:mm:ss"), $heapMB, $hd, $totMB, $td, $rate)
    }
}
