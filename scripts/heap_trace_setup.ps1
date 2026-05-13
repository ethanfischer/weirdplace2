<#
.SYNOPSIS
    Enables ETW heap stack tracing for UnrealEditor.exe (one-time, persistent).

.DESCRIPTION
    Writes the user-mode stack-trace flag to the Image File Execution Options
    registry key for UnrealEditor.exe so that subsequent launches record
    callstacks for every HeapAlloc/HeapFree. Required before running
    heap_trace_capture.ps1.

    Self-elevates via UAC if not already admin.

    Run heap_trace_teardown.ps1 when you're done to remove the registry entry
    and avoid leaving heap tracing enabled.
#>

$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]"Administrator")
if (-not $isAdmin) {
    Write-Host "Not running as admin; relaunching elevated..."
    Start-Process -FilePath "powershell.exe" -ArgumentList "-ExecutionPolicy","Bypass","-File","`"$PSCommandPath`"" -Verb RunAs
    exit 0
}

Write-Host "Enabling heap stack tracing for UnrealEditor.exe..."
& wpr -heaptracingconfig UnrealEditor.exe on
if ($LASTEXITCODE -ne 0) {
    Write-Error "wpr -heaptracingconfig failed with exit code $LASTEXITCODE"
    Read-Host "Press Enter to close"
    exit 1
}

Write-Host ""
Write-Host "Done. Next steps:"
Write-Host "  1. Close UnrealEditor if running (this flag applies to NEW processes only)."
Write-Host "  2. Relaunch UE from Rider."
Write-Host "  3. Wait ~3 min for warmup."
Write-Host "  4. Run scripts\heap_trace_capture.ps1 to record a 5-min heap trace."
Write-Host ""
Read-Host "Press Enter to close"
