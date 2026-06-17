param(
    [switch]$Headless
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$UProject = Join-Path $RepoRoot 'weirdplace2.uproject'
$ProbeScript = Join-Path $RepoRoot 'scripts\ue_remote_exec.py'

if ($Headless) {
    $EditorExe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
} else {
    $EditorExe = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe'
}

function Test-EditorReady {
    try {
        $out = & python $ProbeScript --code "print('READY')" --mode ExecuteStatement 2>$null
    } catch {
        return $false
    }
    if ($LASTEXITCODE -eq 0 -and ($out -match '^READY$')) {
        return $true
    }
    return $false
}

$start = Get-Date

if (Test-EditorReady) {
    Write-Output 'EDITOR ALREADY RUNNING'
    exit 0
}

$proc = Start-Process -FilePath $EditorExe -ArgumentList "`"$UProject`"" -PassThru
$pidVal = $proc.Id

for ($i = 1; $i -le 60; $i++) {
    Start-Sleep -Seconds 3
    if (Test-EditorReady) {
        $elapsed = [int]((Get-Date) - $start).TotalSeconds
        Write-Output "EDITOR READY (pid $pidVal) after ${elapsed}s"
        exit 0
    }
}

Write-Output 'EDITOR TIMEOUT after 180s'
exit 1
