<#
.SYNOPSIS
    Opens the captured heap ETL in Windows Performance Analyzer with the right
    symbol paths set up. Use this when you return to analyze the leak callstacks.

.DESCRIPTION
    xperf's CLI heap analysis chokes on .NET CLR events from background Rider
    processes that were running alongside UE during the capture (HRESULT
    0x80070032 on a CLR event xperf doesn't decode). WPA's GUI handles those
    events without issue, so we use WPA instead.

    Set _NT_SYMBOL_PATH so WPA can resolve module+function names for
    UnrealEditor.exe and its DLLs.
#>

param(
    [string]$Etl
)

$ErrorActionPreference = "Stop"

if (-not $Etl) {
    $dir = "C:\Users\ethan\repos\weirdplace2\Saved\Profiling\Heap"
    $latest = Get-ChildItem -Path $dir -Filter "heap_*.etl" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $latest) { Write-Error "No heap_*.etl under $dir"; exit 1 }
    $Etl = $latest.FullName
}

$cache = "C:\Symbols"
if (-not (Test-Path $cache)) { New-Item -ItemType Directory -Path $cache -Force | Out-Null }

# Order matters: local PDB folders first, MS symbol server last (slowest).
$paths = @(
    "C:\Users\ethan\repos\weirdplace2\Binaries\Win64",
    "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64",
    "$Etl.EmbeddedPdbs",
    "$Etl.NGENPDB",
    "SRV*$cache*https://msdl.microsoft.com/download/symbols"
) -join ";"
$env:_NT_SYMBOL_PATH = $paths

Write-Host "Opening $Etl in WPA..."
Write-Host "Symbol path: $env:_NT_SYMBOL_PATH"
Write-Host ""
Write-Host "Once WPA loads (may take 1-3 min for symbols), do this:"
Write-Host "  1. Trace -> Load Symbols (wait for it to finish - status bar will say 'Idle')"
Write-Host "  2. Graph Explorer (left panel) -> Memory -> Heap Allocations"
Write-Host "  3. Drag the 'Outstanding Size by Process, Stack' graph to the analysis area"
Write-Host "  4. In the table, filter Process to UnrealEditor.exe, sort 'Outstanding Size' descending"
Write-Host "  5. Expand the top stack - that callstack is the leak."
Write-Host ""

$wpa = "C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\wpa.exe"
Start-Process -FilePath $wpa -ArgumentList ("`"$Etl`"")
