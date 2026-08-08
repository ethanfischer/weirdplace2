param(
    [switch]$Headless
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$UProject = Join-Path $RepoRoot 'weirdplace2.uproject'
$ProbeScript = Join-Path $RepoRoot 'scripts\ue_remote_exec.py'

if ($Headless) {
    $EditorExe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    # Fab is enabled in the .uproject, but its browser tab auto-restores at startup and
    # CreateBrowserWindow returns null without a renderer -> FFabBrowser::OpenTab asserts
    # (exit 3). Headless never needs Fab, so disable it here. The GUI launch keeps Fab.
    $ExtraArgs = ' -DisablePlugins=Fab'
} else {
    $EditorExe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
    $ExtraArgs = ''
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

$proc = Start-Process -FilePath $EditorExe -ArgumentList "`"$UProject`"$ExtraArgs" -PassThru
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
