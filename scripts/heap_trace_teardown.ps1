<#
.SYNOPSIS
    Disables ETW heap stack tracing for UnrealEditor.exe.
    Run after you're done capturing to avoid keeping the registry flag set.
#>

$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]"Administrator")
if (-not $isAdmin) {
    Write-Host "Not running as admin; relaunching elevated..."
    Start-Process -FilePath "powershell.exe" -ArgumentList "-ExecutionPolicy","Bypass","-File","`"$PSCommandPath`"" -Verb RunAs
    exit 0
}

& wpr -heaptracingconfig UnrealEditor.exe off
Write-Host "Heap tracing config removed for UnrealEditor.exe."
Read-Host "Press Enter to close"
